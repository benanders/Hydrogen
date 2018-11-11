
// x64.c
// By Ben Anderson
// November 2018

#include <stdbool.h>
#include <assert.h>

#include "../assembler.h"

#ifdef ASM_DEBUG
#include <stdio.h>
#endif

// Possible floating point calculation modes.
// See:
// * https://www.cs.uaf.edu/2012/fall/cs301/lecture/11_02_other_float.html
// * https://en.wikipedia.org/wiki/X86_instruction_listings
// * https://stackoverflow.com/questions/28939652/how-to-detect-sse-sse2-avx-
//   avx2-avx-512-avx-128-fma-kcvi-availability-at-compile
// * https://en.wikipedia.org/wiki/Advanced_Vector_Extensions
// * https://msdn.microsoft.com/en-us/library/b0084kay.aspx (for Windows)
#define HY_FPU  0
#define HY_SSE2 1 // We need SSE2 or greater (not SSE) for doubles.
#define HY_AVX  2

// Detect which floating point arithmetic instructions to use.
#if HY_ARCH == HY_ARCH_X86 || HY_ARCH == HY_ARCH_X64
#if defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__)
#define HY_ARCH_FP HY_AVX
#define HY_ARCH_NUM_REGS 16
#elif (defined(__SSE2__) || defined(__SSE3__) || defined(__SSE4_1__) || \
	defined(__SSE4_2__) || (_M_IX86_FP == 2))
#define HY_ARCH_FP HY_SSE2
#define HY_ARCH_NUM_REGS 16
#else
#define HY_ARCH_FP HY_FPU
#error "SSE2 or above required"
#endif
#endif


// ---- Register Allocation ---------------------------------------------------

// Calculates the live range of each instruction.
static void asm_calculate_live_ranges(Trace *trace, IrRef *live_ranges) {
	// Iterate over all instructions in reverse order. The last instruction to
	// use a variable defines its live range (a property of SSA form).
	for (IrRef i = trace->ir_count - 1; i >= 1; i--) {
		IrIns ins = trace->ir[i];

		// Depending on if the instruction has a reference to another 
		// instruction, which we can determine from its opcode prefix
		if (ir_op_prefix(ins) == IROP_PREFIX_LOAD) {
			// No reference to any other instructions
		} else {
			// Two references to other instructions
			IrRef arg1 = ir_arg1(ins);
			IrRef arg2 = ir_arg2(ins);

			// If the live range of one of these arguments has already been set,
			// then it was used in a later instruction (since we're iterating
			// in reverse order). So only update the argument's live range if
			// it hasn't already been set
			if (live_ranges[arg1] == IR_NONE) {
				live_ranges[arg1] = i;
			}
			if (live_ranges[arg2] == IR_NONE) {
				live_ranges[arg2] = i;
			}
		}
	}
}

// Allocates a register to the results of instructions in the IR.
static void asm_allocate_registers(Trace *trace) {
	// Calculate the live range of each instruction. Use calloc because this 
	// initialises all live ranges to 0, the equivalent of IR_NONE.
	IrRef *live_ranges = calloc(trace->ir_count, sizeof(IrRef));
	asm_calculate_live_ranges(trace, live_ranges);

	// Keep track of when each register is no longer in use
	IrRef reg_end[HY_ARCH_NUM_REGS] = {IR_NONE};

	// Iterate over the instructions in order
	for (IrRef ins_ref = 1; ins_ref < trace->ir_count; ins_ref++) {
		IrIns *ins = &trace->ir[ins_ref];

		// Free any registers that finish being used at this instruction by
		// iterating over all the registers
		for (int reg = 0; reg < HY_ARCH_NUM_REGS; reg++) {
			// If this register's live range ends at this instruction, free it
			// from being used
			if (reg_end[reg] == ins_ref) {
				reg_end[reg] = IR_NONE;
			}
		}

		// Find the next register that's not in use
		bool found_reg = false;
		for (int reg = 0; reg < HY_ARCH_NUM_REGS; reg++) {
			// Check to see if this register isn't already in use
			if (reg_end[reg] == IR_NONE) {
				found_reg = true;

				// Use this register for the result of this instruction
				ir_set_reg(ins, reg);

				// The use of this register ends when this instruction's live
				// range ends
				reg_end[reg] = live_ranges[ins_ref];
				break;
			}
		}

		// If we couldn't find a register to allocate to the result of this
		// instruction, then spill a register to memory
		if (!found_reg) {
			// TODO: register spilling (we assume for now that we have enough
			// registers to satisfy all instruction live ranges, i.e. no more
			// than HY_ARCH_NUM_REGS will be in use at any time)
			assert(false);
		}
	}
}


// ---- Machine Code Generation -----------------------------------------------

// Assemble a load stack instruction.
static void asm_load_stack(MCodeChunk *chunk, Trace *trace, IrIns ins) {
	// movsd xmm<reg>, [<stack> + <offset> * 8]
	uint16_t dest_reg = ir_reg(ins);
	uint32_t stack_slot = ir_arg32(ins);
#ifdef ASM_DEBUG
	if (stack_slot == 0) {
		printf("movsd xmm%d, [rdx]\n", dest_reg);
	} else {
		printf("movsd xmm%d, [rdx + 0x%x]\n", dest_reg, stack_slot * 8);
	}
#endif

	// TODO: instruction encoding
}

// Assemble a load constant instruction.
static void asm_load_const(MCodeChunk *chunk, Trace *trace, IrIns ins) {
	// movsd xmm<reg>, [<consts> + <offset> * 8]
	uint16_t dest_reg = ir_reg(ins);
	uint32_t const_slot = ir_arg32(ins);
	size_t offset = (size_t) &trace->vm->consts[const_slot];
#ifdef ASM_DEBUG
	printf("movsd xmm%d, [0x%zx]\n", dest_reg, offset);
#endif
}

// Assemble an add instruction.
static void asm_add(MCodeChunk *chunk, Trace *trace, IrIns ins) {
	// movsd xmm<dest>, xmm<arg1>
	// addsd xmm<dest>, xmm<arg2>
	uint16_t dest_reg = ir_reg(ins);
	uint16_t arg1_reg = ir_reg(trace->ir[ir_arg1(ins)]);
	uint16_t arg2_reg = ir_reg(trace->ir[ir_arg2(ins)]);
#ifdef ASM_DEBUG
	if (dest_reg != arg1_reg) {
		printf("movsd xmm%d, xmm%d\n", dest_reg, arg1_reg);
	}
	printf("addsd xmm%d, xmm%d\n", dest_reg, arg2_reg);
#endif
}

// Assemble a single IR instruction.
static void asm_ins(MCodeChunk *chunk, Trace *trace, IrIns ins) {
	switch (ir_op(ins)) {
		// Loads
	case IR_LOAD_STACK: asm_load_stack(chunk, trace, ins); break;
	case IR_LOAD_CONST: asm_load_const(chunk, trace, ins); break;

		// Arithmetic
	case IR_ADD: asm_add(chunk, trace, ins); break;

		// Other (do nothing)
	default: break;
	}
}

// Assembles an IR trace into a chunk of machine code.
MCodeChunk jit_assemble(Trace *trace) {
	// Perform register allocation first
	asm_allocate_registers(trace);

	// Create an empty machine code chunk
	MCodeChunk chunk = asm_new();

	// Assemble the IR instruction by instruction
	for (size_t i = 1; i < trace->ir_count; i++) {
		asm_ins(&chunk, trace, trace->ir[i]);
	}

	return chunk;
}
