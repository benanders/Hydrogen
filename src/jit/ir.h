
// ir.h
// By Ben Anderson
// October 2018

// An IR instruction is a u64 split into 4, 16 bit segments. The first segment
// (lowest 16 bits) is the opcode. The next 2 segments are the arguments to the
// opcode. The remaining segment contains information about the instruction 
// (e.g. the argument types, the register assigned to the result of the 
// instruction, etc.)
//
// The IR is stored in an array. Variables are not explicitly numbered. Instead,
// each IR instruction is thought of as assigning a "new" variable, which can
// then be referenced by its index in the array. For example, consider a JIT
// trace of the code `a = a + 1`. This has the bytecode:
//
//   ADD_LN  0  0  0
//
// And the compiled (and optimised) SSA IR:
//
//   0: LOAD_CONST  0
//   1: LOAD_LOCAL  [0]
//   2: ADD  1  0
//   3: ---- LOOP ----
//   4: ADD  2  0
//   5: PHI  2  4
//
// The result of the LOAD_CONST instruction (loading the 0th constant from the 
// VM's constants list) is referred to as variable "1"; the result of the ADD
// instruction is referred to as variable "2". 
//
// ***
//
// The type of every variable in the IR is completely deterministic (as opposed
// to stochastic or random). That means that if a variable is a floating point
// number on the first iteration of the loop, then it'll always be a floating
// point number no matter what. There's no way for it to suddenly become a
// pointer or integer at some later iteration.
//
// Why is this the case? 1) The IR is a linear trace through the bytecode, there
// are no conditional jumps ("if" statements) - these are replaced by guard 
// assertions. 2) There are no native function calls from IR. JITing of any hot
// loop containing a native function call is aborted immediately.
//
// Why is this useful? It means the IR effectively becomes a "statically" typed
// language where the type of every variable is determined by whatever it is on
// the first loop iteration. We know the type of every variable when the IR is
// compiled.

#ifndef IR_H
#define IR_H

#include <stdint.h>

// Various prefixes for opcode types.
#define IROP_PREFIX_LOAD  0x00
#define IROP_PREFIX_ARITH 0x01

// All IR opcodes. 
typedef enum {
	// Loads (prefix 0x00)
	IR_LOAD_STACK = 0x0000, // Load a local from the stack
	IR_LOAD_CONST = 0x0001, // Load a constant from the constants list

	// Arithmetic (prefix 0x01)
	IR_ADD = 0x0100, // Add two numbers together
} IrOp;

// String representations of each opcode.
static char * IROP_NAMES[] = {
	// Loads
	"LOAD_STACK", "LOAD_CONST", 

	// Arithmetic
	[0x0100] = "ADD", 
};

// An IR instruction is a 64 bit unsigned integer, consisting of 4, 16 bit
// segments.
typedef uint64_t IrIns;

// We start counting IR instructions at 1 rather than 0 so that we can use an
// IR reference to 0 as a "does not exist" reference.
#define IR_NONE 0

// A reference to an IR instruction which can be used to index the IR array. IR
// references are 16 bits so they can fit into an argument for an IR 
// instruction.
typedef uint16_t IrRef;

// Creates a new IR instruction with 2 arguments.
static inline IrIns ir_new2(IrOp op, IrRef arg1, IrRef arg2) {
	return (IrIns) op | ((IrIns) arg1) << 16 | ((IrIns) arg2) << 32;
}

// Creates a new IR instruction with a single argument.
static inline IrIns ir_new1(IrOp op, uint32_t arg) {
	return (IrIns) op | ((IrIns) arg) << 16;
}

// Returns the opcode for an instruction.
static inline IrIns ir_op(IrIns ins) {
	return (IrOp) (ins & 0x000000000000ffff);
}

// Returns the prefix for an instruction's opcode.
static inline uint16_t ir_op_prefix(IrIns ins) {
	return (uint16_t) (ins & 0x000000000000ff00);
}

// Set the opcode for an instruction.
static inline void ir_set_op(IrIns *ins, IrOp op) {
	*ins = (*ins & 0xffffffffffff0000) | (IrIns) op;
}

// Returns the first argument for an instruction.
static inline IrRef ir_arg1(IrIns ins) {
	return (IrRef) (ins >> 16);
}

// Set the first argument for an instruction.
static inline void ir_set_arg1(IrIns *ins, IrRef arg1) {
	*ins = (*ins & 0xffffffff0000ffff) | ((IrIns) arg1) << 16;
}

// Returns the second argument for an instruction.
static inline IrRef ir_arg2(IrIns ins) {
	return (IrRef) (ins >> 32);
}

// Set the second argument for an instruction.
static inline void ir_set_arg2(IrIns *ins, IrRef arg2) {
	*ins = (*ins & 0xffff0000ffffffff) | ((IrIns) arg2) << 32;
}

// Returns the 32 bit argument (merge of arg1 and arg2).
static inline uint32_t ir_arg32(IrIns ins) {
	return (uint32_t) (ins >> 16);
}

// Returns the register to be used for the result of an instruction.
static inline uint16_t ir_reg(IrIns ins) {
	return (uint16_t) (ins >> 48);
}

// Set the register to be used for the result of an instruction.
static inline void ir_set_reg(IrIns *ins, uint16_t reg) {
	*ins = (*ins & 0x0000ffffffffffff) | ((IrIns) reg) << 48;
}

#endif
