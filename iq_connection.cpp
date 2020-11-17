#include "iq_connection.hpp"

#include <cstdio>
#include <cstring>
#include <unistd.h>

IQSocket::IQSocket(uint16_t port, Ringbuffer<float>* new_ringbuffer) {
    ringbuffer = new_ringbuffer;

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
            new IQConnection(client_sock, ringbuffer);
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

IQConnection::IQConnection(int client_sock, Ringbuffer<float>* new_ringbuffer) {
    sock = client_sock;
    ringbuffer = new_ringbuffer;
    thread = std::thread( [this] { loop(); });
    thread.detach();
}

void IQConnection::loop() {
    fprintf(stderr, "client connection established\n");

    uint32_t read_pos = ringbuffer->get_write_pos();
    ssize_t sent;

    while (run) {
        ringbuffer->wait();
        //pthread_mutex_lock(&wait_mutex);
        //pthread_cond_wait(&wait_condition, &wait_mutex);
        //pthread_mutex_unlock(&wait_mutex);
        //if (rtltcp_compat && run && use_float) {
        //    read_bytes = recv(client_sock, &buf, 256, MSG_DONTWAIT | MSG_PEEK);
        //    if (read_bytes > 0) {
        //        fprintf(stderr, "unexpected data on socket; assuming rtl_tcp client, switching to u8 buffer\n");
        //        use_float = false;
        //        ringbuffer = ringbuffer_u8;
        //        sample_size = sizeof(uint8_t);
        //    }
        //}
        float* read_pointer;
        int available;
        while ((read_pointer = ringbuffer->get_read_pointer(read_pos)) != nullptr) {
            available = ringbuffer->available_bytes(read_pos);
            sent = send(sock, read_pointer, available * sizeof(float), MSG_NOSIGNAL);
            read_pos = (read_pos + available) % ringbuffer->get_length();
            if (sent <= 0) {
                run = false;
            }
        }
    }
    fprintf(stderr, "closing client socket\n");
    close(sock);
}