
// compile.c
// By Ben Anderson
// October 2018

#include "compile.h"

#include <assert.h>
#include <stdio.h>

// Macro for unimplemented functions.
#define UNIMPLEMENTED() while (1) { assert(false); }

// Create a new JIT trace.
void jit_trace_new(Trace *trace, VM *vm) {
	trace->vm = vm;
	trace->ir_count = 0;
	trace->ir_capacity = 256;
	trace->ir_offset = trace->ir_capacity / 2;
	trace->ir = malloc(sizeof(IrIns) * trace->ir_capacity);
	printf("Started!!\n");
}

// Release resources associated with a trace.
void jit_trace_free(Trace *trace) {
	free(trace->ir);
}

// Finish recording a JIT trace.
void jit_rec_finish(Trace *trace) {
	printf("Finished!\n");
}

// Stores
void jit_rec_MOV(Trace *trace, BcIns bc)   { UNIMPLEMENTED(); }
void jit_rec_SET_N(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_SET_P(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_SET_F(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }

// Arithmetic
void jit_rec_ADD_LL(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_ADD_LN(Trace *trace, BcIns bc) { printf("Add LN\n"); }
void jit_rec_SUB_LL(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_SUB_LN(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_SUB_NL(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_MUL_LL(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_MUL_LN(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_DIV_LL(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_DIV_LN(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_DIV_NL(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_NEG(Trace *trace, BcIns bc)    { UNIMPLEMENTED(); }

// Relational operators
void jit_rec_EQ_LL(Trace *trace, BcIns bc)   { UNIMPLEMENTED(); }
void jit_rec_EQ_LN(Trace *trace, BcIns bc)   { UNIMPLEMENTED(); }
void jit_rec_EQ_LP(Trace *trace, BcIns bc)   { UNIMPLEMENTED(); }
void jit_rec_NEQ_LL(Trace *trace, BcIns bc)  { UNIMPLEMENTED(); }
void jit_rec_NEQ_LN(Trace *trace, BcIns bc)  { UNIMPLEMENTED(); }
void jit_rec_NEQ_LP(Trace *trace, BcIns bc)  { UNIMPLEMENTED(); }
void jit_rec_LT_LL(Trace *trace, BcIns bc)   { UNIMPLEMENTED(); }
void jit_rec_LT_LN(Trace *trace, BcIns bc)   { UNIMPLEMENTED(); }
void jit_rec_LE_LL(Trace *trace, BcIns bc)   { UNIMPLEMENTED(); }
void jit_rec_LE_LN(Trace *trace, BcIns bc)   { UNIMPLEMENTED(); }
void jit_rec_GT_LL(Trace *trace, BcIns bc)   { UNIMPLEMENTED(); }
void jit_rec_GT_LN(Trace *trace, BcIns bc)   { UNIMPLEMENTED(); }
void jit_rec_GE_LL(Trace *trace, BcIns bc)   { UNIMPLEMENTED(); }
void jit_rec_GE_LN(Trace *trace, BcIns bc)   { UNIMPLEMENTED(); }

// Control flow
void jit_rec_CALL(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_RET(Trace *trace, BcIns bc)  { UNIMPLEMENTED(); }
