#include "connector.hpp"
#include "version.h"
#include "iq_connection.hpp"
#include "control_connection.hpp"
#include "fmv.h"
#include <stdlib.h>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <climits>
#include <cstring>

void Connector::init_buffers() {
    float_buffer = new Ringbuffer<float>(10 * get_buffer_size());
    if (rtltcp_port > 0) {
        uint8_buffer = new Ringbuffer<uint8_t>(10 * get_buffer_size());
    }
    // biggest samples that we cane process right now = float
    conversion_buffer = malloc(get_buffer_size() * sizeof(float));
}

int Connector::main(int argc, char** argv) {
    int r = get_arguments(argc, argv);
    if (r == 1) {
        // print usage and exit
        return 0;
    } else if (r != 0) {
        return 1;
    }

    init_buffers();

    if (control_port > 0) {
        new ControlSocket(this, control_port);
    }

    IQSocket<float>* iq_socket = new IQSocket<float>(port, float_buffer);
    iq_socket->start();

    if (rtltcp_port > 0) {
        IQSocket<uint8_t>* rtltcp_socket = new IQSocket<uint8_t>(rtltcp_port, uint8_buffer);
        rtltcp_socket->start();
    }

    r = open();
    if (r != 0) {
        std::cerr << "Connector::open() failed\n";
        return 1;
    }

    r = setup();
    if (r != 0) {
        std::cerr << "Connector::setup() failed\n";
        return 2;
    }

    r = read();
    if (r != 0) {
        std::cerr << "Connector::read() failed\n";
        return 3;
    }

    r = close();
    if (r != 0) {
        std::cerr << "Connector::close() failed\n";
        return 4;
    }

    return 0;
}

std::vector<struct option> Connector::getopt_long_options() {
    return std::vector<struct option> {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"device", required_argument, NULL, 'd'},
        {"port", required_argument, NULL, 'p'},
        {"frequency", required_argument, NULL, 'f'},
        {"samplerate", required_argument, NULL, 's'},
        {"gain", required_argument, NULL, 'g'},
        {"control", required_argument, NULL, 'c'},
        {"ppm", required_argument, NULL, 'P'},
        {"iqswap", no_argument, NULL, 'i'},
        {"rtltcp", required_argument, NULL, 'r'},
    };
}

int Connector::get_arguments(int argc, char** argv) {
    std::vector<struct option> long_options = getopt_long_options();
    long_options.push_back({ NULL, 0, NULL, 0 });

    std::vector<std::string> short_options;
    std::transform(
        long_options.begin(),
        long_options.end(),
        std::inserter(short_options, short_options.end()),
        [](struct option opt){
            return std::string(1, opt.val) + (opt.has_arg == required_argument ? ":" : "");
        }
    );
    std::string short_options_string = std::accumulate(short_options.begin(), short_options.end(), std::string(""));

    program_name = argv[0];
    int c, r;
    while ((c = getopt_long(argc, argv, short_options_string.c_str(), long_options.data(), NULL)) != -1) {
        r = receive_option(c, optarg);
        if (r != 0) return r;
    }
    return 0;
}

int Connector::receive_option(int c, char* optarg) {
    switch (c) {
        case 'h':
            print_usage();
            return 1;
        case 'v':
            print_version();
            return 1;
        case 'd':
            device_id = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'f':
            center_frequency = strtod(optarg, NULL);
            break;
        case 's':
            sample_rate = strtod(optarg, NULL);
            break;
        case 'g':
            gain = GainSpec::parse(new std::string(optarg));
            break;
        case 'c':
            control_port = atoi(optarg);
            break;
        case 'P':
            ppm = atoi(optarg);
            break;
        case 'i':
            iqswap = true;
            break;
        case 'r':
            rtltcp_port = atoi(optarg);
            break;
    }
    return 0;
}

std::stringstream Connector::get_usage_string() {
    std::stringstream s;
    s <<
        program_name << " version " << VERSION << "\n\n" <<
        "Usage: " << program_name << " [options]\n\n" <<
        "Available options:\n" <<
        " -h, --help              show this message\n" <<
        " -v, --version           print version and exit\n" <<
        " -d, --device            device index or selector\n" <<
        " -p, --port              listen port (default: 4590)\n" <<
        " -f, --frequency         tune to specified frequency\n" <<
        " -s, --samplerate        use the specified samplerate\n" <<
        " -g, --gain              set the gain level (default: 0; accepts 'auto' for agc)\n" <<
        " -c, --control           control socket port (default: disabled)\n" <<
        " -P, --ppm               set frequency correction ppm\n" <<
        " -i, --iqswap            swap I and Q samples (reverse spectrum)\n" <<
        " -r, --rtltcp            enable rtl_tcp compatibility mode\n"
    ;
    return s;
}

void Connector::print_usage() {
    std::cerr << get_usage_string().str();
}

