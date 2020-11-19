#include "connector.hpp"
#include "version.h"
#include "iq_connection.hpp"
#include "control_connection.hpp"
#include <stdlib.h>
#include <algorithm>
#include <numeric>
#include <iostream>

void Connector::init_buffers() {
    float_buffer = new Ringbuffer<float>(10 * get_buffer_size());
}

int Connector::main(int argc, char** argv) {
    int r = get_arguments(argc, argv);
    if (r == 1) {
        // print usage and exit
        return 0;
    } else if (r != 0) {
        return 1;
    }

    if (control_port > 0) {
        new ControlSocket(this, control_port);
    }

    IQSocket* iq_socket = new IQSocket(port, float_buffer);
    iq_socket->start();

    setup_and_read();
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
        {"rtltcp", no_argument, NULL, 'r'},
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
            set_iqswap(true);
            break;
        case 'r':
            // TODO implement rtl_tcp compat
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

int Connector::setup_and_read() {
    int r = 0;
    r = open();
    if (r != 0) {
        std::cerr << "Handler::open() failed\n";
        return 1;
    }

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

    r = read();
    if (r != 0) {
        std::cerr << "Handler::read() failed\n";
        return 100;
    }


    r = close();
    if (r != 0) {
        std::cerr << "Handler::close() failed\n";
        return 101;
    }

    return 0;
}

int Connector::set_iqswap(bool new_iqswap) {
    iqswap = new_iqswap;
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