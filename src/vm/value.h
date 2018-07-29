
// value.h
// By Ben Anderson
// July 2018

#ifndef VALUE_H
#define VALUE_H

// Bits that, when set, indicate a quiet NaN value.
#define QUIET_NAN ((uint64_t) 0x7ffc000000000000)

// The sign bit. Only set if the value is a pointer.
#define SIGN ((uint64_t) 1 << 63)

// Flag bits used to indicate a value is of various types. These bits are set
// immediately after the lowest 2 bytes, where additional information (e.g.
// the primitive type or function index) is stored.
#define FLAG_PRIM (QUIET_NAN | 0x10000)
#define FLAG_FN   (QUIET_NAN | 0x20000)

// Various flag bits to indicate different primitives.
typedef enum {
	PRIM_FALSE = 0x0,
	PRIM_TRUE  = 0x1,
	PRIM_NIL   = 0x2,
} Primitive;

// Evaluates to true if a value is a number.
static inline int val_is_num(uint64_t val) {
	// Is a number if the quiet NaN bits are not set
	return (val & QUIET_NAN) != QUIET_NAN;
}

// Converts a value into a floating point number.
static inline double v2n(uint64_t val) {
	// Use a union to perform to bitwise conversion
	union {
		double num;
		uint64_t val;
	} convert;
	convert.val = val;
	return convert.num;
}

// Converts a floating point number into a value.
static inline uint64_t n2v(double num) {
	// Use a union to perform to bitwise conversion
	union {
		double num;
		uint64_t val;
	} convert;
	convert.num = num;
	return convert.val;
}

#endif
