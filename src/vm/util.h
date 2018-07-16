
// util.h
// By Ben Anderson
// July 2018

#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdint.h>

// Reads the contents of a file as a string. Returns NULL if the file couldn't
// be read. The returned string must be freed.
char * read_file(char *path);

// Extracts the name of a package from its file path and returns its hash.
// Returns !0 if a valid package name could not be extracted from the path.
uint64_t extract_pkg_name(char *path);

// Computes the FNV hash of a string.
uint64_t hash_string(char *string, size_t length);

#endif
