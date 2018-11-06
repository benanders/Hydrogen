
// test_compiler.cpp
// By Ben Anderson
// October 2018

#include <gtest/gtest.h>

extern "C" {
	#include <jit/compiler.h>
}

// Compiles a bytecode trace into IR and iterates over the output, allowing us
// to easily and sequentially assert instructions.
class MockCompiler {
public:
	// The only thing we use the VM for is constants and the runtime stack.
	VM vm;
	Trace *trace;
	size_t cur_ins;

	// Compiles a bytecode trace into IR.
	MockCompiler(BcIns *bytecode, size_t trace_length) {
		vm = vm_new();
		cur_ins = 1; // Start from 1 as per ir.h
		trace = jit_trace_new(&vm);
		compile(bytecode, trace_length);
	}

	// Free all resources allocated by the mock compiler.
	~MockCompiler() {
		jit_trace_free(trace);
		vm_free(&vm);
	}

	// Compile a bytecode trace into IR.
	void compile(BcIns *bytecode, size_t bytecode_length) {
		for (size_t i = 0; i < bytecode_length; i++) {
			compile_ins(bytecode[i]);
		}
	}

	// Compile a single bytecode instruction from a trace.
	void compile_ins(BcIns ins) {
		switch (bc_op(ins)) {
		// Stores
		case BC_MOV: jit_rec_MOV(trace, ins); break;
		case BC_SET_N: jit_rec_SET_N(trace, ins); break;
		case BC_SET_P: jit_rec_SET_P(trace, ins); break;
		case BC_SET_F: jit_rec_SET_F(trace, ins); break;

		// Arithmetic
		case BC_ADD_LL: jit_rec_ADD_LL(trace, ins); break;
		case BC_ADD_LN: jit_rec_ADD_LN(trace, ins); break;
		case BC_SUB_LL: jit_rec_SUB_LL(trace, ins); break;
		case BC_SUB_LN: jit_rec_SUB_LN(trace, ins); break;
		case BC_SUB_NL: jit_rec_SUB_NL(trace, ins); break;
		case BC_MUL_LL: jit_rec_MUL_LL(trace, ins); break;
		case BC_MUL_LN: jit_rec_MUL_LN(trace, ins); break;
		case BC_DIV_LL: jit_rec_DIV_LL(trace, ins); break;
		case BC_DIV_LN: jit_rec_DIV_LN(trace, ins); break;
		case BC_DIV_NL: jit_rec_DIV_NL(trace, ins); break;
		case BC_NEG: jit_rec_NEG(trace, ins); break;

		// Relational operators
		case BC_EQ_LL: jit_rec_EQ_LL(trace, ins); break;
		case BC_EQ_LN: jit_rec_EQ_LN(trace, ins); break;
		case BC_EQ_LP: jit_rec_EQ_LP(trace, ins); break;
		case BC_NEQ_LL: jit_rec_NEQ_LL(trace, ins); break;
		case BC_NEQ_LN: jit_rec_NEQ_LN(trace, ins); break;
		case BC_NEQ_LP: jit_rec_NEQ_LP(trace, ins); break;
		case BC_LT_LL: jit_rec_LT_LL(trace, ins); break;
		case BC_LT_LN: jit_rec_LT_LN(trace, ins); break;
		case BC_LE_LL: jit_rec_LE_LL(trace, ins); break;
		case BC_LE_LN: jit_rec_LE_LN(trace, ins); break;
		case BC_GT_LL: jit_rec_GT_LL(trace, ins); break;
		case BC_GT_LN: jit_rec_GT_LN(trace, ins); break;
		case BC_GE_LL: jit_rec_GE_LL(trace, ins); break;
		case BC_GE_LN: jit_rec_GE_LN(trace, ins); break;

		// Control flow
		case BC_CALL: jit_rec_CALL(trace, ins); break;
		case BC_RET: jit_rec_RET(trace, ins); break;

		// Instruction not allowed in a trace
		default: assert(false); break;
		}
	}

	// Dump the compiled IR to the standard output.
	void dump() {
		jit_trace_dump(trace);
	}

	// Increment the current instruction counter and return the next instruction
	// to assert.
	IrIns next() {
		return trace->ir[cur_ins++];
	}
};

// Creates a new bytecode instruction.
#define BC3(op, a, b, c) bc_new3(op, a, b, c)
#define BC2(op, a, b) bc_new2(op, a, b)
#define BC1(op, a) bc_new1(op, a)

// Assert an IR instruction's operand and arguments.
#define INS(opcode, a, b) {                               \
		ASSERT_TRUE(mock.cur_ins < mock.trace->ir_count); \
		IrIns ins = mock.next();                          \
		ASSERT_EQ(ir_op(ins), opcode);                    \
		ASSERT_EQ(ir_arg1(ins), a);                       \
		ASSERT_EQ(ir_arg2(ins), b);                       \
	}

// Creates a new mock compiler from a list of bytecode instruction. Stores the
// mock compiler in a variable called `mock`.
#define COUNT_INS(...) (sizeof((uint32_t[]) {__VA_ARGS__}) / sizeof(uint32_t))
#define MOCK(...)        \
	BcIns arr[] = {__VA_ARGS__}; \
	MockCompiler mock(arr, COUNT_INS(__VA_ARGS__));

TEST(Arithmetic, AddLocals) {
	// let a = 0 while true { a = a + b }
	MOCK(
		BC3(BC_ADD_LL, 0, 0, 1),
	);

	INS(IR_LOAD_STACK, 0, 0);
	INS(IR_LOAD_STACK, 1, 0);
	INS(IR_ADD, 1, 2);
}

TEST(Arithmetic, AddNumbers) {
	// let a = 0 while true { a = a + 1 }
	MOCK(
		BC3(BC_ADD_LN, 0, 0, 0),
	);

	INS(IR_LOAD_STACK, 0, 0);
	INS(IR_LOAD_CONST, 0, 0);
	INS(IR_ADD, 1, 2);
}

TEST(Arithmetic, NumReuse) {
	// let a = 0 while true { a = a + 1 a = a + 1}
	MOCK(
		BC3(BC_ADD_LN, 0, 0, 0),
		BC3(BC_ADD_LN, 0, 0, 0),
	);

	INS(IR_LOAD_STACK, 0, 0);
	INS(IR_LOAD_CONST, 0, 0);
	INS(IR_ADD, 1, 2);
	INS(IR_ADD, 3, 2);
}

TEST(Arithemtic, LocalReuse) {
	// let a = 0 while true { a = a + 1 a = a + 2}
	MOCK(
		BC3(BC_ADD_LN, 0, 0, 0),
		BC3(BC_ADD_LN, 0, 0, 1),
	);

	INS(IR_LOAD_STACK, 0, 0);
	INS(IR_LOAD_CONST, 0, 0);
	INS(IR_ADD, 1, 2);
	INS(IR_LOAD_CONST, 1, 0);
	INS(IR_ADD, 3, 4);
}

TEST(Arithmetic, MultipleLocals) {
	// let a = 0 let b = 0 while true { a = a + 1 b = b + 2 }
	MOCK(
		BC3(BC_ADD_LN, 0, 0, 0),
		BC3(BC_ADD_LN, 1, 1, 1),
	);

	INS(IR_LOAD_STACK, 0, 0);
	INS(IR_LOAD_CONST, 0, 0);
	INS(IR_ADD, 1, 2);
	INS(IR_LOAD_STACK, 1, 0);
	INS(IR_LOAD_CONST, 1, 0);
	INS(IR_ADD, 4, 5);
}
