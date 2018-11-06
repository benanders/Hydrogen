
// compiler.c
// By Ben Anderson
// October 2018

#include "compiler.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// Macro for unimplemented functions.
#define UNIMPLEMENTED() while (1) { assert(false); }

// Create a new JIT trace.
Trace * jit_trace_new(VM *vm) {
	Trace *trace = malloc(sizeof(Trace));
	trace->vm = vm;
	trace->ir_count = 1;
	trace->ir_capacity = 256;
	trace->ir = malloc(sizeof(IrIns) * trace->ir_capacity);
	memset(trace->last_modified, 0, sizeof(IrRef) * MAX_LOCALS_IN_FN);
	memset(trace->const_loads, 0, sizeof(IrRef) * MAX_CONSTS);
	return trace;
}

// Release resources associated with a trace.
void jit_trace_free(Trace *trace) {
	free(trace->ir);
	free(trace);
}

// Pretty print the compiled IR for a trace to the standard output.
void jit_trace_dump(Trace *trace) {
	printf("---- Trace ----\n");
	for (int i = 1; i < trace->ir_count; i++) {
		IrIns *ins = &trace->ir[i];
		printf("  %0.4d  %s  %d  %d\n", i, IROP_NAMES[ir_op(*ins)], 
			ir_arg1(*ins), ir_arg2(*ins));
	}
}

// Emit an IR instruction that isn't a constant load. Returns the index that can
// be used to reference the IR instruction.
static IrRef ir_emit(Trace *trace, IrIns ins) {
	// Check if we need to reallocate the IR array
	if (trace->ir_count >= trace->ir_capacity) {
		trace->ir_capacity *= 2;
		trace->ir = realloc(trace->ir, sizeof(IrIns) * trace->ir_capacity);
	}

	// Add the IR instruction
	trace->ir[trace->ir_count++] = ins;
	return trace->ir_count - 1;
}

// If a stack variable hasn't been loaded yet, then emits a stack load
// instruction and returns the IR reference to it. Otherwise returns a reference
// to the most recent instruction to modify the local.
static IrRef ir_load_stack(Trace *trace, uint8_t slot) {
	if (trace->last_modified[slot] == IR_NONE) {
		// Emit a stack load instruction
		IrRef load = ir_emit(trace, ir_new1(IR_LOAD_STACK, slot));
		trace->last_modified[slot] = load;
		return load;
	} else {
		// Return the last instruction to modify this local
		return trace->last_modified[slot];
	}
}

// If a constant hasn't been loaded yet, then emits a load instruction and
// returns the IR reference to it. Otherwise returns a reference to the already
// existing constant load.
static IrRef ir_load_const(Trace *trace, int const_idx) {
	if (trace->const_loads[const_idx] == IR_NONE) {
		// Emit a load constant instruction
		IrIns ins = ir_new1(IR_LOAD_CONST, (uint32_t) const_idx);
		IrRef load = ir_emit(trace, ins);
		trace->const_loads[const_idx] = load;
		return load;
	} else {
		// Return the index of the constant load instruction
		return trace->const_loads[const_idx];
	}
}

// Finishing a trace involves optimising the IR, register allocation, and
// machine code generation.
void jit_rec_finish(Trace *trace) {
	
}


// ---- Stores ----------------------------------------------------------------

void jit_rec_MOV(Trace *trace, BcIns bc)   {
	// Implement a MOV by updating the last instruction to modify the
	// destination slot
	trace->last_modified[bc_arg1(bc)] = trace->last_modified[bc_arg2(bc)];
}

void jit_rec_SET_N(Trace *trace, BcIns bc) {
	// Load the constant
	IrRef load = ir_load_const(trace, bc_arg16(bc));
	trace->last_modified[bc_arg1(bc)] = load;
}

void jit_rec_SET_P(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_SET_F(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }


// ---- Arithmetic ------------------------------------------------------------

void jit_rec_ADD_LL(Trace *trace, BcIns bc) {
	// Load the arguments to the add instruction
	IrRef left = ir_load_stack(trace, bc_arg2(bc));
	IrRef right = ir_load_stack(trace, bc_arg3(bc));

	// Emit an add instruction
	IrRef result = ir_emit(trace, ir_new2(IR_ADD, left, right));

	// Keep track of the last instruction that modified the destination slot
	trace->last_modified[bc_arg1(bc)] = result;

	// All the remaining arithmetic instructions are exactly like this...
}

void jit_rec_ADD_LN(Trace *trace, BcIns bc) { 
	IrRef left = ir_load_stack(trace, bc_arg2(bc));
	IrRef right = ir_load_const(trace, bc_arg3(bc));
	IrRef result = ir_emit(trace, ir_new2(IR_ADD, left, right));
	trace->last_modified[bc_arg1(bc)] = result;
}

void jit_rec_SUB_LL(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_SUB_LN(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_SUB_NL(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_MUL_LL(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_MUL_LN(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_DIV_LL(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_DIV_LN(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_DIV_NL(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_NEG(Trace *trace, BcIns bc)    { UNIMPLEMENTED(); }


// ---- Relational Operators --------------------------------------------------

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


// ---- Control Flow ----------------------------------------------------------

void jit_rec_CALL(Trace *trace, BcIns bc) { UNIMPLEMENTED(); }
void jit_rec_RET(Trace *trace, BcIns bc)  { UNIMPLEMENTED(); }
