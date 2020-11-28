#pragma once

#include "iq_connection.hpp"

namespace Owrx {

    class RtlTcpSocket: public IQSocket<uint8_t> {
        public:
            RtlTcpSocket(uint16_t port, Ringbuffer<uint8_t>* ringbuffer): IQSocket(port, ringbuffer) {};
        protected:
            virtual void startNewConnection(int client_sock) override;
    };

    class RtlTcpConnection: public IQConnection<uint8_t> {
        public:
            RtlTcpConnection(int client_sock, Ringbuffer<uint8_t>* ringbuffer): IQConnection(client_sock, ringbuffer) {};
        protected:
            virtual void sendHeaders() override;
    };

}