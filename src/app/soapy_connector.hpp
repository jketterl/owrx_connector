#pragma once

#include "owrx/connector.hpp"
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Version.hpp>
#include <SoapySDR/Formats.hpp>
#include <string>

using namespace Owrx;

#define SOAPY_BUFFER_SIZE 64512 * 4
#define SOAPY_RESTART_SECS 60

class SoapyConnector: public Connector {
    public:
        virtual void applyChange(std::string key, std::string value) override;
    protected:
        virtual void print_version() override;
        virtual uint32_t get_buffer_size() override;
        virtual int open() override;
        virtual int setup() override;
        virtual int read() override;
        virtual int close() override;
        virtual int set_center_frequency(double frequency) override;
        virtual int set_sample_rate(double sample_rate) override;
        virtual int set_gain(GainSpec* gain) override;
        virtual int set_ppm(double ppm) override;
    private:
        uint32_t soapy_buffer_size = SOAPY_BUFFER_SIZE;
        SoapySDR::Device* dev = nullptr;
        size_t channel = 0;
        std::string antenna = "";
        std::string settings = "";

        std::stringstream get_usage_string() override;
        std::vector<struct option> getopt_long_options() override;
        int receive_option(int c, char* optarg) override;
        int setAntenna(std::string antenna);
        int setSettings(std::string settings);
};
