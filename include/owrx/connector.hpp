#pragma once
#pragma GCC visibility push(default)

#include <string>
#include <sstream>
#include <getopt.h>
#include <vector>
#include <map>
#include <csdr/ringbuffer.hpp>

namespace Owrx {

    // forward class definitions for the internal API
    class GainSpec;

    class Connector {
        public:
            Connector();
            virtual ~Connector() = default;
            int main(int argc, char** argv);
            void handle_signal(int signal);

            virtual void applyChange(std::string key, std::string value);

            static std::map<std::string, std::string> parseSettings(std::string input);
        protected:
            char* device_id = nullptr;
            bool iqswap = false;
            int rtltcp_port = -1;
            bool run = true;
            std::mutex devMutex;

            bool convertBooleanValue(std::string input);

            template <typename T>
            void processSamples(T* input, uint32_t len);

            // methods that come with a reasonable default behaviour, but can be overridden
            virtual int parse_arguments(int argc, char** argv);
            virtual std::stringstream get_usage_string();
            virtual std::vector<struct option> getopt_long_options();
            virtual int receive_option(int c, char* optarg);
            virtual void print_version();
            virtual int set_iqswap(bool iqswap);
            virtual int set_rtltcp_port(int rtltcp_port);
            virtual int setup();
            virtual int stop();

            virtual double get_center_frequency();
            virtual double get_sample_rate();
            virtual GainSpec* get_gain();
            virtual double get_ppm();

            // methods that must be overridden for the individual hardware
            virtual uint32_t get_buffer_size() = 0;
            virtual int open() = 0;
            virtual int read() = 0;
            virtual int close() = 0;
            virtual int set_center_frequency(double frequency) = 0;
            virtual int set_sample_rate(double sample_rate) = 0;
            virtual int set_gain(GainSpec* gain) = 0;
            virtual int set_ppm(double ppm) = 0;
        private:
            char* program_name;
            uint16_t port = 4950;
            int32_t control_port = -1;
            double center_frequency;
            double sample_rate;
            double ppm;
            GainSpec* gain;
            Csdr::Ringbuffer<float>* float_buffer;
            Csdr::Ringbuffer<uint8_t>* uint8_buffer;
            void* conversion_buffer;

            void init_buffers();
            void print_usage();

            template <typename T>
            void swapIQ(T* input, T* output, uint32_t len);

            void convert(uint8_t* input, float* output, uint32_t len);
            void convert(int16_t* input, float* output, uint32_t len);
            void convert(int32_t* input, float* output, uint32_t len);
            void convert(float* input, float* output, uint32_t len);

            void convert(uint8_t* input, uint8_t* output, uint32_t len);
            void convert(int16_t* input, uint8_t* output, uint32_t len);
            void convert(int32_t* input, uint8_t* output, uint32_t len);
            void convert(float* input, uint8_t* output, uint32_t len);
    };
}