
// vm.c
// By Ben Anderson
// July 2018

#include "vm.h"
#include "parser.h"
#include "err.h"
#include "util.h"
#include "value.h"

#include "jit/compile.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

// Creates a new virtual machine instance.
VM vm_new() {
	VM vm;
	vm.pkgs_capacity = 4;
	vm.pkgs_count = 0;
	vm.pkgs = malloc(sizeof(Package) * vm.pkgs_capacity);

	vm.fns_capacity = 16;
	vm.fns_count = 0;
	vm.fns = malloc(sizeof(Function) * vm.fns_capacity);

	vm.consts_capacity = 16;
	vm.consts_count = 0;
	vm.consts = malloc(sizeof(Value) * vm.consts_capacity);

	vm.stack_size = 1024;
	vm.stack = malloc(sizeof(Value) * vm.stack_size);
	return vm;
}

// Frees all the resources allocated by a virtual machine.
void vm_free(VM *vm) {
	for (int i = 0; i < vm->fns_count; i++) {
		free(vm->fns[i].ins);
	}
	free(vm->pkgs);
	free(vm->fns);
	free(vm->consts);
	free(vm->stack);
}

// Creates a new package on the VM and returns its index.
int vm_new_pkg(VM *vm, uint64_t name) {
	if (vm->pkgs_count >= vm->pkgs_capacity) {
		vm->pkgs_capacity *= 2;
		vm->pkgs = realloc(vm->pkgs, sizeof(Package) * vm->pkgs_capacity);
	}

	Package *pkg = &vm->pkgs[vm->pkgs_count++];
	pkg->name = name;
	pkg->main_fn = vm_new_fn(vm, vm->pkgs_count - 1);
	return vm->pkgs_count - 1;
}

// Creates a new function on the VM and returns its index.
int vm_new_fn(VM *vm, int pkg_idx) {
	if (vm->fns_count >= vm->fns_capacity) {
		vm->fns_capacity *= 2;
		vm->fns = realloc(vm->fns, sizeof(Function) * vm->fns_capacity);
	}

	Function *fn = &vm->fns[vm->fns_count++];
	fn->pkg = pkg_idx;
	fn->ins = NULL; // Lazily instatiate the bytecode array
	fn->ins_count = 0;
	fn->ins_capacity = 0;
	return vm->fns_count - 1;
}

// Adds a constant number to the VM's constants list, returning its index.
int vm_add_num(VM *vm, double num) {
	uint64_t converted = n2v(num);

	// Check if the constant already exists
	for (int i = 0; i < vm->consts_count; i++) {
		if (vm->consts[i] == converted) {
			return i;
		}
	}

	if (vm->consts_count >= vm->consts_capacity) {
		vm->consts_capacity *= 2;
		vm->consts = realloc(vm->consts, sizeof(Value) * vm->consts_capacity);
	}

	// Add the converted number to the constants list if it doesn't already
	// exist
	vm->consts[vm->consts_count++] = converted;
	return vm->consts_count - 1;
}

// Emits a bytecode instruction to a function.
int fn_emit(Function *fn, BcIns ins) {
	if (fn->ins == NULL) {
		// Lazily instantiate the bytecode array
		fn->ins_capacity = 32;
		fn->ins = malloc(sizeof(BcIns) * fn->ins_capacity);
	} else if (fn->ins_count >= fn->ins_capacity) {
		// Increase the capacity of the bytecode array
		fn->ins_capacity *= 2;
		fn->ins = realloc(fn->ins, sizeof(BcIns) * fn->ins_capacity);
	}

	fn->ins[fn->ins_count++] = ins;
	return fn->ins_count - 1;
}

// Dumps the bytecode for a function to the standard output.
void fn_dump(Function *fn) {
	printf("---- Function ----\n");
	for (int i = 0; i < fn->ins_count; i++) {
		BcIns *ins = &fn->ins[i];
		printf("  %0.4d  %s  ", i, BCOP_NAMES[bc_op(*ins)]);
		if (bc_op(*ins) == BC_JMP) {
			// Print the jump offset and target instruction
			int offset = bc_arg24(*ins) - JMP_BIAS + 1;
			printf("%d  => %0.4d\n", offset, i + offset);
		} else {
			printf("%d  %d  %d\n", bc_arg1(*ins), bc_arg2(*ins),
				bc_arg3(*ins));
		}
	}
}

