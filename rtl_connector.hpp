#pragma once

#include "connector.hpp"
#include "ringbuffer.hpp"
#include "gainspec.hpp"
#include <stdint.h>
#include <rtl-sdr.h>

// this should be the default according to rtl-sdr.h
#define RTL_BUFFER_SIZE 16 * 32 * 512

class RtlConnector: public Connector {
    public:
        void callback(unsigned char* buf, uint32_t len);
    protected:
        uint32_t get_buffer_size() override;
        int open() override;
        int read() override;
        int close() override;
        int set_center_frequency(double frequency) override;
        int set_sample_rate(double sample_rate) override;
        int set_gain(GainSpec* gain) override;
        int set_ppm(int32_t ppm) override;
        int set_iqswap(bool iqswap) override;
    private:
        uint32_t rtl_buffer_size = RTL_BUFFER_SIZE;
        rtlsdr_dev_t* dev;
        bool iqswap;
        uint8_t* conversion_buffer = (uint8_t*) malloc(sizeof(uint8_t) * rtl_buffer_size);

        int verbose_device_search(char const *s);
};