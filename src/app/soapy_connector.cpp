#include "soapy_connector.hpp"
#include "owrx/gainspec.hpp"
#include <iostream>
#include <algorithm>
#include <getopt.h>
#include <vector>
#include <SoapySDR/Modules.hpp>
#include <SoapySDR/Registry.hpp>

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
        " -t, --settings          set sdr specific settings\n" <<
        " -l, --listdrivers       list installed SoapySDR drivers\n";
    return s;
}

std::vector<struct option> SoapyConnector::getopt_long_options() {
    std::vector<struct option> long_options = Connector::getopt_long_options();
    long_options.push_back({"antenna", required_argument, NULL, 'a'});
    long_options.push_back({"settings", required_argument, NULL, 't'});
    long_options.push_back({"listdrivers", no_argument, NULL, 'l'});
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
        case 'l':
            listDrivers();
            return 1;
        default:
            return Connector::receive_option(c, optarg);
    }
    return 0;
}

void SoapyConnector::print_version() {
    std::cout << "soapy_connector version " << VERSION << std::endl;
    Connector::print_version();
}

void SoapyConnector::listDrivers() {
    // populate the registry
    for (const auto &mod : SoapySDR::listModules()) {
        SoapySDR::loadModule(mod);
    }

    // then print drivers, one per line
    for (const auto &it : SoapySDR::Registry::listFindFunctions()) {
        std::cout << it.first << std::endl;
    }
}

int SoapyConnector::open() {
    try {
        std::lock_guard<std::mutex> lck(devMutex);
        dev = SoapySDR::Device::make(device_id == nullptr ? "" : std::string(device_id));
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
};

int SoapyConnector::setup() {
    int r;
    if (antenna != "") {
        r = setAntenna(antenna);
        if (r != 0) {
            std::cerr << "Setting antenna failed" << std::endl;
            return 1;
        }
    }

    r = Connector::setup();
    if (r != 0) return r;

    if (settings != "") {
        r = setSettings(settings);
        if (r != 0) {
            std::cerr << "Setting settings failed" << std::endl;
            return 1;
        }
    }

    return 0;
}

int SoapyConnector::setAntenna(std::string antenna) {
    try {
        std::lock_guard<std::mutex> lck(devMutex);
        if (dev == nullptr) return 0;
        dev->setAntenna(SOAPY_SDR_RX, channel, antenna);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}

int SoapyConnector::setSettings(std::string settings) {
    std::lock_guard<std::mutex> lck(devMutex);
    if (dev == nullptr) return 0;
    SoapySDR::Kwargs args = Connector::parseSettings(settings);
    for (const auto &p : args) {
        std::string key = p.first;
        std::string value = p.second;
        try {
            dev->writeSetting(key, value);
        } catch (const std::exception& e) {
            std::cerr << "WARNING: setting key " << key << "failed: " << e.what() << std::endl;
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
        std::cerr << "Invalid channel " << channel << " selected" << std::endl;
        return 9;
    }

    SoapySDR::Stream* stream;
    try {
        stream = dev->setupStream(SOAPY_SDR_RX, format, std::vector<size_t>{channel});
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 10;
    }
    dev->activateStream(stream);

    while (run) {
        samples_read = dev->readStream(stream, buffs, soapy_buffer_size, flags, timeNs, timeoutNs);
        // std::cout << "samples read from sdr: " << samples_read << std::endl;

        if (samples_read > 0) {
            uint32_t len = samples_read * 2;
            if (format == SOAPY_SDR_CS16) {
                processSamples((int16_t*) buf, len);
            } else if (format == SOAPY_SDR_CF32) {
                processSamples((float*) buf, len);
            }
        } else if (samples_read == SOAPY_SDR_OVERFLOW) {
            // overflows do happen, they are non-fatal. a warning should do
            std::cerr << "WARNING: Soapy overflow" << std::endl;
        } else if (samples_read == SOAPY_SDR_TIMEOUT) {
            // timeout should not break the read loop.
            // TODO or should they? I tried, but airspyhf devices will end up here on sample rate changes.
            std::cerr << "WARNING: SoapySDR::Device::readStream timeout!" << std::endl;
        } else {
            // other errors should break the read loop.
            std::cerr << "ERROR: Soapy error " << samples_read << std::endl;
            break;
        }
    }

    dev->deactivateStream(stream);

    return 0;
};

int SoapyConnector::close() {
    try {
        std::lock_guard<std::mutex> lck(devMutex);
        if (dev == nullptr) return 0;
        auto old = dev;
        dev = nullptr;
        SoapySDR::Device::unmake(old);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
};

int SoapyConnector::set_center_frequency(double frequency) {
    try {
        std::lock_guard<std::mutex> lck(devMutex);
        if (dev == nullptr) return 0;
        dev->setFrequency(SOAPY_SDR_RX, channel, frequency);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
};

int SoapyConnector::set_sample_rate(double sample_rate) {
    try {
        std::lock_guard<std::mutex> lck(devMutex);
        if (dev == nullptr) return 0;
        dev->setSampleRate(SOAPY_SDR_RX, channel, sample_rate);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
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
        std::cerr << "WARNING: setting \"" << key << "\" failed: " << r << std::endl;
    }
}

int SoapyConnector::set_gain(GainSpec* gain) {
    std::lock_guard<std::mutex> lck(devMutex);
    if (dev == nullptr) return 0;
    SimpleGainSpec* simple_gain;
    MultiGainSpec* multi_gain;
    if (dynamic_cast<AutoGainSpec*>(gain) != nullptr) {
        try {
            dev->setGainMode(SOAPY_SDR_RX, channel, true);
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return 1;
        }
    } else if ((simple_gain = dynamic_cast<SimpleGainSpec*>(gain)) != nullptr) {
        try {
            dev->setGainMode(SOAPY_SDR_RX, channel, false);
            dev->setGain(SOAPY_SDR_RX, channel, simple_gain->getValue());
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
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
            std::cerr << e.what() << std::endl;
            return 1;
        }
    } else {
        std::cerr << "unsupported gain settings" << std::endl;
        return 100;
    }

    return 0;
};

int SoapyConnector::set_ppm(double ppm) {
    std::lock_guard<std::mutex> lck(devMutex);
    if (dev == nullptr) return 0;
    try {
#if defined(SOAPY_SDR_API_VERSION) && (SOAPY_SDR_API_VERSION >= 0x00060000)
        dev->setFrequencyCorrection(SOAPY_SDR_RX, channel, ppm);
#else
        dev->setFrequency(SOAPY_SDR_RX, channel, "CORR", ppm);
#endif
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
};

