
// parser.h
// By Ben Anderson
// July 2018

#ifndef PARSER_H
#define PARSER_H

#include "vm.h"

// Parses the source code into bytecode. All bytecode for top level code gets
// appended to the package's main function. All other functions defined in the
// code get created on the VM and associated with the given package.
HyErr * parse(HyVM *vm, int pkg, char *path, char *code);

#endif
