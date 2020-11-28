#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <thread>
#include "ringbuffer.hpp"

namespace Owrx {

    template <typename T>
    class IQSocket {
        public:
            IQSocket<T>(uint16_t port, Ringbuffer<T>* ringbuffer);
            void start();
        protected:
            Ringbuffer<T>* ringbuffer;
            virtual void startNewConnection(int client_sock);
        private:
            int sock;
            std::thread thread;
            bool run = true;

            void accept_loop();
    };

    template <typename T>
    class IQConnection {
        public:
            IQConnection(int client_sock, Ringbuffer<T>* ringbuffer);
        protected:
            virtual void sendHeaders();
        private:
            int sock;
            std::thread thread;
            bool run = true;
            Ringbuffer<T>* ringbuffer;

            void loop();
    };

}