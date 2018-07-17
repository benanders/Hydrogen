
// err.c
// By Ben Anderson
// July 2018

#include "err.h"

#include <stdio.h>
#include <string.h>

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

// Creates a new error from a vararg list.
HyErr * err_vnew(char *fmt, va_list args) {
	char *desc = malloc(sizeof(char) * ERR_MAX_DESC_LEN);
	vsnprintf(desc, ERR_MAX_DESC_LEN, fmt, args);

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

// Triggers a longjmp back to the most recent setjmp protection.
void err_trigger(HyVM *vm, HyErr *err) {
	vm->err = err;
	longjmp(vm->guard, 1);
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

// Copies a file path into a new heap allocated string to save with the error.
void err_set_file(HyErr *err, char *path) {
	if (path == NULL) {
		return;
	}
	err->file = malloc(sizeof(char) * (strlen(path) + 1));
	strcpy(err->file, path);
}

// Returns the line number that the error occurred on, or -1 if the error has
// no associated line number.
int hy_err_line(HyErr *err) {
	return err->line;
}

// ANSI terminal color codes.
#define TEXT_RESET_ALL "\033[0m"
#define TEXT_BOLD      "\033[1m"
#define TEXT_RED       "\033[31m"
#define TEXT_GREEN     "\033[32m"
#define TEXT_BLUE      "\033[34m"
#define TEXT_WHITE     "\033[37m"

// Pretty prints an error to the standard output with terminal color codes.
static void err_print_color(HyErr *err) {
	printf("error: %s\n", err->desc);
}

// Pretty prints an error to the standard output in black and white.
static void err_print_bw(HyErr *err) {
	printf("error: %s\n", err->desc);
}

// Pretty prints the error to the standard output. If `use_color` is true, then
// terminal color codes will be printed alongside the error information.
void hy_err_print(HyErr *err, bool use_color) {
	if (err == NULL) {
		return;
	}
	if (use_color) {
		err_print_color(err);
	} else {
		err_print_bw(err);
	}
}
