#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
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

static SoapySDRDevice* dev = NULL;
size_t channel = 0;

bool global_run = true;

int verbose_device_search(char *s, SoapySDRDevice **devOut)
{
	SoapySDRDevice *dev = NULL;

	dev = SoapySDRDevice_makeStrArgs(s);
	if (!dev) {
		fprintf(stderr, "SoapySDRDevice_make failed\n");
		return -1;
	}

	*devOut = dev;
	return 0;
}

int verbose_gain_str_set(SoapySDRDevice *dev, char *gain_str, size_t channel)
{
	int r = 0;

	if (strchr(gain_str, '=')) {
		// Set each gain individually (more control)
		SoapySDRKwargs args = SoapySDRKwargs_fromString(gain_str);

		for (size_t i = 0; i < args.size; ++i) {
			const char *name = args.keys[i];
			double value = atof(args.vals[i]);

			fprintf(stderr, "Setting gain element %s: %f dB\n", name, value);
			r = SoapySDRDevice_setGainElement(dev, SOAPY_SDR_RX, channel, name, value);
			if (r != 0) {
				fprintf(stderr, "WARNING: setGainElement(%s, %f) failed: %s\n", name, value,  SoapySDRDevice_lastError());
			}
		}

		SoapySDRKwargs_clear(&args);
	} else {
		// Set overall gain and let SoapySDR distribute amongst components
		double value = atof(gain_str);
		r = SoapySDRDevice_setGain(dev, SOAPY_SDR_RX, channel, value);
		if (r != 0) {
			fprintf(stderr, "WARNING: Failed to set tuner gain: %s\n", SoapySDRDevice_lastError());
		} else {
			fprintf(stderr, "Tuner gain set to %0.2f dB.\n", value);
		}
		// TODO: read back and print each individual getGainElement()s
	}
	return r;
}


// this should be the default according to rtl-sdr.h
#define SOAPY_BUFFER_SIZE 64512 * 4
uint32_t soapy_buffer_size = SOAPY_BUFFER_SIZE;
// make the buffer a multiple of the soapy buffer size so we don't have to split writes / reads
uint32_t ringbuffer_size = 10 * SOAPY_BUFFER_SIZE;
uint8_t* ringbuffer_u8;
float* ringbuffer_f;
uint32_t write_pos = 0;

pthread_cond_t wait_condition;
pthread_mutex_t wait_mutex;

