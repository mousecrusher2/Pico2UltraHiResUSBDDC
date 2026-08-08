#ifndef PICO2_CLANG_TIDY_PICO_H
#define PICO2_CLANG_TIDY_PICO_H

// Clang's ACLE intrinsics and Pico SDK 2.3 define the same lower-case barrier
// helpers with incompatible signatures. Load CMSIS first, then give the Pico
// helpers private names while clang-tidy parses the normal GCC build command.
#if defined(__clang__)
#include <arm_math.h>

#define __nop __pico_sdk_nop
#define __dmb __pico_sdk_dmb
#define __dsb __pico_sdk_dsb
#define __isb __pico_sdk_isb
#endif

#endif
