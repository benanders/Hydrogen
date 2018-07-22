
// test_parser.cpp
// By Ben Anderson
// July 2018

#include <gtest/gtest.h>

extern "C" {
	#include <hydrogen.h>
	#include <vm/parser.h>
	#include <vm/bytecode.h>
	#include <vm/value.h>
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
		fn_dump(&vm->fns[cur_fn]);
	}

	// Increment the current instruction counter and return the next instruction
	// to assert.
	Instruction next() {
		return vm->fns[cur_fn].ins[cur_ins++];
	}
};

// Sets the current function that we're asserting the bytecode for.
#define FN(fn_idx)    \
	mock.cur_ins = 0; \
	mock.cur_fn = (fn_idx);

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
		ASSERT_EQ(ins_arg24(ins), JMP_BIAS + offset - 1);                \
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

TEST(Assignment, AugmentedAssignment) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"a += b\n"
		"b -= a + b * b\n"
		"b *= a + b + a * b\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);

	INS(OP_ADD_LL, 0, 0, 1);

	INS(OP_MUL_LL, 2, 1, 1);
	INS(OP_ADD_LL, 2, 0, 2);
	INS(OP_SUB_LL, 1, 1, 2);

	INS(OP_ADD_LL, 2, 0, 1);
	INS(OP_MUL_LL, 3, 0, 1);
	INS(OP_ADD_LL, 2, 2, 3);
	INS(OP_MUL_LL, 1, 1, 2);

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
	INS2(OP_ADD_LN, 1, 0);
	INS(OP_MUL_LN, 2, 0, 1);
	INS(OP_RET, 0, 0, 0);
}

TEST(Arithmetic, FoldBinary) {
	MockParser mock(
		"let a = 3 + 4\n"
		"let b = 3 + 4 * 5\n"
		"let c = (3 + 10) * 2\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);
	INS2(OP_SET_N, 2, 2);
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

TEST(Logic, Equality) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"let c = a == b\n"
		"let d = a != b\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);

	INS2(OP_NEQ_LL, 0, 1);
	JMP(3);
	INS2(OP_SET_P, 2, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 2, PRIM_FALSE);

	INS2(OP_EQ_LL, 0, 1);
	JMP(3);
	INS2(OP_SET_P, 3, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 3, PRIM_FALSE);

	INS(OP_RET, 0, 0, 0);
}

TEST(Logic, FoldEquality) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"let c = 3 == 4\n"
		"let d = 3 == 3\n"
		"let e = 3 == 8-5\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);
	INS2(OP_SET_P, 2, PRIM_FALSE);
	INS2(OP_SET_P, 3, PRIM_TRUE);
	INS2(OP_SET_P, 4, PRIM_TRUE);
	INS(OP_RET, 0, 0, 0);
}

TEST(Logic, Order) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"let c = a <= b\n"
		"let d = a >= b\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);

	INS2(OP_GT_LL, 0, 1);
	JMP(3);
	INS2(OP_SET_P, 2, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 2, PRIM_FALSE);

	INS2(OP_LT_LL, 0, 1);
	JMP(3);
	INS2(OP_SET_P, 3, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 3, PRIM_FALSE);

	INS(OP_RET, 0, 0, 0);
}

TEST(Logic, FoldOrder) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"let c = 3 > 4\n"
		"let d = 3 <= 3\n"
		"let e = 10 < (5 + 6)"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);
	INS2(OP_SET_P, 2, PRIM_FALSE);
	INS2(OP_SET_P, 3, PRIM_TRUE);
	INS2(OP_SET_P, 4, PRIM_TRUE);
	INS(OP_RET, 0, 0, 0);
}

