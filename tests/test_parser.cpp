
// test_parser.cpp
// By Ben Anderson
// July 2018

#include <gtest/gtest.h>

extern "C" {
	#include <hydrogen.h>
	#include <vm/parser.h>
	#include <vm/bytecode.h>
	#include <vm/util.h>
}

// Parses a piece of source code and iterates over the emitted bytecode,
// allowing us to easily and sequentially assert instructions.
class MockParser {
public:
	HyVM *vm;
	size_t cur_fn, cur_ins;

	// Create a new mock parser object.
	MockParser(const char *code) {
		vm = hy_new_vm();
		cur_fn = 0;
		cur_ins = 0;

		// Add a package (its main function is created automatically)
		int pkg = vm_new_pkg(vm, hash_string("test", 4));

		// Parse the source code
		HyErr *err = parse(vm, pkg, NULL, (char *) code);
		if (err != NULL) {
			ADD_FAILURE() << hy_err_desc(err) << " at line " << hy_err_line(err);
		}
	}

	// Free resources allocated by a mock parser object.
	~MockParser() {
		hy_free_vm(vm);
	}

	// Dump all parsed functions to the standard output.
	void dump() {
		// TODO
	}

	// Increment the current instruction counter and return the next instruction
	// to assert.
	Instruction next() {
		return vm->fns[cur_fn].ins[cur_ins++];
	}
};

// Asserts the current bytecode instruction's opcode and arguments.
#define INS(opcode, a, b, c) {                                           \
		ASSERT_TRUE(mock.cur_ins < mock.vm->fns[mock.cur_fn].ins_count); \
		Instruction ins = mock.next();                                   \
		ASSERT_EQ(ins_op(ins), opcode);                                  \
		ASSERT_EQ(ins_arg1(ins), a);                                     \
		ASSERT_EQ(ins_arg2(ins), b);                                     \
		ASSERT_EQ(ins_arg3(ins), c);                                     \
	}

// Asserts the current instruction as an extended, 2 argument instruction.
#define INS2(opcode, a, d) {                                             \
		ASSERT_TRUE(mock.cur_ins < mock.vm->fns[mock.cur_fn].ins_count); \
		Instruction ins = mock.next();                                   \
		ASSERT_EQ(ins_op(ins), opcode);                                  \
		ASSERT_EQ(ins_arg1(ins), a);                                     \
		ASSERT_EQ(ins_arg16(ins), d);                                    \
	}

// Asserts the current instruction is a JMP, with the given offset.
#define JMP(offset) {                                                    \
		ASSERT_TRUE(mock.cur_ins < mock.vm->fns[mock.cur_fn].ins_count); \
		Instruction ins = mock.next();                                   \
		ASSERT_EQ(ins_op(ins), OP_JMP);                                  \
		ASSERT_EQ(ins_arg24(ins), offset);                               \
	}

TEST(Assignment, NumberAssignment) {
	MockParser mock("let a = 3.1415926535");
	INS2(OP_SET_N, 0, 0);
	INS(OP_RET, 0, 0, 0);
}

TEST(Assignment, MultipleAssignments) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"let c = 10\n"
		"let d = 3\n" // Re-use of constants
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);
	INS2(OP_SET_N, 2, 2);
	INS2(OP_SET_N, 3, 0);
	INS(OP_RET, 0, 0, 0);
}

TEST(Assignment, Reassignment) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"a = 5\n"
		"b = 6\n"
		"b = a\n"
		"a = b + 7\n" // Relocatable expressions
		"a = -b\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);

	INS2(OP_SET_N, 0, 2);
	INS2(OP_SET_N, 1, 3);
	INS2(OP_MOV, 1, 0);

	INS(OP_ADD_LN, 0, 1, 4);
	INS2(OP_NEG, 0, 1);

	INS(OP_RET, 0, 0, 0);
}

TEST(Arithmetic, UnaryOperations) {
	MockParser mock(
		"let a = 3\n"
		"let b = -a\n"
		"let c = --a\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS(OP_NEG, 1, 0, 0);
	INS(OP_NEG, 2, 0, 0);
	INS(OP_NEG, 2, 2, 0);
	INS(OP_RET, 0, 0, 0);
}

TEST(Arithmetic, FoldUnary) {
	MockParser mock(
		"let a = -3\n"
		"let b = --4\n"
		"let c = ---5\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);
	INS2(OP_SET_N, 2, 2);
	INS(OP_RET, 0, 0, 0);
}

TEST(Arithmetic, BinaryOperations) {
	MockParser mock(
		"let a = 3\n"
		"let b = a + 3\n"
		"let c = a * 10\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS(OP_ADD_LN, 1, 0, 0);
	INS(OP_MUL_LN, 2, 0, 1);
	INS(OP_RET, 0, 0, 0);
}

TEST(Arithmetic, Associativity) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"let c = 5\n"
		"let d = a + b + c\n"
		"let e = a * b * c * d\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);
	INS2(OP_SET_N, 2, 2);

	INS(OP_ADD_LL, 3, 0, 1);
	INS(OP_ADD_LL, 3, 3, 2);

	INS(OP_MUL_LL, 4, 0, 1);
	INS(OP_MUL_LL, 4, 4, 2);
	INS(OP_MUL_LL, 4, 4, 3);

	INS(OP_RET, 0, 0, 0);
}

TEST(Arithmetic, Precedence) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"let c = 5\n"
		"let d = a * b + c\n"
		"let e = a + b * c\n"
		"let f = a * b + c * d\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);
	INS2(OP_SET_N, 2, 2);

	INS(OP_MUL_LL, 3, 0, 1);
	INS(OP_ADD_LL, 3, 3, 2);

	INS(OP_MUL_LL, 4, 1, 2);
	INS(OP_ADD_LL, 4, 0, 4);

	INS(OP_MUL_LL, 5, 0, 1);
	INS(OP_MUL_LL, 6, 2, 3);
	INS(OP_ADD_LL, 5, 5, 6);

	INS(OP_RET, 0, 0, 0);
}

TEST(Arithmetic, Subexpression) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"let c = 5\n"
		"let d = (a + b) * c\n"
		"let e = (a + b) * (c + d)\n"
		"let f = a * (a + b * c)\n"
		"let g = c * (a + b)\n"
		"let h = a * (b + c * (d + e))"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);
	INS2(OP_SET_N, 2, 2);

	INS(OP_ADD_LL, 3, 0, 1);
	INS(OP_MUL_LL, 3, 3, 2);

	INS(OP_ADD_LL, 4, 0, 1);
	INS(OP_ADD_LL, 5, 2, 3);
	INS(OP_MUL_LL, 4, 4, 5);

	INS(OP_MUL_LL, 5, 1, 2);
	INS(OP_ADD_LL, 5, 0, 5);
	INS(OP_MUL_LL, 5, 0, 5);

	INS(OP_ADD_LL, 6, 0, 1);
	INS(OP_MUL_LL, 6, 2, 6);

	INS(OP_ADD_LL, 7, 3, 4);
	INS(OP_MUL_LL, 7, 2, 7);
	INS(OP_ADD_LL, 7, 1, 7);
	INS(OP_MUL_LL, 7, 0, 7);

	INS(OP_RET, 0, 0, 0);
}
