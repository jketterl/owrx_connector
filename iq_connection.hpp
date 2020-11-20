#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <thread>
#include "ringbuffer.hpp"

template <typename T>
class IQSocket {
    public:
        IQSocket<T>(uint16_t port, Ringbuffer<T>* ringbuffer);
        void start();
    private:
        int sock;
        std::thread thread;
        bool run = true;
        Ringbuffer<T>* ringbuffer;

        void accept_loop();
};

template <typename T>
class IQConnection {
    public:
        IQConnection(int client_sock, Ringbuffer<T>* ringbuffer);
    private:
        int sock;
        std::thread thread;
        bool run = true;
        Ringbuffer<T>* ringbuffer;

        void loop();
};