void Connector::print_version() {
    std::cout << "owrx-connector version " << VERSION << "\n";
}

int Connector::setup() {
    int r = 0;

    r = set_center_frequency(center_frequency);
    if (r != 0) {
        std::cerr << "setting center frequency failed\n";
        return 2;
    }

    r = set_sample_rate(sample_rate);
    if (r != 0) {
        std::cerr << "setting sample rate failed\n";
        return 3;
    }

    r = set_ppm(ppm);
    if (r != 0) {
        std::cerr << "setting ppm failed\n";
        return 4;
    }

    r = set_gain(gain);
    if (r != 0) {
        std::cerr << "setting gain failed\n";
        return 5;
    }

    r = set_iqswap(iqswap);
    if (r != 0) {
        std::cerr << "setting iqswap failed\n";
        return 6;
    }

    r = set_rtltcp_port(rtltcp_port);
    if (r != 0) {
        std::cerr << "setting rtltcp_compat failed\n";
        return 7;
    }

    return 0;
}

int Connector::set_iqswap(bool new_iqswap) {
    iqswap = new_iqswap;
    return 0;
}

int Connector::set_rtltcp_port(int new_rtltcp_port) {
    rtltcp_port = new_rtltcp_port;
    return 0;
}

void Connector::applyChange(std::string key, std::string value) {
    int r = 0;
    if (key == "center_freq") {
        center_frequency = std::stoul(value);
        r = set_center_frequency(center_frequency);
    } else if (key == "samp_rate") {
        sample_rate = std::stoul(value);
        r = set_sample_rate(sample_rate);
    } else if (key == "rf_gain") {
        gain = GainSpec::parse(&value);
        r = set_gain(gain);
    } else if (key == "ppm") {
        ppm = stoi(value);
        r = set_ppm(ppm);
    } else if (key == "iqswap") {
        iqswap = convertBooleanValue(value);
        r = set_iqswap(iqswap);
    } else {
        std::cerr << "could not set unknown key: \"" << key << "\"\n";
    }
    if (r != 0) {
        std::cerr << "WARNING: setting \"" << key << "\" failed: " << r << "\n";
    }
}

bool Connector::convertBooleanValue(std::string input) {
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });
    return lower == "1" || lower == "true";
}

template <typename T>
void Connector::swapIQ(T* input, T* output, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        output[i] = input[i ^ 1];
    }
}

template <typename T>
void Connector::processSamples(T* input, uint32_t len) {
    T* source = input;
    if (iqswap) {
        source = (T*) conversion_buffer;
        swapIQ(input, source, len);
    }
    uint32_t consumed = 0;
    uint32_t available;
    while (consumed < len) {
        available = float_buffer->get_writeable_samples(len - consumed);
        convert(source + consumed, float_buffer->get_write_pointer(), available);
        float_buffer->advance(available);
        consumed += available;
    }
    if (rtltcp_port > 0) {
        consumed = 0;
        while (consumed < len) {
            available = uint8_buffer->get_writeable_samples(len - consumed);
            convert(source + consumed, uint8_buffer->get_write_pointer(), available);
            uint8_buffer->advance(available);
            consumed += available;
        }
    }
}

template void Connector::processSamples<float>(float*, uint32_t);
template void Connector::processSamples<int16_t>(int16_t*, uint32_t);
template void Connector::processSamples<uint8_t>(uint8_t*, uint32_t);

OWRX_CONNECTOR_TARGET_CLONES
void Connector::convert(uint8_t* __restrict__ input, float* __restrict__ output, uint32_t len) {
    uint32_t i;
    for (i = 0; i < len; i++) {
        output[i] = ((float) (input[i])) / (UINT8_MAX / 2.0f) - 1.0f;
    }
}

OWRX_CONNECTOR_TARGET_CLONES
void Connector::convert(int16_t* __restrict__ input, float* __restrict__ output, uint32_t len) {
    uint32_t i;
    for (i = 0; i < len; i++) {
        output[i] = (float)input[i] / SHRT_MAX;
    }
}

void Connector::convert(float* input, float* output, uint32_t len) {
    std::memcpy(output, input, len * sizeof(float));
}

OWRX_CONNECTOR_TARGET_CLONES
void Connector::convert(uint8_t* __restrict__ input, uint8_t* __restrict__ output, uint32_t len) {
    uint32_t i;
    for (i = 0; i < len; i++) {
        output[i] = input[i] * UCHAR_MAX * 0.5f + 128;
    }
}

OWRX_CONNECTOR_TARGET_CLONES
void Connector::convert(int16_t* __restrict__ input, uint8_t* __restrict__ output, uint32_t len) {
    uint32_t i;
    for (i = 0; i < len; i++) {
        output[i] = input[i] / 32767.0f * 128.0f + 127.4f;
    }
}

void Connector::convert(float* input, uint8_t* output, uint32_t len) {
    std::memcpy(output, input, len * sizeof(float));
}

