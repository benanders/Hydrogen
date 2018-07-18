
// parser.c
// By Ben Anderson
// July 2018

// A new `Parser` object is created for each module that we want to parse. Each
// source code file (the initially provided file and any imported ones) is
// treated as a separate module. See `Parser::parse_import`.
//
// When parsing each module, a `FunctionScope` is created for each function
// definition. The function scopes are stacked for nested function definitions
// (see `Parser::parse_fn`):
//
//   fn outer() {
//     let a = fn() { /* ... */ }
//   }
//
// The inner-most scope is on the top of the stack (the front of the linked
// list). Any instructions are emitted to the function prototype corresponding
// to this inner-most scope.
//
// An initial function scope is created for the top level of the module. This is
// considered the module's "main" function under which all top-level functions
// are nested. See `Parser::parse`.
//
// Each individual function is parsed by blocks. The body of a function is
// treated as a single block. Each block consists of a series of statements.
// Statements are things like `let` statements to define variables, function
// calls, variable re-assignments, etc. Some statements contain nested blocks,
// like `if` statements and `while` loops. These nested blocks are parsed
// recursively. See `Parser::parse_block`.
//
// Local variables within functions are added to the `locals` list on the
// parser. All locals in all nested function scopes are added to the same list.
// A local is referenced by its "stack slot" from within instructions. A local's
// stack slot is relative to the first local defined within the function scope:
//
//  fn example() {   // slot 0 in the package's main function
//    let a = 3      // slot 0 in function `example`
//    let c = fn() { // slot 1 in function `example`
//      let d = 5    // slot 0 in the anonymous function
//    }
//  }
//
// Functions keep track of the index (in the list of all locals) of the first
// local that was defined in the function's scope. Functions also keep track of
// the next stack slot that is available (i.e. not already taken by a variable
// defined by a `let` statement) within their scope. When we exit a block, all
// locals that were created in that block are destroyed.

#include "parser.h"

#include "lexer.h"
#include "err.h"

#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>

// Stores the name of a local that was created in a function definition on the
// stack. The local's slot can be determined by subtracting the first local
// in the function definition scope's index in the parser's local array, from
// this local's in the locals array.
typedef struct {
	uint64_t name;
} Local;

// Each time we encounter a new function definition, we create a new function
// definition scope (`FnScope`) and put it at the top of the parser's scope
// stack (represented as a linked list). We emit all bytecode to the top-most
// function scope on this stack.
typedef struct fn_scope {
	// Index into the VM's functions list.
	int fn;

	// The index of the first local defined in this function scope in the
	// parser's locals array.
	int first_local;

	// The index of the next available slot on the stack that we can store a
	// value into. Keeps track of the current number of both named and temporary
	// local variables.
	int next_slot;

	// The outer function definition directly containing this one, or NULL if
	// this is the top level function scope in a package.
	struct fn_scope *outer_scope;
} FnScope;

// Converts a stream of tokens from the lexer into bytecode.
typedef struct {
	HyVM *vm;

	// Supplies the stream of tokens we're parsing.
	Lexer lxr;

	// The package containing the functions we're parsing bytecode into.
	int pkg;

	// Linked list of function definition scopes. The inner-most function scope
	// (to which we emit bytecode) is at the top of the stack.
	FnScope *scope;

	// Stores a list of all named local variables within all active function
	// definition scopes.
	Local *locals;
	int locals_count, locals_capacity;
} Parser;

// Create a new parser.
static Parser psr_new(HyVM *vm, int pkg, char *path, char *code) {
	Parser psr;
	psr.vm = vm;
	psr.lxr = lex_new(vm, path, code);
	psr.pkg = pkg;
	psr.scope = NULL;
	psr.locals_capacity = 16;
	psr.locals_count = 0;
	psr.locals = malloc(sizeof(Local) * psr.locals_capacity);
	return psr;
}

// Free resources allocated by a parser.
static void psr_free(Parser *psr) {
	free(psr->locals);
}

// Returns a pointer to the package we're parsing.
static inline Package * psr_pkg(Parser *psr) {
	return &psr->vm->pkgs[psr->pkg];
}

// Returns a pointer to the function we're currently emitting bytecode to.
static inline Function * psr_fn(Parser *psr) {
	return &psr->vm->fns[psr->scope->fn];
}

