#pragma once

#include "owrx/connector.hpp"

using namespace Owrx;

#define RTL_BUFFER_SIZE 16 * 32 * 512

// the basic form which is expected by rtl_tcp
// the mappings for the commands can be found in the rtl_tcp source code
struct command {
    unsigned char cmd;
    unsigned int param;
}__attribute((packed));

class RtlTcpConnector: public Connector {
    public:
        void applyChange(std::string key, std::string value) override;
    protected:
        std::stringstream get_usage_string() override;
        std::vector<struct option> getopt_long_options() override;
        int receive_option(int c, char* optarg) override;
        virtual int parse_arguments(int argc, char** argv) override;
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
        int set_direct_sampling(int direct_sampling);
        int set_bias_tee(bool bias_tee);
    private:
        uint32_t rtl_buffer_size = RTL_BUFFER_SIZE;
        std::string host = "127.0.0.1";
        uint16_t port = 1234;
        int sock;
        int direct_sampling = -1;
        bool bias_tee = false;

        int send_command(struct command cmd);
};