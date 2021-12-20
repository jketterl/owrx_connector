#include "rtl_connector.hpp"
#include "owrx/gainspec.hpp"
#include <cstring>
#include <stdlib.h>
#include <iostream>

uint32_t RtlConnector::get_buffer_size() {
    return rtl_buffer_size;
}

std::stringstream RtlConnector::get_usage_string() {
    std::stringstream s = Connector::get_usage_string();
    s <<
#if HAS_RTLSDR_SET_BIAS_TEE
        " -b, --biastee           enable bias-tee voltage if supported by hardware\n" <<
#endif
        " -e, --directsampling    enable direct sampling on the specified input\n" <<
        "                         (0 = disabled, 1 = I-input, 2 = Q-input)\n";
    return s;
}

std::vector<struct option> RtlConnector::getopt_long_options(){
    std::vector<struct option> long_options = Connector::getopt_long_options();
    long_options.push_back({"directsampling", required_argument, NULL, 'e'});
#if HAS_RTLSDR_SET_BIAS_TEE
    long_options.push_back({"biastee", no_argument, NULL, 'b'});
#endif
    return long_options;
}

int RtlConnector::receive_option(int c, char* optarg) {
    switch (c) {
#if HAS_RTLSDR_SET_BIAS_TEE
        case 'b':
            bias_tee = true;
            break;
#endif
        case 'e':
            direct_sampling = std::strtoul(optarg, NULL, 10);
            break;
        default:
            return Connector::receive_option(c, optarg);
    }
    return 0;
}

void RtlConnector::print_version() {
    std::cout << "rtl_connector version " << VERSION << "\n";
    Connector::print_version();
}

int RtlConnector::open() {
    int dev_index = verbose_device_search(device_id);

    if (dev_index < 0) {
        std::cerr << "no device found.\n";
        return 1;
    }

    rtlsdr_open(&dev, (uint32_t)dev_index);
    if (NULL == dev) {
        std::cerr << "device could not be opened\n";
        return 2;
    }

    return 0;
}

int RtlConnector::setup() {
    int r = Connector::setup();
    if (r != 0) return r;

#if HAS_RTLSDR_SET_BIAS_TEE
    r = set_bias_tee( bias_tee);
    if (r != 0) {
        std::cerr << "setting biastee failed\n";
        return 10;
    }
#endif

    if (direct_sampling >= 0 && direct_sampling <= 2) {
        r = set_direct_sampling(direct_sampling);
        if (r != 0) {
            std::cerr << "setting direct sampling mode failed\n";
            return 11;
        }
    }

    return 0;
}

void rtlsdr_callback(unsigned char* buf, uint32_t len, void* ctx) {
    ((RtlConnector*) ctx)->callback(buf, len);
}

int RtlConnector::read() {
    uint32_t buf_num = 2;
    int r;

    r = rtlsdr_reset_buffer(dev);
    if (r < 0) {
        std::cerr <<  "WARNING: Failed to reset buffers.\n";
    }

    r = rtlsdr_read_async(dev, rtlsdr_callback, this, buf_num, rtl_buffer_size);
    if (r != 0) {
        std::cerr << "WARNING: rtlsdr_read_async failed with r = " << r << "\n";
    }

    return 0;
}

int RtlConnector::stop() {
    int r = rtlsdr_cancel_async(dev);
    if (r != 0) {
        std::cerr << "WARNING: rtlsdr_cancel_async failed\n";
        return r;
    }
    return Connector::stop();
}

void RtlConnector::callback(unsigned char* buf, uint32_t len) {
    if (len != rtl_buffer_size) {
        std::cerr << "WARNING: invalid buffer size received; skipping input\n";
        return;
    }
    processSamples((uint8_t*) buf, len);
}

int RtlConnector::close() {
    return rtlsdr_close(dev);
}

