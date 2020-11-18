#include "control_connection.hpp"
#include <cstdio>
#include <cstring>
#include <string>

ControlSocket::ControlSocket(Connector* new_connector, uint16_t port) {
    connector = new_connector;

    struct sockaddr_in local;
    const char* addr = "127.0.0.1";

    fprintf(stderr, "setting up control socket...\n");

    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = inet_addr(addr);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
    bind(sock, (struct sockaddr *)&local, sizeof(local));
    listen(sock, 1);

    fprintf(stderr, "control socket started on %i\n", port);

    thread = std::thread([this] { loop(); });
}

void ControlSocket::loop() {
    struct sockaddr_in remote;
    ssize_t read_bytes;

    while (run) {
        socklen_t rlen = sizeof(remote);
        int control_sock = accept(sock, (struct sockaddr *)&remote, &rlen);
        fprintf(stderr, "control connection established\n");

        bool run = true;
        uint8_t buf[256];

        while (run) {
            read_bytes = recv(control_sock, &buf, 256, 0);
            if (read_bytes <= 0) {
                run = false;
            } else {
                std::string message = std::string(reinterpret_cast<char const*>(&buf), read_bytes);
                size_t newline_pos;
                while ((newline_pos = message.find('\n')) != std::string::npos) {
                    std::string line = message.substr(0, newline_pos);
                    message = message.substr(newline_pos + 1);

                    size_t colon_pos = line.find(':');
                    if (colon_pos == std::string::npos) {
                        fprintf(stderr, "invalid message: \"%s\"\n", line.c_str());
                        continue;
                    }

                    std::string key = line.substr(0, colon_pos);
                    std::string value = line.substr(colon_pos + 1);

                    connector->applyChange(key, value);
                }
                /*
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
                            params->gain = (int)(atof(value) * 10); */ /* tenths of a dB */ /*
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
                */
            }
        }
        fprintf(stderr, "control connection ended\n");
    }
}