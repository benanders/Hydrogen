
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
typedef struct {
	// Index into the VM's functions list.
	int fn;

	// The index of the first local defined in this function scope in the
	// parser's locals array.
	int first_local;

	// The index of the next available slot on the stack that we can store a
	// value into. Keeps track of the current number of both named and temporary
	// local variables.
	int next_slot;
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
Parser psr_new(HyVM *vm, int pkg, char *path, char *code) {
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
void psr_free(Parser *psr) {
	free(psr->locals);
}

// Trigger a new error at the lexer's current token's line number.
void psr_trigger_err(Parser *psr, char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	HyErr *err = err_vnew(fmt, args);
	va_end(args);

	err_set_file(err, psr->lxr.path);
	err->line = psr->lxr.tk.line;
	err_trigger(psr->vm, err);
}

// Start parsing the given source code.
void psr_parse(Parser *psr) {
	psr_trigger_err(psr, "this is a test");
}

// Parses the source code into bytecode. All bytecode for top level code gets
// appended to the package's main function. All other functions defined in the
// code get created on the VM and associated with the given package.
HyErr * parse(HyVM *vm, int pkg, char *path, char *code) {
	Parser psr = psr_new(vm, pkg, path, code);

	// Set up an error guard
	vm->err = NULL;
	if (!setjmp(vm->guard)) {
		psr_parse(&psr);
	}

	psr_free(&psr);
	return vm->err;
}
