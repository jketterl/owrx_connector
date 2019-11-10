#include <rtl-sdr.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "version.h"

static rtlsdr_dev_t *dev = NULL;

int verbose_device_search(char *s)
{
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

void print_usage() {
    fprintf(stderr,
        "rtl_connector version %s\n\n"
        "Usage: rtl_connector [options]\n\n"
        "Available options:\n"
        " -h, --help          show this message\n"
        " -v, --version       print version and exit\n"
        " -d, --device        device index (default: 0)\n"
        " -p, --port          listen port (default: 4590)\n"
        VERSION
    );
}

int ringbuffer_size = 1024 * 1024;
unsigned char* ringbuffer;
int write_pos = 0;

pthread_cond_t wait_condition;
pthread_mutex_t wait_mutex;

void rtlsdr_callback(unsigned char* buf, uint32_t len, void* ctx) {
    /*
    memcpy(&ringbuffer[write_pos], buf, len);
    write_pos = (write_pos + len) % ringbuffer_size;
    */
    int i;
    for (i = 0; i < len; i++) {
        ringbuffer[write_pos++] = buf[i];
        if (write_pos >= ringbuffer_size) write_pos = 0;
    }
    pthread_mutex_lock(&wait_mutex);
    pthread_cond_broadcast(&wait_condition);
    pthread_mutex_unlock(&wait_mutex);
}

void* iq_worker(void* arg) {
    bool run = true;
    uint32_t buf_num = 2;
    int r;
    while (run) {
        r = rtlsdr_read_async(dev, rtlsdr_callback, NULL, buf_num, 0);
        if (r != 0) {
            fprintf(stderr, "WARNING: rtlsdr_read_async failed with r = %i\n", r);
        }
    }
}

// modulo that will respect the sign
unsigned int mod(int n, int x) { return ((n%x)+x)%x; }

int ringbuffer_bytes(int read_pos) {
    return mod(write_pos - read_pos, ringbuffer_size);
}

void* client_worker(void* s) {
    bool run = true;
    int read_pos = write_pos;
    int client_sock = *(int*) s;
    ssize_t sent;
    int i;
    while (run) {
        pthread_mutex_lock(&wait_mutex);
        pthread_cond_wait(&wait_condition, &wait_mutex);
        pthread_mutex_unlock(&wait_mutex);
        while (read_pos != write_pos && run) {
            int available = ringbuffer_bytes(read_pos);
            //fprintf(stderr, "writing %i bytes to client... ", available);

            float output[available];
            for (int i = 0; i < available; i++) {
                output[i]=((float)ringbuffer[read_pos])/(UCHAR_MAX/2.0)-1.0; //@convert_u8_f
                read_pos = (read_pos + 1) % ringbuffer_size;
            }
            sent = send(client_sock, output, sizeof(output), MSG_NOSIGNAL);
            if (sent <= 0) {
                //fprintf(stderr, "send failed ");
                run = false;
            } else {
                //fprintf(stderr, "sent: %i ", sent);
            }
            //fprintf(stderr, "done\n");
        }
    }
    fprintf(stderr, "closing client socket\n");
    close(client_sock);
}

int main(int argc, char** argv) {
    int c;
    int r;
    int port = 4950;
    char* device_id = "0";
    int sock;
    struct sockaddr_in local, remote;
    char* addr = "127.0.0.1";
    uint32_t frequency = 145000000;
    uint32_t samp_rate = 2400000;
    int gain = 0;

    ringbuffer = (unsigned char*) malloc(sizeof(char) * ringbuffer_size);

    static struct option long_options[] = {
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {"device", required_argument, NULL, 'd'},
        {"port", required_argument, NULL, 'p'},
        {"frequency", required_argument, NULL, 'f'},
        {"samplerate", required_argument, NULL, 's'},
        {"gain", required_argument, NULL, 'g'},
        { NULL, 0, NULL, 0 }
    };
    while ((c = getopt_long(argc, argv, "vhd:p:f:s:g:", long_options, NULL)) != -1) {
        switch (c) {
            case 'v':
                print_version();
                return 0;
            case 'h':
                print_usage();
                return 0;
            case 'd':
                device_id = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'f':
                frequency = (uint32_t)strtoul(optarg, NULL, 10);
                break;
            case 's':
                samp_rate = (uint32_t)strtoul(optarg, NULL, 10);
                break;
            case 'g':
                gain = (int)(atof(optarg) * 10); /* tenths of a dB */
                break;
        }
    }
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

    r = rtlsdr_set_sample_rate(dev, samp_rate);
    if (r < 0) {
        fprintf(stderr, "setting sample rate failed\n");
        return 3;
    }

    r = rtlsdr_set_center_freq(dev, frequency);
    if (r < 0) {
        fprintf(stderr, "setting frequency failed\n");
        return 4;
    }

    r = rtlsdr_set_tuner_gain_mode(dev, 0);
    if (r < 0) {
        fprintf(stderr, "setting gain mode failed\n");
        return 4;
    }

    r = rtlsdr_set_tuner_gain(dev, gain);
    if (r < 0) {
        fprintf(stderr, "setting gain failed\n");
        return 4;
    }

    r = rtlsdr_reset_buffer(dev);
    if (r < 0) {
        fprintf(stderr, "WARNING: Failed to reset buffers.\n");
    }

    pthread_cond_init(&wait_condition, NULL);
    pthread_mutex_init(&wait_mutex, NULL);

    pthread_t iq_worker_thread;
    pthread_create(&iq_worker_thread, NULL, iq_worker, NULL);

    fprintf(stderr, "IQ worker thread started\n");

    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = inet_addr(addr);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
    bind(sock, (struct sockaddr *)&local, sizeof(local));

    fprintf(stderr, "socket setup complete, waiting for connections\n");

    listen(sock, 1);
    while (1) {
        int rlen = sizeof(remote);
        int client_sock = accept(sock, (struct sockaddr *)&remote, &rlen);
        fprintf(stderr, "we got a connection\n");

        pthread_t client_worker_thread;
        pthread_create(&client_worker_thread, NULL, client_worker, &client_sock);
    }
}