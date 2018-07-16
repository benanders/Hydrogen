
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

// Maximum length of an error description string.
#define ERR_MAX_DESC_LEN 255

// Contains all information about an error.
struct HyErr {
	// Heap-allocated description string.
	char *desc;

	// Path to the file in which the error occurred, or NULL if the error has
	// no associated file (e.g. it occurred in a string).
	char *file;

	// Line on which the error occurred, or -1 if the error has no associated
	// line number.
	int line;
};

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

	// The most recent error. This is set just before a longjmp back to the
	// protecting setjmp call.
	HyErr *err;
	jmp_buf guard;
};

// Creates a new package on the VM and returns its index.
int vm_new_pkg(HyVM *vm, uint64_t name);

// Creates a new function on the VM and returns its index.
int vm_new_fn(HyVM *vm, int pkg_idx);

// Emits a bytecode instruction to a function.
int fn_emit(HyVM *vm, int fn_idx, Instruction ins);

// Creates a new error from a format string.
HyErr * err_new(char *fmt, ...);

// Copies a file path into a new heap allocated string to save with the error.
void err_set_file(HyErr *err, char *path);

// Triggers a longjmp back to the most recent setjmp protection.
void err_trigger(HyVM *vm, HyErr *err);

// Reads the contents of a file as a string. Returns NULL if the file couldn't
// be read. The returned string must be freed.
char * read_file(char *path);

// Extracts the name of a package from its file path and returns its hash.
// Returns !0 if a valid package name could not be extracted from the path.
uint64_t extract_pkg_name(char *path);

// Computes the FNV hash of a string.
uint64_t hash_string(char *string, size_t length);

#endif
