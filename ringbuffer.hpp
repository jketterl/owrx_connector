#pragma once

#include <stdint.h>

template <typename T>
class Ringbuffer {
    public:
        Ringbuffer(uint32_t len);
        T* get_write_pointer();
        void advance(uint32_t how_much);
        int available_bytes(int read_pos);
    private:
        T* buffer;
        uint32_t len;
        uint32_t write_pos;

        unsigned int mod(int n, int x);
};
