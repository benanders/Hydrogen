
// vm.h
// By Ben Anderson
// July 2018

#ifndef VM_H
#define VM_H

#include <hydrogen.h>

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <setjmp.h>

#include "bytecode.h"

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
	Instruction *ins;
	int ins_count, ins_capacity;
} Function;

// Hydrogen has no global state; everything that's needed is stored in this
// struct. You can create multiple VMs and they'll all function independently.
struct HyVM {
	// We keep a list of all loaded packages so that if a piece of code attempts
	// to import a package we've already loaded, we don't have to re-load that
	// package.
	Package *pkgs;
	int pkgs_count, pkgs_capacity;

	// We keep a global list of functions, rather than a per-package list,
	// mainly because we can refer to a function just by its index in this list.
	// When we go to call a function with a bytecode instruction, we only have
	// to specify the function index, rather than both a package AND function
	// index.
	Function *fns;
	int fns_count, fns_capacity;

	// Global list of constants that we can reference by index.
	uint64_t *consts;
	int consts_count, consts_capacity;

	// The most recent error. This is set just before a longjmp back to the
	// protecting setjmp call.
	HyErr *err;
	jmp_buf guard;

	// Memory used for the runtime stack. This is persisted across calls to
	// `hy_run...` so that we can implement the REPL.
	uint64_t *stack;
	int stack_size;
};

// Creates a new package on the VM and returns its index.
int vm_new_pkg(HyVM *vm, uint64_t name);

// Creates a new function on the VM and returns its index.
int vm_new_fn(HyVM *vm, int pkg_idx);

// Adds a constant number to the VM's constants list, returning its index.
int vm_add_const_num(HyVM *vm, double num);

// Emits a bytecode instruction to a function.
int fn_emit(Function *fn, Instruction ins);

// Dumps the bytecode for a function to the standard output.
void fn_dump(Function *fn);

#endif
