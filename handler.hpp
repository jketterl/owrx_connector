#pragma once

#include "ringbuffer.hpp"
#include "gainspec.hpp"
#include <stdint.h>

class Handler {
    public:
        virtual void set_device(char* device) = 0;
        virtual void set_buffers(Ringbuffer<float>* float_buffer) = 0;
};