// Trigger a new error at the lexer's current token's line number.
static void psr_trigger_err(Parser *psr, char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	HyErr *err = err_vnew(fmt, args);
	va_end(args);

	err_set_file(err, psr->lxr.path);
	err->line = psr->lxr.tk.line;
	err_trigger(psr->vm, err);
}

// Creates a new local in the parser's locals list.
static void psr_new_local(Parser *psr, uint64_t name) {
	if (psr->locals_count >= psr->locals_capacity) {
		psr->locals_capacity *= 2;
		psr->locals = realloc(psr->locals, sizeof(Local) * psr->locals_capacity);
	}
	psr->locals[psr->locals_count++].name = name;
}

// Possible types of expression nodes (operands in an expression).
//
// We differentiate between "pre-discharged" and "discharged" operands. A
// pre-discharged operand is a raw value (e.g. a number, a local). A discharged
// operand represents the value of an operand in an expression after it it has
// been used as an operand in a bytecode instruction. Operands are "discharged"
// right before they're used in instructions.
//
// `NON_RELOC` (non-relocatable) values are used to represent references to
// fixed stack slots. `RELOC` (relocatable) values reference emitted
// instructions that have to be backpatched with a destination stack slot into
// which they will store their result. The backpatching is done when the
// expression is assigned a specific stack slot through one of the functions
// `expr_to_any_slot` or `expr_to_next_slot`.
//
// The pre-discharged and discharged operands must be kept together in one
// enum, since when we go to emit a bytecode instruction, we might want to
// output either a discharged value (e.g. if we emit an instruction that stores
// a value to a stack slot), or a pre-discharged value (e.g. if we fold the
// result of the operation to create another number).
typedef enum {
	NODE_NUM,
	NODE_LOCAL,

	NODE_CONST,
	NODE_RELOC,
	NODE_NON_RELOC,
} NodeType;

// An expression node (an operand to an operator).
typedef struct {
	NodeType type;
	union {
		// Number that hasn't been assigned a constant index in the VM yet.
		double num;

		// The slot for a local, or the slot in which a non-relocatable value is
		// stored.
		uint8_t slot;

		// The index of a discharged constant in the VM's constants array.
		uint16_t const_idx;

		// The PC of a relocatable instruction.
		int reloc_pc;
	};
} Node;

// Binary operator precedence, in numerical order from lowest to highest
// precedence.
typedef enum {
	PREC_NONE,
	PREC_OR,      // `||`
	PREC_AND,     // `&&`
	PREC_EQ,      // `==`, `!=`
	PREC_ORD,     // `>`, `>=`, `<`, `<=`
	PREC_CONCAT,  // `..`
	PREC_ADD,     // `+`, `-`
	PREC_MUL,     // `*`, `/`, `%`
	PREC_UNARY,   // `-`
	PREC_POSTFIX, // Function calls, array indexing, struct field access
} Precedence;

// Forward declaration.
static Node parse_subexpr(Parser *psr, Precedence minimum);

// Returns the precedence of a binary operator, or -1 if the token can't be
// converted to a binary operator.
static int binop_prec(Tk binop) {
	switch (binop) {
	case TK_OR:
		return PREC_OR;
	case TK_AND:
		return PREC_AND;
	case TK_EQ: case TK_NEQ:
		return PREC_EQ;
	case '>': case '<': case TK_GE: case TK_LE:
		return PREC_ORD;
	case TK_CONCAT:
		return PREC_CONCAT;
	case '+': case '-':
		return PREC_ADD;
	case '*': case '/':
		return PREC_MUL;
	default:
		return -1;
	}
}

// Returns true if the binary operator is an arithemtic operator.
static inline bool binop_is_arith(Tk binop) {
	return binop == '+' || binop == '-' || binop == '*' || binop == '/';
}

// Returns true if the binary operator is commutative.
static inline bool binop_is_commutative(Tk binop) {
	return binop == '+' || binop == '*';
}

// Returns the base opcode to use for a binary operator.
static inline Opcode binop_opcode(Tk binop) {
	switch (binop) {
		case '+': return OP_ADD_LL;
		case '-': return OP_SUB_LL;
		case '*': return OP_MUL_LL;
		case '/': return OP_DIV_LL;
		// Unreachable
		default: assert(false);
	}
}

