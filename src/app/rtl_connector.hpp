#pragma once

#include "owrx/connector.hpp"
#include <stdint.h>
#include <rtl-sdr.h>

// this should be the default according to rtl-sdr.h
#define RTL_BUFFER_SIZE 16 * 32 * 512

class RtlConnector: public Connector {
    public:
        void callback(unsigned char* buf, uint32_t len);
        void applyChange(std::string key, std::string value) override;
    protected:
        std::stringstream get_usage_string() override;
        std::vector<struct option> getopt_long_options() override;
        int receive_option(int c, char* optarg);
        uint32_t get_buffer_size() override;
        int open() override;
        int setup() override;
        int read() override;
        int close() override;
        int set_center_frequency(double frequency) override;
        int set_sample_rate(double sample_rate) override;
        int set_gain(GainSpec* gain) override;
        int set_ppm(int32_t ppm) override;
        int set_direct_sampling(int direct_sampling);
#if HAS_RTLSDR_SET_BIAS_TEE
        int set_bias_tee(bool bias_tee);
#endif
    private:
        uint32_t rtl_buffer_size = RTL_BUFFER_SIZE;
        rtlsdr_dev_t* dev;
        uint8_t* conversion_buffer = (uint8_t*) malloc(sizeof(uint8_t) * rtl_buffer_size);
        int direct_sampling = -1;
#if HAS_RTLSDR_SET_BIAS_TEE
        bool bias_tee = false;
#endif

        int verbose_device_search(char const *s);
};