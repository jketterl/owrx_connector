#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <stdint.h>

class ControlSocket {
    public:
        ControlSocket(uint16_t port);
    private:
        int sock;
        bool run = true;
        std::thread thread;

        void loop();
};