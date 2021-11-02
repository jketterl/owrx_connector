#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <thread>
#include <csdr/ringbuffer.hpp>

namespace Owrx {

    template <typename T>
    class IQSocket {
        public:
            IQSocket<T>(uint16_t port, Csdr::Ringbuffer<T>* ringbuffer);
            virtual ~IQSocket() = default;
            void start();
        protected:
            Csdr::Ringbuffer<T>* ringbuffer;
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
            IQConnection(int client_sock, Csdr::RingbufferReader<T>* ringbuffer);
            virtual ~IQConnection() = default;
        protected:
            virtual void sendHeaders();
            int sock;
        private:
            std::thread thread;
            bool run = true;
            Csdr::RingbufferReader<T>* reader;

            void loop();
    };

}