// Maximum length of an error description string.
#define ERR_MAX_DESC_LEN 255

// Creates a new error from a format string.
Err * err_new(char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char *desc = malloc(sizeof(char) * ERR_MAX_DESC_LEN);
	vsnprintf(desc, ERR_MAX_DESC_LEN, fmt, args);
	va_end(args);

	Err *err = malloc(sizeof(Err));
	err->desc = desc;
	err->line = -1;
	err->file = NULL;
	return err;
}

// Creates a new error from a vararg list.
Err * err_vnew(char *fmt, va_list args) {
	char *desc = malloc(sizeof(char) * ERR_MAX_DESC_LEN);
	vsnprintf(desc, ERR_MAX_DESC_LEN, fmt, args);

	Err *err = malloc(sizeof(Err));
	err->desc = desc;
	err->line = -1;
	err->file = NULL;
	return err;
}

// Frees resources allocated by an error.
void err_free(Err *err) {
	if (err == NULL) {
		return;
	}
	free(err->desc);
	free(err->file);
	free(err);
}

// Triggers a longjmp back to the most recent setjmp protection.
void err_trigger(VM *vm, Err *err) {
	vm->err = err;
	longjmp(vm->guard, 1);
}

// Copies a file path into a new heap allocated string to save with the error.
void err_file(Err *err, char *path) {
	if (path == NULL) {
		return;
	}
	err->file = malloc(sizeof(char) * (strlen(path) + 1));
	strcpy(err->file, path);
}

// ANSI terminal color codes.
#define TEXT_RESET_ALL "\033[0m"
#define TEXT_BOLD      "\033[1m"
#define TEXT_RED       "\033[31m"
#define TEXT_GREEN     "\033[32m"
#define TEXT_BLUE      "\033[34m"
#define TEXT_WHITE     "\033[37m"

// Pretty prints an error to the standard output with terminal color codes.
static void err_print_color(Err *err) {
	printf("error: %s\n", err->desc);
}

// Pretty prints an error to the standard output in black and white.
static void err_print_bw(Err *err) {
	printf("error: %s\n", err->desc);
}

// Pretty prints the error to the standard output. If `use_color` is true, then
// terminal color codes will be printed alongside the error information.
void err_print(Err *err, bool use_color) {
	if (err == NULL) {
		return;
	}
	if (use_color) {
		err_print_color(err);
	} else {
		err_print_bw(err);
	}
}

// Forward declaration.
static Err * vm_run(VM *vm, int fn_idx, int ins_idx);

// Executes some code. The code is run within the package's "main" function,
// and can access any variables, functions, imports, etc. that were created by
// a previous piece of code run on this package. This functionality is used to
// create the REPL.
//
// Since no file path is specified, any imports are relative to the current
// working directory.
//
// If an error occurs, then the return value will be non-NULL and the error must
// be freed using `hy_free_err`.
Err * vm_run_string(VM *vm, int pkg, char *code) {
	// TODO: save and restore VM state in case of error

	// Parse the source code
	Err *err = parse(vm, pkg, NULL, code);
	if (err != NULL) {
		return err;
	}

	// Run the code
	return vm_run(vm, vm->pkgs[pkg].main_fn, 0);
}

// Executes a file. A new package is created for the file and is named based off
// the name of the file. The package can be later imported by other pieces of
// code.
//
// Both the directory containing the file and the current working directory are
// searched when the file attempts to import any other packages.
//
// If an error occurs, then the return value is non-NULL and the error must be
// freed.
Err * vm_run_file(VM *vm, char *path) {
	// TODO: save and restore VM state in case of error

	// Extract the package name from the file path
	uint64_t name = extract_pkg_name(path);
	if (name == ~((uint64_t) 0)) {
		Err *err = err_new("invalid package name from file path `%s`", path);
		err_file(err, path);
		return err;
	}

	// Read the file contents
	char *code = read_file(path);
	if (code == NULL) {
		Err *err = err_new("failed to open file `%s`", path);
		err_file(err, path);
		return err;
	}

	// Parse the source code
	int pkg = vm_new_pkg(vm, name);
	Err *err = parse(vm, pkg, path, code);
	free(code);
	if (err != NULL) {
		return err;
	}

	// Run the code
	return vm_run(vm, vm->pkgs[pkg].main_fn, 0);
}

