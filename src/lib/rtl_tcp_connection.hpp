#pragma once

#include "iq_connection.hpp"

#include <csdr/ringbuffer.hpp>

namespace Owrx {

    class RtlTcpSocket: public IQSocket<uint8_t> {
        public:
            RtlTcpSocket(uint16_t port, Csdr::Ringbuffer<uint8_t>* ringbuffer): IQSocket(port, ringbuffer) {};
        protected:
            void startNewConnection(int client_sock) override;
    };

    class RtlTcpConnection: public IQConnection<uint8_t> {
        public:
            RtlTcpConnection(int client_sock, Csdr::RingbufferReader<uint8_t>* reader): IQConnection(client_sock, reader) {};
        protected:
            void sendHeaders() override;
    };

}