
// vm.c
// By Ben Anderson
// July 2018

#include "vm.h"
#include "parser.h"

#include <stdlib.h>
#include <stdio.h>

// Creates a new virtual machine instance.
HyVM * hy_new_vm() {
	HyVM *vm = malloc(sizeof(HyVM));
	vm->pkgs_capacity = 4;
	vm->pkgs_count = 0;
	vm->pkgs = malloc(sizeof(Package) * vm->pkgs_capacity);

	vm->fns_capacity = 16;
	vm->fns_count = 0;
	vm->fns = malloc(sizeof(Function) * vm->fns_capacity);
	return vm;
}

// Creates a new package on the VM and returns its index.
int vm_new_pkg(HyVM *vm, uint64_t name) {
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
int vm_new_fn(HyVM *vm, int pkg_idx) {
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

// Emits a bytecode instruction to a function.
int fn_emit(HyVM *vm, int fn_idx, Instruction ins) {
	Function *fn = &vm->fns[fn_idx];

	if (fn->ins == NULL) {
		// Lazily instantiate the bytecode array
		fn->ins_capacity = 32;
		fn->ins = malloc(sizeof(Instruction) * fn->ins_capacity);
	} else if (fn->ins_count >= fn->ins_capacity) {
		// Up the capacity of the bytecode array
		fn->ins_capacity *= 2;
		fn->ins = realloc(fn->ins, sizeof(Instruction) * fn->ins_capacity);
	}

	fn->ins[fn->ins_count++] = ins;
	return fn->ins_count - 1;
}

// Frees all the resources allocated by a virtual machine.
void hy_free_vm(HyVM *vm) {
	for (int i = 0; i < vm->fns_count; i++) {
		free(vm->fns[i].ins);
	}
	free(vm->pkgs);
	free(vm->fns);
	free(vm);
}

// Creates a new package on a virtual machine.
HyPkg hy_new_package(HyVM *vm, char *name) {
	return vm_new_pkg(vm, hash_string(name));
}

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
HyErr * hy_run_string(HyVM *vm, HyPkg pkg, char *code) {
	// TODO: save and restore VM state in case of error

	// Parse the source code
	HyErr *err = psr_parse(vm, pkg, code);
	if (err != NULL) {
		return err;
	}

	// TODO: run the code
	return NULL;
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
HyErr * hy_run_file(HyVM *vm, char *path) {
	// TODO: save and restore VM state in case of error

	// Extract the package name from the file path
	uint64_t name = extract_pkg_name(path);
	if (name == !((uint64_t) 0)) {
		return err_new("invalid package name from file path `%s`", path);
	}

	// Read the file contents
	char *code = read_file(path);
	if (code == NULL) {
		return err_new("failed to open file `%s`", path);
	}

	// Parse the source code
	int pkg = vm_new_pkg(vm, name);
	HyErr *err = psr_parse(vm, pkg, code);
	if (err != NULL) {
		return err;
	}

	// TODO: run the code
	return NULL;
}

// Extracts the name of a package from its file path and returns its hash.
// Returns !0 if a valid package name could not be extracted from the path.
uint64_t extract_pkg_name(char *path) {
	// TODO
	return !0;
}

// Creates a new error from a format string.
HyErr * err_new(char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char *desc = malloc(sizeof(char) * ERR_MAX_DESC_LEN);
	vsnprintf(desc, ERR_MAX_DESC_LEN, fmt, args);
	va_end(args);

	HyErr *err = malloc(sizeof(HyErr));
	err->desc = desc;
	err->line = -1;
	err->file = NULL;
	return err;
}

// Frees resources allocated by an error.
void hy_free_err(HyErr *err) {
	// Built-in resilience against a dumb user
	if (err == NULL) {
		return;
	}
	free(err->desc);
	free(err->file);
	free(err);
}

// Returns a description of the error that's occurred.
char * hy_err_desc(HyErr *err) {
	return err->desc;
}

// Returns the path to the file that an error occurred in, or NULL if the error
// has no associated file. The returned string should NOT be freed.
char * hy_err_file(HyErr *err) {
	return err->file;
}

// Returns the line number that the error occurred on, or -1 if the error has
// no associated line number.
int hy_err_line(HyErr *err) {
	return err->line;
}

// Pretty prints the error to the standard output. If `use_color` is true, then
// terminal color codes will be printed alongside the error information.
void hy_err_print(HyErr *err, bool use_color) {
	// TODO
}

// Reads the contents of a file as a string. Returns NULL if the file couldn't
// be read. The returned string must be freed.
char * read_file(char *path) {
	FILE *f = fopen(path, "r");
	if (f == NULL) {
		return NULL;
	}

	// Get the length of the file
	// TODO: proper error handling; so many of these functions set errno
	fseek(f, 0, SEEK_END);
	size_t length = (size_t) ftell(f);
	rewind(f);

	// Read its contents
	char *contents = malloc(length + 1);
	fread(contents, sizeof(char), length, f);
	fclose(f);
	contents[length] = '\0';
	return contents;
}

// Magic prime number for FNV hashing.
#define FNV_64_PRIME ((uint64_t) 0x100000001b3ULL)

// Computes the FNV hash of a string.
uint64_t hash_string(char *string) {
	// Convert to an unsigned string
	unsigned char *str = (unsigned char *) string;

	// Hash each byte of the string
	uint64_t hash = 0;
	while (*str != '\0') {
		// Multiply by the magic prime, modulo 2^64 from integer overflow
		hash *= FNV_64_PRIME;

		// XOR the lowest byte of the hash with the current octet
		hash ^= (uint64_t) *str++;
	}

	return hash;
}
