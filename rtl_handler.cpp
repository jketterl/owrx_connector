#include "rtl_handler.hpp"
#include <cstdio>

int RtlHandler::setup_and_read() {
    fprintf(stderr, "reading...\n");
    return 0;
}

int RtlHandler::set_center_frequency(double frequency) {
    return 0;
}

int RtlHandler::set_sample_rate(double sample_rate) {
    return 0;
}

// TODO introduce gainspec
int RtlHandler::set_gain() {
    return 0;
}

int RtlHandler::set_ppm(int ppm) {
    return 0;
};
