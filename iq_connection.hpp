#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <thread>
#include "ringbuffer.hpp"

class IQSocket {
    public:
        IQSocket(uint16_t port, Ringbuffer<float>* ringbuffer);
        void start();
    private:
        int sock;
        std::thread thread;
        bool run = true;
        Ringbuffer<float>* ringbuffer;

        void accept_loop();
};

class IQConnection {
    public:
        IQConnection(int client_sock, Ringbuffer<float>* ringbuffer);
    private:
        int sock;
        std::thread thread;
        bool run = true;
        Ringbuffer<float>* ringbuffer;

        void loop();
};