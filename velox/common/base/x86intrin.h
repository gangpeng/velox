// Compatibility shim: MSVC doesn't have <x86intrin.h>, use <intrin.h> instead.
#pragma once
#ifdef _MSC_VER
#include <intrin.h>
#include <immintrin.h>
#else
#include_next <x86intrin.h>
#endif
