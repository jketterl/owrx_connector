#include "iq_connection.hpp"
#include "shims.h"

#include <cstring>
#include <unistd.h>
#include <iostream>

using namespace Owrx;

template <typename T>
IQSocket<T>::IQSocket(uint16_t port, Ringbuffer<T>* new_ringbuffer) {
    ringbuffer = new_ringbuffer;

    struct sockaddr_in local;
    const char* addr = "127.0.0.1";

    std::memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = inet_addr(addr);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
    bind(sock, (struct sockaddr *)&local, sizeof(local));

    std::cerr << "socket setup complete, waiting for connections\n";

    listen(sock, 1);
}

template <typename T>
void IQSocket<T>::start() {
    thread = std::thread( [this] { accept_loop(); });
}

template <typename T>
void IQSocket<T>::accept_loop() {
    struct sockaddr_in remote;
    unsigned int rlen = sizeof(remote);

    while (run) {
        int client_sock = accept(sock, (struct sockaddr *)&remote, &rlen);

        if (client_sock >= 0) {
            new IQConnection<T>(client_sock, ringbuffer);
        }
    }
}

template class IQSocket<float>;
template class IQSocket<uint8_t>;

template <typename T>
IQConnection<T>::IQConnection(int client_sock, Ringbuffer<T>* new_ringbuffer) {
    sock = client_sock;
    ringbuffer = new_ringbuffer;
    thread = std::thread( [this] { loop(); });
    thread.detach();
}

template <typename T>
void IQConnection<T>::loop() {
    std::cerr << "client connection established\n";

    uint32_t read_pos = ringbuffer->get_write_pos();
    ssize_t sent;

    T* read_pointer;
    while (run) {
        ringbuffer->wait();
        int available;
        while ((read_pointer = ringbuffer->get_read_pointer(read_pos)) != nullptr) {
            available = ringbuffer->available_samples(read_pos);
            sent = send(sock, read_pointer, available * sizeof(T), MSG_NOSIGNAL);
            read_pos = (read_pos + available) % ringbuffer->get_length();
            if (sent <= 0) {
                run = false;
            }
        }
    }
    std::cerr << "closing client socket\n";
    close(sock);
}

template class IQConnection<float>;
template class IQConnection<uint8_t>;