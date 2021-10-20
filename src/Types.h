#include <wchar.h>
#include <cstdint>
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef u8 b8;
typedef u16 b16;
typedef u32 b32;
typedef u64 b64;

typedef float f32;
typedef double f64;

typedef wchar_t wchar;

// Trying these out from casey's recommendation.
#define internal static
#define local_persist static
#define global_var static
// My own addition. normally const members in classes still take up memory!
#define class_const static const

#define Kilobyte(A) ((A)*1024)
#define Megabyte(A) (Kilobyte(A)*1024)
#define Gigabyte(A) (Megabyte(A)*1024)

#define ArraySize(A) (sizeof(A)/sizeof(*(A)))

#ifndef NDEBUG
#define Assert(Brk) if (!(Brk)) { __debugbreak(); }
#else
#define Assert(Brk) 
#endif
