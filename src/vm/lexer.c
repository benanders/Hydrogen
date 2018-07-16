
// lexer.c
// By Ben Anderson
// July 2018

#include "lexer.h"
#include "vm.h"

#include <string.h>
#include <errno.h>

// Returns true if the given character is whitespace.
static inline int is_whitespace(char ch) {
	return ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ';
}

// Returns true if the given character can start a number.
static inline int is_decimal_digit(char ch) {
	return ch >= '0' && ch <= '9';
}

// Returns true if the given character can start an identifier.
static inline int is_ident_start(char ch) {
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

// Returns true if the given character can continue an identifier.
static inline int is_ident_continue(char ch) {
	return is_ident_start(ch) || is_decimal_digit(ch);
}

// Creates a new lexer over the given source code.
Lexer lex_new(HyVM *vm, char *path, char *code) {
	Lexer lxr;
	lxr.vm = vm;
	lxr.path = path;
	lxr.code = code;
	lxr.cursor = 0;
	lxr.line = 1;
	lxr.tk.type = TK_EOF;
	lxr.tk.start = 0;
	lxr.tk.length = 0;
	lxr.tk.line = 1;
	lxr.tk.ident_hash = 0;
	return lxr;
}

// Consume all whitespace up until the first non-whitespace character.
static void lex_whitespace(Lexer *lxr) {
	while (is_whitespace(lxr->code[lxr->cursor])) {
		char ch = lxr->code[lxr->cursor];
		if (ch == '\n' || ch == '\r') {
			// Treat `\r\n` as a single newline
			if (ch == '\r' && lxr->code[lxr->cursor + 1] == '\n') {
				lxr->cursor++;
			}
			lxr->line++;
		}
		lxr->cursor++;
	}
}

// Lex an identifier or a reserved language keyword.
static void lex_ident(Lexer *lxr) {
	// Find the end of the identifier
	char *candidate = &lxr->code[lxr->cursor];
	while (is_ident_continue(lxr->code[lxr->cursor])) {
		lxr->cursor++;
	}
	lxr->tk.length = lxr->cursor - lxr->tk.start;

	// Compare the identifier against reserved language keywords
	static char *keywords[] = {"let", "if", "else", "elseif", "loop", "while",
		"for", NULL};
	static Tk keyword_tks[] = {TK_LET, TK_IF, TK_ELSE, TK_ELSEIF, TK_LOOP,
		TK_WHILE, TK_FOR};
	for (int i = 0; keywords[i] != NULL; i++) {
		if (lxr->tk.length == strlen(keywords[i]) &&
				strncmp(candidate, keywords[i], lxr->tk.length) == 0) {
			// Found a matching keyword
			lxr->tk.type = keyword_tks[i];
			return;
		}
	}

	// Didn't find a matching keyword, so we have an identifier
	lxr->tk.type = TK_IDENT;
	lxr->tk.ident_hash = hash_string(candidate, lxr->tk.length);
}

// Lex an integer with a specific base.
static void lex_int(Lexer *lxr, int base) {
	// Skip over the prefix for every number that isn't base 10 or 16;
	// annoyingly, strtoull handles the base 16 prefix `0x` for us
	if (base != 10 && base != 16) {
		lxr->cursor += 2;
	}

	// To check for a parse error, the standard library requires us to reset
	// `errno`
	errno = 0;

	// Read the number
	char *start = &lxr->code[lxr->cursor];
	char *end;
	uint64_t value = strtoull(start, &end, base);

	// Check for a parse error
	if (errno != 0) {
		HyErr *err = err_new("failed to parse number");
		err->line = lxr->tk.line;
		err_set_file(err, lxr->path);
		err_trigger(lxr->vm, err);
		return;
	}

	lxr->tk.type = TK_NUM;
	lxr->tk.length = end - start;
	lxr->tk.num = (double) value;
	lxr->cursor += lxr->tk.length;
}

// Lex a floating point value.
static void lex_float(Lexer *lxr) {
	// To check for a parse error, the standard library requires us to reset
	// `errno`
	errno = 0;

	// Read the number
	char *start = &lxr->code[lxr->cursor];
	char *end;
	double value = strtod(start, &end);

	// Check for a parse error
	if (errno != 0) {
		HyErr *err = err_new("failed to parse number");
		err->line = lxr->tk.line;
		err_set_file(err, lxr->path);
		err_trigger(lxr->vm, err);
		return;
	}

	lxr->tk.type = TK_NUM;
	lxr->tk.length = end - start;
	lxr->tk.num = value;
	lxr->cursor += lxr->tk.length;
}

// Lex a number.
static void lex_num(Lexer *lxr) {
	// Lex an optional prefix (0x, 0b, or 0o)
	int base = 10;
	if (lxr->code[lxr->cursor] == '0') {
		switch (lxr->code[lxr->cursor + 1]) {
			case 'x': case 'X': base = 16; break;
			case 'o': case 'O': base = 8; break;
			case 'b': case 'B': base = 2; break;
		}
	}

	// Lex an integer only when we have a number that isn't base 10
	if (base == 10) {
		lex_float(lxr);
	} else {
		lex_int(lxr, base);
	}
}

// Shorthand for multi-character token switch case.
#define MULTI_CHAR_TK(first, second, tk_type)       \
	case first:                                     \
		if (lxr->code[lxr->cursor + 1] == second) { \
			lxr->tk.type = tk_type;                 \
			lxr->tk.length = 2;                     \
			break;                                  \
		}

// Lexes the next token, storing the result in `lxr->tk`.
void lex_next(Lexer *lxr) {
	// Copy across the current line number and start position to the token
	lxr->tk.line = lxr->line;
	lxr->tk.start = lxr->cursor;
	lxr->tk.ident_hash = 0;

	// We use so many case statements in the hope that the compiler is smart
	// enough to convert this switch into a jump table
	switch (lxr->code[lxr->cursor]) {
		// End of file
	case '\0':
		lxr->tk.type = TK_EOF;
		lxr->tk.length = 0;
		return;

		// Whitespace
	case ' ': case '\n': case '\r': case '\t':
		lex_whitespace(lxr);
		lex_next(lxr);
		return;

		// Identifier
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
	case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
	case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
	case 'v': case 'w': case 'x': case 'y': case 'z':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
	case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
	case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
	case 'V': case 'W': case 'X': case 'Y': case 'Z':
	case '_':
		lex_ident(lxr);
		return;

		// Number
	case '0': case '1': case '2': case '3': case '4': case '5': case '6':
	case '7': case '8': case '9':
		lex_num(lxr);
		return;

		// Multi-character symbols
	MULTI_CHAR_TK('.', '.', TK_CONCAT)

	MULTI_CHAR_TK('+', '=', TK_ADD_ASSIGN)
	MULTI_CHAR_TK('-', '=', TK_SUB_ASSIGN)
	MULTI_CHAR_TK('*', '=', TK_MUL_ASSIGN)
	MULTI_CHAR_TK('/', '=', TK_DIV_ASSIGN)
	MULTI_CHAR_TK('%', '=', TK_MOD_ASSIGN)

	MULTI_CHAR_TK('<', '=', TK_LE)
	MULTI_CHAR_TK('>', '=', TK_GE)
	MULTI_CHAR_TK('=', '=', TK_EQ)
	MULTI_CHAR_TK('!', '=', TK_NEQ)

	MULTI_CHAR_TK('&', '&', TK_AND)
	MULTI_CHAR_TK('|', '|', TK_OR)

		// Single character symbol
	default:
		lxr->tk.type = lxr->code[lxr->cursor];
		lxr->tk.length = 1;
		break;
	}

	// If we reach here, then we haven't updated the cursor position yet
	lxr->cursor += lxr->tk.length;
}
