#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <thread>

class IQSocket {
    public:
        IQSocket(uint16_t port);
        void start();
    private:
        int sock;
        std::thread thread;
        bool run = true;

        void accept_loop();
};

class IQConnection {
    public:
        IQConnection(int client_sock);
    private:
        int sock;
        std::thread thread;
        bool run = true;

        void loop();
};