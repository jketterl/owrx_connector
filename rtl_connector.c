#include <rtl-sdr.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include "version.h"

static rtlsdr_dev_t* dev = NULL;

bool global_run = true;
bool iqswap = false;
bool rtltcp_compat = false;

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

// this should be the default according to rtl-sdr.h
#define RTL_BUFFER_SIZE 16 * 32 * 512
uint32_t rtl_buffer_size = RTL_BUFFER_SIZE;
// make the buffer a multiple of the rtl buffer size so we don't have to split writes / reads
uint32_t ringbuffer_size = 10 * RTL_BUFFER_SIZE;
uint8_t* ringbuffer_u8;
unsigned char* conversion_buffer;
float* ringbuffer_f;
uint32_t write_pos = 0;

pthread_cond_t wait_condition;
pthread_mutex_t wait_mutex;

void convert_u8_cf32(unsigned char* in, float* out, uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; i++) {
        out[i] = ((float)in[i])/(UCHAR_MAX/2.0)-1.0;
    }
}

void rtlsdr_callback(unsigned char* buf, uint32_t len, void* ctx) {
    if (len != rtl_buffer_size) {
        fprintf(stderr, "WARNING: invalid buffer size received; skipping input\n");
        return;
    }
    unsigned char* source = buf;
    if (iqswap) {
        source = conversion_buffer;
        uint32_t i;
        for (i = 0; i < len; i++) {
            source[i] = buf[i ^ 1];
        }
    }
    convert_u8_cf32(source, ringbuffer_f + write_pos, len);
    if (rtltcp_compat) {
        memcpy(ringbuffer_u8 + write_pos, source, len);
    }

    write_pos += len;
    if (write_pos >= ringbuffer_size) write_pos = 0;
    pthread_mutex_lock(&wait_mutex);
    pthread_cond_broadcast(&wait_condition);
    pthread_mutex_unlock(&wait_mutex);
}

