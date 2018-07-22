
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
#include "value.h"
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
	NODE_PRIM,

	NODE_CONST,
	NODE_RELOC,
	NODE_NON_RELOC,
	NODE_JMP,
} NodeType;

// `true_list` stores the absolute PC of the first jump instruction in the
// condition's true case jump list.
//
// A "jump list" is a collection of emitted JMP instructions within a function's
// bytecode. The list is stored similar to a linked list. The `true_list` value
// stores the absolute index into the bytecode array of the head of the linked
// list. The next element in the list is found by adding the jump instruction's
// offset (stored in the instruction itself) to the PC of the instruction.
//
// The head always points to the jump instruction at the LARGEST PC. Thus, all
// other instructions in the list have NEGATIVE offsets as they point to
// instructions BEFORE them in the bytecode.
//
// A conditional expression always has 2 possible outcomes - true or false.
// Different code should be executed depending on the outcome. The "true case"
// refers to the code to execute if the conditional expression evaluates to
// true, and vice versa for the "false case". The true jump list stores all jump
// instructions that should have their jump targets backpatched to point to the
// first instruction in the true case code.
//
// `false_list` stores the absolute PC of the first jump instruction in the
// condition's false case jump list.
//
// The false jump list stores all jump instructions that should have their jump
// targets backpatched to point to the first instruction in the false case code.
typedef struct {
	int true_list, false_list;
} JmpInfo;

