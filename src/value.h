
// value.h
// By Ben Anderson
// July 2018

#ifndef VALUE_H
#define VALUE_H

// A value stored on the stack.
typedef uint64_t Value;

// Bits that, when set, indicate a quiet NaN value.
#define QUIET_NAN ((uint64_t) 0x7ffc000000000000)

// The sign bit. Only set if the value is a pointer.
#define SIGN ((uint64_t) 1 << 63)

// Various flag bits to indicate different primitives.
//
// We use 0b0010 and 0b0011 for boolean values so that we can check for true
// or false by xoring out TAG_BOOL.
typedef enum {
	PRIM_NIL   = 0x0, // 0b0000
	PRIM_FALSE = 0x2, // 0b0010
	PRIM_TRUE  = 0x3, // 0b0011
} Primitive;

// Flag bits used to indicate a value is of various types. These bits are set
// immediately after the lowest 2 bytes, where additional information (e.g.
// the primitive type or function index) is stored.
#define TAG_PTR    (QUIET_NAN | SIGN)
#define TAG_PRIM   (QUIET_NAN | 0x10000)
#define TAG_BOOL   (QUIET_NAN | 0x00002)
#define TAG_FN     (QUIET_NAN | 0x20000)
#define TAG_NATIVE (QUIET_NAN | 0x30000)

#define VAL_NIL    (TAG_PRIM | PRIM_NIL)

// Converts a value into a floating point number.
static inline double v2n(Value val) {
	// Use a union to perform to bitwise conversion
	union {
		double num;
		Value val;
	} convert;
	convert.val = val;
	return convert.num;
}

// Converts a floating point number into a value.
static inline Value n2v(double num) {
	// Use a union to perform to bitwise conversion
	union {
		double num;
		Value val;
	} convert;
	convert.num = num;
	return convert.val;
}

// Converts a value to a pointer.
static inline void * v2ptr(Value val) {
	// Get the first 48 bits storing the pointer value
	return (void *) (val & 0xffffffffffffffff);
}

// Converts a pointer to a value.
static inline Value ptr2v(void *ptr) {
	return ((Value) ptr) | TAG_PTR;
}

#endif
