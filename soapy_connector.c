#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <SoapySDR/Version.h>
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

#if SOAPY_SDR_API_VERSION < 0x00060000
#include <ctype.h>
#endif

static SoapySDRDevice* dev = NULL;
size_t channel = 0;

bool global_run = true;
bool iqswap = false;
bool rtltcp_compat = false;

#if SOAPY_SDR_API_VERSION < 0x00060000
char *trimwhitespace(char *str)
{
    char *end;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}



SoapySDRKwargs parseKwArgs(char* markup) {
    SoapySDRKwargs kwargs;
    kwargs.size = 0;
    kwargs.keys = (char **) malloc(0);
    kwargs.vals = (char **) malloc(0);

    bool inKey = true;
    char* key = (char*) malloc(sizeof(char) * 255);
    char* val = (char*) malloc(sizeof(char) * 255);
    key[0] = 0;
    val[0] = 0;
    for (size_t i = 0; i < strlen(markup); i++) {
        const char ch = markup[i];
        if (inKey){
            if (ch == '=') inKey = false;
            else if (ch == ',') inKey = true;
            else strncat(key, &ch, 1);
        } else {
            if (ch == ',') inKey = true;
            else strncat(val, &ch, 1);
        }
        if ((inKey and (strlen(val) > 0 or (ch == ','))) or ((i+1) == strlen(markup))) {
            char* key_trimmed = trimwhitespace(key);
            char* val_trimmed = trimwhitespace(val);
            char* key_copy = malloc(sizeof(char) * strlen(key_trimmed) + 1);
            strcpy(key_copy, key_trimmed);
            char* val_copy = malloc(sizeof(char) * strlen(val_trimmed) + 1);
            strcpy(val_copy, val_trimmed);
            if (strlen(key_copy) > 0) SoapySDRKwargs_set(&kwargs, key_copy, val_copy);
            key[0] = 0;
            val[0] = 0;
        }
    }
    free(key);
    free(val);

    return kwargs;
}
#endif

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

bool convertBooleanValue(char* value) {
    char* lower = strtolower(value);
    return strcmp(value, "1") == 0 || strcmp(lower, "true") == 0;
}