void* iq_worker(void* arg) {
    SoapySDRStream* stream = NULL;
    int16_t* buf = malloc(soapy_buffer_size * SoapySDR_formatToSize(SOAPY_SDR_CS16));
    void* buffs[] = {buf};
    int samples_read;
    long long timeNs = 0;
    long timeoutNs = 1E6;
    int flags = 0;
    int i;
    while (global_run) {
        SoapySDRKwargs stream_args = {0};
        size_t num_channels = SoapySDRDevice_getNumChannels(dev, SOAPY_SDR_RX);
        if(((size_t) channel) >= num_channels){
            fprintf(stderr, "Invalid channel %d selected\n", (int)channel);
            return -3;
        }
        #if SOAPY_SDR_API_VERSION < 0x00080000
        if (SoapySDRDevice_setupStream(dev, &stream, SOAPY_SDR_RX, SOAPY_SDR_CS16, &channel, 1, &stream_args) != 0) {
            fprintf(stderr, "SoapySDRDevice_setupStream failed: %s\n", SoapySDRDevice_lastError());
            return -3;
        }
        #else
        stream = SoapySDRDevice_setupStream(dev, SOAPY_SDR_RX, SOAPY_SDR_CS16, &channel, 1, &stream_args);
        if (stream == NULL) {
            fprintf(stderr, "SoapySDRDevice_setupStream failed: %s\n", SoapySDRDevice_lastError());
            return -3;
        }
        #endif
        SoapySDRDevice_activateStream(dev, stream, 0, 0, 0);

        while (global_run){
            samples_read = SoapySDRDevice_readStream(dev, stream, buffs, soapy_buffer_size, &flags, &timeNs, timeoutNs);
            //fprintf(stderr, "samples read from sdr: %i\n", samples_read);

            if (samples_read >= 0) {
                for (i = 0; i < samples_read * 2; i++) {
                    int w = (write_pos + i) % ringbuffer_size;
                    ringbuffer_u8[w] = ((int16_t)buf[i] / 32767.0 * 128.0 + 127.4);
                    ringbuffer_f[w] = (float) buf[i] / SHRT_MAX;
                }
                write_pos = (write_pos + samples_read * 2) % ringbuffer_size;
                pthread_mutex_lock(&wait_mutex);
                pthread_cond_broadcast(&wait_condition);
                pthread_mutex_unlock(&wait_mutex);
            } else {
                if (samples_read == SOAPY_SDR_OVERFLOW) {
                    fprintf(stderr, "WARNING: Soapy overflow\n");
                    continue;
                }
            }
        }

        SoapySDRDevice_deactivateStream(dev, stream, 0, 0);
        SoapySDRDevice_closeStream(dev, stream);
    }
    free(buf);
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
        if (run && use_float) {
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

void* control_worker(void* p) {
    int port = *(int*) p;
    struct sockaddr_in local, remote;
    char* addr = "127.0.0.1";
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
                        r = SoapySDRDevice_setSampleRate(dev, SOAPY_SDR_RX, channel, samp_rate);
                    } else if (strcmp(key, "center_freq") == 0) {
                        uint32_t frequency = (uint32_t)strtoul(value, NULL, 10);
                        SoapySDRKwargs args = {0};
                        r = SoapySDRDevice_setFrequency(dev, SOAPY_SDR_RX, channel, frequency, &args);
                    } else if (strcmp(key, "ppm") == 0) {
                        int ppm = atoi(value);
                        r = SoapySDRDevice_setFrequencyCorrection(dev, SOAPY_SDR_RX, channel, ppm);
                    } else if (strcmp(key, "rf_gain") == 0) {
                        r = verbose_gain_str_set(dev, value, channel);
                    } else if (strcmp(key, "antenna") == 0) {
                        r = SoapySDRDevice_setAntenna(dev, SOAPY_SDR_RX, channel, value);
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
}

void print_usage() {
    fprintf(stderr,
        "soapy_connector version %s\n\n"
        "Usage: soapy_connector [options]\n\n"
        "Available options:\n"
        " -h, --help          show this message\n"
        " -v, --version       print version and exit\n"
        " -d, --device        device index (default: 0)\n"
        " -p, --port          listen port (default: 4590)\n"
        " -f, --frequency     tune to specified frequency\n"
        " -s, --samplerate    use the specified samplerate\n"
        " -g, --gain          set the gain level (default: 30)\n"
        " -c, --control       control socket port (default: disabled)\n"
        " -P, --ppm           set frequency correction ppm\n"
        " -a, --antenna       select antenna input\n",
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
    uint32_t samp_rate = 2048000;
    char* gain = "";
    int ppm = 0;
    char* antenna = "";

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sighandler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    ringbuffer_u8 = (uint8_t*) malloc(sizeof(uint8_t) * ringbuffer_size);
    ringbuffer_f = (float*) malloc(sizeof(float) * ringbuffer_size);

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
        {"antenna", required_argument, NULL, 'a'},
        { NULL, 0, NULL, 0 }
    };
    while ((c = getopt_long(argc, argv, "vhd:p:f:s:g:c:P:a:", long_options, NULL)) != -1) {
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
                gain = optarg;
                break;
            case 'c':
                control_port = atoi(optarg);
                break;
            case 'P':
                ppm = atoi(optarg);
                break;
            case 'a':
                antenna = optarg;
                break;
        }
    }

    r = verbose_device_search(device_id, &dev);
    if (r != 0) {
        return 1;
    }

    r = SoapySDRDevice_setSampleRate(dev, SOAPY_SDR_RX, channel, samp_rate);
    if (r < 0) {
        fprintf(stderr, "setting sample rate failed\n");
        return 3;
    }

    SoapySDRKwargs args = {0};
    r = SoapySDRDevice_setFrequency(dev, SOAPY_SDR_RX, channel, frequency, &args);
    if (r < 0) {
        fprintf(stderr, "setting frequency failed\n");
        return 4;
    }

    verbose_gain_str_set(dev, gain, channel);

    if (ppm != 0) {
        r = SoapySDRDevice_setFrequencyCorrection(dev, SOAPY_SDR_RX, channel, ppm);
        if (r < 0) {
            fprintf(stderr, "setting ppm failed\n");
            return 7;
        }
    }

    r = SoapySDRDevice_setAntenna(dev, SOAPY_SDR_RX, channel, antenna);
    if (r < 0) {
        fprintf(stderr, "setting antenna failed\n");
        return 8;
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
