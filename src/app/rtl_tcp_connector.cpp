#include "rtl_tcp_connector.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <cstring>
#include <unistd.h>

int main (int argc, char** argv) {
    Connector* connector = new RtlTcpConnector();
    return connector->main(argc, argv);
}

uint32_t RtlTcpConnector::get_buffer_size() {
    return rtl_buffer_size;
}

int RtlTcpConnector::parse_arguments(int argc, char** argv) {
    int r = Connector::parse_arguments(argc, argv);
    if (r != 0) return r;

    if (argc - optind >= 2) {
        host = std::string(argv[optind]);
        port = (uint16_t) strtoul(argv[optind + 1], NULL, 10);
    } else if (optind < argc) {
        std::string argument = std::string(argv[optind]);
        size_t colon_pos = argument.find(':');
        if (colon_pos == std::string::npos) {
            host = argument;
        } else {
            host = argument.substr(0, colon_pos);
            port = stoi(argument.substr(colon_pos + 1));
        }
    }

    return 0;
}

int RtlTcpConnector::open() {
    struct hostent* hp = gethostbyname(host.c_str());
    if (hp == NULL) {
        std::cerr << "gethostbyname() failed\n";
        return 3;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "rtl_tcp socket creation error: " << sock << "\n";
        return 1;
    }

    struct sockaddr_in remote;

    std::memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    remote.sin_addr = *((struct in_addr *) hp->h_addr);

    if (connect(sock, (struct sockaddr *)&remote, sizeof(remote)) < 0) {
        fprintf(stderr, "rtl_tcp connection failed\n");
        return 2;
    }
    return 0;
}

int RtlTcpConnector::send_command(struct command cmd) {
    ssize_t len = sizeof(cmd);
    ssize_t sent = send(sock, &cmd, len, 0);
    return len == sent ? 0 : -1;
}

int RtlTcpConnector::read() {
    ssize_t bytes_read;
    uint8_t* buf = (uint8_t*) malloc(rtl_buffer_size);

    while (run) {
        bytes_read = recv(sock, buf, rtl_buffer_size, 0);
        if (bytes_read > 0) {
            processSamples(buf, bytes_read);
        } else {
            fprintf(stderr, "error or no data on rtl_tcp socket, closing connection\n");
            break;
        }
    }

    free(buf);

    return 0;
}

int RtlTcpConnector::close() {
    return ::close(sock);
}

/*
    // send initial configuration
    if (params->directsampling >= 0 && params->directsampling <= 2) {
        send_command(params->socket, (struct command) {0x09, htonl(params->directsampling)});
    }
    send_command(params->socket, (struct command) {0x0e, htonl((unsigned int) params->biastee)});
*/

int RtlTcpConnector::set_center_frequency(double frequency) {
    return send_command((struct command) {0x01, htonl(frequency)});
}

int RtlTcpConnector::set_sample_rate(double sample_rate) {
    return send_command((struct command) {0x02, htonl(sample_rate)});
}

int RtlTcpConnector::set_gain(GainSpec* gain) {
    if (dynamic_cast<AutoGainSpec*>(gain) != nullptr) {
        return send_command((struct command) {0x03, htonl(0)});
    }

    SimpleGainSpec* simple_gain;
    if ((simple_gain = dynamic_cast<SimpleGainSpec*>(gain)) != nullptr) {
        int r = send_command((struct command) {0x03, htonl(1)});
        if (r < 0) {
            std::cerr << "setting gain mode failed\n";
            return 2;
        }

        r = send_command((struct command) {0x04, htonl(simple_gain->getValue() * 10)});
        if (r < 0) {
            std::cerr << "setting gain failed\n";
            return 3;
        }

        return 0;
    }

    std::cerr << "unsupported gain settings\n";
    return 100;
}

int RtlTcpConnector::set_ppm(int ppm) {
    return send_command((struct command) {0x05, htonl(ppm)});
}

