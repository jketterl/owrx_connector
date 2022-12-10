#include "control_connection.hpp"
#include <cstring>
#include <string>
#include <iostream>

using namespace Owrx;

ControlSocket::ControlSocket(Connector* new_connector, uint16_t port) {
    connector = new_connector;

    struct sockaddr_in local;
    const char* addr = "127.0.0.1";

    std::cout << "setting up control socket...\n";

    std::memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = inet_addr(addr);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
    bind(sock, (struct sockaddr *)&local, sizeof(local));
    listen(sock, 1);

    std::cout << "control socket started on " << port << "\n";

    thread = std::thread([this] { loop(); });
}

void ControlSocket::loop() {
    struct sockaddr_in remote;
    ssize_t read_bytes;

    while (run) {
        socklen_t rlen = sizeof(remote);
        int control_sock = accept(sock, (struct sockaddr *)&remote, &rlen);
        std::cout << "control connection established\n";

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
                        std::cerr << "invalid message: \"" << line << "\"\n";
                        continue;
                    }

                    std::string key = line.substr(0, colon_pos);
                    std::string value = line.substr(colon_pos + 1);

                    connector->applyChange(key, value);
                }
            }
        }
        std::cout << "control connection ended\n";
    }
}