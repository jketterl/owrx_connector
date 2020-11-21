#include "soapy_connector.hpp"
#include <iostream>
#include <algorithm>

int main (int argc, char** argv) {
    Connector* connector = new SoapyConnector();
    return connector->main(argc, argv);
}

uint32_t SoapyConnector:: get_buffer_size() {
    return soapy_buffer_size;
};

int SoapyConnector:: open() {
    try {
        dev = SoapySDR::Device::make(device_id == nullptr ? "" : std::string(device_id));
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
};

int SoapyConnector:: read() {
    std::string format = SOAPY_SDR_CS16;
    std::vector<std::string> formats = dev->getStreamFormats(SOAPY_SDR_RX, channel);
    // use native CF32 if available
    if (std::find(formats.begin(), formats.end(), SOAPY_SDR_CF32) != formats.end()) {
        fprintf(stderr, "using soapy f32 conversion\n");
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
        fprintf(stderr, "Invalid channel %d selected\n", (int)channel);
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
        // fprintf(stderr, "samples read from sdr: %i\n", samples_read);

        if (samples_read >= 0) {
            uint32_t len = samples_read * 2;
            if (format == SOAPY_SDR_CS16) {
                processSamples((int16_t*) buf, len);
            } else if (format == SOAPY_SDR_CF32) {
                processSamples((float*) buf, len);
            }
        } else if (samples_read == SOAPY_SDR_OVERFLOW) {
            // overflows do happen, they are non-fatal. a warning should do
            fprintf(stderr, "WARNING: Soapy overflow\n");
        } else if (samples_read == SOAPY_SDR_TIMEOUT) {
            // timeout should not break the read loop.
            // TODO or should they? I tried, but airspyhf devices will end up here on sample rate changes.
            fprintf(stderr, "WARNING: SoapySDRDevice_readStream timeout!\n");
        } else {
            // other errors should break the read loop.
            fprintf(stderr, "ERROR: Soapy error %i\n", samples_read);
            break;
        }
    }

    dev->deactivateStream(stream);


    return 0;
};

int SoapyConnector:: close() {
    try {
        SoapySDR::Device::unmake(dev);
        dev = nullptr;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
};

int SoapyConnector:: set_center_frequency(double frequency) {
    try {
        dev->setFrequency(SOAPY_SDR_RX, channel, frequency);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
};

int SoapyConnector:: set_sample_rate(double sample_rate) {
    try {
        dev->setSampleRate(SOAPY_SDR_RX, channel, sample_rate);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
};

int SoapyConnector:: set_gain(GainSpec* gain) {
    SimpleGainSpec* simple_gain;
    if (dynamic_cast<AutoGainSpec*>(gain) != nullptr) {
        try {
            dev->setGainMode(SOAPY_SDR_RX, channel, true);
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
    } else if ((simple_gain = dynamic_cast<SimpleGainSpec*>(gain)) != nullptr) {
        try{
            dev->setGainMode(SOAPY_SDR_RX, channel, false);
            dev->setGain(SOAPY_SDR_RX, channel, simple_gain->getValue());
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
    // TODO: combined gain spec
    } else {
        std::cerr << "unsupported gain settings\n";
        return 100;
    }

    return 0;
};

int SoapyConnector:: set_ppm(int ppm) {
    try {
#if defined(SOAPY_SDR_API_VERSION) && (SOAPY_SDR_API_VERSION >= 0x00060000)
        dev->setFrequencyCorrection(SOAPY_SDR_RX, channel, ppm);
#else
        SoapySDRKwargs args = {0};
        dev->setFrequencyComponent(SOAPY_SDR_RX, channel, "CORR", ppm);
#endif
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
};

