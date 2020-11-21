#pragma once

#include "owrx/connector.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <stdint.h>

class ControlSocket {
    public:
        ControlSocket(Connector* connector, uint16_t port);
    private:
        Connector* connector;
        int sock;
        bool run = true;
        std::thread thread;

        void loop();
};