// Returns true if an expression node is a constant.
static inline bool node_is_const(Node *node) {
	return node->type == NODE_NUM || node->type == NODE_CONST;
}

// Returns the precedence of a unary operator, or -1 if the token isn't a valid
// unary operator.
static int unop_prec(Tk unop) {
	switch (unop) {
	case '-': case '!':
		return PREC_UNARY;
	default:
		return -1;
	}
}

// If the given expression node is non-relocatable and holds the top-most slot
// on the stack, then this function releases the slot so that it can be re-used
// immediately.
static void expr_free_node(Parser *psr, Node *node) {
	// The number of locals assigned a name (i.e. that are not temporary) in the
	// current function definition scope
	int active_locals = psr->locals_count - psr->scope->first_local;

	// Check the node is a non-reloc and is temporary
	if (node->type == NODE_NON_RELOC && node->slot >= active_locals) {
		// Free the top most stack slot
		psr->scope->next_slot--;

		// Make sure we actually freed the temporary local that was on top of
		// the stack
		assert(node->slot == psr->scope->next_slot);
	}
}

// Convert a pre-discharged expression operand into a discharged one. Adds
// constants to the VM's constants list, etc.
static void expr_discharge(Parser *psr, Node *node) {
	switch (node->type) {
	case NODE_NUM:
		// Check we don't exceed the maximum number of allowed constants
		if (psr->vm->consts_count >= USHRT_MAX) {
			psr_trigger_err(psr, "too many constants");
			assert(false);
		}
		node->type = NODE_CONST;
		node->const_idx = (uint16_t) vm_add_const_num(psr->vm, node->num);
		break;
	case NODE_LOCAL:
		node->type = NODE_NON_RELOC;
		// Slot value remains the same
		break;
	default:
		// All other node values are already discharged
		break;
	}
}

// Puts an operand into the given stack slot.
static void expr_to_slot(Parser *psr, uint8_t dest, Node *node) {
	// Only deal with discharged values
	expr_discharge(psr, node);
	switch (node->type) {
	case NODE_NON_RELOC:
		// Only emit a MOV if the destination is different from the source slot
		if (node->slot != dest) {
			Instruction ins = ins_new2(OP_MOV, dest, node->slot);
			fn_emit(psr_fn(psr), ins);
		}
		break;
	case NODE_RELOC: {
		// Modify the destination stack slot for the relocatable instruction
		Instruction *ins = &psr_fn(psr)->ins[node->reloc_pc];
		ins_set_arg1(ins, dest);
		break;
	}
	case NODE_CONST: {
		// The only constant type we have at the moment is a number
		Instruction ins = ins_new2(OP_SET_N, dest, node->const_idx);
		fn_emit(psr_fn(psr), ins);
		break;
	}
	default:
		// Unreachable
		assert(false);
	}

	// `node` is now a non-reloc in a specific slot
	node->type = NODE_NON_RELOC;
	node->slot = dest;
}

// Puts an operand into the NEXT available stack slot. Returns this stack slot.
// * Non-relocatables: a MOV instruction is emitted if necessary
// * Everything else: the required SET instruction is emitted
static uint8_t expr_to_next_slot(Parser *psr, Node *node) {
	// Only deal with discharged values
	expr_discharge(psr, node);

	// Free the node on top of the stack, so we can re-use it
	expr_free_node(psr, node);

	// Allocate a new slot on top of the stack for storing the node into
	uint8_t slot = psr->scope->next_slot;

	// Check we don't overflow the stack with too many locals
	if (slot >= 255) {
		psr_trigger_err(psr, "too many locals in function");
		assert(false);
	}

	// Advance the next slot counter and store the local
	psr->scope->next_slot++;
	expr_to_slot(psr, slot, node);
	return slot;
}

// Puts an operand into any stack slot (i.e. we don't care where). Returns this
// stack slot.
// * Non-relocatables: left in the stack slot they're already in
// * Everything else: put into the next available stack slot
static uint8_t expr_to_any_slot(Parser *psr, Node *node) {
	// Only deal with discharged values
	expr_discharge(psr, node);
	switch (node->type) {
	case NODE_NON_RELOC:
		return node->slot;
	default:
		return expr_to_next_slot(psr, node);
	}
}