int RtlConnector::verbose_device_search(char const *s) {
    int i, device_count, device, offset;
    char *s2;
    char vendor[256], product[256], serial[256];
    device_count = rtlsdr_get_device_count();
    if (!device_count) {
        std::cerr << "No supported devices found.\n";
        return -1;
    }
    std::cerr << "Found " << device_count << " device(s):\n";
    for (i = 0; i < device_count; i++) {
        rtlsdr_get_device_usb_strings(i, vendor, product, serial);
        std::cerr << "  " << i << ":  " << vendor << ", " << product << ", SN: " << serial << "\n";
    }
    std::cerr << "\n";
    /* if no device has been selected by the user, use the first one */
    if (s == nullptr) {
        if (device_count > 0) return 0;
        return -1;
    }
    /* does string begin with "serial=" and exact match a serial */
    if (strncmp(s, "serial=", 7) == 0) {
        s2 = (char*) s + 7;
        for (i = 0; i < device_count; i++) {
            rtlsdr_get_device_usb_strings(i, vendor, product, serial);
            if (strcmp(s2, serial) == 0) {
                device = i;
                std::cerr << "Using device " << device << ": " <<
                    rtlsdr_get_device_name((uint32_t)device) << "\n";
                return device;
            }
        }
    /* does string begin with "index=" and match an available index */
    } else if (strncmp(s, "index=", 6) == 0) {
        device = (int)strtol((char *) s + 6, &s2, 0);
        if (s2[0] == '\0' && device >= 0 && device < device_count) {
            std::cerr << "Using device " << device << ": " <<
                rtlsdr_get_device_name((uint32_t)device) << "\n";
            return device;
        }
    } else {
        /* does string exact match a serial */
        for (i = 0; i < device_count; i++) {
            rtlsdr_get_device_usb_strings(i, vendor, product, serial);
            if (strcmp(s, serial) == 0) {
                device = i;
                std::cerr << "Using device " << device << ": " <<
                    rtlsdr_get_device_name((uint32_t)device) << "\n";
                return device;
            }
        }
        /* does string look like raw id number */
        device = (int)strtol(s, &s2, 0);
        if (s2[0] == '\0' && device >= 0 && device < device_count) {
            std::cerr << "Using device " << device << ": " <<
                rtlsdr_get_device_name((uint32_t)device) << "\n";
            return device;
        }
    }
    std::cerr << "No matching devices found.\n";
    return -1;
}

void RtlConnector::applyChange(std::string key, std::string value) {
    int r = 0;
    if (key == "direct_sampling") {
        if (value == "None") {
            direct_sampling = 0;
        } else {
            direct_sampling = std::stoul(value, NULL, 10);
        }
        r = set_direct_sampling(direct_sampling);
#if HAS_RTLSDR_SET_BIAS_TEE
    } else if (key == "bias_tee") {
        bias_tee = convertBooleanValue(value);
        r = set_bias_tee(bias_tee);
#endif
    } else {
        Connector::applyChange(key, value);
        return;
    }
    if (r != 0) {
        std::cerr << "WARNING: setting \"" << key << "\" failed: " << r << "\n";
    }
}

int RtlConnector::set_center_frequency(double frequency) {
    return rtlsdr_set_center_freq(dev, frequency);
}

int RtlConnector::set_sample_rate(double sample_rate) {
    return rtlsdr_set_sample_rate(dev, sample_rate);
}

int RtlConnector::set_gain(GainSpec* gain) {
    int r;
    if (dynamic_cast<AutoGainSpec*>(gain) != nullptr) {
        r = rtlsdr_set_tuner_gain_mode(dev, 0);
        if (r < 0) {
            std::cerr << "setting gain mode failed\n";
            return 1;
        }
        return 0;
    }

    SimpleGainSpec* simple_gain;
    if ((simple_gain = dynamic_cast<SimpleGainSpec*>(gain)) != nullptr) {
        r = rtlsdr_set_tuner_gain_mode(dev, 1);
        if (r < 0) {
            std::cerr << "setting gain mode failed\n";
            return 2;
        }

        r = rtlsdr_set_tuner_gain(dev, (int)(simple_gain->getValue() * 10));
        if (r < 0) {
            std::cerr << "setting gain failed\n";
            return 3;
        }
        return 0;
    }

    std::cerr << "unsupported gain settings\n";
    return 100;

    return 0;
}

int RtlConnector::set_ppm(double ppm) {
    // setting the same value again results in error, so check beforehand
    int corr = rtlsdr_get_freq_correction(dev);
    if (corr == ppm) {
        return 0;
    }
    return rtlsdr_set_freq_correction(dev, ppm);
};

int RtlConnector::set_direct_sampling(int new_direct_sampling) {
    int r;
    r = rtlsdr_set_direct_sampling(dev, new_direct_sampling);
    if (r != 0) {
        std::cerr << "rtlsdr_set_direct_sampling() failed with rc = " << r << "\n";
        return r;
    }
    // switching direct sampling mode requires setting the frequency again
    r = set_center_frequency(get_center_frequency());
    if (r != 0) {
        std::cerr << "set_center_frequency() failed with rc = " << r << "\n";
        return r;
    }
    if (direct_sampling == 0) {
        // gain is off when switching out of direct sampling, so reset it
        r = set_gain(get_gain());
        if (r != 0) {
            std::cerr << "set_gain() failed with rc = " << r << "\n";
            return r;
        }
    }
    return 0;
}

#if HAS_RTLSDR_SET_BIAS_TEE
int RtlConnector::set_bias_tee(bool new_bias_tee) {
    return rtlsdr_set_bias_tee(dev, (int) new_bias_tee);
}
#endif

int main (int argc, char** argv) {
    Connector* connector = new RtlConnector();
    return connector->main(argc, argv);
}