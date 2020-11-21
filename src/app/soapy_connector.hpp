#pragma once

#include "owrx/connector.hpp"
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Version.hpp>
#include <SoapySDR/Formats.hpp>

using namespace Owrx;

#define SOAPY_BUFFER_SIZE 64512 * 4

class SoapyConnector: public Connector {
    public:
        virtual uint32_t get_buffer_size() override;
        virtual int open() override;
        virtual int read() override;
        virtual int close() override;
        virtual int set_center_frequency(double frequency) override;
        virtual int set_sample_rate(double sample_rate) override;
        virtual int set_gain(GainSpec* gain) override;
        virtual int set_ppm(int ppm) override;
    private:
        uint32_t soapy_buffer_size = SOAPY_BUFFER_SIZE;
        SoapySDR::Device* dev = nullptr;
        size_t channel = 0;
};