// Converts an operand into an 8 bit value that can be used as an argument to an
// instruction.
// * Constants: if the constant index fits into an 8 bit value, then this index
//   is returned. Otherwise the constant is put into a stack slot
// * Non-relocatables: returns their slot as an 8 bit value
// * Everything else: put into the next available slot
static uint8_t expr_to_ins_arg(Parser *psr, Node *node) {
	// Only deal with discharged values
	expr_discharge(psr, node);
	switch (node->type) {
	case NODE_CONST:
		if (node->const_idx < 256) {
			return (uint8_t) node->const_idx;
		}
	case NODE_NON_RELOC:
		return node->slot;
	default:
		return expr_to_next_slot(psr, node);
	}
}

// Attempt to fold an arithmetic operation. Returns true on success and modifies
// `left` to contain the folded value.
static bool expr_fold_arith(Parser *psr, Tk binop, Node *left, Node right) {
	// TODO: arithmetic folding
	return false;
}

// Emit bytecode for a binary arithmetic operation.
static void expr_emit_arith(Parser *psr, Tk binop, Node *left, Node right) {
	// Check if we can fold the arithmetic operation
	if (expr_fold_arith(psr, binop, left, right)) {
		return;
	}

	// If the arithemtic operator is commutative, we need to make sure the
	// constant number is on the right
	Node *result = left;
	Node l, r;
	if (binop_is_commutative(binop) && node_is_const(left)) {
		l = right;
		r = *left;
	} else {
		l = *left;
		r = right;
	}

	// Convert the operands into instruction args
	uint8_t larg = expr_to_ins_arg(psr, &l);
	uint8_t rarg = expr_to_ins_arg(psr, &r);

	// Assuming both left and right represent slots (and not an index into
	// the context's constants array), we need to free temporary slots.
	// Since temporary slots need to be free'd from top down, the order
	// depends on if left or right is the most recently allocated temporary
	// slot
	if (larg > rarg) {
		expr_free_node(psr, &l);
		expr_free_node(psr, &r);
	} else {
		expr_free_node(psr, &r);
		expr_free_node(psr, &l);
	}

	// Calculate the opcode to use off the binary operator
	Opcode opcode = binop_opcode(binop) + node_is_const(&r) +
		node_is_const(&l) * 2;

	// Generate the relocatable bytecode instruction
	Instruction ins = ins_new3(opcode, 0, larg, rarg);
	int pc = fn_emit(psr_fn(psr), ins);

	// Set the result as a relocatable node
	result->type = NODE_RELOC;
	result->reloc_pc = pc;
}

// Emit bytecode for a binary operation. Modifies `left` in place to the result
// of the binary operation.
static void expr_emit_binary(Parser *psr, Tk binop, Node *left, Node right) {
	switch (binop) {
	case '+': case '-': case '*': case '/':
		expr_emit_arith(psr, binop, left, right);
		break;
	default:
		// Unreachable
		assert(false);
	}
}

// Emit bytecode for the left operand to a binary expression, before the right
// operand is parsed. Modifies `operand` in place.
static void expr_emit_binary_left(Parser *psr, Tk binop, Node *left) {
	// All instruction arguments need to fit into a uint8_t. There's some
	// exceptions to this, which are all handled individually below.

	// There's specialised instructions for arithmetic with numbers and locals
	if (binop_is_arith(binop) && left->type == NODE_NUM) {
		return;
	}

	// Otherwise, ensure the node is usable as an instruction argument
	expr_to_ins_arg(psr, left);
}

// Emit bytecode for a unary negation operation.
static void expr_emit_unary_neg(Parser *psr, Node *operand) {
	// Check if we can fold the operation
	if (operand->type == NODE_NUM) {
		operand->num = -operand->num;
		return;
	}

	// Convert the operand to a stack slot that we can negate (since OP_NEG
	// only operates on stack slots)
	uint8_t slot = expr_to_any_slot(psr, operand);

	// Free the operand if it's on top of the stack, so we can re-use its slot
	expr_free_node(psr, operand);

	// Generate a relocatable instruction
	Instruction ins = ins_new2(OP_NEG, 0, slot);
	int pc = fn_emit(psr_fn(psr), ins);

	// The result of the negation is a relocatable instruction
	operand->type = NODE_RELOC;
	operand->reloc_pc = pc;
}

