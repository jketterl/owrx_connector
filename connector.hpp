#pragma once
#pragma GCC visibility push(default)

#include "ringbuffer.hpp"
#include "gainspec.hpp"
#include <string>
#include <sstream>
#include <getopt.h>
#include <vector>

class Connector {
    public:
        int main(int argc, char** argv);

        virtual void applyChange(std::string key, std::string value);
    protected:
        char* device_id = nullptr;
        Ringbuffer<float>* float_buffer;
        Ringbuffer<uint8_t>* uint8_buffer;
        void* conversion_buffer;
        bool iqswap = false;
        int rtltcp_port = -1;

        bool convertBooleanValue(std::string input);
        void processSamples(int16_t* input, uint32_t len);
        void processSamples(float* input, uint32_t len);
        void processSamples(uint8_t* input, uint32_t len);

        // methods that come with a reasonable default behaviour, but can be overridden
        virtual std::stringstream get_usage_string();
        virtual std::vector<struct option> getopt_long_options();
        virtual int receive_option(int c, char* optarg);
        virtual int set_iqswap(bool iqswap);
        virtual int set_rtltcp_port(int rtltcp_port);
        virtual int setup();

        // methods that must be overridden for the individual hardware
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

        void init_buffers();
        int get_arguments(int argc, char** argv);
        void print_usage();
        void print_version();
        void swapIQ(int16_t* input, int16_t* output, uint32_t len);
        void swapIQ(float* input, float* output, uint32_t len);
        void swapIQ(uint8_t* input, uint8_t* output, uint32_t len);
};