void* iq_worker(void* arg) {
    bool run = true;
    uint32_t buf_num = 2;
    int r;
    while (run && global_run) {
        r = rtlsdr_read_async(dev, rtlsdr_callback, NULL, buf_num, rtl_buffer_size);
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
    uint32_t read_pos = write_pos;
    int client_sock = *(int*) s;
    ssize_t sent;
    uint8_t buf[256];
    ssize_t read_bytes;
    bool use_float = true;
    void* ringbuffer = ringbuffer_f;
    int sample_size = sizeof(float);

    fprintf(stderr, "client connection establised\n");
    while (run && global_run) {
        pthread_mutex_lock(&wait_mutex);
        pthread_cond_wait(&wait_condition, &wait_mutex);
        pthread_mutex_unlock(&wait_mutex);
        if (rtltcp_compat && run && use_float) {
            read_bytes = recv(client_sock, &buf, 256, MSG_DONTWAIT | MSG_PEEK);
            if (read_bytes > 0) {
                fprintf(stderr, "unexpected data on socket; assuming rtl_tcp client, switching to u8 buffer\n");
                use_float = false;
                ringbuffer = ringbuffer_u8;
                sample_size = sizeof(uint8_t);
            }
        }
        while (read_pos != write_pos && run && global_run) {
            int available = ringbuffer_bytes(read_pos);
            if (read_pos < write_pos) {
                sent = send(client_sock, ringbuffer + read_pos * sample_size, available * sample_size, MSG_NOSIGNAL);
                read_pos = (read_pos + available) % ringbuffer_size;
            } else {
                sent  = send(client_sock, ringbuffer + read_pos * sample_size, (ringbuffer_size - read_pos) * sample_size, MSG_NOSIGNAL);
                read_pos = write_pos;
                sent += send(client_sock, ringbuffer, read_pos * sample_size, MSG_NOSIGNAL);
            }
            if (sent <= 0) {
                run = false;
            }
        }
    }
    fprintf(stderr, "closing client socket\n");
    close(client_sock);
}

char* strtolower(char* input) {
    int i, s = strlen(input);
    char* lower = malloc(sizeof(char) * (s + 1));
    for (i = 0; i < s; i++) {
        lower[i] = tolower(input[i]);
    }
    lower[s] = 0;
    return lower;
}

bool convertBooleanValue(char* value) {
    char* lower = strtolower(value);
    return strcmp(value, "1") == 0 || strcmp(lower, "true") == 0;
}

void* control_worker(void* p) {
    int port = *(int*) p;
    struct sockaddr_in local, remote;
    char* addr = "0.0.0.0";
    ssize_t read_bytes;

    fprintf(stderr, "setting up control socket...\n");

    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = inet_addr(addr);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
    bind(listen_sock, (struct sockaddr *)&local, sizeof(local));

    fprintf(stderr, "control socket started on %i\n", port);

    listen(listen_sock, 1);
    while (global_run) {
        int rlen = sizeof(remote);
        int sock = accept(listen_sock, (struct sockaddr *)&remote, &rlen);
        fprintf(stderr, "control connection established\n");

        bool run = true;
        uint8_t buf[256];

        while (run && global_run) {
            read_bytes = recv(sock, &buf, 256, 0);
            if (read_bytes <= 0) {
                run = false;
            } else {
                char* message = malloc(sizeof(char) * (read_bytes + 1));
                // need to make copies since strtok() modifies the original
                memcpy(message, buf, read_bytes);
                message[read_bytes] = '\0';

                char* chunk_token;
                char* chunk = strtok_r(message, "\n", &chunk_token);
                while (chunk != NULL) {
                    // need to make copies since strtok() modifies the original
                    char* pair = (char *) malloc(sizeof(char) * (strlen(chunk) + 1));
                    strcpy(pair, chunk);
                    char* pair_token;
                    char* key = strtok_r(pair, ":", &pair_token);
                    char* value = strtok_r(NULL, ":", &pair_token);
                    int r = 0;
                    if (strcmp(key, "samp_rate") == 0) {
                        uint32_t samp_rate = (uint32_t)strtoul(value, NULL, 10);
                        r = rtlsdr_set_sample_rate(dev, samp_rate);
                    } else if (strcmp(key, "center_freq") == 0) {
                        uint32_t frequency = (uint32_t)strtoul(value, NULL, 10);
                        r = rtlsdr_set_center_freq(dev, frequency);
                    } else if (strcmp(key, "ppm") == 0) {
                        int ppm = atoi(value);
                        r = rtlsdr_set_freq_correction(dev, ppm);
                    } else if (strcmp(key, "rf_gain") == 0) {
                        char* lower = strtolower(value);
                        if (strcmp(lower, "auto") == 0 || strcmp(lower, "none") == 0) {
                            rtlsdr_set_tuner_gain_mode(dev, 0);
                        } else {
                            int gain = (int)(atof(value) * 10); /* tenths of a dB */
                            r = rtlsdr_set_tuner_gain(dev, gain);
                        }
                    } else if (strcmp(key, "iqswap") == 0) {
                        iqswap = convertBooleanValue(value);
                    } else if (strcmp(key, "bias_tee") == 0) {
                        r = rtlsdr_set_bias_tee(dev, (int) convertBooleanValue(value));
                    } else if (strcmp(key, "direct_sampling") == 0) {
                        r = rtlsdr_set_direct_sampling(dev, (int)strtol(value, NULL, 10));
                    } else {
                        fprintf(stderr, "could not set unknown key: \"%s\"\n", key);
                    }
                    if (r != 0) {
                        fprintf(stderr, "WARNING: setting %s failed: %i\n", key, r);
                    }
                    free(pair);

                    chunk = strtok_r(NULL, "\n", &chunk_token);
                }
                free(message);
            }
        }
        fprintf(stderr, "control connection ended\n");
    }
}

void sighandler(int signo) {
    fprintf(stderr, "signal %i caught\n", signo);
    global_run = false;
    rtlsdr_cancel_async(dev);
}

void print_usage() {
    fprintf(stderr,
        "rtl_connector version %s\n\n"
        "Usage: rtl_connector [options]\n\n"
        "Available options:\n"
        " -h, --help              show this message\n"
        " -v, --version           print version and exit\n"
        " -d, --device            device index (default: 0)\n"
        " -p, --port              listen port (default: 4590)\n"
        " -f, --frequency         tune to specified frequency\n"
        " -s, --samplerate        use the specified samplerate\n"
        " -g, --gain              set the gain level (default: 0; accepts 'auto' for agc)\n"
        " -c, --control           control socket port (default: disabled)\n"
        " -P, --ppm               set frequency correction ppm\n"
        " -r, --rtltcp            enable rtl_tcp compatibility mode\n"
        " -b, --biastee           enable bias-tee voltage if supported by hardware\n"
        " -e, --directsampling    enable direct sampling on the specified input\n"
        "                         (0 = disabled, 1 = I-input, 2 = Q-input)\n",
        VERSION
    );
}

int main(int argc, char** argv) {
    int c;
    int r;
    int port = 4950;
    int control_port = -1;
    char* device_id = "0";
    int sock;
    struct sockaddr_in local, remote;
    char* addr = "127.0.0.1";
    uint32_t frequency = 145000000;
    uint32_t samp_rate = 2400000;
    bool agc = false;
    int gain = 0;
    int ppm = 0;
    bool biastee = false;
    int directsampling = -1;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sighandler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

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
        {"biastee", no_argument, NULL, 'b'},
        {"directsampling", required_argument, NULL, 'e'},
        { NULL, 0, NULL, 0 }
    };
    while ((c = getopt_long(argc, argv, "vhd:p:f:s:g:c:P:irbe:", long_options, NULL)) != -1) {
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
                if (strcmp(strtolower(optarg), "auto") == 0) {
                    agc = true;
                } else {
                    gain = (int)(atof(optarg) * 10); /* tenths of a dB */
                }
                break;
            case 'c':
                control_port = atoi(optarg);
                break;
            case 'P':
                ppm = atoi(optarg);
                break;
            case 'i':
                iqswap = true;
                break;
            case 'r':
                rtltcp_compat = true;
                break;
            case 'b':
                biastee = true;
                break;
            case 'e':
                directsampling = (int)strtol(optarg, NULL, 10);
                break;
        }
    }

    if (rtltcp_compat) {
        ringbuffer_u8 = (uint8_t*) malloc(sizeof(uint8_t) * ringbuffer_size);
    }
    ringbuffer_f = (float*) malloc(sizeof(float) * ringbuffer_size);
    conversion_buffer = (unsigned char*) malloc(sizeof(unsigned char) * rtl_buffer_size);

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

    int gainmode = agc ? 0 : 1;
    r = rtlsdr_set_tuner_gain_mode(dev, gainmode);
    if (r < 0) {
        fprintf(stderr, "setting gain mode failed\n");
        return 5;
    }

    if (!agc) {
        r = rtlsdr_set_tuner_gain(dev, gain);
        if (r < 0) {
            fprintf(stderr, "setting gain failed\n");
            return 6;
        }
    }

    if (ppm != 0) {
        r = rtlsdr_set_freq_correction(dev, ppm);
        if (r < 0) {
            fprintf(stderr, "setting ppm failed\n");
            return 7;
        }
    }

    r = rtlsdr_set_bias_tee(dev, (int) biastee);
    if (r < 0) {
        fprintf(stderr, "setting biastee failed\n");
    }

    if (directsampling >= 0 && directsampling <= 2) {
        r = rtlsdr_set_direct_sampling(dev, directsampling);
        if (r < 0) {
            fprintf(stderr, "setting direct sampling mode failed\n");
        }
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

    if (control_port > 0) {
        pthread_t control_worker_thread;
        pthread_create(&control_worker_thread, NULL, control_worker, &control_port);
    }

    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = inet_addr(addr);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
    bind(sock, (struct sockaddr *)&local, sizeof(local));

    fprintf(stderr, "socket setup complete, waiting for connections\n");

    listen(sock, 1);
    while (global_run) {
        int rlen = sizeof(remote);
        int client_sock = accept(sock, (struct sockaddr *)&remote, &rlen);

        if (client_sock >= 0) {
            pthread_t client_worker_thread;
            pthread_create(&client_worker_thread, NULL, client_worker, &client_sock);
        }
    }
}
