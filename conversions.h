#pragma once
#pragma GCC visibility push(default)

#include "fmv.h"
#include <stdint.h>

void convert_u8_f32(uint8_t* input, float* out, uint32_t count);
