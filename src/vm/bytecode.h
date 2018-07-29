
// bytecode.h
// By Ben Anderson
// July 2018

// A bytecode instruction is a u32 split into 4, 8 bit segments.
//
// Information about the arguments of various instructions:
//
// * Store instructions (Mov, Set N, etc.) take the destination stack slot in
//   the first argument, and the source stack slot, constant index, primitive
//   type, etc. in the combined 16 bit argument.
// * Binary arithmetic instructions take the destination stack slot in the first
//   argument, the left operand to the instruction in the second argument, and
//   the right operand in the third argument
// * Unary arithmetic instructions take their only operand in the combined 24
//   bit argument.
// * Jmp takes its jump offset as a biased 24 bit value. See the notes in
//   `src/vm/vm.asm` for more information about the bias
// * Jmp has its jump offset relative to the instruction AFTER the Jmp
//   instruction, due to a quirk in instruction parsing in the interpreter
//
// Because bytecode instructions have to fit stack slot indices into 8 bits,
// we're limited to 256 (2^8) available stack slots within each function scope.

#ifndef BYTECODE_H
#define BYTECODE_H

#include <stdint.h>

// All bytecode opcodes. We can have up to 256 opcodes, since they must be
// storable in a single byte.
typedef enum {
	// Stores
	OP_MOV,
	OP_SET_N,
	OP_SET_P,
	OP_SET_F,

	// Arithmetic operators
	OP_ADD_LL,
	OP_ADD_LN,
	OP_SUB_LL,
	OP_SUB_LN,
	OP_SUB_NL,
	OP_MUL_LL,
	OP_MUL_LN,
	OP_DIV_LL,
	OP_DIV_LN,
	OP_DIV_NL,
	OP_NEG,

	// Relational operators
	OP_EQ_LL,  // Equality
	OP_EQ_LN,
	OP_EQ_LP,
	OP_NEQ_LL, // Inequality
	OP_NEQ_LN,
	OP_NEQ_LP,
	OP_LT_LL,  // Less than
	OP_LT_LN,
	OP_LE_LL,  // Less than or equal to
	OP_LE_LN,
	OP_GT_LL,  // Greater than
	OP_GT_LN,
	OP_GE_LL,  // Greater than or equal to
	OP_GE_LN,

	// Control flow
	OP_JMP,
	OP_CALL, // Args: function slot, first argument slot, argument count
	OP_RET,
} Opcode;

// String representation of each opcode.
static char * OPCODE_NAMES[] = {
	// Stores
	"OP_MOV", "OP_SET_N", "OP_SET_P", "OP_SET_F",

	// Arithmetic operators
	"OP_ADD_LL", "OP_ADD_LN", "OP_SUB_LL", "OP_SUB_LN", "OP_SUB_NL",
	"OP_MUL_LL", "OP_MUL_LN", "OP_DIV_LL", "OP_DIV_LN", "OP_DIV_NL", "OP_NEG",

	// Relational operators
	"OP_EQ_LL", "OP_EQ_LN", "OP_EQ_LP", "OP_NEQ_LL", "OP_NEQ_LN", "OP_NEQ_LP",
	"OP_LT_LL", "OP_LT_LN", "OP_LE_LL", "OP_LE_LN", "OP_GT_LL", "OP_GT_LN",
	"OP_GE_LL", "OP_GE_LN",

	// Control flow
	"OP_JMP", "OP_CALL", "OP_RET",
};

// A bytecode instruction is a 32 bit integer, containing 4 8-bit parts. The
// first part (the lowest byte) is the opcode, and the remaining 3 parts are
// arguments to the instruction.
typedef uint32_t Instruction;

// Creates a new instruction with 3 arguments.
static inline Instruction ins_new3(Opcode op, uint8_t arg1, uint8_t arg2,
		uint8_t arg3) {
	return (uint32_t) op | ((uint32_t) arg1) << 8 | ((uint32_t) arg2) << 16 |
		((uint32_t) arg3) << 24;
}

// Creates a new instruction with 2 arguments. The first argument is an 8 bit
// value, and the second a combined 16 bit value (e.g. for stores).
static inline Instruction ins_new2(Opcode op, uint8_t arg1, uint16_t arg2) {
	return (uint32_t) op | ((uint32_t) arg1) << 8 | ((uint32_t) arg2) << 16;
}

// Creates a new instruction with a single, 24 bit argument (stored in the
// lowest 24 bits of a 32 bit value).
static inline Instruction ins_new1(Opcode op, uint32_t arg) {
	return (uint32_t) op | ((uint32_t) arg) << 8;
}

// Returns the opcode for an instruction.
static inline Opcode ins_op(Instruction ins) {
	return (Opcode) (ins & 0x000000ff);
}

// Sets the opcode for an instruction.
static inline void ins_set_op(Instruction *ins, Opcode opcode) {
	*ins = (*ins & 0xffffff00) | ((uint32_t) opcode);
}

// Returns the first argument for an instruction.
static inline uint8_t ins_arg1(Instruction ins) {
	return (uint8_t) ((ins & ((uint32_t) 0x0000ff00)) >> 8);
}

// Sets the first argument of an instruction.
static inline void ins_set_arg1(Instruction *ins, uint8_t arg1) {
	*ins = (*ins & 0xffff00ff) | ((uint32_t) arg1) << 8;
}

// Returns the second argument for an instruction.
static inline uint8_t ins_arg2(Instruction ins) {
	return (uint8_t) ((ins & ((uint32_t) 0x00ff0000)) >> 16);
}

// Returns the third argument for an instruction.
static inline uint8_t ins_arg3(Instruction ins) {
	return (uint8_t) ((ins & ((uint32_t) 0xff000000)) >> 24);
}

// Returns the combined 24 bit argument for a one-argument instruction.
static inline uint32_t ins_arg24(Instruction ins) {
	return (uint32_t) ((ins & ((uint32_t) 0xffffff00)) >> 8);
}

// Set the combined 24 bit argument for a one-argument instruction.
static inline void ins_set_arg24(Instruction *ins, uint32_t arg24) {
	*ins = (*ins & 0x000000ff) | arg24 << 8;
}

// Returns the combined 16 bit argument for a two-argument instruction.
static inline uint16_t ins_arg16(Instruction ins) {
	return (uint16_t) ((ins & ((uint32_t) 0xffff0000)) >> 16);
}

#endif
