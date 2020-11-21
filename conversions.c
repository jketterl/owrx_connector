#include "conversions.h"
#include <limits.h>

OWRX_CONNECTOR_TARGET_CLONES
void convert_u8_f32(uint8_t* restrict input, float* restrict out, uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; i++) {
        out[i] = ((float) (input[i])) / (UINT8_MAX / 2.0f) - 1.0f;
    }
}

OWRX_CONNECTOR_TARGET_CLONES
void convert_s16_f(int16_t* restrict input, float* restrict output, uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; i++) {
        output[i] = (float)input[i] / SHRT_MAX;
    }
}

OWRX_CONNECTOR_TARGET_CLONES
void convert_s16_u8(int16_t* restrict input, uint8_t* restrict output, uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; i++) {
        output[i] = input[i] / 32767.0f * 128.0f + 127.4f;
    }
}

OWRX_CONNECTOR_TARGET_CLONES
void convert_f32_u8(float* restrict in, uint8_t* restrict out, uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; i++) {
        out[i] = in[i] * UCHAR_MAX * 0.5f + 128;
    }
}
