
// err.h
// By Ben Anderson
// July 2018

#ifndef ERR_H
#define ERR_H

#include "vm.h"

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

// Creates a new error from a format string.
HyErr * err_new(char *fmt, ...);

// Copies a file path into a new heap allocated string to save with the error.
void err_set_file(HyErr *err, char *path);

// Triggers a longjmp back to the most recent setjmp protection.
void err_trigger(HyVM *vm, HyErr *err);

#endif
