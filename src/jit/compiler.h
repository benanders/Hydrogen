
// compiler.h
// By Ben Anderson
// October 2018

// A JIT compiler records linear traces through hot loops in the bytecode and 
// turns them into SSA form IR. If this doesn't make any sense to you, go do 
// some pre-reading (the Wikipedia page on tracing JIT compilers and SSA IR is
// a good place to start).
//
// The IR is stored in an array. An IR instruction in this array is referenced
// by its index. Load instructions for constants are added to the start of the 
// array, and compiled instructions are added to the end (the array is 
// bi-directionally grown).

#ifndef COMPILE_H
#define COMPILE_H

#include "../vm.h"
#include "ir.h"

// Threshold number of iterations that loop has to execute before we trigger the
// JIT compiler.
#define JIT_THRESHOLD 50

// The maximum number of IR instructions that we can emit.
#define MAX_IR_INS 2048

// Information required to compile an IR trace.
typedef struct {
	// Pointer to the VM.
	VM *vm;

	// The compiled IR for the trace so far. 
	IrIns *ir;
	int ir_count, ir_capacity;

	// The most recent instruction to modify a stack variable (an array indexed
	// by the stack slot of the variable), used to construct SSA form IR.
	//
	// We start counting IR references at 1 so that we can initialise this whole
	// array to 0 (meaning "does not exist") using calloc, rather than having to
	// iterate over it to set every entry to -1.
	IrRef last_modified[MAX_LOCALS_IN_FN];

	// Indexing this array by the index of a constant in the VM's constants list
	// gives an IR reference to the load instruction for that constant.
	IrRef const_loads[MAX_CONSTS];
} Trace;

// Create a new JIT trace.
Trace * jit_trace_new(VM *vm);

// Release resources associated with a trace.
void jit_trace_free(Trace *trace);

// Pretty print the compiled IR for a trace to the standard output.
void jit_trace_dump(Trace *trace);

// Finishing a trace involves optimising the IR, register allocation, and
// machine code generation.
void jit_rec_finish(Trace *trace);


// Trace recording functions. These are called by the interpreter during runtime
// to compile a bytecode instruction to some IR. There's one for (almost) every
// bytecode instruction (except LOOP).

// Stores
void jit_rec_MOV(Trace *trace, BcIns bc);
void jit_rec_SET_N(Trace *trace, BcIns bc);
void jit_rec_SET_P(Trace *trace, BcIns bc);
void jit_rec_SET_F(Trace *trace, BcIns bc);

// Arithmetic
void jit_rec_ADD_LL(Trace *trace, BcIns bc); // Addition
void jit_rec_ADD_LN(Trace *trace, BcIns bc);
void jit_rec_SUB_LL(Trace *trace, BcIns bc); // Subtraction
void jit_rec_SUB_LN(Trace *trace, BcIns bc);
void jit_rec_SUB_NL(Trace *trace, BcIns bc);
void jit_rec_MUL_LL(Trace *trace, BcIns bc); // Multiplication
void jit_rec_MUL_LN(Trace *trace, BcIns bc);
void jit_rec_DIV_LL(Trace *trace, BcIns bc); // Division
void jit_rec_DIV_LN(Trace *trace, BcIns bc);
void jit_rec_DIV_NL(Trace *trace, BcIns bc);
void jit_rec_NEG(Trace *trace, BcIns bc);    // Negation

// Relational operators
void jit_rec_EQ_LL(Trace *trace, BcIns bc);  // Equality
void jit_rec_EQ_LN(Trace *trace, BcIns bc);
void jit_rec_EQ_LP(Trace *trace, BcIns bc);
void jit_rec_NEQ_LL(Trace *trace, BcIns bc); // Inequality
void jit_rec_NEQ_LN(Trace *trace, BcIns bc);
void jit_rec_NEQ_LP(Trace *trace, BcIns bc);
void jit_rec_LT_LL(Trace *trace, BcIns bc);  // Less than
void jit_rec_LT_LN(Trace *trace, BcIns bc);
void jit_rec_LE_LL(Trace *trace, BcIns bc);  // Less than or equal to
void jit_rec_LE_LN(Trace *trace, BcIns bc);
void jit_rec_GT_LL(Trace *trace, BcIns bc);  // Greater than
void jit_rec_GT_LN(Trace *trace, BcIns bc);
void jit_rec_GE_LL(Trace *trace, BcIns bc);  // Greater than or equal to
void jit_rec_GE_LN(Trace *trace, BcIns bc);

// Control flow
void jit_rec_CALL(Trace *trace, BcIns bc);
void jit_rec_RET(Trace *trace, BcIns bc);

#endif
