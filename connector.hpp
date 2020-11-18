#pragma once

#include "handler.hpp"
#include "ringbuffer.hpp"
#include "gainspec.hpp"
#include <string>

class Connector {
    public:
        Connector(Handler* handler);
        int main(int argc, char** argv);
        void applyChange(std::string key, std::string value);
    private:
        Handler* handler;
        uint16_t port = 4950;
        int32_t control_port = -1;
        double center_frequency;
        double sample_rate;
        int32_t ppm;
        bool iqswap = false;
        GainSpec* gain = new AutoGainSpec();
        Ringbuffer<float>* float_buffer;

        int get_arguments(int argc, char** argv);
        void print_usage(char* program);
        void print_version();
        int setup_and_read();
        bool convertBooleanValue(std::string input);
};
