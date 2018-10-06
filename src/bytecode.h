
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

// All bytecode opcodes. We can have up to 256 opcodes, since they need to fit
// into a single byte.
//
// Meaning of the various suffixes:
// * N = number
// * P = primitive (false, true, nil)
// * F = function
typedef enum {
	// Stores
	BC_MOV,
	BC_SET_N,
	BC_SET_P,
	BC_SET_F,

	// Arithmetic operators
	BC_ADD_LL,
	BC_ADD_LN,
	BC_SUB_LL,
	BC_SUB_LN,
	BC_SUB_NL,
	BC_MUL_LL,
	BC_MUL_LN,
	BC_DIV_LL,
	BC_DIV_LN,
	BC_DIV_NL,
	BC_NEG,

	// Relational operators
	BC_EQ_LL,  // Equality
	BC_EQ_LN,
	BC_EQ_LP,
	BC_NEQ_LL, // Inequality
	BC_NEQ_LN,
	BC_NEQ_LP,
	BC_LT_LL,  // Less than
	BC_LT_LN,
	BC_LE_LL,  // Less than or equal to
	BC_LE_LN,
	BC_GT_LL,  // Greater than
	BC_GT_LN,
	BC_GE_LL,  // Greater than or equal to
	BC_GE_LN,

	// Control flow
	BC_JMP,
	BC_LOOP, // Identical to the JMP instruction, but does hot loop detection
	         // for the JIT compiler
	BC_CALL, // Args: function slot, first argument slot, argument count
	BC_RET,
} BcOp;

// String representation of each opcode.
static char * BCOP_NAMES[] = {
	// Stores
	"MOV", "SETN", "SETP", "SETF",

	// Arithmetic operators
	"ADDLL", "ADDLN", "SUBLL", "SUBLN", "SUBNL",
	"MULLL", "MULLN", "DIVLL", "DIVLN", "DIVNL", "NEG",

	// Relational operators
	"EQLL", "EQLN", "EQLP", "NEQLL", "NEQLN", "NEQLP",
	"LTLL", "LTLN", "LELL", "LELN", "GTLL", "GTLN",
	"GELL", "GELN",

	// Control flow
	"JMP", "LOOP", "CALL", "RET",
};

// A bytecode instruction is a 32 bit integer, containing 4 8-bit parts. The
// first part (the lowest byte) is the opcode, and the remaining 3 parts are
// arguments to the instruction.
typedef uint32_t BcIns;

// Creates a new instruction with 3 arguments.
static inline BcIns bc_new3(BcOp op, uint8_t arg1, uint8_t arg2, uint8_t arg3) {
	return (BcIns) op | ((BcIns) arg1) << 8 | ((BcIns) arg2) << 16 |
		((BcIns) arg3) << 24;
}

// Creates a new instruction with 2 arguments. The first argument is an 8 bit
// value, and the second a combined 16 bit value (e.g. for stores).
static inline BcIns bc_new2(BcOp op, uint8_t arg1, uint16_t arg2) {
	return (BcIns) op | ((BcIns) arg1) << 8 | ((BcIns) arg2) << 16;
}

// Creates a new instruction with a single, 24 bit argument (stored in the
// lowest 24 bits of a 32 bit value).
static inline BcIns bc_new1(BcOp op, uint32_t arg) {
	return (BcIns) op | ((BcIns) arg) << 8;
}

// Returns the opcode for an instruction.
static inline BcOp bc_op(BcIns ins) {
	return (BcOp) (ins & 0x000000ff);
}

// Sets the opcode for an instruction.
static inline void bc_set_op(BcIns *ins, BcOp opcode) {
	*ins = (*ins & 0xffffff00) | ((BcIns) opcode);
}

// Returns the first argument for an instruction.
static inline uint8_t bc_arg1(BcIns ins) {
	return (uint8_t) ((ins & ((BcIns) 0x0000ff00)) >> 8);
}

// Sets the first argument of an instruction.
static inline void bc_set_arg1(BcIns *ins, uint8_t arg1) {
	*ins = (*ins & 0xffff00ff) | ((BcIns) arg1) << 8;
}

// Returns the second argument for an instruction.
static inline uint8_t bc_arg2(BcIns ins) {
	return (uint8_t) ((ins & ((BcIns) 0x00ff0000)) >> 16);
}

// Returns the third argument for an instruction.
static inline uint8_t bc_arg3(BcIns ins) {
	return (uint8_t) ((ins & ((BcIns) 0xff000000)) >> 24);
}

// Returns the combined 24 bit argument for a one-argument instruction.
static inline uint32_t bc_arg24(BcIns ins) {
	return (BcIns) ((ins & ((BcIns) 0xffffff00)) >> 8);
}

// Set the combined 24 bit argument for a one-argument instruction.
static inline void bc_set_arg24(BcIns *ins, uint32_t arg24) {
	*ins = (*ins & 0x000000ff) | arg24 << 8;
}

// Returns the combined 16 bit argument for a two-argument instruction.
static inline uint16_t bc_arg16(BcIns ins) {
	return (uint16_t) ((ins & ((BcIns) 0xffff0000)) >> 16);
}

#endif
