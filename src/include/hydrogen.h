
//
//  Hydrogen Programming Language
//  By Ben Anderson
//  July 2018
//

#ifndef HYDROGEN_H
#define HYDROGEN_H

#include <stdbool.h>

// The version of this Hydrogen distribution, expressed using semantic
// versioning.
#define HY_VERSION_MAJOR 0
#define HY_VERSION_MINOR 1
#define HY_VERSION_PATCH 0

// Human-readable version string.
#define HY_VERSION_STRING "0.1.0"

// Hydrogen has no global state; everything that's needed is stored in this
// struct. You can create multiple VMs and they'll all function independently.
typedef struct HyVM HyVM;

// To execute some code, it needs to live inside a package. Packages are also
// the only way to use Hydrogen's FFI.
typedef int HyPkg;

// Tells you a bunch of information when something goes wrong. Used for all
// types of errors, including parsing and runtime errors.
typedef struct HyErr HyErr;

// Creates a new virtual machine instance.
HyVM * hy_new_vm();

// Frees all the resources allocated by a virtual machine.
void hy_free_vm(HyVM *vm);

// Creates a new package on a virtual machine.
HyPkg hy_new_pkg(HyVM *vm, char *name);

// Executes some code. The code is run within the package's "main" function,
// and can access any variables, functions, imports, etc. that were created by
// a previous piece of code run on this package. This functionality is used to
// create the REPL.
//
// For example:
//    HyVM *vm = hy_new_vm();
//    HyPkg pkg = hy_new_pkg(vm, "test");
//    hy_run(vm, pkg, "let a = 3");
//    hy_run(vm, pkg, "a = 4"); // We're still able to access the variable `a`
//
// Since no file path is specified, any imports are relative to the current
// working directory.
//
// If an error occurs, then the return value will be non-NULL and the error must
// be freed using `hy_free_err`.
HyErr * hy_run_string(HyVM *vm, HyPkg pkg, char *code);

// Executes a file. A new package is created for the file and is named based off
// the name of the file. The package can be later imported by other pieces of
// code.
//
// Both the directory containing the file and the current working directory are
// searched when the file attempts to import any other packages.
//
// If an error occurs, then the return value is non-NULL and the error must be
// freed.
HyErr * hy_run_file(HyVM *vm, char *path);


// Returns a description of the error that's occurred.
char * hy_err_desc(HyErr *err);

// Returns the path to the file that an error occurred in, or NULL if the error
// has no associated file. The returned string should NOT be freed.
char * hy_err_file(HyErr *err);

// Returns the line number that the error occurred on, or -1 if the error has
// no associated line number.
int hy_err_line(HyErr *err);

// Pretty prints the error to the standard output. If `use_color` is true, then
// terminal color codes will be printed alongside the error information.
void hy_err_print(HyErr *err, bool use_color);

// Frees resources allocated by an error.
void hy_free_err(HyErr *err);

#endif
