#pragma once

#include "handler.hpp"
#include <stdint.h>
#include <rtl-sdr.h>

class RtlHandler: public Handler {
    public:
        void set_device(char* device) override;
        int open() override;
        int read() override;
        int close() override;
        int set_center_frequency(double frequency) override;
        int set_sample_rate(double sample_rate) override;
        // TODO introduce gainspec
        int set_gain() override;
        int set_ppm(int32_t ppm) override;
        void callback(unsigned char* buf, uint32_t len);
    private:
        char* device_id = "0";
        rtlsdr_dev_t* dev;
        uint32_t verbose_device_search(char *s);
};