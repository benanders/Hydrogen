
// arch.h
// By Ben Anderson
// October 2018

#ifndef ARCH_H
#define ARCH_H

// Possible architectures.
#define HY_ARCH_X86    0
#define HY_ARCH_X64    1

// Detect the target architecture.
#ifndef HY_ARCH // Let HY_ARCH be overridden.
#if defined(__i386) || defined(__i386__) || defined(_M_IX86)
#define HY_ARCH HY_ARCH_X86
#elif (defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) || \
	defined(_M_AMD64))
#define HY_ARCH HY_ARCH_X64
#else
#error "Architecture not supported"
#endif
#endif

// Possible supported operating systems.
#define HY_OS_WINDOWS 0
#define HY_OS_LINUX   1
#define HY_OS_OSX     2
#define HY_OS_OTHER   3

// Detect the target operating system.
#ifndef HY_OS // Let HY_OS be overridden.
#if defined(_WIN32)
#define HY_OS HY_OS_WINDOWS
#elif defined(__linux__)
#define HY_OS HY_OS_LINUX
#elif defined(__MACH__) && defined(__APPLE__)
#define HY_OS HY_OS_OSX
#else
#define HY_OS HY_OS_OTHER
#endif
#endif

// Possible endian-ness.
#define HY_ENDIAN_LITTLE 0
#define HY_ENDIAN_BIG    1

// Set target architecture properties.
#if HY_ARCH == HY_ARCH_X86
#define HY_ARCH_NAME   "x86"
#define HY_ARCH_BITS   32
#define HY_ARCH_ENDIAN HY_ENDIAN_LITTLE
#elif HY_ARCH == HY_ARCH_X64
#define HY_ARCH_NAME   "x64"
#define HY_ARCH_BITS   64
#define HY_ARCH_ENDIAN HY_ENDIAN_LITTLE
#else
#error "No target architecture defined"
#endif

#endif
