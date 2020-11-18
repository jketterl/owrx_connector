#include "rtl_connector.hpp"
extern "C" {
#include "conversions.h"
}
#include <cstdio>
#include <cstring>
#include <stdlib.h>

uint32_t RtlConnector::get_buffer_size() {
    return rtl_buffer_size;
}

int RtlConnector::open() {
    int dev_index = verbose_device_search(device_id);

    if (dev_index < 0) {
        fprintf(stderr, "no device found.\n");
        return 1;
    }

    rtlsdr_open(&dev, (uint32_t)dev_index);
    if (NULL == dev) {
        fprintf(stderr, "device could not be opened\n");
        return 2;
    }

    return 0;
}

void rtlsdr_callback(unsigned char* buf, uint32_t len, void* ctx) {
    ((RtlConnector*) ctx)->callback(buf, len);
}

int RtlConnector::read() {
    uint32_t buf_num = 2;
    int r;

#if HAS_RTLSDR_SET_BIAS_TEE
    //r = rtlsdr_set_bias_tee(dev, (int) params->biastee);
    //if (r < 0) {
    //    fprintf(stderr, "setting biastee failed\n");
    //}
#endif

    //if (params->directsampling >= 0 && params->directsampling <= 2) {
    //    r = rtlsdr_set_direct_sampling(dev, params->directsampling);
    //    if (r < 0) {
    //        fprintf(stderr, "setting direct sampling mode failed\n");
    //    }
    //}

    r = rtlsdr_reset_buffer(dev);
    if (r < 0) {
        fprintf(stderr, "WARNING: Failed to reset buffers.\n");
    }

    r = rtlsdr_read_async(dev, rtlsdr_callback, this, buf_num, rtl_buffer_size);
    if (r != 0) {
        fprintf(stderr, "WARNING: rtlsdr_read_async failed with r = %i\n", r);
    }
    return r;
}

void RtlConnector::callback(unsigned char* buf, uint32_t len) {
    if (len != rtl_buffer_size) {
        fprintf(stderr, "WARNING: invalid buffer size received; skipping input\n");
        return;
    }
    uint8_t* source = (uint8_t*) buf;
    if (iqswap) {
        source = conversion_buffer;
        uint32_t i;
        for (i = 0; i < len; i++) {
            source[i] = buf[i ^ 1];
        }
    }
    convert_u8_f32(source, float_buffer->get_write_pointer(), len);
    //if (rtltcp_compat) {
    //    memcpy(ringbuffer_u8 + write_pos, source, len);
    //}

    float_buffer->advance(len);
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
		fprintf(stderr, "No supported devices found.\n");
		return -1;
	}
	fprintf(stderr, "Found %d device(s):\n", device_count);
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		fprintf(stderr, "  %d:  %s, %s, SN: %s\n", i, vendor, product, serial);
	}
	fprintf(stderr, "\n");
	/* if no device has been selected by the user, use the first one */
	if (s == nullptr) {
	    if (device_count > 0) return 0;
	    return -1;
	}
    /* does string look like raw id number */
    device = (int)strtol(s, &s2, 0);
	if (s2[0] == '\0' && device >= 0 && device < device_count) {
		fprintf(stderr, "Using device %d: %s\n",
			device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	/* does string exact match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (strcmp(s, serial) != 0) {
			continue;}
		device = i;
		fprintf(stderr, "Using device %d: %s\n",
			device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	/* does string prefix match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		if (strncmp(s, serial, strlen(s)) != 0) {
			continue;}
		device = i;
		fprintf(stderr, "Using device %d: %s\n",
			device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	/* does string suffix match a serial */
	for (i = 0; i < device_count; i++) {
		rtlsdr_get_device_usb_strings(i, vendor, product, serial);
		offset = strlen(serial) - strlen(s);
		if (offset < 0) {
			continue;}
		if (strncmp(s, serial+offset, strlen(s)) != 0) {
			continue;}
		device = i;
		fprintf(stderr, "Using device %d: %s\n",
			device, rtlsdr_get_device_name((uint32_t)device));
		return device;
	}
	fprintf(stderr, "No matching devices found.\n");
	return -1;
}

void RtlConnector::applyChange(std::string key, std::string value) {
    Connector::applyChange(key, value);
}

int RtlConnector::set_center_frequency(double frequency) {
    return rtlsdr_set_center_freq(dev, frequency);
}

int RtlConnector::set_sample_rate(double sample_rate) {
    return rtlsdr_set_sample_rate(dev, sample_rate);
}

int RtlConnector::set_gain(GainSpec* gain) {
    int r;
    SimpleGainSpec* simple_gain;
    if (dynamic_cast<AutoGainSpec*>(gain) != nullptr) {
        r = rtlsdr_set_tuner_gain_mode(dev, 0);
        if (r < 0) {
            fprintf(stderr, "setting gain mode failed\n");
            return 1;
        }
    } else if ((simple_gain = dynamic_cast<SimpleGainSpec*>(gain)) != nullptr) {
        r = rtlsdr_set_tuner_gain_mode(dev, 1);
        if (r < 0) {
            fprintf(stderr, "setting gain mode failed\n");
            return 2;
        }

        r = rtlsdr_set_tuner_gain(dev, (int)(simple_gain->getValue() * 10));
        if (r < 0) {
            fprintf(stderr, "setting gain failed\n");
            return 3;
        }
    } else {
        fprintf(stderr, "unsupported gain settings\n");
        return 100;
    }

    return 0;
}

int RtlConnector::set_ppm(int32_t ppm) {
    if (ppm == 0) {
        return 0;
    }

    return rtlsdr_set_freq_correction(dev, ppm);
};

int main (int argc, char** argv) {
    Connector* connector = new RtlConnector();
    connector->init_buffers();
    return connector->main(argc, argv);
}