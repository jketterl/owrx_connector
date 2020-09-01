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
#include "version.h"
#include "fmv.h"
#include "connector_params.h"
#include "shims.h"
#include "control_worker_args.h"
#include "strtolower.h"

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

OWRX_CONNECTOR_TARGET_CLONES
void convert_u8_cf32(unsigned char* restrict in, float* out, uint32_t count) {
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

int setup_and_read(rtl_connector_params* params) {
    uint32_t buf_num = 2;
    int r;
    int dev_index = verbose_device_search(params->device_id);

    if (dev_index < 0) {
        fprintf(stderr, "no device found.\n");
        return 1;
    }

    rtlsdr_open(&dev, (uint32_t)dev_index);
    if (NULL == dev) {
        fprintf(stderr, "device could not be opened\n");
        return 2;
    }

    r = rtlsdr_set_sample_rate(dev, params->samp_rate);
    if (r < 0) {
        fprintf(stderr, "setting sample rate failed\n");
        return 3;
    }

    r = rtlsdr_set_center_freq(dev, params->frequency);
    if (r < 0) {
        fprintf(stderr, "setting frequency failed\n");
        return 4;
    }

    int gainmode = params->agc ? 0 : 1;
    r = rtlsdr_set_tuner_gain_mode(dev, gainmode);
    if (r < 0) {
        fprintf(stderr, "setting gain mode failed\n");
        return 5;
    }

    if (!params->agc) {
        r = rtlsdr_set_tuner_gain(dev, params->gain);
        if (r < 0) {
            fprintf(stderr, "setting gain failed\n");
            return 6;
        }
    }

    if (params->ppm != 0) {
        r = rtlsdr_set_freq_correction(dev, params->ppm);
        if (r < 0) {
            fprintf(stderr, "setting ppm failed\n");
            return 7;
        }
    }

#if HAS_RTLSDR_SET_BIAS_TEE
    r = rtlsdr_set_bias_tee(dev, (int) params->biastee);
    if (r < 0) {
        fprintf(stderr, "setting biastee failed\n");
    }
#endif

    if (params->directsampling >= 0 && params->directsampling <= 2) {
        r = rtlsdr_set_direct_sampling(dev, params->directsampling);
        if (r < 0) {
            fprintf(stderr, "setting direct sampling mode failed\n");
        }
    }

    r = rtlsdr_reset_buffer(dev);
    if (r < 0) {
        fprintf(stderr, "WARNING: Failed to reset buffers.\n");
    }

    r = rtlsdr_read_async(dev, rtlsdr_callback, NULL, buf_num, rtl_buffer_size);
    if (r != 0) {
        fprintf(stderr, "WARNING: rtlsdr_read_async failed with r = %i\n", r);
    }

    r = rtlsdr_close(dev);
    if (r != 0) {
        fprintf(stderr, "WARNING: could not close device");
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

bool convertBooleanValue(char* value) {
    char* lower = strtolower(value);
    return strcmp(value, "1") == 0 || strcmp(lower, "true") == 0;
}

void* control_worker(void* p) {
    control_worker_args* args = (control_worker_args*) p;
    rtl_connector_params* params = args->params;
    int listen_sock = args->socket;
    free(args);
    struct sockaddr_in remote;
    ssize_t read_bytes;

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
                        params->samp_rate = (uint32_t)strtoul(value, NULL, 10);
                        r = rtlsdr_set_sample_rate(dev, params->samp_rate);
                    } else if (strcmp(key, "center_freq") == 0) {
                        params->frequency = (uint32_t)strtoul(value, NULL, 10);
                        r = rtlsdr_set_center_freq(dev, params->frequency);
                    } else if (strcmp(key, "ppm") == 0) {
                        params->ppm = atoi(value);
                        r = rtlsdr_set_freq_correction(dev, params->ppm);
                    } else if (strcmp(key, "rf_gain") == 0) {
                        char* lower = strtolower(value);
                        if (strcmp(lower, "auto") == 0 || strcmp(lower, "none") == 0) {
                            params->agc = true;
                            rtlsdr_set_tuner_gain_mode(dev, 0);
                        } else {
                            params->gain = (int)(atof(value) * 10); /* tenths of a dB */
                            r = rtlsdr_set_tuner_gain(dev, params->gain);
                        }
                    } else if (strcmp(key, "iqswap") == 0) {
                        iqswap = convertBooleanValue(value);
#if HAS_RTLSDR_SET_BIAS_TEE
                    } else if (strcmp(key, "bias_tee") == 0) {
                        params->biastee = convertBooleanValue(value);
                        r = rtlsdr_set_bias_tee(dev, (int) params->biastee);
#endif
                    } else if (strcmp(key, "direct_sampling") == 0) {
                        params->directsampling = (int)strtol(value, NULL, 10);
                        r = rtlsdr_set_direct_sampling(dev, params->directsampling);
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

void* iq_connection_worker(void* p) {
    int port = *(int*) p;
    int sock;
    struct sockaddr_in local, remote;
    char* addr = "127.0.0.1";

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
        unsigned int rlen = sizeof(remote);
        int client_sock = accept(sock, (struct sockaddr *)&remote, &rlen);

        if (client_sock >= 0) {
            pthread_t client_worker_thread;
            int r = pthread_create(&client_worker_thread, NULL, client_worker, &client_sock);
            if (r != 0) {
                fprintf(stderr, "WARNING: could not create client worker thread: %i\n", r);
		continue;
            }
            r = pthread_detach(client_worker_thread);
            if (r != 0) {
                fprintf(stderr, "WARNING: could not detach client worker thread: %i\n", r);
                continue;
            }
        }
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
#if HAS_RTLSDR_SET_BIAS_TEE
        " -b, --biastee           enable bias-tee voltage if supported by hardware\n"
#endif
        " -e, --directsampling    enable direct sampling on the specified input\n"
        "                         (0 = disabled, 1 = I-input, 2 = Q-input)\n",
        VERSION
    );
}

int main(int argc, char** argv) {
    int c;
    int port = 4950;
    int control_port = -1;

    rtl_connector_params* params = malloc(sizeof(rtl_connector_params));
    params->device_id = "0";
    params->frequency = 145000000;
    params->samp_rate = 2400000;
    params->agc = false;
    params->gain = 0;
    params->ppm = 0;
    params->directsampling = -1;
    params->biastee = false;

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
#if HAS_RTLSDR_SET_BIAS_TEE
        {"biastee", no_argument, NULL, 'b'},
#endif
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
                params->device_id = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'f':
                params->frequency = (uint32_t)strtoul(optarg, NULL, 10);
                break;
            case 's':
                params->samp_rate = (uint32_t)strtoul(optarg, NULL, 10);
                break;
            case 'g':
                if (strcmp(strtolower(optarg), "auto") == 0) {
                    params->agc = true;
                } else {
                    params->gain = (int)(atof(optarg) * 10); /* tenths of a dB */
                }
                break;
            case 'c':
                control_port = atoi(optarg);
                break;
            case 'P':
                params->ppm = atoi(optarg);
                break;
            case 'i':
                iqswap = true;
                break;
            case 'r':
                rtltcp_compat = true;
                break;
#if HAS_RTLSDR_SET_BIAS_TEE
            case 'b':
                params->biastee = true;
                break;
#endif
            case 'e':
                params->directsampling = (int)strtol(optarg, NULL, 10);
                break;
        }
    }

    if (rtltcp_compat) {
        ringbuffer_u8 = (uint8_t*) malloc(sizeof(uint8_t) * ringbuffer_size);
    }
    ringbuffer_f = (float*) malloc(sizeof(float) * ringbuffer_size);
    conversion_buffer = (unsigned char*) malloc(sizeof(unsigned char) * rtl_buffer_size);

    pthread_cond_init(&wait_condition, NULL);
    pthread_mutex_init(&wait_mutex, NULL);

    if (control_port > 0) {
        struct sockaddr_in local;
        char* addr = "127.0.0.1";

        fprintf(stderr, "setting up control socket...\n");

        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons(control_port);
        local.sin_addr.s_addr = inet_addr(addr);

        int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
        bind(listen_sock, (struct sockaddr *)&local, sizeof(local));
        listen(listen_sock, 1);

        fprintf(stderr, "control socket started on %i\n", port);

        control_worker_args* args = malloc(sizeof(control_worker_args));
        args->socket = listen_sock;
        args->params = params;
        pthread_t control_worker_thread;
        pthread_create(&control_worker_thread, NULL, control_worker, args);
    }

    pthread_t iq_connection_worker_thread;
    pthread_create(&iq_connection_worker_thread, NULL, iq_connection_worker, &port);

    while (global_run) {
        int r = setup_and_read(params);
        if (r != 0) {
            global_run = false;
        } else if (global_run) {
            // give the system 5 seconds to recover
            sleep(5);
        }
    }

    global_run = false;
    void* retval = NULL;

    pthread_kill(iq_connection_worker_thread, SIGINT);
    pthread_join(iq_connection_worker_thread, retval);
}
