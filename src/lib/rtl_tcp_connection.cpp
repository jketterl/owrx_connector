#include "rtl_tcp_connection.hpp"

#include <iostream>

using namespace Owrx;

void RtlTcpSocket::startNewConnection(int client_sock) {
    new RtlTcpConnection(client_sock, ringbuffer);
}

void RtlTcpConnection::sendHeaders() {
    std::cerr << "would send rtl_tcp headers now!\n";
}