int verbose_gain_str_set(SoapySDRDevice *dev, char *gain_str, size_t channel)
{
    int r = 0;

    char* lower = strtolower(gain_str);
    bool agc = strcmp(lower, "auto") == 0 || strcmp(lower, "none") == 0;
    r = SoapySDRDevice_setGainMode(dev, SOAPY_SDR_RX, channel, agc);
    if (r != 0) {
        fprintf(stderr, "WARNING: setting agc failed\n");
    }

    // early exit when agc is on
    if (agc) return r;

    if (strchr(gain_str, '=')) {
        // Set each gain individually (more control)
#if defined(SOAPY_SDR_API_VERSION) && (SOAPY_SDR_API_VERSION >= 0x00060000)
        SoapySDRKwargs args = SoapySDRKwargs_fromString(gain_str);
#else
        SoapySDRKwargs args = parseKwArgs(gain_str);
#endif

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

void verbose_settings_set(SoapySDRDevice* dev, char* settings) {
#if defined(SOAPY_SDR_API_VERSION) && (SOAPY_SDR_API_VERSION >= 0x00060000)
    SoapySDRKwargs s = SoapySDRKwargs_fromString(settings);
#else
    SoapySDRKwargs s = parseKwArgs(settings);
#endif
    unsigned int i;
    for (i = 0; i < s.size; i++) {
        const char *key = s.keys[i];
        const char *value = s.vals[i];

#if defined(SOAPY_SDR_API_VERSION) && (SOAPY_SDR_API_VERSION >= 0x00060000)
        // return code has been added in soapy 0.6
        if(SoapySDRDevice_writeSetting(dev, key, value) != 0) {
            fprintf(stderr, "WARNING: key set failed: %s\n", SoapySDRDevice_lastError());
        }
#else
        // return type is void in older versions
        SoapySDRDevice_writeSetting(dev, key, value);
#endif
    }
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

OWRX_CONNECTOR_TARGET_CLONES
void convert_cs16_f(int16_t* restrict in, float* restrict out, uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; i++) {
        out[i] = (float)in[i] / SHRT_MAX;
    }
}

OWRX_CONNECTOR_TARGET_CLONES
void convert_cs16_u8(int16_t* restrict in, uint8_t* restrict out, uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; i++) {
        out[i] = in[i] / 32767.0 * 128.0 + 127.4;
    }
}

OWRX_CONNECTOR_TARGET_CLONES
void convert_cf32_u8(float* restrict in, uint8_t* restrict out, uint32_t count) {
    uint32_t i;
    for (i = 0; i < count; i++) {
        out[i] = in[i] * UCHAR_MAX * 0.5 + 128;
    }
}

int setup_and_read(soapy_connector_params* params, uint16_t* modified, pthread_mutex_t modification_mutex) {
    int r;

    r = verbose_device_search(params->device_id, &dev);
    if (r != 0) {
        return 1;
    }

    r = SoapySDRDevice_setAntenna(dev, SOAPY_SDR_RX, channel, params->antenna);
    if (r < 0) {
        fprintf(stderr, "setting antenna failed\n");
        return 8;
    }

    r = SoapySDRDevice_setSampleRate(dev, SOAPY_SDR_RX, channel, params->samp_rate);
    if (r < 0) {
        fprintf(stderr, "setting sample rate failed\n");
        return 3;
    }

    SoapySDRKwargs args = {0};
    r = SoapySDRDevice_setFrequency(dev, SOAPY_SDR_RX, channel, params->frequency, &args);
    if (r < 0) {
        fprintf(stderr, "setting frequency failed\n");
        return 4;
    }

    verbose_gain_str_set(dev, params->gain, channel);

    if (params->ppm != 0) {
#if defined(SOAPY_SDR_API_VERSION) && (SOAPY_SDR_API_VERSION >= 0x00060000)
        r = SoapySDRDevice_setFrequencyCorrection(dev, SOAPY_SDR_RX, channel, params->ppm);
#else
        SoapySDRKwargs args = {0};
        r = SoapySDRDevice_setFrequencyComponent(dev, SOAPY_SDR_RX, channel, "CORR", params->ppm, &args);
#endif
        if (r < 0) {
            fprintf(stderr, "setting ppm failed\n");
            return 7;
        }
    }

    if (strlen(params->settings) > 0) {
        verbose_settings_set(dev, params->settings);
    }


    char* format = SOAPY_SDR_CS16;
    size_t length;
    char** formats = SoapySDRDevice_getStreamFormats(dev, SOAPY_SDR_RX, channel, &length);
    // use native CF32 if available
    for (unsigned int i = 0; i < length; i++) {
        if(strcmp(formats[i], SOAPY_SDR_CF32) == 0) {
            fprintf(stderr, "using soapy f32 conversion\n");
            format = SOAPY_SDR_CF32;
        }
    }
    SoapySDRStream* stream = NULL;
    void* buf = malloc(soapy_buffer_size * SoapySDR_formatToSize(format));
    void* buffs[] = {buf};
    void* conversion_buffer = malloc(soapy_buffer_size * SoapySDR_formatToSize(format));
    int samples_read;
    long long timeNs = 0;
    long timeoutNs = 1E6;
    int flags = 0;
    uint32_t i;



    SoapySDRKwargs stream_args = {0};
    size_t num_channels = SoapySDRDevice_getNumChannels(dev, SOAPY_SDR_RX);
    if(((size_t) channel) >= num_channels){
        fprintf(stderr, "Invalid channel %d selected\n", (int)channel);
        return 9;
    }

#if SOAPY_SDR_API_VERSION < 0x00080000
    if (SoapySDRDevice_setupStream(dev, &stream, SOAPY_SDR_RX, format, &channel, 1, &stream_args) != 0) {
        fprintf(stderr, "SoapySDRDevice_setupStream failed: %s\n", SoapySDRDevice_lastError());
        return 10;
    }
#else
    stream = SoapySDRDevice_setupStream(dev, SOAPY_SDR_RX, format, &channel, 1, &stream_args);
    if (stream == NULL) {
        fprintf(stderr, "SoapySDRDevice_setupStream failed: %s\n", SoapySDRDevice_lastError());
        return 11;
    }
#endif
    SoapySDRDevice_activateStream(dev, stream, 0, 0, 0);

    while (global_run){
        samples_read = SoapySDRDevice_readStream(dev, stream, buffs, soapy_buffer_size, &flags, &timeNs, timeoutNs);
        // fprintf(stderr, "samples read from sdr: %i\n", samples_read);

        if (samples_read >= 0) {
            uint32_t len = samples_read * 2;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
            if (format == SOAPY_SDR_CS16) {
#pragma GCC diagnostic pop
                int16_t* source = (int16_t*) buf;
                if (iqswap) {
                    source = (int16_t*) conversion_buffer;
                    for (i = 0; i < len; i++) {
                        source[i] = ((int16_t *)buf)[i ^ 1];
                    }
                }
                if (write_pos + len <= ringbuffer_size) {
                    convert_cs16_f(source, ringbuffer_f + write_pos, len);
                    if (rtltcp_compat) {
                        convert_cs16_u8(source, ringbuffer_u8 + write_pos, len);
                    }
                } else {
                    uint32_t remaining = ringbuffer_size - write_pos;
                    convert_cs16_f(source, ringbuffer_f + write_pos, remaining);
                    convert_cs16_f(source + remaining, ringbuffer_f, len - remaining);
                    if (rtltcp_compat) {
                        convert_cs16_u8(source, ringbuffer_u8 + write_pos, remaining);
                        convert_cs16_u8(source + remaining, ringbuffer_u8, len - remaining);
                    }
                }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
            } else if (format == SOAPY_SDR_CF32) {
#pragma GCC diagnostic pop
                float* source = (float*) buf;
                if (iqswap) {
                    source = (float*) conversion_buffer;
                    for (i = 0; i < len; i++) {
                        source[i] = ((float *)buf)[i ^ 1];
                    }
                }
                if (write_pos + len <= ringbuffer_size) {
                    memcpy(ringbuffer_f + write_pos, source, len * 4);
                    if (rtltcp_compat) {
                        convert_cf32_u8(source, ringbuffer_u8 + write_pos, len);
                    }
                } else {
                    uint32_t remaining = ringbuffer_size - write_pos;
                    memcpy(ringbuffer_f + write_pos, source, remaining * 4);
                    memcpy(ringbuffer_f, source + remaining, (len - remaining) * 4);
                    if (rtltcp_compat) {
                        convert_cf32_u8(source, ringbuffer_u8 + write_pos, remaining);
                        convert_cf32_u8(source + remaining, ringbuffer_u8, len - remaining);
                    }
                }
            }
            write_pos = (write_pos + len) % ringbuffer_size;
            pthread_mutex_lock(&wait_mutex);
            pthread_cond_broadcast(&wait_condition);
            pthread_mutex_unlock(&wait_mutex);

            pthread_mutex_lock(&modification_mutex);
            if (*modified > 0) {
                int r = 0;
                // perform device modifications here
                // antenna comes first since sdrplay is switching tuners that way
                if (*modified & MODIFIED_ANTENNA) {
                    r = SoapySDRDevice_setAntenna(dev, SOAPY_SDR_RX, channel, params->antenna);
                    if (r != 0) fprintf(stderr, "WARNING: setting antanna failed: %i\n", r);
                }
                if (*modified & MODIFIED_SAMPLE_RATE) {
                    r = SoapySDRDevice_setSampleRate(dev, SOAPY_SDR_RX, channel, params->samp_rate);
                    if (r != 0) fprintf(stderr, "WARNING: setting sample rate failed: %i\n", r);
                }
                if (*modified & MODIFIED_FREQUENCY) {
                    SoapySDRKwargs args = {0};
                    r = SoapySDRDevice_setFrequency(dev, SOAPY_SDR_RX, channel, params->frequency, &args);
                    if (r != 0) fprintf(stderr, "WARNING: setting frequency failed: %i\n", r);
                }
                if (*modified & MODIFIED_PPM) {
#if defined(SOAPY_SDR_API_VERSION) && (SOAPY_SDR_API_VERSION >= 0x00060000)
                    r = SoapySDRDevice_setFrequencyCorrection(dev, SOAPY_SDR_RX, channel, params->ppm);
#else
                    SoapySDRKwargs args = {0};
                    r = SoapySDRDevice_setFrequencyComponent(dev, SOAPY_SDR_RX, channel, "CORR", params->ppm, &args);
#endif
                    if (r != 0) fprintf(stderr, "WARNING: setting ppm failed: %i\n", r);
                }
                if (*modified & MODIFIED_GAIN) {
                    r = verbose_gain_str_set(dev, params->gain, channel);
                    if (r != 0) fprintf(stderr, "WARNING: setting gain failed: %i\n", r);
                }
                if (*modified & MODIFIED_SETTINGS) {
                    verbose_settings_set(dev, params->settings);
                }
                *modified = 0;
            }
            pthread_mutex_unlock(&modification_mutex);
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

    SoapySDRDevice_deactivateStream(dev, stream, 0, 0);
    SoapySDRDevice_closeStream(dev, stream);
    SoapySDRDevice_unmake(dev);
    fprintf(stderr, "device closed\n");

    free(buf);
    free(conversion_buffer);

    return 0;
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
    int available;

    fprintf(stderr, "client connection established\n");
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
            available = ringbuffer_bytes(read_pos);
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

    return NULL;
}

void push_modification(uint16_t* modified, int new_modification, pthread_mutex_t mutex) {
    pthread_mutex_lock(&mutex);
    *modified |= new_modification;
    pthread_mutex_unlock(&mutex);
}

void* control_worker(void* p) {
    control_worker_args* args = (control_worker_args*) p;
    soapy_connector_params* params = args->params;
    int listen_sock = args->socket;
    uint16_t* modified = args->modified;
    pthread_mutex_t modification_mutex = args->modification_mutex;
    free(args);

    struct sockaddr_in remote;
    ssize_t read_bytes;

    while (global_run) {
        socklen_t rlen = sizeof(remote);
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
                    if (strcmp(key, "samp_rate") == 0) {
                        params->samp_rate = strtod(value, NULL);
                        push_modification(modified, MODIFIED_SAMPLE_RATE, modification_mutex);
                    } else if (strcmp(key, "center_freq") == 0) {
                        params->frequency = strtod(value, NULL);
                        push_modification(modified, MODIFIED_FREQUENCY, modification_mutex);
                    } else if (strcmp(key, "ppm") == 0) {
                        params->ppm = atoi(value);
                        push_modification(modified, MODIFIED_PPM, modification_mutex);
                    } else if (strcmp(key, "rf_gain") == 0) {
                        strcpy(params->gain, value);
                        push_modification(modified, MODIFIED_GAIN, modification_mutex);
                    } else if (strcmp(key, "antenna") == 0) {
                        strcpy(params->antenna, value);
                        push_modification(modified, MODIFIED_ANTENNA, modification_mutex);
                    } else if (strcmp(key, "settings") == 0) {
                        strcpy(params->settings, value);
                        push_modification(modified, MODIFIED_SETTINGS, modification_mutex);
                    } else if (strcmp(key, "iqswap") == 0) {
                        // this one can go straight through since it's not a device setting
                        iqswap = convertBooleanValue(value);
                    } else {
                        fprintf(stderr, "could not set unknown key: \"%s\"\n", key);
                    }
                    free(pair);

                    chunk = strtok_r(NULL, "\n", &chunk_token);
                }
                free(message);
            }
        }
        fprintf(stderr, "control connection ended\n");
    }

    return NULL;
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

    return NULL;
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
        " -g, --gain          set the gain level (default: 0; accepts 'auto' for agc)\n"
        " -c, --control       control socket port (default: disabled)\n"
        " -P, --ppm           set frequency correction ppm\n"
        " -a, --antenna       select antenna input\n"
        " -t, --settings      set sdr specific settings\n"
        " -r, --rtltcp        enable rtl_tcp compatibility mode\n",
        VERSION
    );
}

