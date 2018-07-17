
//
//  Hydrogen Command Line Interface
//  By Ben Anderson
//  July 2018
//

#include <hydrogen.h>

#include <stdio.h>
#include <string.h>

// Only if colors are supported
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

// Returns true if the terminal has color support.
int supports_color() {
#if defined(_WIN32) || defined(_WIN64)
	// Don't bother with color support on Windows; too hard
	return 0;
#else
	// Base color input only on the fact if the standard output is a terminal
	//
	// We should probably be checking the TERM environment variable for a
	// dumb terminal, or using the terminfo database, but in practice this
	// is probably unnecessary as we don't expect to have the CLI run on
	// really old hardware.
	return isatty(fileno(stdout));
#endif
}

// Prints a version message.
void print_version() {
	printf(
		"The Hydrogen Programming Language\n"
		"Version " HY_VERSION_STRING "\n"
		"Hydrogen is a toy programming language by Ben Anderson.\n"
	);
}

// Prints a help message.
void print_help() {
	print_version();
	printf(
		"\n"
		"Usage:\n"
		"  hydrogen [file] [arguments...]\n"
		"\n"
		"Options:\n"
		"  --version, -v   Show Hydrogen's version number\n"
		"  --help, -h      Show this help text\n"
		"A REPL is run if no file path is specified.\n"
	);
}

// Run the REPL.
int run_repl() {
	printf("REPL isn't implemented yet, sorry! :(\n");
	return 0;
}

// Run a source code string.
int run_string(char *code) {
	HyVM *vm = hy_new_vm();
	HyPkg pkg = hy_new_pkg(vm, NULL);
	HyErr *err = hy_run_string(vm, pkg, code);

	// Check for error
	int return_code = 0;
	if (err != NULL) {
		hy_err_print(err, supports_color());
		return_code = 1;
	}
	hy_free_vm(vm);
	return return_code;
}

// Run a file.
int run_file(char *path) {
	HyVM *vm = hy_new_vm();
	HyErr *err = hy_run_file(vm, path);

	// Check for error
	int return_code = 0;
	if (err != NULL) {
		hy_err_print(err, supports_color());
		return_code = 1;
	}
	hy_free_vm(vm);
	return return_code;
}

int main(int argc, char *argv[]) {
	// Help message
	if (argc >= 2 && (strcmp(argv[1], "--help") == 0 ||
			strcmp(argv[1], "-h") == 0)) {
		print_help();
		return 0;
	}

	// Version message
	if (argc >= 2 && (strcmp(argv[1], "--version") == 0 ||
			strcmp(argv[1], "-v") == 0)) {
		print_version();
		return 0;
	}

	// Run a file if there's a file path provided
	if (argc >= 2) {
		return run_file(argv[1]);
	} else {
		return run_repl();
	}
}
