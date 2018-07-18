
// vm.c
// By Ben Anderson
// July 2018

#include "vm.h"
#include "parser.h"
#include "err.h"
#include "util.h"

#include <string.h>

// Creates a new virtual machine instance.
HyVM * hy_new_vm() {
	HyVM *vm = malloc(sizeof(HyVM));
	vm->pkgs_capacity = 4;
	vm->pkgs_count = 0;
	vm->pkgs = malloc(sizeof(Package) * vm->pkgs_capacity);

	vm->fns_capacity = 16;
	vm->fns_count = 0;
	vm->fns = malloc(sizeof(Function) * vm->fns_capacity);

	vm->consts_capacity = 16;
	vm->consts_count = 0;
	vm->consts = malloc(sizeof(uint64_t) * vm->consts_capacity);
	return vm;
}

// Frees all the resources allocated by a virtual machine.
void hy_free_vm(HyVM *vm) {
	for (int i = 0; i < vm->fns_count; i++) {
		free(vm->fns[i].ins);
	}
	free(vm->pkgs);
	free(vm->fns);
	free(vm->consts);
	free(vm);
}

// Adds a constant number to the VM's constants list, returning its index.
int vm_add_const_num(HyVM *vm, double num) {
	if (vm->consts_capacity >= vm->consts_count) {
		vm->consts_capacity *= 2;
		vm->consts = realloc(vm->consts, sizeof(uint64_t) * vm->consts_capacity);
	}

	// Convert the number bitwise into a uint64_t
	union {
		double num;
		uint64_t val;
	} conversion;
	conversion.num = num;

	// Check if the constant already exists
	for (int i = 0; i < vm->consts_count; i++) {
		if (vm->consts[i] == conversion.val) {
			return i;
		}
	}

	// Add the converted number to the constants list if it doesn't already
	// exist
	vm->consts[vm->consts_count++] = conversion.val;
	return vm->consts_count - 1;
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

// Creates a new package on a virtual machine.
HyPkg hy_new_pkg(HyVM *vm, char *name) {
	return vm_new_pkg(vm, hash_string(name, strlen(name)));
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
int fn_emit(Function *fn, Instruction ins) {
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
	HyErr *err = parse(vm, pkg, NULL, code);
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
		HyErr *err = err_new("invalid package name from file path `%s`", path);
		err_set_file(err, path);
		return err;
	}

	// Read the file contents
	char *code = read_file(path);
	if (code == NULL) {
		HyErr *err = err_new("failed to open file `%s`", path);
		err_set_file(err, path);
		return err;
	}

	// Parse the source code
	int pkg = vm_new_pkg(vm, name);
	HyErr *err = parse(vm, pkg, path, code);
	free(code);
	if (err != NULL) {
		return err;
	}

	// TODO: run the code
	return NULL;
}
