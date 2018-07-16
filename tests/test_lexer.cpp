
// test_lexer.cpp
// By Ben Anderson
// July 2018

#include <gtest/gtest.h>

extern "C" {
	#include <hydrogen.h>
	#include <vm/lexer.h>
}

// Stores all the information needed to test the lexer.
typedef struct {
	HyVM *vm;
	Lexer lxr;
} MockLexer;

// Creates a new mock lexer.
MockLexer mock_new(const char *code) {
	MockLexer mock;
	mock.vm = hy_new_vm();
	mock.lxr = lex_new(mock.vm, NULL, (char *) code);
	return mock;
}

// Free resources allocated by the mock lexer.
void mock_free(MockLexer *mock) {
	hy_free_vm(mock->vm);
}

TEST(Lexer, SingleCharSymbols) {
	// Cast explicitly to a non-const char pointer to silence a warning from the
	// C++ compiler
	MockLexer mock = mock_new("+ - ( ) [ ]");
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, '+');
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, '-');
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, '(');
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, ')');
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, '[');
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, ']');
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_EOF);
	mock_free(&mock);
}

TEST(Lexer, MultiCharSymbols) {
	MockLexer mock = mock_new("+= -= >= <= ..");
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_ADD_ASSIGN);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_SUB_ASSIGN);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_GE);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_LE);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_CONCAT);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_EOF);
	mock_free(&mock);
}

TEST(Lexer, Empty) {
	MockLexer mock = mock_new("");
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_EOF);
	mock_free(&mock);
	mock = mock_new(" \n\r\r   \t\n");
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_EOF);
	mock_free(&mock);
}

TEST(Lexer, Whitespace) {
	MockLexer mock = mock_new(" +\n\r -(\t\t\n\r)\r\n [ \n\r]\n");
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, '+');
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, '-');
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, '(');
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, ')');
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, '[');
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, ']');
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_EOF);
	mock_free(&mock);
}

TEST(Lexer, LineNumbers) {
	MockLexer mock = mock_new(" +\n\r -(\t\t\n\r)\r\n [ \n\r]\n");
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, '+');
	ASSERT_EQ(mock.lxr.tk.line, 1);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, '-');
	ASSERT_EQ(mock.lxr.tk.line, 3);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, '(');
	ASSERT_EQ(mock.lxr.tk.line, 3);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, ')');
	ASSERT_EQ(mock.lxr.tk.line, 5);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, '[');
	ASSERT_EQ(mock.lxr.tk.line, 6);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, ']');
	ASSERT_EQ(mock.lxr.tk.line, 8);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_EOF);
	ASSERT_EQ(mock.lxr.tk.line, 9);
	mock_free(&mock);
}

TEST(Lexer, Identifiers) {
	MockLexer mock = mock_new("hello _3hello h_e_ll_o h3ll0 _014 _h35_o");
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_IDENT);
	ASSERT_EQ(mock.lxr.tk.ident_hash, hash_string("hello", 5));
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_IDENT);
	ASSERT_EQ(mock.lxr.tk.ident_hash, hash_string("_3hello", 7));
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_IDENT);
	ASSERT_EQ(mock.lxr.tk.ident_hash, hash_string("h_e_ll_o", 8));
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_IDENT);
	ASSERT_EQ(mock.lxr.tk.ident_hash, hash_string("h3ll0", 5));
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_IDENT);
	ASSERT_EQ(mock.lxr.tk.ident_hash, hash_string("_014", 4));
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_IDENT);
	ASSERT_EQ(mock.lxr.tk.ident_hash, hash_string("_h35_o", 6));
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_EOF);
	mock_free(&mock);
}

TEST(Lexer, Keywords) {
	MockLexer mock = mock_new("if elseif else while for loop");
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_IF);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_ELSEIF);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_ELSE);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_WHILE);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_FOR);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_LOOP);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_EOF);
	mock_free(&mock);
}

TEST(Lexer, Integers) {
	MockLexer mock = mock_new("3 0 1503 19993");
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 3.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 0.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 1503.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 19993.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_EOF);
	mock_free(&mock);
}

TEST(Lexer, PrefixedIntegers) {
	MockLexer mock = mock_new("0xf 0XF1 0b0110 0o777");
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 15.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 241.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 6.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 511.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_EOF);
	mock_free(&mock);
}

TEST(Lexer, Floats) {
	MockLexer mock = mock_new("3.0 4.0000 3.1415926535 3. 42.09 3e4 3e+4 "
		"3e-4 3.14e2 42.51E2");
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 3.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 4.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 3.1415926535);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 3.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 42.09);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 30000.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 30000.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 0.0003);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 314.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_NUM);
	ASSERT_EQ(mock.lxr.tk.num, 4251.0);
	lex_next(&mock.lxr); ASSERT_EQ(mock.lxr.tk.type, TK_EOF);
	mock_free(&mock);
}
