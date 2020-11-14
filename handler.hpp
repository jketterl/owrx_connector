#pragma once

#include <stdint.h>

class Handler {
    public:
        virtual int setup_and_read() = 0;
        virtual int set_center_frequency(double frequency) = 0;
        virtual int set_sample_rate(double sample_rate) = 0;
        // TODO introduce gainspec
        virtual int set_gain() = 0;
        virtual int set_ppm(int ppm) = 0;
};