#pragma once

#include "ringbuffer.hpp"
#include "gainspec.hpp"
#include <stdint.h>

class Handler {
    public:
        virtual void set_device(char* device) = 0;
        virtual int open() = 0;
        virtual int read() = 0;
        virtual int close() = 0;
        virtual int set_center_frequency(double frequency) = 0;
        virtual int set_sample_rate(double sample_rate) = 0;
        virtual int set_gain(GainSpec* gain) = 0;
        virtual int set_ppm(int ppm) = 0;
        virtual int set_iqswap(bool iqswap) = 0;
        virtual uint32_t get_buffer_size() = 0;
        virtual void set_buffers(Ringbuffer<float>* float_buffer) = 0;
};