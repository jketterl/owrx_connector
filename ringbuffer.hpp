#pragma once

#include <stdint.h>

template <typename T>
class Ringbuffer {
    public:
        Ringbuffer(uint32_t len);
        T* get_write_pointer();
    private:
        T* buffer;
        uint32_t len;
        uint32_t write_pos;
};
