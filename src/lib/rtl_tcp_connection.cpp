#include "rtl_tcp_connection.hpp"

#include <cstring>

using namespace Owrx;

typedef struct { /* structure size must be multiple of 2 bytes */
	char magic[4];
	uint32_t tuner_type;
	uint32_t tuner_gain_count;
} dongle_info_t;

void RtlTcpSocket::startNewConnection(int client_sock) {
    new RtlTcpConnection(client_sock, ringbuffer);
}

void RtlTcpConnection::sendHeaders() {
    dongle_info_t dongle_info;
    memcpy(&dongle_info.magic, "RTL0", 4);
    dongle_info.tuner_type = 0; // unknown
    dongle_info.tuner_gain_count = 0;
    send(sock, (const char *)&dongle_info, sizeof(dongle_info), MSG_NOSIGNAL);
}