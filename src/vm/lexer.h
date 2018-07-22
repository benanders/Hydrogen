
// lexer.h
// By Ben Anderson
// July 2018

#ifndef LEXER_H
#define LEXER_H

#include <stdint.h>

#include "vm.h"
#include "err.h"
#include "util.h"

// Tokens are represented by a single 32 bit integer. If the token is a single
// character long, then its corresponding ASCII code is used. If the token is
// more than one character long, then one of the integer values in the below
// enum is used.
typedef int Tk;

// Multi-character tokens. Single character tokens are represented by their
// corresponding ASCII character.
enum multi_tks {
	TK_CONCAT = 256,
	TK_ADD_ASSIGN, TK_SUB_ASSIGN, TK_MUL_ASSIGN, TK_DIV_ASSIGN, TK_MOD_ASSIGN,
	TK_EQ, TK_NEQ, TK_LE, TK_GE,
	TK_AND, TK_OR,
	TK_LET, TK_IF, TK_ELSE, TK_ELSEIF, TK_LOOP, TK_WHILE, TK_FOR, TK_FN,
	TK_IDENT, TK_NUM, TK_FALSE, TK_TRUE, TK_NIL,
	TK_EOF,
};

// Additional information associated with a token.
typedef struct {
	Tk type;

	// Byte position of the first character of the token in the source code.
	int start;

	// Length (in bytes) of the token.
	int length;

	// Line number for the FIRST character of the token.
	int line;

	// Union containing extracted information about the token
	union {
		double num;
		uint64_t ident_hash;
	};
} TkInfo;

// Stores state information required by the lexer.
typedef struct {
	HyVM *vm;
	char *path;
	char *code;

	// Byte position of the cursor in the source code.
	int cursor;

	// Line number on which the cursor is currently sitting.
	int line;

	// Information about the most recently lexed token.
	TkInfo tk;
} Lexer;

// Creates a new lexer over the given source code. If the code is from a file,
// the path to the file is also given (this can be NULL).
Lexer lex_new(HyVM *vm, char *path, char *code);

// Lexes the next token, storing the result in `lxr->tk`.
void lex_next(Lexer *lxr);

// Triggers an error if the current token isn't what's expected.
void lex_expect(Lexer *lxr, Tk expected);

// Saved lexer state information.
typedef struct {
	int cursor, line;
	TkInfo tk;
} SavedLexer;

// Saves the lexer's current state for later restoration.
SavedLexer lex_save(Lexer *lxr);

// Restores the lexer's state to a previously saved state.
void lex_restore(Lexer *lxr, SavedLexer saved);

#endif