// Executes some bytecode, starting at a particular instruction within a
// function. Returns any runtime errors that might occur.
static Err * vm_run(VM *vm, int fn_idx, int ins_idx) {
	// Dispatch table when we're running the regular interpreter without a JIT
	// trace
	static void *interpreter_dispatch[] = {
		// Stores
		&&op_MOV, &&op_SET_N, &&op_SET_P, &&op_SET_F,

		// Arithmetic operators
		&&op_ADD_LL, &&op_ADD_LN, &&op_SUB_LL, &&op_SUB_LN, &&op_SUB_NL,
		&&op_MUL_LL, &&op_MUL_LN, &&op_DIV_LL, &&op_DIV_LN, &&op_DIV_NL,
		&&op_NEG,

		// Relational operators
		&&op_EQ_LL, &&op_EQ_LN, &&op_EQ_LP, &&op_NEQ_LL, &&op_NEQ_LN,
		&&op_NEQ_LP, &&op_LT_LL, &&op_LT_LN, &&op_LE_LL, &&op_LE_LN,
		&&op_GT_LL, &&op_GT_LN, &&op_GE_LL, &&op_GE_LN,

		// Control flow
		&&op_JMP, &&op_LOOP, &&op_CALL, &&op_RET,
	};

	// Dispatch table for when we're running a JIT trace
	static void *jit_dispatch[] = {
		// Stores
		&&jit_MOV, &&jit_SET_N, &&jit_SET_P, &&jit_SET_F,

		// Arithmetic operators
		&&jit_ADD_LL, &&jit_ADD_LN, &&jit_SUB_LL, &&jit_SUB_LN, &&jit_SUB_NL,
		&&jit_MUL_LL, &&jit_MUL_LN, &&jit_DIV_LL, &&jit_DIV_LN, &&jit_DIV_NL,
		&&jit_NEG,

		// Relational operators
		&&jit_EQ_LL, &&jit_EQ_LN, &&jit_EQ_LP, &&jit_NEQ_LL, &&jit_NEQ_LN,
		&&jit_NEQ_LP, &&jit_LT_LL, &&jit_LT_LN, &&jit_LE_LL, &&jit_LE_LN,
		&&jit_GT_LL, &&jit_GT_LN, &&jit_GE_LL, &&jit_GE_LN,

		// Control flow
		&&jit_JMP, &&jit_LOOP, &&jit_CALL, &&jit_RET,
	};

	// The current dispatch table (default to the normal interpreter)
	void **dispatch;
	dispatch = interpreter_dispatch;

	// Move some variables into the function's local scope
	Value *k = vm->consts;
	Value *stk = vm->stack;

	// Move some important state information into local variables for easy
	// access
	Function *fn = &vm->fns[fn_idx]; // Currently executing function
	BcIns *ip = &fn->ins[ins_idx];   // Current instruction
	Trace trace;                     // Current JIT trace we're recording
	Err *err = NULL;                 // Most recent error
	fn_dump(fn);

	// Loop iteration table, which keeps track of how many iterations each loop
	// has gone through so far. When the #iterations hits the threshold, we
	// start a JIT trace
	#define ITER_TABLE_SIZE 1024
	uint8_t loop_iter_count[ITER_TABLE_SIZE] = {0};

	// Some helpful macros to reduce repetition
#define OPCODE(mnemonic)                 \
	jit_##mnemonic:                      \
		jit_rec_##mnemonic(&trace, *ip); \
	op_##mnemonic:
#define DISPATCH() goto *dispatch[bc_op(*ip)]
#define NEXT() goto *dispatch[bc_op(*(++ip))]

	// Execute the first instruction
	DISPATCH();


	// **** Storage ****

OPCODE(MOV)
	stk[bc_arg1(*ip)] = stk[bc_arg16(*ip)];
	NEXT();
OPCODE(SET_N)
	stk[bc_arg1(*ip)] = k[bc_arg16(*ip)];
	NEXT();
OPCODE(SET_P)
	stk[bc_arg1(*ip)] = TAG_PRIM | bc_arg16(*ip);
	NEXT();
OPCODE(SET_F)
	stk[bc_arg1(*ip)] = TAG_FN | bc_arg16(*ip);
	NEXT();


	// **** Arithmetic Operations ****

#define BC_ARITH(name, operation)                      \
	OPCODE(name##_LL) {                                \
		double left = v2n(stk[bc_arg2(*ip)]);          \
		double right = v2n(stk[bc_arg3(*ip)]);         \
		stk[bc_arg1(*ip)] = n2v(left operation right); \
		NEXT();                                        \
	}                                                  \
	OPCODE(name##_LN) {                                \
		double left = v2n(stk[bc_arg2(*ip)]);          \
		double right = v2n(k[bc_arg3(*ip)]);           \
		stk[bc_arg1(*ip)] = n2v(left operation right); \
		NEXT();                                        \
	}

#define BC_ARITH_NON_COMMUTATIVE(name, operation)      \
	BC_ARITH(name, operation)                          \
	OPCODE(name##_NL) {                                \
		double left = v2n(k[bc_arg2(*ip)]);            \
		double right = v2n(stk[bc_arg3(*ip)]);         \
		stk[bc_arg1(*ip)] = n2v(left operation right); \
		NEXT();                                        \
	}

	BC_ARITH(ADD, +)
	BC_ARITH_NON_COMMUTATIVE(SUB, -)
	BC_ARITH(MUL, *)
	BC_ARITH_NON_COMMUTATIVE(DIV, /)

OPCODE(NEG)
	stk[bc_arg1(*ip)] = n2v(-v2n(stk[bc_arg16(*ip)]));
	NEXT();


	// **** Relational Operators ****

#define BC_EQ(name, op)                                               \
	OPCODE(name##_LL)                                                 \
		if (stk[bc_arg1(*ip)] op stk[bc_arg2(*ip)]) { ip++; }         \
		NEXT();                                                       \
	OPCODE(name##_LN)                                                 \
		if (stk[bc_arg1(*ip)] op k[bc_arg2(*ip)]) { ip++; }           \
		NEXT();                                                       \
	OPCODE(name##_LP)                                                 \
		if (stk[bc_arg1(*ip)] op (TAG_PRIM | bc_arg2(*ip))) { ip++; } \
		NEXT();

	// We invert the condition because we want to skip the following JMP only
	// if the condition turns out to be false - we want to take the JMP if the
	// condition is true
	BC_EQ(EQ, !=)
	BC_EQ(NEQ, ==)

#define BC_ORD(name, op)                                                \
	OPCODE(name##_LL)                                                   \
		if (v2n(stk[bc_arg1(*ip)]) op v2n(stk[bc_arg2(*ip)])) { ip++; } \
		NEXT();                                                         \
	OPCODE(name##_LN)                                                   \
		if (v2n(stk[bc_arg1(*ip)]) op v2n(k[bc_arg2(*ip)])) { ip++; }   \
		NEXT();

	// Invert the conditions, for the reason given above
	BC_ORD(LT, >=)
	BC_ORD(LE, >)
	BC_ORD(GT, <=)
	BC_ORD(GE, <)


	// **** Hot Loop Detection and Jumps ****

jit_LOOP:
	// Halt the JIT trace
	jit_rec_finish(&trace);

	// Swap the dispatch tables back
	dispatch = interpreter_dispatch;

	// Stop everything for now
	goto finish; // TODO

op_LOOP: {
	// Hot loop detection is pretty simple. If a loop is executed more than 50
	// times, start a JIT trace. We keep track of loop iteration counts using
	// the instruction pointer as an index to a table. We reduce the resolution
	// of the table by bit-shifting the IP left by 2, and modulo-ing the pointer
	// by the size of the table. We don't really care about the high collision
	// rate this might cause.
	size_t idx = (((uintptr_t) ip) >> 2) & (ITER_TABLE_SIZE - 1);
	loop_iter_count[idx]++;
	if (loop_iter_count[idx] >= JIT_THRESHOLD) {
		// Reset the iteration count
		loop_iter_count[idx] = 0;

		// Create a new trace (we only ever create one trace at the moment, so
		// don't worry about freeing any previous trace)
		jit_trace_new(&trace, vm);

		// Start the JIT trace by swapping out the dispatch table
		dispatch = jit_dispatch;
	}

	// Fall through to the JMP instruction...
}

	// We don't bother JITing JMPs, we just follow them wherever they go and
	// record the next instruction. Guards are added only when we enounter the
	// above conditional instructions
jit_JMP:
op_JMP:
	ip += (int32_t) bc_arg24(*ip) - JMP_BIAS;
	NEXT();


	// **** Other Control Flow ****

OPCODE(CALL) // TODO
OPCODE(RET)  // TODO
	// Fall through to finish for now...
finish:
	// Termination
	printf("First stack slot %g\n", v2n(stk[0]));
	return err;
}
