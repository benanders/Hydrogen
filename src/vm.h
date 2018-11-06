
// vm.h
// By Ben Anderson
// July 2018

#ifndef VM_H
#define VM_H

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>
#include <limits.h>

#include "bytecode.h"
#include "value.h"

// Limits.
#define MAX_LOCALS_IN_FN  255
#define MAX_CONSTS        USHRT_MAX

// A package contains a collection of function definitions.
typedef struct {
	// There are a couple of options for storing strings extracted from source
	// code (like the names of variables, functions, etc.):
	// 1) Store a length/pointer pair into the original source code. This
	//    requires us to keep a copy of the original source code around
	// 2) Copy out the name into a new heap allocated string. This means quite
	//    a lot of heap allocations
	// 3) Hash the string and ignore the fact that there might be collisions.
	//    The FNV hashing algorithm we use is strong enough that collisions are
	//    only going to occur if people deliberately name their variables after
	//    known collisions
	// I went with the hashing option because it's the easiest for me.
	//
	// If the package is anonymous (i.e. doesn't have a name and can't be
	// imported), then this is set to !0.
	uint64_t name;

	// Each package has a "main" function that stores the bytecode for any top
	// level code outside of any explicit user-defined function.
	int main_fn;
} Package;

// A function definition stores a list of parsed bytecode instructions.
typedef struct {
	// The index of the package that this function is associated with.
	int pkg;

	// The number of arguments to the function (vararg functions aren't yet
	// supported).
	int args_count;

	// Note that we can't have more than INT_MAX bytecode instructions, since we
	// need to occasionally refer to instructions using signed indices.
	BcIns *ins;
	int ins_count, ins_capacity;
} Function;

// Emits a bytecode instruction to a function.
int fn_emit(Function *fn, BcIns ins);

// Dumps the bytecode for a function to the standard output.
void fn_dump(Function *fn);

// Contains all information about an error.
typedef struct {
	// Heap-allocated description string.
	char *desc;

	// Path to the file in which the error occurred, or NULL if the error has
	// no associated file (e.g. it occurred in a string).
	char *file;

	// Line on which the error occurred, or -1 if the error has no associated
	// line number.
	int line;
} Err;

// Creates a new error from a format string.
Err * err_new(char *fmt, ...);

// Creates a new error from a vararg list.
Err * err_vnew(char *fmt, va_list args);

// Frees resources allocated by an error.
void err_free(Err *err);

// Copies a file path into a new heap allocated string to save with the error.
void err_file(Err *err, char *path);

// Pretty prints the error to the standard output. If `use_color` is true, then
// terminal color codes will be printed alongside the error information.
void err_print(Err *err, bool use_color);

// Hydrogen has no global state; everything that's needed is stored in this
// struct. You can create multiple VMs and they'll all function independently.
typedef struct {
	// We keep a list of all loaded packages so that if a piece of code attempts
	// to import a package we've already loaded, we don't have to re-load that
	// package.
	Package *pkgs;
	int pkgs_count, pkgs_capacity;

	// We keep a global list of functions rather than a per-package list
	// mainly because we can refer to a function just by its index in this list.
	// When we go to call a function with a bytecode instruction, we only have
	// to specify the function index, rather than both a package AND function
	// index.
	Function *fns;
	int fns_count, fns_capacity;

	// Global list of constants that we can reference by index.
	Value *consts;
	int consts_count, consts_capacity;

	// The most recent error. This is set just before a longjmp back to the
	// protecting setjmp call.
	Err *err;
	jmp_buf guard;

	// Memory used for the runtime stack. This is persisted across calls to
	// `hy_run...` so that we can implement the REPL.
	Value *stack;
	int stack_size;
} VM;

// Creates a new virtual machine instance.
VM vm_new();

// Frees all the resources allocated by a virtual machine.
void vm_free(VM *vm);

// Creates a new package on the VM and returns its index.
int vm_new_pkg(VM *vm, uint64_t name);

// Creates a new function on the VM and returns its index.
int vm_new_fn(VM *vm, int pkg);

// Adds a constant number to the VM's constants list, returning its index.
int vm_add_num(VM *vm, double num);

// Triggers a longjmp back to the most recent setjmp protection.
void err_trigger(VM *vm, Err *err);

// Executes some code. The code is run within the package's "main" function,
// and can access any variables, functions, imports, etc. that were created by
// a previous piece of code run on this package. This functionality is used to
// create the REPL.
//
// Since no file path is specified, any imports are relative to the current
// working directory.
//
// If an error occurs, then the return value will be non-NULL.
Err * vm_run_string(VM *vm, int pkg, char *code);

// Executes a file. A new package is created for the file and is named based off
// the name of the file. The package can be later imported by other pieces of
// code.
//
// Both the directory containing the file and the current working directory are
// searched when the file attempts to import any other packages.
//
// If an error occurs, then the return value will be non-NULL.
Err * vm_run_file(VM *vm, char *path);

#endif
