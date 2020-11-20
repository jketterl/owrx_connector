#pragma once
#pragma GCC visibility push(default)

#include "fmv.h"
#include <stdint.h>

void convert_u8_f32(uint8_t* input, float* output, uint32_t count);
void convert_s16_f(int16_t* input, float* output, uint32_t count);
void convert_s16_u8(int16_t* input, uint8_t* output, uint32_t count);
void convert_f32_u8(float* in, uint8_t* out, uint32_t count);
