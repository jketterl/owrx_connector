#include "iq_connection.hpp"
#include "shims.h"

#include <cstring>
#include <unistd.h>
#include <iostream>

using namespace Owrx;

template <typename T>
IQSocket<T>::IQSocket(uint16_t port, Csdr::Ringbuffer<T>* new_ringbuffer) {
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

    std::cout << "socket setup complete, waiting for connections" << std::endl;

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
            startNewConnection(client_sock);
        }
    }
}

template <typename T>
void IQSocket<T>::startNewConnection(int client_sock) {
    new IQConnection<T>(client_sock, new Csdr::RingbufferReader<T>(ringbuffer));
}

template <typename T>
IQConnection<T>::IQConnection(int client_sock, Csdr::RingbufferReader<T>* reader) {
    sock = client_sock;
    this->reader = reader;
    thread = std::thread( [this] {
        sendHeaders();
        loop();
        delete this;
    });
    thread.detach();
}

template <typename T>
void IQConnection<T>::sendHeaders() {
    // NOOP
}

template <typename T>
void IQConnection<T>::loop() {
    std::cout << "client connection established" << std::endl;

    ssize_t sent;

    while (run) {
        reader->wait();
        int available;
        while ((available = reader->available()) > 0) {
            sent = send(sock, reader->getReadPointer(), available * sizeof(T), MSG_NOSIGNAL);
            reader->advance(available);
            if (sent <= 0) {
                run = false;
            }
        }
    }
    std::cout << "closing client socket" << std::endl;
    close(sock);
}

namespace Owrx {
    template class IQSocket<float>;
    template class IQSocket<uint8_t>;

    template class IQConnection<float>;
    template class IQConnection<uint8_t>;
}

