#include "iq_connection.hpp"

#include <cstdio>
#include <cstring>

IQSocket::IQSocket(uint16_t port) {
    struct sockaddr_in local;
    char* addr = "127.0.0.1";

    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = inet_addr(addr);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
    bind(sock, (struct sockaddr *)&local, sizeof(local));

    fprintf(stderr, "socket setup complete, waiting for connections\n");

    listen(sock, 1);
}

void IQSocket::start() {
    thread = std::thread( [this] { accept_loop(); });
}

void IQSocket::accept_loop() {
    struct sockaddr_in remote;
    unsigned int rlen = sizeof(remote);

    while (run) {
        int client_sock = accept(sock, (struct sockaddr *)&remote, &rlen);

        if (client_sock >= 0) {
            IQConnection* worker = new IQConnection(client_sock);
            /*
            pthread_t client_worker_thread;
            int r = pthread_create(&client_worker_thread, NULL, client_worker, &client_sock);
            if (r != 0) {
                fprintf(stderr, "WARNING: could not create client worker thread: %i\n", r);
                continue;
            }
            r = pthread_detach(client_worker_thread);
            if (r != 0) {
                fprintf(stderr, "WARNING: could not detach client worker thread: %i\n", r);
                continue;
            }
            */
        }
    }
}

IQConnection::IQConnection(int client_sock) {
    sock = client_sock;
    thread = std::thread( [this] { loop(); });
    thread.detach();
}

void IQConnection::loop() {
    fprintf(stderr, "client connection established\n");
}