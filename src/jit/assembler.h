
// assembler.h
// By Ben Anderson
// November 2018

#ifndef ASSEMBLER_H

#include "arch.h"
#include "compiler.h"
#include "ir.h"

#include <stdint.h>

// We define a common interface for all assemblers for each architecture. An
// assembler just has to implement code for these functions.

// Since we output assembly code on the fly (there's no intermediate 
// representation), if we wanted to print the assembly code we're outputting on
// the fly we'd require an "if" condition at the start of each assembly output
// function. This would be inefficient in production, so instead we define a
// compiler flag for debugging, rather than a runtime flag.
#define ASM_DEBUG // Uncomment to print assembled code

// Assembled code is just a sequence of encoded machine instructions, which we
// store as a byte array.
typedef struct {
	uint8_t *ins;
	size_t ins_count, ins_capacity;
} MCodeChunk;


// Interface requiring implementation

// Assembles an IR trace into a chunk of machine code.
MCodeChunk jit_assemble(Trace *trace);


// Common helper functions

// Allocates a new machine code chunk.
MCodeChunk asm_new();

// Appends a byte to the assembly chunk.
void asm_append_u8(MCodeChunk *chunk, uint8_t byte);

// Appends a uint16_t to the assembly chunk, with the correct byte order
// depending on the target architecture.
void asm_append_u16(MCodeChunk *chunk, uint16_t word);

// Appends a uint32_t to the assembly chunk, with the correct byte order
// depending on the target architecture.
void asm_append_u32(MCodeChunk *chunk, uint32_t quad);

// Appends a uint64_t to the assembly chunk, with the correct byte order
// depending on the target architecture.
void asm_append_u64(MCodeChunk *chunk, uint64_t double_quad);

#endif