// Emit bytecode for a unary operation. Modifies `operand` in place to the
// result of the operation.
static void expr_emit_unary(Parser *psr, Tk unop, Node *operand) {
	switch (unop) {
	case '-':
		expr_emit_unary_neg(psr, operand);
		break;
	default:
		// Unreachable
		assert(false);
	}
}

// Parse a number operand.
static Node expr_operand_num(Parser *psr) {
	Node operand;
	operand.type = NODE_NUM;
	operand.num = psr->lxr.tk.num;
	lex_next(&psr->lxr);
	return operand;
}

// Parse a local operand.
static Node expr_operand_local(Parser *psr) {
	// Check the local exists
	uint64_t name = psr->lxr.tk.ident_hash;
	int slot = -1;
	for (int i = psr->scope->first_local; i < psr->locals_count; i++) {
		if (psr->locals[i].name == name) {
			slot = i - psr->scope->first_local;
			break;
		}
	}

	// Variable doesn't exist
	if (slot == -1) {
		psr_trigger_err(psr, "variable not defined");
		assert(false);
	}

	Node result;
	result.type = NODE_LOCAL;
	result.slot = slot;
	lex_next(&psr->lxr);
	return result;
}

// Parse a subexpression operand.
static Node expr_operand_subexpr(Parser *psr) {
	// Skip the opening paranthesis
	lex_next(&psr->lxr);

	// Parse the contents of the expression
	Node subexpr = parse_subexpr(psr, PREC_NONE);

	// Expect a closing parenthesis
	lex_expect(&psr->lxr, ')');
	lex_next(&psr->lxr);
	return subexpr;
}

// Parse an operand to a binary or unary operation.
static Node expr_operand(Parser *psr) {
	switch (psr->lxr.tk.type) {
	case TK_NUM:   return expr_operand_num(psr);
	case TK_IDENT: return expr_operand_local(psr);
	case '(':      return expr_operand_subexpr(psr);
	default:
		// We always call `expr_operand` expecting there to actually be an
		// operand; since we didn't find one, trigger an error
		psr_trigger_err(psr, "expected expression");
		assert(false);
	}
}

// Parse a unary operation.
static Node expr_unary(Parser *psr) {
	// Check if we have a unary operator or not
	if (unop_prec(psr->lxr.tk.type) >= 0) {
		// Skip the unary operator
		Tk unop = psr->lxr.tk.type;
		lex_next(&psr->lxr);

		// Parse the operand to the unary operator
		Node operand = parse_subexpr(psr, unop_prec(unop));

		// Emit bytecode for the operation
		expr_emit_unary(psr, unop, &operand);
		return operand;
	} else {
		// No unary operator, just parse a normal operand
		return expr_operand(psr);
	}
}

// Parse a subset of an expression, stopping once the binary operator's
// precedence is less than the given minimum.
static Node parse_subexpr(Parser *psr, Precedence minimum) {
	// Parse the left operand to the binary operation
	Node left = expr_unary(psr);

	// Keep parsing binary operators until we encounter one with a precedence
	// less than the minimum
	// We need the explicit cast to an int, or else it tries to convert -1
	// returned by `binop_prec` into an unsigned value
	while (binop_prec(psr->lxr.tk.type) > (int) minimum) {
		// Skip the binary operator token
		Tk binop = psr->lxr.tk.type;
		lex_next(&psr->lxr);

		// Some binary operations require us to emit code for the left operand
		// BEFORE we parse the right one
		expr_emit_binary_left(psr, binop, &left);

		// Parse the right operand to the binary operator
		Node right = parse_subexpr(psr, binop_prec(binop));

		// Emit bytecode for the whole binary operation
		expr_emit_binary(psr, binop, &left, right);
	}
	return left;
}

// Parse an expression.
static Node parse_expr(Parser *psr) {
	return parse_subexpr(psr, PREC_NONE);
}

