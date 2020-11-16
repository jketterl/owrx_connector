#pragma once

#include "handler.hpp"
#include "ringbuffer.hpp"

class Connector {
    public:
        Connector(Handler* handler);
        int main(int argc, char** argv);
    private:
        Handler* handler;
        uint16_t port = 4950;
        int32_t control_port = -1;
        double center_frequency;
        double sample_rate;
        int32_t ppm;
        bool iqswap = false;
        Ringbuffer<float>* float_buffer;

        int get_arguments(int argc, char** argv);
        void print_usage(char* program);
        void print_version();
        int setup_and_read();
};
