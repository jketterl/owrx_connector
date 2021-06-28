#include "soapy_connector.hpp"
#include "owrx/gainspec.hpp"
#include <iostream>
#include <algorithm>
#include <getopt.h>
#include <vector>

int main (int argc, char** argv) {
    Connector* connector = new SoapyConnector();
    return connector->main(argc, argv);
}

uint32_t SoapyConnector:: get_buffer_size() {
    return soapy_buffer_size;
};

std::stringstream SoapyConnector::get_usage_string() {
    std::stringstream s = Connector::get_usage_string();
    s <<
        " -a, --antenna           select antenna input\n" <<
        " -t, --settings          set sdr specific settings\n";
    return s;
}

std::vector<struct option> SoapyConnector::getopt_long_options() {
    std::vector<struct option> long_options = Connector::getopt_long_options();
    long_options.push_back({"antenna", required_argument, NULL, 'a'});
    long_options.push_back({"settings", required_argument, NULL, 't'});
    return long_options;
}

int SoapyConnector::receive_option(int c, char* optarg) {
    switch (c) {
        case 'a':
            antenna = std::string(optarg);
            break;
        case 't':
            settings = std::string(optarg);
            break;
        default:
            return Connector::receive_option(c, optarg);
    }
    return 0;
}

int SoapyConnector::open() {
    try {
        dev = SoapySDR::Device::make(device_id == nullptr ? "" : std::string(device_id));
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
};

int SoapyConnector::setup() {
    int r;
    if (antenna != "") {
        r = setAntenna(antenna);
        if (r != 0) {
            std::cerr << "Setting antenna failed\n";
            return 1;
        }
    }

    r = Connector::setup();
    if (r != 0) return r;

    if (settings != "") {
        r = setSettings(settings);
        if (r != 0) {
            std::cerr << "Setting settings failed\n";
            return 1;
        }
    }

    return 0;
}

int SoapyConnector::setAntenna(std::string antenna) {
    try {
        dev->setAntenna(SOAPY_SDR_RX, channel, antenna);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}

int SoapyConnector::setSettings(std::string settings) {
    SoapySDR::Kwargs args = Connector::parseSettings(settings);
    for (const auto &p : args) {
        std::string key = p.first;
        std::string value = p.second;
        try {
            dev->writeSetting(key, value);
        } catch (const std::exception& e) {
            std::cerr << "WARNING: setting key " << key << "failed: " << e.what() << "\n";
        }
    }
    return 0;
}

int SoapyConnector::read() {
    std::string format = SOAPY_SDR_CS16;
    std::vector<std::string> formats = dev->getStreamFormats(SOAPY_SDR_RX, channel);
    // use native CF32 if available
    if (std::find(formats.begin(), formats.end(), SOAPY_SDR_CF32) != formats.end()) {
        format = SOAPY_SDR_CF32;
    	std::cerr << "SPEED: using native CF32 format";
    } else {
    	std::cerr << "SPEEDWARN: not using native CF32 format";
    }

    void* buf = malloc(soapy_buffer_size * SoapySDR::formatToSize(format));
    void* buffs[] = {buf};
    int samples_read;
    long long timeNs = 0;
    long timeoutNs = 1E6;
    int flags = 0;

    //SoapySDRKwargs stream_args = {0};
    size_t num_channels = dev->getNumChannels(SOAPY_SDR_RX);
    if (((size_t) channel) >= num_channels){
        std::cerr << "Invalid channel " << channel << " selected\n";
        return 9;
    }

    SoapySDR::Stream* stream;
    try {
        stream = dev->setupStream(SOAPY_SDR_RX, format, std::vector<size_t>{channel});
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 10;
    }
    dev->activateStream(stream);

    while (run) {
        samples_read = dev->readStream(stream, buffs, soapy_buffer_size, flags, timeNs, timeoutNs);
        // std::cerr << "samples read from sdr: " << samples_read << "\n";

        if (samples_read > 0) {
            uint32_t len = samples_read * 2;
            if (format == SOAPY_SDR_CS16) {
                processSamples((int16_t*) buf, len);
            } else if (format == SOAPY_SDR_CF32) {
                processSamples((float*) buf, len);
            }
        } else if (samples_read == SOAPY_SDR_OVERFLOW) {
            // overflows do happen, they are non-fatal. a warning should do
            std::cerr << "WARNING: Soapy overflow\n";
        } else if (samples_read == SOAPY_SDR_TIMEOUT) {
            // timeout should not break the read loop.
            // TODO or should they? I tried, but airspyhf devices will end up here on sample rate changes.
            std::cerr << "WARNING: SoapySDR::Device::readStream timeout!\n";
        } else {
            // other errors should break the read loop.
            std::cerr << "ERROR: Soapy error " << samples_read << "\n";
            break;
        }
    }

    dev->deactivateStream(stream);

    return 0;
};

int SoapyConnector::close() {
    try {
        SoapySDR::Device::unmake(dev);
        dev = nullptr;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
};

int SoapyConnector::set_center_frequency(double frequency) {
    try {
        dev->setFrequency(SOAPY_SDR_RX, channel, frequency);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
};

int SoapyConnector::set_sample_rate(double sample_rate) {
    try {
        dev->setSampleRate(SOAPY_SDR_RX, channel, sample_rate);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
};

void SoapyConnector::applyChange(std::string key, std::string value) {
    int r = 0;
    if (key == "antenna") {
        antenna = value;
        r = setAntenna(antenna);
    } else if (key == "settings") {
        settings = value;
        r = setSettings(settings);
    } else {
        Connector::applyChange(key, value);
        return;
    }
    if (r != 0) {
        std::cerr << "WARNING: setting \"" << key << "\" failed: " << r << "\n";
    }
}

int SoapyConnector::set_gain(GainSpec* gain) {
    SimpleGainSpec* simple_gain;
    MultiGainSpec* multi_gain;
    if (dynamic_cast<AutoGainSpec*>(gain) != nullptr) {
        try {
            dev->setGainMode(SOAPY_SDR_RX, channel, true);
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
    } else if ((simple_gain = dynamic_cast<SimpleGainSpec*>(gain)) != nullptr) {
        try {
            dev->setGainMode(SOAPY_SDR_RX, channel, false);
            dev->setGain(SOAPY_SDR_RX, channel, simple_gain->getValue());
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
    } else if ((multi_gain = dynamic_cast<MultiGainSpec*>(gain)) != nullptr) {
        std::map<std::string, std::string> gains = multi_gain->getValue();
        try {
            dev->setGainMode(SOAPY_SDR_RX, channel, false);
            for (const auto &p : gains) {
                dev->setGain(SOAPY_SDR_RX, channel, p.first, std::stod(p.second));
            }
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
    } else {
        std::cerr << "unsupported gain settings\n";
        return 100;
    }

    return 0;
};

int SoapyConnector::set_ppm(double ppm) {
    try {
#if defined(SOAPY_SDR_API_VERSION) && (SOAPY_SDR_API_VERSION >= 0x00060000)
        dev->setFrequencyCorrection(SOAPY_SDR_RX, channel, ppm);
#else
        dev->setFrequency(SOAPY_SDR_RX, channel, "CORR", ppm);
#endif
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
};