// Parse an assignment.
static void parse_assign(Parser *psr) {
	// Get the name of the variable to assign to
	uint64_t name = psr->lxr.tk.ident_hash;
	lex_next(&psr->lxr);

	// Check the variable exists
	int dest = -1;
	for (int i = psr->scope->first_local; i < psr->locals_count; i++) {
		if (psr->locals[i].name == name) {
			dest = i - psr->scope->first_local;
			break;
		}
	}

	// Assignment destination doesn't exist
	if (dest == -1) {
		psr_trigger_err(psr, "variable not defined");
		assert(false);
	}

	// Check for an augmented assignment
	// TODO: augmented assignments

	// Skip the assignment token
	lex_next(&psr->lxr);

	// Expect an expression
	Node result = parse_expr(psr);

	// Put the assignment result into the correct slot
	expr_to_slot(psr, dest, &result);
}

// Parse an assignment or expression statement (we're not sure which one it is
// at this point).
static void parse_assign_or_expr(Parser *psr) {
	// Get the token after the identifier
	SavedLexer saved = lex_save(&psr->lxr);
	lex_next(&psr->lxr);
	Tk after = psr->lxr.tk.type;
	lex_restore(&psr->lxr, saved);

	// Inspect the token
	if (after == '=' || (after >= TK_ADD_ASSIGN && after <= TK_MOD_ASSIGN)) {
		parse_assign(psr);
	} else {
		// Throw away the result of the expression since we don't use it
		parse_expr(psr);
	}
}

// Parse a `let` assignment statement.
static void parse_let(Parser *psr) {
	// Skip the `let` token
	lex_next(&psr->lxr);

	// Expect an identifier
	lex_expect(&psr->lxr, TK_IDENT);
	uint64_t name = psr->lxr.tk.ident_hash;

	// Ensure another local with the same name doesn't already exist
	for (int i = psr->scope->first_local; i < psr->locals_count; i++) {
		if (psr->locals[i].name == name) {
			psr_trigger_err(psr, "variable already defined");
			assert(false);
		}
	}
	lex_next(&psr->lxr);

	// Expect `=`
	lex_expect(&psr->lxr, '=');
	lex_next(&psr->lxr);

	// Expect an expression
	Node result = parse_expr(psr);
	expr_to_next_slot(psr, &result);

	// Add a new local to the parser's locals list
	psr_new_local(psr, name);
}

// Parse a block (a sequence of statements).
static void parse_block(Parser *psr) {
	// Save the initial number of locals and the next slot, so we can discard
	// the locals created in this block once we reach the end of it
	int locals_count = psr->locals_count;
	int next_slot = psr->scope->next_slot;

	// Continually parse statements
	bool have_statement = true;
	while (have_statement) {
		switch (psr->lxr.tk.type) {
		case TK_LET:
			parse_let(psr);
			break;
		case TK_IDENT:
			parse_assign_or_expr(psr);
			break;
		case '(':
			// Throw away the result of the expression
			parse_expr(psr);
			break;
		default:
			have_statement = false;
			break;
		}
	}

	// Discard all locals created in this block
	psr->locals_count = locals_count;
	psr->scope->next_slot = next_slot;
}

// Start parsing the given source code.
static void parse_code(Parser *psr) {
	// parse the first lexer token. we leave this until now since it might
	// generate an error, which needs to be caught by the error guard that is
	// only set up just before this function is called
	lex_next(&psr->lxr);

	// Create a function scope for the top level code in the package
	FnScope top_level;
	top_level.fn = psr_pkg(psr)->main_fn;
	top_level.first_local = 0;
	top_level.next_slot = 0;
	top_level.outer_scope = NULL;
	psr->scope = &top_level;

	// Parse the package's top level source code
	parse_block(psr);

	// Add a RET instruction at the end of the package
	Instruction ret = ins_new3(OP_RET, 0, 0, 0);
	fn_emit(psr_fn(psr), ret);
}

// Parses the source code into bytecode. All bytecode for top level code gets
// appended to the package's main function. All other functions defined in the
// code get created on the VM and associated with the given package.
HyErr * parse(HyVM *vm, int pkg, char *path, char *code) {
	Parser psr = psr_new(vm, pkg, path, code);

	// Set up an error guard
	vm->err = NULL;
	if (!setjmp(vm->guard)) {
		parse_code(&psr);
	}

	psr_free(&psr);
	return vm->err;
}