TEST(Logic, And) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"let c = a == 3 && b == 4\n"
		"let d = a == 3 && b == 4 && c == 5\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);

	INS2(OP_NEQ_LN, 0, 0);
	JMP(5);
	INS2(OP_NEQ_LN, 1, 1);
	JMP(3);
	INS2(OP_SET_P, 2, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 2, PRIM_FALSE);

	INS2(OP_NEQ_LN, 0, 0);
	JMP(7);
	INS2(OP_NEQ_LN, 1, 1);
	JMP(5);
	INS2(OP_NEQ_LN, 2, 2);
	JMP(3);
	INS2(OP_SET_P, 3, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 3, PRIM_FALSE);

	INS(OP_RET, 0, 0, 0);
}

TEST(Logic, Or) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"let c = a == 3 || b == 4\n"
		"let d = a == 3 || b == 4 || c == 5\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);

	INS2(OP_EQ_LN, 0, 0);
	JMP(3);
	INS2(OP_NEQ_LN, 1, 1);
	JMP(3);
	INS2(OP_SET_P, 2, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 2, PRIM_FALSE);

	INS2(OP_EQ_LN, 0, 0);
	JMP(5);
	INS2(OP_EQ_LN, 1, 1);
	JMP(3);
	INS2(OP_NEQ_LN, 2, 2);
	JMP(3);
	INS2(OP_SET_P, 3, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 3, PRIM_FALSE);

	INS(OP_RET, 0, 0, 0);
}

TEST(Logic, AndOr) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"let c = 5\n"
		"let d = a == 3 && b == 4 || c == 5\n"
		"let e = (a == 3 || b == 4) && c == 5\n"
		"let f = a == 3 && (b == 4 || c == 5)\n"
		"let g = a == 3 && b == 4 || c == 5 && d == 6\n"
		"let h = (a == 3 || b == 4) && (c == 5 || d == 6)\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);
	INS2(OP_SET_N, 2, 2);

	INS2(OP_NEQ_LN, 0, 0);
	JMP(3);
	INS2(OP_EQ_LN, 1, 1);
	JMP(3);
	INS2(OP_NEQ_LN, 2, 2);
	JMP(3);
	INS2(OP_SET_P, 3, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 3, PRIM_FALSE);

	INS2(OP_EQ_LN, 0, 0);
	JMP(3);
	INS2(OP_NEQ_LN, 1, 1);
	JMP(5);
	INS2(OP_NEQ_LN, 2, 2);
	JMP(3);
	INS2(OP_SET_P, 4, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 4, PRIM_FALSE);

	INS2(OP_NEQ_LN, 0, 0);
	JMP(7);
	INS2(OP_EQ_LN, 1, 1);
	JMP(3);
	INS2(OP_NEQ_LN, 2, 2);
	JMP(3);
	INS2(OP_SET_P, 5, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 5, PRIM_FALSE);

	INS2(OP_NEQ_LN, 0, 0);
	JMP(3);
	INS2(OP_EQ_LN, 1, 1);
	JMP(5);
	INS2(OP_NEQ_LN, 2, 2);
	JMP(5);
	INS2(OP_NEQ_LN, 3, 3);
	JMP(3);
	INS2(OP_SET_P, 6, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 6, PRIM_FALSE);

	INS2(OP_EQ_LN, 0, 0);
	JMP(3);
	INS2(OP_NEQ_LN, 1, 1);
	JMP(7);
	INS2(OP_EQ_LN, 2, 2);
	JMP(3);
	INS2(OP_NEQ_LN, 3, 3);
	JMP(3);
	INS2(OP_SET_P, 7, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 7, PRIM_FALSE);

	INS(OP_RET, 0, 0, 0);
}

TEST(Logic, Not) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"let c = !a\n"
		"let d = !(a < 3)\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);

	INS2(OP_EQ_LP, 0, PRIM_TRUE);
	JMP(3);
	INS2(OP_SET_P, 2, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 2, PRIM_FALSE);

	INS2(OP_LT_LN, 0, 0);
	JMP(3);
	INS2(OP_SET_P, 3, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 3, PRIM_FALSE);

	INS(OP_RET, 0, 0, 0);
}

