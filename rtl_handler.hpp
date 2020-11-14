#pragma once

#include "handler.hpp"

class RtlHandler: public Handler {
    public:
        int setup_and_read() override;
        int set_center_frequency(double frequency) override;
        int set_sample_rate(double sample_rate) override;
        // TODO introduce gainspec
        int set_gain() override;
        int set_ppm(int ppm) override;
};