// An expression node (an operand to an operator).
typedef struct {
	NodeType type;
	union {
		// Number that hasn't been assigned a constant index in the VM yet.
		double num;

		// The slot for a local, or the slot in which a non-relocatable value is
		// stored.
		uint8_t slot;

		// The value of a primitive (true, false, or nil).
		Primitive prim;

		// The index of a discharged constant in the VM's constants array.
		uint16_t const_idx;

		// The bytecode index of a relocatable instruction.
		int reloc_idx;

		// Information for a conditional JMP node.
		JmpInfo jmp;
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

// Returns true if the binary operator is a relational operator.
static inline bool binop_is_rel(Tk binop) {
	return binop == TK_EQ || binop == TK_NEQ || binop == '>' ||
		binop == TK_GE || binop == '<' || binop == TK_LE;
}

// Returns true if the binary operator is an order operator.
static inline bool binop_is_ord(Tk binop) {
	return binop == '>' || binop == TK_GE || binop == '<' || binop == TK_LE;
}

// Returns true if the binary operator is commutative.
static inline bool binop_is_commutative(Tk binop) {
	return binop == '+' || binop == '*' || binop == TK_EQ || binop == TK_NEQ;
}

// Returns true if an expression node is a constant.
static inline bool node_is_const(Node *node) {
	return node->type == NODE_NUM || node->type == NODE_PRIM ||
		node->type == NODE_CONST;
}

// Returns the inverted relational operator.
static Tk binop_invert_rel(Tk relop) {
	switch (relop) {
		case TK_EQ: return TK_NEQ;
		case TK_NEQ: return TK_EQ;
		case '>': return TK_LE;
		case TK_GE: return '<';
		case '<': return TK_GE;
		case TK_LE: return '>';
		default: assert(false); // Unreachable
	}
}

// Returns the inverted base opcode for a relational operation.
static Opcode relop_invert(Opcode relop) {
	switch (relop) {
		case OP_EQ_LL:    return OP_NEQ_LL;
		case OP_EQ_LN:    return OP_NEQ_LN;
		case OP_EQ_LP:    return OP_NEQ_LP;
		case OP_NEQ_LL:   return OP_EQ_LL;
		case OP_NEQ_LN:   return OP_EQ_LN;
		case OP_NEQ_LP:   return OP_EQ_LP;
		case OP_LT_LL:    return OP_GE_LL;
		case OP_LT_LN:    return OP_GE_LN;
		case OP_LE_LL:    return OP_GT_LL;
		case OP_LE_LN:    return OP_GT_LN;
		case OP_GT_LL:    return OP_LE_LL;
		case OP_GT_LN:    return OP_LE_LN;
		case OP_GE_LL:    return OP_LT_LL;
		case OP_GE_LN:    return OP_LT_LN;
		default: assert(false); // Unreachable
	}
}

// Returns the base opcode to use for a binary operator.
static Opcode binop_opcode(Tk binop) {
	switch (binop) {
		// Arithmetic operators
		case '+': return OP_ADD_LL;
		case '-': return OP_SUB_LL;
		case '*': return OP_MUL_LL;
		case '/': return OP_DIV_LL;

		// Relational operators
		case TK_EQ: return OP_EQ_LL;
		case TK_NEQ: return OP_NEQ_LL;
		case '>': return OP_GT_LL;
		case TK_GE: return OP_GE_LL;
		case '<': return OP_LT_LL;
		case TK_LE: return OP_LE_LL;

		// Unreachable
		default: assert(false);
	}
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

// Returns the absolute bytecode index of the target of a JMP instruction.
static int jmp_follow(Parser *psr, int jmp_idx) {
	if (jmp_idx < 0) {
		return jmp_idx;
	}

	// Remember, the jump offset is relative to the instruction that FOLLOWS the
	// jump instruction
	Instruction *jmp = &psr_fn(psr)->ins[jmp_idx];

	// The reason we use a jump bias is best explained by considering the
	// alternatives. The first is to use signed offsets, and the second is to
	// split forward and backward jumps into two separate opcodes.
	//
	// Signed offsets are problematic because extracting instruction arguments
	// in the assembly interpreter is done using `shr` and `movzx` instructions.
	// Both of which ignore the sign bit.
	//
	// Splitting into a forward and backward jump opcode is problematic because
	// we use `lea` instruction to perform jumps in the assembly interpreter:
	//   lea PC, [PC + (RC - JMP_BIAS) * 4]
	// We use `lea` because it can execute in parallel with arithmetic
	// operations on the ALU, making the interpreter faster. `lea` doesn't
	// support subtraction of registers:
	//   lea PC, [PC - RC * 4]   ; NOT SUPPORTED
	// So a backwards jump opcode wouldn't be possible.
	//
	// BUT, `lea` does support subtraction of constants!
	//  lea PC, [PC + RC * 4 - JMP_BIAS * 4]
	// Thus, we use a jump bias.
	//
	// Jump offsets are stored in 24 bit integers. Thus, the jump bias is
	// defined as half the maximum value of a 24-bit integer = 2^24 / 2 = 2^23
	// = 0x800000.
	int offset = ins_arg24(*jmp) - JMP_BIAS + 1;
	return jmp_idx + offset;
}

// Sets the target of a JMP instruction.
static void jmp_set_target(Parser *psr, int jmp_idx, int target_idx) {
	// Remember, the jump offset is relative to the instruction that FOLLOWS the
	// jump instruction
	int offset = target_idx - jmp_idx + JMP_BIAS - 1;
	Instruction *ins = &psr_fn(psr)->ins[jmp_idx];
	ins_set_arg24(ins, (uint32_t) offset);
}

// Sets the target of every JMP in a jump list.
static void jmp_list_patch(Parser *psr, int head_idx, int target_idx) {
	if (head_idx < 0) {
		// Don't do anything if the list is empty
	} else if (jmp_follow(psr, head_idx) == -1) {
		// We've reached the last element in the jump list
		jmp_set_target(psr, head_idx, target_idx);
	} else {
		// Preserve the next JMP in the list
		int next_idx = jmp_follow(psr, head_idx);
		jmp_set_target(psr, head_idx, target_idx);

		// Do the recursive call last so hopefully the compiler can do a tail
		// call optimisation
		jmp_list_patch(psr, next_idx, target_idx);
	}
}

// Appends a JMP to the head of a jump list.
static void jmp_list_append(Parser *psr, int *head, int to_add) {
	if (*head == -1) {
		// Nothing in the jump list yet; set the head
		*head = to_add;

		// Reset the target of the jump to add, in case it was pointing to
		// something else previously
		jmp_set_target(psr, to_add, -1);
	} else {
		jmp_set_target(psr, to_add, *head);
		*head = to_add;
	}
}

// Merges two jump lists. `left`'s head must come BEFORE right's tail element.
// Returns the head of the merged list.
static int jmp_list_merge(Parser *psr, int left, int right) {
	// If either jump list is empty, the result is trivial
	if (right == -1) {
		return left;
	} else if (left == -1) {
		return right;
	}

	// Find the last element in `right`
	int last = right;
	while (jmp_follow(psr, last) != -1) {
		last = jmp_follow(psr, last);
	}

	// Point the last element in `right` to `left`
	jmp_set_target(psr, last, left);

	// The head of the merged list must be `right`
	return right;
}

// For a conditional jmp, either the jump is triggered, or the condition "falls
// through" to the next instruction. Sometimes, we need a particular case (the
// true or false case) to fall through. This function modifies the conditions
// in the node's jump lists to ensure that the true case falls through.
static void jmp_ensure_true_falls_through(Parser *psr, Node *node) {
	// Note `true_list` and `false_list` point to the JMP instruction with the
	// LARGEST PC in the true and false case jump lists. Thus if the true list
	// comes last, then the false case must fall through. We don't want this -
	// we want the true case to fall through - so invert the last instruction.
	if (node->jmp.true_list > node->jmp.false_list) {
		// Invert the condition
		Instruction *cond = &psr_fn(psr)->ins[node->jmp.true_list - 1];
		Opcode inverted = relop_invert(ins_op(*cond));
		ins_set_op(cond, inverted);

		// Remove this particular jump from the true list
		int tmp = node->jmp.true_list;
		node->jmp.true_list = jmp_follow(psr, node->jmp.true_list);

		// Add the jump to the false list
		jmp_list_append(psr, &node->jmp.false_list, tmp);
	}
}

// Does the opposite of the above function and ensures the false case falls
// through.
static void jmp_ensure_false_falls_through(Parser *psr, Node *node) {
	// Note `true_list` and `false_list` point to the JMP instruction with the
	// LARGEST PC in the true and false case jump lists. Thus if the false list
	// comes last, then the true case must fall through. We don't want this -
	// we want the false case to fall through - so invert the last instruction.
	if (node->jmp.false_list > node->jmp.true_list) {
		// Invert the condition
		Instruction *cond = &psr_fn(psr)->ins[node->jmp.false_list - 1];
		Opcode inverted = relop_invert(ins_op(*cond));
		ins_set_op(cond, inverted);

		// Remove this particular jump from the false list
		int tmp = node->jmp.false_list;
		node->jmp.false_list = jmp_follow(psr, node->jmp.false_list);

		// Add the jump to the true list
		jmp_list_append(psr, &node->jmp.true_list, tmp);
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
		// All other node types are already discharged
		break;
	}
}

// Puts an operand into the given stack slot.
static void expr_to_slot(Parser *psr, uint8_t dest, Node *node) {
	// Only deal with discharged values
	expr_discharge(psr, node);
	switch (node->type) {
	case NODE_PRIM: {
		// Emit an OP_SET_P instruction
		Instruction ins = ins_new2(OP_SET_P, dest, node->prim);
		fn_emit(psr_fn(psr), ins);
		break;
	}

	case NODE_NON_RELOC:
		// Only emit a MOV if the destination is different from the source slot
		if (node->slot != dest) {
			Instruction ins = ins_new2(OP_MOV, dest, node->slot);
			fn_emit(psr_fn(psr), ins);
		}
		break;

	case NODE_RELOC: {
		// Modify the destination stack slot for the relocatable instruction
		Instruction *ins = &psr_fn(psr)->ins[node->reloc_idx];
		ins_set_arg1(ins, dest);
		break;
	}

	case NODE_CONST: {
		// The only constant type we have at the moment is a number
		Instruction ins = ins_new2(OP_SET_N, dest, node->const_idx);
		fn_emit(psr_fn(psr), ins);
		break;
	}

	case NODE_JMP: {
		// Ensure the true case falls through
		jmp_ensure_true_falls_through(psr, node);

		// Emit a set/jmp/set sequence
		int tcase = fn_emit(psr_fn(psr), ins_new2(OP_SET_P, dest, PRIM_TRUE));
		fn_emit(psr_fn(psr), ins_new1(OP_JMP, JMP_BIAS + 1));
		int fcase = fn_emit(psr_fn(psr), ins_new2(OP_SET_P, dest, PRIM_FALSE));

		// Patch the true and false lists to their respective cases
		jmp_list_patch(psr, node->jmp.true_list, tcase);
		jmp_list_patch(psr, node->jmp.false_list, fcase);
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
	case NODE_PRIM:
		return (uint8_t) node->prim;
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

// Emits bytecode to convert an operand to a jump node (e.g. if we had just
// `a && b == 3`, we'd need to emit a jump on the truth-ness of `a`).
static void expr_to_jmp(Parser *psr, Node *node) {
	// Only deal with discharged nodes
	expr_discharge(psr, node);

	switch (node->type) {
	case NODE_RELOC: case NODE_PRIM: case NODE_CONST:
		// Dishcarge relocations and constants to stack slots for comparison;
		// we don't bother trying to fold constants because we likely already
		// emitted bytecode for the left operand we don't want to have to undo
		expr_to_next_slot(psr, node);

		// Fall through to the non-reloc case...

	case NODE_NON_RELOC: {
		// Emit a jump on the truthness of the value
		fn_emit(psr_fn(psr), ins_new2(OP_EQ_LP, node->slot, PRIM_TRUE));
		int jmp_idx = fn_emit(psr_fn(psr), ins_new1(OP_JMP, 0));
		jmp_set_target(psr, jmp_idx, -1);

		// Set the result
		node->type = NODE_JMP;
		node->jmp.true_list = jmp_idx;
		node->jmp.false_list = -1;
		break;
	}

	default:
		// The only other operand type is NODE_JMP, which we don't do anything
		// with
		break;
	}
}

// Attempt to fold an arithmetic operation. Returns true on success and modifies
// `left` to contain the folded value.
static bool expr_fold_arith(Parser *psr, Tk binop, Node *left, Node right) {
	// Only fold if both operands are numbers
	if (left->type != NODE_NUM || right.type != NODE_NUM) {
		return false;
	}

	// Compute the result of the fold
	switch (binop) {
		case '+': left->num += right.num; break;
		case '-': left->num -= right.num; break;
		case '*': left->num *= right.num; break;
		case '/': left->num /= right.num; break;
	}
	return true;
}

// Emit bytecode for a binary arithmetic operation.
static void expr_emit_arith(Parser *psr, Tk binop, Node *left, Node right) {
	// Check for valid operand types
	if (right.type == NODE_PRIM) {
		psr_trigger_err(psr, "invalid operand to binary operator");
		assert(false);
	}

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
	int idx = fn_emit(psr_fn(psr), ins);

	// Set the result as a relocatable node
	result->type = NODE_RELOC;
	result->reloc_idx = idx;
}

// Attempt to fold an order operation. Returns true if we could successfully
// fold the operation, and sets `left` to the result of the fold
static bool expr_fold_rel(Parser *psr, Tk binop, Node *left, Node right) {
	// Both types must be equal
	if (left->type != right.type) {
		return false;
	}

	if (left->type == NODE_NUM) {
		// Compare the two numbers
		bool result;
		switch (binop) {
			case TK_EQ:  result = left->num == right.num; break;
			case TK_NEQ: result = left->num != right.num; break;
			case '>':    result = left->num > right.num;  break;
			case TK_GE:  result = left->num >= right.num; break;
			case '<':    result = left->num < right.num;  break;
			case TK_LE:  result = left->num <= right.num; break;
			default: assert(false); // Unreachable
		}

		// Set the result to be a primitive
		left->type = NODE_PRIM;
		left->prim = (Primitive) result;
		return true;
	} else if (left->type == NODE_PRIM) { // Only for TK_EQ and TK_NEQ
		// Compare the two primitives
		bool result;
		switch (binop) {
			case TK_EQ:  result = left->prim == right.prim;
			case TK_NEQ: result = left->prim != right.prim;
			default: assert(false); // Unreachable
		}

		// Set the result to be a primitive
		left->type = NODE_PRIM;
		left->prim = (Primitive) result;
		return true;
	}

	// Can't fold any other operation
	return false;
}

// Emit bytecode for an order operation.
static void expr_emit_rel(Parser *psr, Tk binop, Node *left, Node right) {
	// Check for valid operand types
	if (binop_is_ord(binop) && right.type == NODE_PRIM) {
		psr_trigger_err(psr, "invalid operand to binary operator");
		assert(false);
	}

	// Check if we can fold the order operation
	if (expr_fold_rel(psr, binop, left, right)) {
		return;
	}

	// We need to ensure the constant is ALWAYS the right operand
	Node *result = left;
	Node l, r;
	if (node_is_const(left)) {
		// We can swap the arguments for == and != freely, but for <, >, <= and
		// >= we have to invert the operator when we swap the arguments
		if (!binop_is_commutative(binop)) {
			binop = binop_invert_rel(binop);
		}
		l = right;
		r = *left;
	} else {
		l = *left;
		r = right;
	}

	// Convert the operands into uint8_t instruction arguments
	uint8_t larg = expr_to_ins_arg(psr, &l);
	uint8_t rarg = expr_to_ins_arg(psr, &r);

	// See comment under `expr_emit_arith`
	if (larg > rarg) {
		expr_free_node(psr, &l);
		expr_free_node(psr, &r);
	} else {
		expr_free_node(psr, &r);
		expr_free_node(psr, &l);
	}

	// Calculate the opcode to use based on the types of the right operand
	int opcode_offset;
	switch (r.type) {
		case NODE_NON_RELOC: opcode_offset = 0; break;
		case NODE_CONST:     opcode_offset = 1; break;
		case NODE_PRIM:      opcode_offset = 2; break;
		default: assert(false); // Unreachable
	}
	Opcode opcode = binop_opcode(binop) + opcode_offset;

	// Emit the condition instruction and the following jump
	Instruction condition = ins_new2(opcode, larg, rarg);
	fn_emit(psr_fn(psr), condition);

	// Have the target of this jump be -1
	Instruction jmp = ins_new1(OP_JMP, 0);
	int jmp_idx = fn_emit(psr_fn(psr), jmp);
	jmp_set_target(psr, jmp_idx, -1);

	// Set the result
	result->type = NODE_JMP;
	result->jmp.true_list = jmp_idx;
	result->jmp.false_list = -1;
}

// Emit bytecode for a logical AND operation.
static void expr_emit_and(Parser *psr, Node *left, Node right) {
	// Emit code to convert `right` to a jump, if necessary
	expr_to_jmp(psr, &right);

	// We need the true case to fall through to the start of the `right` operand
	jmp_ensure_true_falls_through(psr, left);

	// Point left's true case to the start of right, which is 1 instruction
	// after the end of left; we KNOW that the false case must come last since
	// we just ensured that in the previous function call
	int target = left->jmp.false_list + 1;
	jmp_list_patch(psr, left->jmp.true_list, target);

	// The result's true case is right's true case
	left->jmp.true_list = right.jmp.true_list;

	// The result's false case is the merge of left and right's false lists
	left->jmp.false_list = jmp_list_merge(psr, left->jmp.false_list,
		right.jmp.false_list);
}

// Emit bytecode for a logical OR operation.
static void expr_emit_or(Parser *psr, Node *left, Node right) {
	// Emit code to convert `right` to a jump, if necessary
	expr_to_jmp(psr, &right);

	// We need to ensure the false case falls through
	jmp_ensure_false_falls_through(psr, left);

	// Point left's false case to the start of right, which is 1 instruction
	// after the end of left; we KNOW that the true case must come last since
	// we just ensured it with the previous function call
	int target = left->jmp.true_list + 1;
	jmp_list_patch(psr, left->jmp.false_list, target);

	// The result's false case is right's false case
	left->jmp.false_list = right.jmp.false_list;

	// The result's true case is the merge of left and right's true lists
	left->jmp.true_list = jmp_list_merge(psr, left->jmp.true_list,
		right.jmp.true_list);
}

// Emit bytecode for a binary operation. Modifies `left` in place to the result
// of the binary operation.
static void expr_emit_binary(Parser *psr, Tk binop, Node *left, Node right) {
	switch (binop) {
		// Arithmetic operators
	case '+': case '-': case '*': case '/':
		expr_emit_arith(psr, binop, left, right);
		break;

		// Relational operators
	case TK_EQ: case TK_NEQ: case '>': case TK_GE: case '<': case TK_LE:
		expr_emit_rel(psr, binop, left, right);
		break;

		// Logical operators
	case TK_AND:
		expr_emit_and(psr, left, right);
		break;
	case TK_OR:
		expr_emit_or(psr, left, right);
		break;

		// Unreachable
	default:
		assert(false);
	}
}

// Emit bytecode for the left operand to a binary expression, before the right
// operand is parsed. Modifies `operand` in place.
static void expr_emit_binary_left(Parser *psr, Tk binop, Node *left) {
	// All instruction arguments need to fit into a uint8_t. There's some
	// exceptions to this, which are all handled individually below.

	// There's specialised instructions for arithmetic with numbers and locals
	if (binop_is_arith(binop)) {
		if (left->type == NODE_NUM) {
			return;
		} else if (left->type == NODE_PRIM) {
			// Invalid operator
			psr_trigger_err(psr, "invalid operand to binary operator");
			assert(false);
		}
	} else if (binop_is_rel(binop)) {
		if (left->type == NODE_NUM) {
			// There are specialised instructions for number operations, like
			// arithmetic operands
			return;
		} else if (binop_is_rel(binop) && left->type == NODE_PRIM) {
			// Can't give primitives to order operations
			psr_trigger_err(psr, "invalid operand to binary operator");
			assert(false);
		}
	} else if (binop == TK_AND || binop == TK_OR) {
		// Turn the operand into a jump, if necessary
		expr_to_jmp(psr, left);
		return;
	}

	// Otherwise, ensure the node is usable as an instruction argument
	expr_to_ins_arg(psr, left);
}

// Emit bytecode for a unary negation operation.
static void expr_emit_neg(Parser *psr, Node *operand) {
	// Check if we can fold the operation
	if (operand->type == NODE_NUM) {
		operand->num = -operand->num;
		return;
	} else if (operand->type == NODE_PRIM) {
		// Invalid operand
		psr_trigger_err(psr, "invalid operand to unary operator");
		assert(false);
	}

	// Convert the operand to a stack slot that we can negate (since OP_NEG
	// only operates on stack slots)
	uint8_t slot = expr_to_any_slot(psr, operand);

	// Free the operand if it's on top of the stack, so we can re-use its slot
	expr_free_node(psr, operand);

	// Generate a relocatable instruction
	Instruction ins = ins_new2(OP_NEG, 0, slot);
	int idx = fn_emit(psr_fn(psr), ins);

	// The result of the negation is a relocatable instruction
	operand->type = NODE_RELOC;
	operand->reloc_idx = idx;
}

// Emit bytecode for a negation operation.
static void expr_emit_not(Parser *psr, Node *operand) {
	// Emit code to convert the operand to a jump, if necessary
	expr_to_jmp(psr, operand);

	// Swap the true and false cases
	int tmp = operand->jmp.true_list;
	operand->jmp.true_list = operand->jmp.false_list;
	operand->jmp.false_list = tmp;
}

// Emit bytecode for a unary operation. Modifies `operand` in place to the
// result of the operation.
static void expr_emit_unary(Parser *psr, Tk unop, Node *operand) {
	switch (unop) {
		case '-': expr_emit_neg(psr, operand); break;
		case '!': expr_emit_not(psr, operand); break;
		default: assert(false); // Unreachable
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

// Parse a primitive operand (true, false, or nil).
static Node expr_operand_prim(Parser *psr) {
	Node node;
	node.type = NODE_PRIM;
	node.prim = (Primitive) (psr->lxr.tk.type - TK_FALSE);
	lex_next(&psr->lxr);
	return node;
}

// Parse an operand to a binary or unary operation.
static Node expr_operand(Parser *psr) {
	switch (psr->lxr.tk.type) {
	case TK_NUM:   return expr_operand_num(psr);
	case TK_IDENT: return expr_operand_local(psr);
	case '(':      return expr_operand_subexpr(psr);
	case TK_TRUE: case TK_FALSE: case TK_NIL: return expr_operand_prim(psr);
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

// Forward declaration.
static void parse_block(Parser *psr);

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
	Tk augmented_tk = '\0';
	switch (psr->lxr.tk.type) {
		case TK_ADD_ASSIGN: augmented_tk = '+'; break;
		case TK_SUB_ASSIGN: augmented_tk = '-'; break;
		case TK_MUL_ASSIGN: augmented_tk = '*'; break;
		case TK_DIV_ASSIGN: augmented_tk = '/'; break;
	}
	lex_next(&psr->lxr);

	// Expect an expression
	Node result = parse_expr(psr);

	// Handle an augmented assignment
	if (augmented_tk != '\0') {
		// Emit a relocatable arithmetic instruction for the assignment
		Node dest_node;
		dest_node.type = NODE_NON_RELOC;
		dest_node.slot = dest;
		expr_emit_arith(psr, augmented_tk, &dest_node, result);

		// Set the destination of the relocatable instruction
		expr_to_slot(psr, dest, &dest_node);
	} else {
		// Put the assignment result into the correct slot
		expr_to_slot(psr, dest, &result);
	}
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

// Parse an `if` statement.
static void parse_if(Parser *psr) {
	// Keep parsing if/elseif statements
	int jmp_head = -1;
	do {
		// Skip the if/elseif token
		lex_next(&psr->lxr);

		// Parse the condition
		Node condition = parse_expr(psr);
		expr_to_jmp(psr, &condition);
		jmp_ensure_true_falls_through(psr, &condition);

		// Patch the condition's true case
		int true_case = psr_fn(psr)->ins_count;
		jmp_list_patch(psr, condition.jmp.true_list, true_case);

		// Parse the contents of the if/elseif
		lex_expect(&psr->lxr, '{');
		lex_next(&psr->lxr);
		parse_block(psr);
		lex_expect(&psr->lxr, '}');
		lex_next(&psr->lxr);

		// If there's another one following
		if (psr->lxr.tk.type == TK_ELSEIF || psr->lxr.tk.type == TK_ELSE) {
			// Add a jump to the end of the if/elseif body
			int jmp_idx = fn_emit(psr_fn(psr), ins_new1(OP_JMP, 0));

			// Add the jump to the jump list, which will get patched to after
			// ALL the if/elseif/else code once we're done
			jmp_list_append(psr, &jmp_head, jmp_idx);
		}

		// Patch the if/elseif's false case here
		int false_case = psr_fn(psr)->ins_count;
		jmp_list_patch(psr, condition.jmp.false_list, false_case);
	} while (psr->lxr.tk.type == TK_ELSEIF);

	// Check for a following `else` statement
	if (psr->lxr.tk.type == TK_ELSE) {
		// Skip the `else` token
		lex_next(&psr->lxr);

		// Parse the contents of the `else`
		lex_expect(&psr->lxr, '{');
		lex_next(&psr->lxr);
		parse_block(psr);
		lex_expect(&psr->lxr, '}');
		lex_next(&psr->lxr);
	}

	// Patch all the jumps at the end of if/elseif bodies here
	jmp_list_patch(psr, jmp_head, psr_fn(psr)->ins_count);
}

// Parse an infinite `loop` statement.
static void parse_loop(Parser *psr) {
	// Skip the `loop` token
	lex_next(&psr->lxr);

	// Save the start of the loop
	int start = psr_fn(psr)->ins_count;

	// Parse the contents of the loop
	lex_expect(&psr->lxr, '{');
	lex_next(&psr->lxr);
	parse_block(psr);
	lex_expect(&psr->lxr, '}');
	lex_next(&psr->lxr);

	// Add a jump back to the start
	int jmp_idx = fn_emit(psr_fn(psr), ins_new1(OP_JMP, 0));
	jmp_set_target(psr, jmp_idx, start);
}

// Parse a `while` loop.
static void parse_while(Parser *psr) {
	// Skip the `while` token
	lex_next(&psr->lxr);

	// Save the start of the loop
	int start = psr_fn(psr)->ins_count;

	// Parse the condition
	Node condition = parse_expr(psr);
	expr_to_jmp(psr, &condition);
	jmp_ensure_true_falls_through(psr, &condition);

	// Patch the true case here
	int true_case = psr_fn(psr)->ins_count;
	jmp_list_patch(psr, condition.jmp.true_list, true_case);

	// Parse the body of the loop
	lex_expect(&psr->lxr, '{');
	lex_next(&psr->lxr);
	parse_block(psr);
	lex_expect(&psr->lxr, '}');
	lex_next(&psr->lxr);

	// Add a jump back to the start
	int jmp_idx = fn_emit(psr_fn(psr), ins_new1(OP_JMP, 0));
	jmp_set_target(psr, jmp_idx, start);

	// Patch the false case here
	int false_case = psr_fn(psr)->ins_count;
	jmp_list_patch(psr, condition.jmp.false_list, false_case);
}

// Parse a function definition.
static void parse_fn(Parser *psr) {
	// Skip the `fn` token
	lex_next(&psr->lxr);

	// Expect the name of the function
	lex_expect(&psr->lxr, TK_IDENT);
	uint64_t fn_name = psr->lxr.tk.ident_hash;
	lex_next(&psr->lxr);

	// Create the new function scope
	FnScope scope;
	scope.fn = vm_new_fn(psr->vm, psr->pkg);
	scope.first_local = psr->locals_count;
	scope.next_slot = 0;
	scope.outer_scope = psr->scope;
	psr->scope = &scope;

	// Expect the argument list, adding each one as a new local
	lex_expect(&psr->lxr, '(');
	lex_next(&psr->lxr);
	while (psr->lxr.tk.type == TK_IDENT) {
		// Add the argument as a local
		uint64_t arg_name = psr->lxr.tk.ident_hash;
		psr_new_local(psr, arg_name);
		scope.next_slot++;
		lex_next(&psr->lxr);

		// Expect a comma or closing parenthesis
		if (psr->lxr.tk.type != ',') {
			break;
		} else {
			// Skip the comma
			lex_next(&psr->lxr);
		}
	}
	lex_expect(&psr->lxr, ')');
	lex_next(&psr->lxr);

	// Parse the contents of the function definition
	lex_expect(&psr->lxr, '{');
	lex_next(&psr->lxr);
	parse_block(psr);
	lex_expect(&psr->lxr, '}');
	lex_next(&psr->lxr);

	// Add the final RET instruction
	fn_emit(psr_fn(psr), ins_new3(OP_RET, 0, 0, 0));

	// Get rid of the function definition arguments on the parser's locals list
	psr->locals_count = scope.first_local;

	// Return to the outer function scope
	psr->scope = scope.outer_scope;

	// Create a new local in the outer scope containing the function we just
	// defined
	psr_new_local(psr, fn_name);
	uint8_t slot = psr->scope->next_slot++;
	fn_emit(psr_fn(psr), ins_new2(OP_SET_F, slot, scope.fn));
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
			case TK_LET:   parse_let(psr); break;
			case TK_IDENT: parse_assign_or_expr(psr); break;
			case '(':      parse_expr(psr); break;
			case TK_IF:    parse_if(psr); break;
			case TK_LOOP:  parse_loop(psr); break;
			case TK_WHILE: parse_while(psr); break;
			case TK_FN:    parse_fn(psr); break;

			// Couldn't find a statement to parse
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
