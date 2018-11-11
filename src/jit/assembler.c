
// assembler.c
// By Ben Anderson
// November 2018

#include "assembler.h"

// Allocates a new machine code chunk.
MCodeChunk asm_new() {
	MCodeChunk chunk;
	chunk.ins_count = 0;
	chunk.ins_capacity = 64;
	chunk.ins = malloc(sizeof(uint8_t) * chunk.ins_capacity);
	return chunk;
}

// Appends a byte to the assembly chunk.
void asm_append_u8(MCodeChunk *chunk, uint8_t arg) {
	if (chunk->ins_count >= chunk->ins_capacity) {
		chunk->ins_capacity *= 2;
		chunk->ins = realloc(chunk->ins, sizeof(uint8_t) * chunk->ins_capacity);
	}
	chunk->ins[chunk->ins_count++] = arg;
}

// Appends a uint16_t to the assembly chunk, with the correct byte order
// depending on the target architecture.
void asm_append_u16(MCodeChunk *chunk, uint16_t arg) {
#if ARCH_ENDIAN == ENDIAN_LITTLE
	asm_append_u8(chunk, (uint8_t) arg);
	asm_append_u8(chunk, (uint8_t) (arg >> 8));
#else
	asm_append_u8(chunk, (uint8_t) (arg >> 8));
	asm_append_u8(chunk, (uint8_t) arg);
#endif
}

// Appends a uint32_t to the assembly chunk, with the correct byte order
// depending on the target architecture.
void asm_append_u32(MCodeChunk *chunk, uint32_t arg) {
#if ARCH_ENDIAN == ENDIAN_LITTLE
	asm_append_u8(chunk, (uint8_t) arg);
	asm_append_u8(chunk, (uint8_t) (arg >> 8));
	asm_append_u8(chunk, (uint8_t) (arg >> 16));
	asm_append_u8(chunk, (uint8_t) (arg >> 24));
#else
	asm_append_u8(chunk, (uint8_t) (arg >> 24));
	asm_append_u8(chunk, (uint8_t) (arg >> 16));
	asm_append_u8(chunk, (uint8_t) (arg >> 8));
	asm_append_u8(chunk, (uint8_t) arg);
#endif
}

// Appends a uint64_t to the assembly chunk, with the correct byte order
// depending on the target architecture.
void asm_append_u64(MCodeChunk *chunk, uint64_t arg) {
#if ARCH_ENDIAN == ENDIAN_LITTLE
	asm_append_u8(chunk, (uint8_t) arg);
	asm_append_u8(chunk, (uint8_t) (arg >> 8));
	asm_append_u8(chunk, (uint8_t) (arg >> 16));
	asm_append_u8(chunk, (uint8_t) (arg >> 24));
	asm_append_u8(chunk, (uint8_t) (arg >> 32));
	asm_append_u8(chunk, (uint8_t) (arg >> 40));
	asm_append_u8(chunk, (uint8_t) (arg >> 48));
	asm_append_u8(chunk, (uint8_t) (arg >> 56));
#else
	asm_append_u8(chunk, (uint8_t) (arg >> 56));
	asm_append_u8(chunk, (uint8_t) (arg >> 48));
	asm_append_u8(chunk, (uint8_t) (arg >> 40));
	asm_append_u8(chunk, (uint8_t) (arg >> 32));
	asm_append_u8(chunk, (uint8_t) (arg >> 24));
	asm_append_u8(chunk, (uint8_t) (arg >> 16));
	asm_append_u8(chunk, (uint8_t) (arg >> 8));
	asm_append_u8(chunk, (uint8_t) arg);
#endif
}