TEST(Logic, NotAndOr) {
	MockParser mock(
		"let a = 3\n"
		"let b = 4\n"
		"let c = 5\n"
		"let d = a == 3 && !(b == 4 || c == 5)\n"
		"let e = !(a == 3 || b == 4) && c == 5\n"
		"let f = a == 3 && b == 4 || !(c == 5 && d == 6)\n"
		"let g = a == 3 || !(b == 4 && c == 5) && d == 6\n"
		"let h = !(a == 3 && b == 4 || c == 5)\n"
		"let i = !(a == 3 || b == 4 && c == 5)\n"
		"let j = !(a == 3 && b == 4 || c == 5) && d == 6\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);
	INS2(OP_SET_N, 2, 2);

	INS2(OP_NEQ_LN, 0, 0);
	JMP(7);
	INS2(OP_EQ_LN, 1, 1);
	JMP(5);
	INS2(OP_EQ_LN, 2, 2);
	JMP(3);
	INS2(OP_SET_P, 3, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 3, PRIM_FALSE);

	INS2(OP_EQ_LN, 0, 0);
	JMP(7);
	INS2(OP_EQ_LN, 1, 1);
	JMP(5);
	INS2(OP_NEQ_LN, 2, 2);
	JMP(3);
	INS2(OP_SET_P, 4, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 4, PRIM_FALSE);

	INS2(OP_NEQ_LN, 0, 0);
	JMP(3);
	INS2(OP_EQ_LN, 1, 1);
	JMP(5);
	INS2(OP_NEQ_LN, 2, 2);
	JMP(3);
	INS2(OP_EQ_LN, 3, 3);
	JMP(3);
	INS2(OP_SET_P, 5, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 5, PRIM_FALSE);

	INS2(OP_EQ_LN, 0, 0);
	JMP(7);
	INS2(OP_NEQ_LN, 1, 1);
	JMP(3);
	INS2(OP_EQ_LN, 2, 2);
	JMP(5);
	INS2(OP_NEQ_LN, 3, 3);
	JMP(3);
	INS2(OP_SET_P, 6, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 6, PRIM_FALSE);

	INS2(OP_NEQ_LN, 0, 0);
	JMP(3);
	INS2(OP_EQ_LN, 1, 1);
	JMP(5);
	INS2(OP_EQ_LN, 2, 2);
	JMP(3);
	INS2(OP_SET_P, 7, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 7, PRIM_FALSE);

	INS2(OP_EQ_LN, 0, 0);
	JMP(7);
	INS2(OP_NEQ_LN, 1, 1);
	JMP(3);
	INS2(OP_EQ_LN, 2, 2);
	JMP(3);
	INS2(OP_SET_P, 8, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 8, PRIM_FALSE);

	INS2(OP_NEQ_LN, 0, 0);
	JMP(3);
	INS2(OP_EQ_LN, 1, 1);
	JMP(7);
	INS2(OP_EQ_LN, 2, 2);
	JMP(5);
	INS2(OP_NEQ_LN, 3, 3);
	JMP(3);
	INS2(OP_SET_P, 9, PRIM_TRUE);
	JMP(2);
	INS2(OP_SET_P, 9, PRIM_FALSE);

	INS(OP_RET, 0, 0, 0);
}

TEST(If, If) {
	MockParser mock(
		"let a = 3\n"
		"if a == 3 {\n"
		"  let b = 4\n"
		"}\n"
		"let c = 5\n"
	);

	INS2(OP_SET_N, 0, 0);

	INS2(OP_NEQ_LN, 0, 0); // If condition
	JMP(2); // Jump to after
	INS2(OP_SET_N, 1, 1); // If body

	INS2(OP_SET_N, 1, 2); // After

	INS(OP_RET, 0, 0, 0);
}

TEST(If, IfElse) {
	MockParser mock(
		"let a = 3\n"
		"if a == 3 {\n"
		"  let b = 4\n"
		"} else {\n"
		"  let b = 5\n"
		"}\n"
		"let c = 6\n"
	);

	INS2(OP_SET_N, 0, 0);

	INS2(OP_NEQ_LN, 0, 0); // If condition
	JMP(3); // Jump to else
	INS2(OP_SET_N, 1, 1); // If body
	JMP(2); // Jump to after
	INS2(OP_SET_N, 1, 2); // Else body

	INS2(OP_SET_N, 1, 3); // After

	INS(OP_RET, 0, 0, 0);
}

TEST(If, IfElseif) {
	MockParser mock(
		"let a = 3\n"
		"if a == 3 {\n"
		"  let b = 4\n"
		"} elseif a == 4 {\n"
		"  let b = 5\n"
		"}\n"
		"let c = 6\n"
	);

	INS2(OP_SET_N, 0, 0);

	INS2(OP_NEQ_LN, 0, 0); // If condition
	JMP(3); // Jump to elseif condition
	INS2(OP_SET_N, 1, 1); // If body
	JMP(4); // Jump to after
	INS2(OP_NEQ_LN, 0, 1); // Elseif condition
	JMP(2);
	INS2(OP_SET_N, 1, 2); // Elseif body

	INS2(OP_SET_N, 1, 3); // After

	INS(OP_RET, 0, 0, 0);
}

TEST(If, IfElseifElseif) {
	MockParser mock(
		"let a = 3\n"
		"if a == 3 {\n"
		"  let b = 4\n"
		"} elseif a == 4 {\n"
		"  let b = 5\n"
		"} elseif a == 5 {\n"
		"  let b = 6\n"
		"}\n"
		"let c = 7\n"
	);

	INS2(OP_SET_N, 0, 0);

	INS2(OP_NEQ_LN, 0, 0); // If condition
	JMP(3); // Jump to elseif 1 condition
	INS2(OP_SET_N, 1, 1); // If body
	JMP(8); // Jump to after
	INS2(OP_NEQ_LN, 0, 1); // Elseif 1 condition
	JMP(3); // Jump to elseif 2 condition
	INS2(OP_SET_N, 1, 2); // Elseif 1 body
	JMP(4); // Jump to after
	INS2(OP_NEQ_LN, 0, 2); // Elseif 2 condition
	JMP(2);
	INS2(OP_SET_N, 1, 3); // Elseif 2 body

	INS2(OP_SET_N, 1, 4); // After

	INS(OP_RET, 0, 0, 0);
}

TEST(If, IfElseifElse) {
	MockParser mock(
		"let a = 3\n"
		"if a == 3 {\n"
		"  let b = 4\n"
		"} elseif a == 4 {\n"
		"  let b = 5\n"
		"} else {\n"
		"  let b = 6\n"
		"}\n"
		"let c = 7\n"
	);

	INS2(OP_SET_N, 0, 0);

	INS2(OP_NEQ_LN, 0, 0); // If condition
	JMP(3); // Jump to elseif condition
	INS2(OP_SET_N, 1, 1); // If body
	JMP(6); // Jump to after
	INS2(OP_NEQ_LN, 0, 1); // Elseif condition
	JMP(3);
	INS2(OP_SET_N, 1, 2); // Elseif body
	JMP(2); // Jump to after
	INS2(OP_SET_N, 1, 3); // Else body

	INS2(OP_SET_N, 1, 4); // After

	INS(OP_RET, 0, 0, 0);
}

TEST(If, IfElseifElseifElse) {
	MockParser mock(
		"let a = 3\n"
		"if a == 3 {\n"
		"  let b = 4\n"
		"} elseif a == 4 {\n"
		"  let b = 5\n"
		"} elseif a == 5 {\n"
		"  let b = 6\n"
		"} else {\n"
		"  let b = 7\n"
		"}\n"
		"let c = 8\n"
	);

	INS2(OP_SET_N, 0, 0);

	INS2(OP_NEQ_LN, 0, 0); // If condition
	JMP(3); // Jump to elseif 1 condition
	INS2(OP_SET_N, 1, 1); // If body
	JMP(10); // Jump to after
	INS2(OP_NEQ_LN, 0, 1); // Elseif 1 condition
	JMP(3); // Jump to elseif 2 condition
	INS2(OP_SET_N, 1, 2); // Elseif 1 body
	JMP(6); // Jump to after
	INS2(OP_NEQ_LN, 0, 2); // Elseif 2 condition
	JMP(3);
	INS2(OP_SET_N, 1, 3); // Elseif 2 body
	JMP(2); // Jump to after
	INS2(OP_SET_N, 1, 4);

	INS2(OP_SET_N, 1, 5); // After

	INS(OP_RET, 0, 0, 0);
}

TEST(Loop, Infinite) {
	MockParser mock(
		"let a = 3\n"
		"loop {\n"
		"  let b = 4\n"
		"}\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_N, 1, 1);
	JMP(-1);
	INS(OP_RET, 0, 0, 0);
}

TEST(Loop, While) {
	MockParser mock(
		"let a = 0\n"
		"while a < 100 {\n"
		"  a += 1\n"
		"}\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_GE_LN, 0, 1); // Condition
	JMP(3); // Jump to after
	INS(OP_ADD_LN, 0, 0, 2); // Body
	JMP(-3); // Jump to condition

	INS(OP_RET, 0, 0, 0); // After
}

TEST(Fn, FnDef) {
	MockParser mock(
		"let a = 3\n"
		"fn hello() {\n"
		"  let b = 4\n"
		"}\n"
		"let c = 5\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_F, 1, 1);
	INS2(OP_SET_N, 2, 2);
	INS(OP_RET, 0, 0, 0);

	FN(1);
	INS2(OP_SET_N, 0, 1);
	INS(OP_RET, 0, 0, 0);
}

TEST(Fn, OneArg) {
	MockParser mock(
		"let a = 3\n"
		"fn hello(a) {\n"
		"  let b = a\n"
		"}\n"
		"let c = 5\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_F, 1, 1);
	INS2(OP_SET_N, 2, 1);
	INS(OP_RET, 0, 0, 0);

	FN(1);
	INS2(OP_MOV, 1, 0);
	INS(OP_RET, 0, 0, 0);
}

TEST(Fn, MultipleArgs) {
	MockParser mock(
		"let a = 3\n"
		"fn hello(a, b, c, d) {\n"
		"  let e = a\n"
		"  let f = c + d\n"
		"}\n"
		"let c = 5\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_F, 1, 1);
	INS2(OP_SET_N, 2, 1);
	INS(OP_RET, 0, 0, 0);

	FN(1);
	INS2(OP_MOV, 4, 0);
	INS(OP_ADD_LL, 5, 2, 3);
	INS(OP_RET, 0, 0, 0);
}

TEST(Fn, MultipleDefs) {
	MockParser mock(
		"let a = 3\n"
		"fn hello() {\n"
		"  let b = 4\n"
		"}\n"
		"fn hello2() {\n"
		"  let b = 5\n"
		"}\n"
		"fn hello3() {\n"
		"  let b = 6\n"
		"}\n"
		"let c = 7\n"
	);

	INS2(OP_SET_N, 0, 0);
	INS2(OP_SET_F, 1, 1);
	INS2(OP_SET_F, 2, 2);
	INS2(OP_SET_F, 3, 3);
	INS2(OP_SET_N, 4, 4);
	INS(OP_RET, 0, 0, 0);

	FN(1);
	INS2(OP_SET_N, 0, 1);
	INS(OP_RET, 0, 0, 0);

	FN(2);
	INS2(OP_SET_N, 0, 2);
	INS(OP_RET, 0, 0, 0);

	FN(3);
	INS2(OP_SET_N, 0, 3);
	INS(OP_RET, 0, 0, 0);
}
