#include "conversions.h"

OWRX_CONNECTOR_TARGET_CLONES
void convert_u8_f32(uint8_t* restrict input, float* out, uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; i++) {
        out[i] = ((float) (input[i])) / (UINT8_MAX / 2.0) - 1.0;
    }
}
