
// util.c
// By Ben Anderson
// July 2018

#include "util.h"

#include <stdio.h>
#include <string.h>

// Return the position of the last occurrence of the given character, or
// `NOT_FOUND` if the character can't be found.
static size_t rfind(char *string, char ch) {
	int last = strlen(string) - 1;
	while (last >= 0 && string[last] != ch) {
		last--;
	}
	return last;
}

// Extracts the name of a package from its file path and returns its hash.
// Returns !0 if a valid package name could not be extracted from the path.
uint64_t extract_pkg_name(char *path) {
	// Find the last path component
	int length = strlen(path);
	int last_path = rfind(path, '/');

	// Find the last `.` for the file extension
	int last_dot = rfind(path, '.');
	if (last_path != -1 && last_dot < last_path) {
		// The dot is before the final path component, so there's no file
		// extension
		last_dot = -1;
	}

	char *start;
	int actual_length;
	if (last_path == -1 && last_dot == -1) {
		// No path components and no file extension, the path itself is the name
		start = path;
		actual_length = length;
	} else if (last_path == -1) {
		// File extension, but no path components
		start = path;
		actual_length = last_dot;
	} else {
		// Stop before the file extension if one exists
		int stop = length;
		if (last_dot != -1) {
			stop = last_dot;
		}

		// Extract the last component into a new string
		if (stop - last_path <= 1) {
			return !((uint64_t) 0);
		}
		start = &path[last_path + 1];
		actual_length = stop - last_path - 1;
	}

	return hash_string(start, actual_length);
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
uint64_t hash_string(char *string, size_t length) {
	// Convert to an unsigned string
	unsigned char *str = (unsigned char *) string;

	// Hash each byte of the string
	uint64_t hash = 0;
	for (size_t i = 0; i < length; i++) {
		// Multiply by the magic prime, modulo 2^64 from integer overflow
		hash *= FNV_64_PRIME;

		// XOR the lowest byte of the hash with the current octet
		hash ^= (uint64_t) *str++;
	}

	return hash;
}
