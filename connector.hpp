#pragma once

#include "handler.hpp"
#include "ringbuffer.hpp"
#include "gainspec.hpp"
#include <string>

class Connector {
    public:
        int main(int argc, char** argv);
        void init_buffers();
        void applyChange(std::string key, std::string value);

    protected:
        char* device_id = nullptr;
        Ringbuffer<float>* float_buffer;

        virtual uint32_t get_buffer_size() = 0;
        virtual int open() = 0;
        virtual int read() = 0;
        virtual int close() = 0;
        virtual int set_center_frequency(double frequency) = 0;
        virtual int set_sample_rate(double sample_rate) = 0;
        virtual int set_gain(GainSpec* gain) = 0;
        virtual int set_ppm(int ppm) = 0;
        virtual int set_iqswap(bool iqswap) = 0;
    private:
        uint16_t port = 4950;
        int32_t control_port = -1;
        double center_frequency;
        double sample_rate;
        int32_t ppm;
        bool iqswap = false;
        GainSpec* gain = new AutoGainSpec();

        int get_arguments(int argc, char** argv);
        void print_usage(char* program);
        void print_version();
        int setup_and_read();
        bool convertBooleanValue(std::string input);
};
