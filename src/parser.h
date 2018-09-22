
// parser.h
// By Ben Anderson
// July 2018

#ifndef PARSER_H
#define PARSER_H

#include "vm.h"

// The jump bias is a number added to all jump offsets. The actual jump offset
// (relative to the instruction AFTER the jump instruction) is calculated by
// subtracting the bias from the instruction argument.
//
// See the comment in `jmp_follow` in parser.c for why we use a jump bias.
#define JMP_BIAS 0x800000

// Parses the source code into bytecode. All bytecode for top level code gets
// appended to the package's main function. All other functions defined in the
// code get created on the VM and associated with the given package.
Err * parse(VM *vm, int pkg, char *path, char *code);

#endif
