#include "rtl_handler.hpp"
extern "C" {
#include "conversions.h"
}
#include <cstdio>
#include <cstring>
#include <stdlib.h>

void RtlHandler::set_device(char* new_device) {
    device_id = new_device;
}

int RtlHandler::open() {
    uint32_t dev_index = verbose_device_search(device_id);

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
    ((RtlHandler*) ctx)->callback(buf, len);
}

int RtlHandler::read() {
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

void RtlHandler::callback(unsigned char* buf, uint32_t len) {
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

    //write_pos += len;
    //if (write_pos >= ringbuffer_size) write_pos = 0;
    //pthread_mutex_lock(&wait_mutex);
    //pthread_cond_broadcast(&wait_condition);
    //pthread_mutex_unlock(&wait_mutex);
}

int RtlHandler::close() {
    return rtlsdr_close(dev);
}

uint32_t RtlHandler::verbose_device_search(char *s) {
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

int RtlHandler::set_center_frequency(double frequency) {
    return rtlsdr_set_center_freq(dev, frequency);
}

int RtlHandler::set_sample_rate(double sample_rate) {
    return rtlsdr_set_sample_rate(dev, sample_rate);
}

// TODO introduce gainspec
int RtlHandler::set_gain() {
    //int gainmode = params->agc ? 0 : 1;
    //r = rtlsdr_set_tuner_gain_mode(dev, gainmode);
    //if (r < 0) {
    //    fprintf(stderr, "setting gain mode failed\n");
    //    return 5;
    //}

    //if (!params->agc) {
    //    r = rtlsdr_set_tuner_gain(dev, params->gain);
    //    if (r < 0) {
    //        fprintf(stderr, "setting gain failed\n");
    //        return 6;
    //    }
    //}

    return 0;
}

int RtlHandler::set_ppm(int32_t ppm) {
    if (ppm == 0) {
        return 0;
    }

    return rtlsdr_set_freq_correction(dev, ppm);
};

int RtlHandler::set_iqswap(bool new_iqswap) {
    iqswap = new_iqswap;
    return 0;
}

uint32_t RtlHandler::get_buffer_size() {
    return rtl_buffer_size;
}

void RtlHandler::set_buffers(Ringbuffer<float>* new_float_buffer) {
    float_buffer = new_float_buffer;
}
