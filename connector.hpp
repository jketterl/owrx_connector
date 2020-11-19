#pragma once

#include "ringbuffer.hpp"
#include "gainspec.hpp"
#include <string>
#include <sstream>
#include <getopt.h>
#include <vector>

class Connector {
    public:
        int main(int argc, char** argv);
        void init_buffers();

        virtual void applyChange(std::string key, std::string value);
    protected:
        char* device_id = nullptr;
        Ringbuffer<float>* float_buffer;
        bool iqswap = false;

        int set_iqswap(bool iqswap);
        bool convertBooleanValue(std::string input);

        virtual std::stringstream get_usage_string();
        virtual std::vector<struct option> getopt_long_options();
        virtual int receive_option(int c, char* optarg);
        virtual uint32_t get_buffer_size() = 0;
        virtual int open() = 0;
        virtual int read() = 0;
        virtual int close() = 0;
        virtual int set_center_frequency(double frequency) = 0;
        virtual int set_sample_rate(double sample_rate) = 0;
        virtual int set_gain(GainSpec* gain) = 0;
        virtual int set_ppm(int ppm) = 0;
    private:
        char* program_name;
        uint16_t port = 4950;
        int32_t control_port = -1;
        double center_frequency;
        double sample_rate;
        int32_t ppm;
        GainSpec* gain = new AutoGainSpec();

        int get_arguments(int argc, char** argv);
        void print_usage();
        void print_version();
        int setup_and_read();
};