int main(int argc, char** argv) {
    int c;
    int port = 4950;
    int control_port = -1;

    soapy_connector_params* params = malloc(sizeof(soapy_connector_params));
    params->device_id = "0";
    params->frequency = 145000000;
    params->samp_rate = 2048000;
    params->gain = malloc(sizeof(char) * 255);
    strcpy(params->gain, "");
    params->ppm = 0;
    params->antenna = malloc(sizeof(char) * 255);
    strcpy(params->antenna, "");
    params->settings = malloc(sizeof(char) * 255);
    strcpy(params->settings, "");

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
        {"antenna", required_argument, NULL, 'a'},
        {"iqswap", no_argument, NULL, 'i'},
        {"settings", required_argument, NULL, 't'},
        {"rtltcp", no_argument, NULL, 'r'},
        { NULL, 0, NULL, 0 }
    };
    while ((c = getopt_long(argc, argv, "vhd:p:f:s:g:c:P:a:it:r", long_options, NULL)) != -1) {
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
                params->frequency = strtod(optarg, NULL);
                break;
            case 's':
                params->samp_rate = strtod(optarg, NULL);
                break;
            case 'g':
                strcpy(params->gain, optarg);
                break;
            case 'c':
                control_port = atoi(optarg);
                break;
            case 'P':
                params->ppm = atoi(optarg);
                break;
            case 'a':
                strcpy(params->antenna, optarg);
                break;
            case 'i':
                iqswap = true;
                break;
            case 't':
                strcpy(params->settings, optarg);
                break;
            case 'r':
                rtltcp_compat = true;
                break;
        }
    }

    if (rtltcp_compat) {
        ringbuffer_u8 = (uint8_t*) malloc(sizeof(uint8_t) * ringbuffer_size);
    }
    ringbuffer_f = (float*) malloc(sizeof(float) * ringbuffer_size);

    pthread_cond_init(&wait_condition, NULL);
    pthread_mutex_init(&wait_mutex, NULL);

    pthread_mutex_t modification_mutex;
    pthread_mutex_init(&modification_mutex, NULL);
    uint16_t modified = 0;

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

        fprintf(stderr, "control socket started on %i\n", port);

        listen(listen_sock, 1);

        control_worker_args* args = malloc(sizeof(control_worker_args));
        args->socket = listen_sock;
        args->params = params;
        args->modified = &modified;
        args->modification_mutex = modification_mutex;
        pthread_t control_worker_thread;
        pthread_create(&control_worker_thread, NULL, control_worker, args);
    }

    pthread_t iq_connection_worker_thread;
    pthread_create(&iq_connection_worker_thread, NULL, iq_connection_worker, &port);

    while (global_run) {
        int r = setup_and_read(params, &modified, modification_mutex);
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
