#include "connector.hpp"
#include "version.h"
#include "iq_connection.hpp"
#include "control_connection.hpp"
#include <cstdio>
#include <getopt.h>
#include <stdlib.h>
#include <algorithm>

Connector::Connector(Handler* new_handler) {
    handler = new_handler;
    float_buffer = new Ringbuffer<float>(10 * handler->get_buffer_size());
    handler->set_buffers(float_buffer);
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

int Connector::get_arguments(int argc, char** argv) {
    // TODO allow additional arguments for specific connector types (e.g. bias-tee)
    static struct option long_options[] = {
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
        { NULL, 0, NULL, 0 }
    };

    int c;
    while ((c = getopt_long(argc, argv, "vhd:p:f:s:g:c:P:ir:", long_options, NULL)) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return 1;
            case 'v':
                print_version();
                return 1;
            case 'd':
                handler->set_device(optarg);
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
                handler->set_iqswap(true);
                break;
            case 'r':
                // TODO implement rtl_tcp compat
                break;
        }
    }
    return 0;
}

void Connector::print_usage(char* program) {
    fprintf(stderr,
        "%1$s version %2$s\n\n"
        "Usage: %1$s [options]\n\n"
        "Available options:\n"
        " -h, --help              show this message\n"
        " -v, --version           print version and exit\n"
        " -d, --device            device index or selector\n"
        " -p, --port              listen port (default: 4590)\n"
        " -f, --frequency         tune to specified frequency\n"
        " -s, --samplerate        use the specified samplerate\n"
        " -g, --gain              set the gain level (default: 0; accepts 'auto' for agc)\n"
        " -c, --control           control socket port (default: disabled)\n"
        " -P, --ppm               set frequency correction ppm\n"
        " -r, --rtltcp            enable rtl_tcp compatibility mode\n",
        program, VERSION
    );
}

void Connector::print_version() {
    fprintf(stdout, "owrx-connector version %s\n", VERSION);
}

int Connector::setup_and_read() {
    int r = 0;
    r = handler->open();
    if (r != 0) {
        fprintf(stderr, "Handler::open() failed\n");
        return 1;
    }

    r = handler->set_center_frequency(center_frequency);
    if (r != 0) {
        fprintf(stderr, "setting center frequency failed\n");
        return 2;
    }

    r = handler->set_sample_rate(sample_rate);
    if (r != 0) {
        fprintf(stderr, "setting sample rate failed\n");
        return 3;
    }

    r = handler->set_ppm(ppm);
    if (r != 0) {
        fprintf(stderr, "setting ppm failed\n");
        return 4;
    }

    r = handler->set_gain(gain);
    if (r != 0) {
        fprintf(stderr, "setting gain failed\n");
        return 5;
    }

    r = handler->set_iqswap(iqswap);
    if (r != 0) {
        fprintf(stderr, "setting iqswap failed\n");
        return 6;
    }

    r = handler->read();
    if (r != 0) {
        fprintf(stderr, "Handler::read() failed\n");
        return 100;
    }


    r = handler->close();
    if (r != 0) {
        fprintf(stderr, "Handler::close() failed\n");
        return 101;
    }

    return 0;
}

void Connector::applyChange(std::string key, std::string value) {
    int r = 0;
    if (key == "center_freq") {
        center_frequency = std::stoul(value);
        r = handler->set_center_frequency(center_frequency);
    } else if (key == "samp_rate") {
        sample_rate = std::stoul(value);
        r = handler->set_sample_rate(sample_rate);
    } else if (key == "rf_gain") {
        gain = GainSpec::parse(&value);
        r = handler->set_gain(gain);
    } else if (key == "ppm") {
        ppm = stoi(value);
        r = handler->set_ppm(ppm);
    } else if (key == "iqswap") {
        iqswap = convertBooleanValue(value);
        r = handler->set_iqswap(iqswap);
    } else {
        fprintf(stderr, "could not set unknown key: \"%s\"\n", key.c_str());
    }
    if (r != 0) {
        fprintf(stderr, "WARNING: setting %s failed: %i\n", key.c_str(), r);
    }
}

bool Connector::convertBooleanValue(std::string input) {
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });
    return lower == "1" || lower == "true";
}