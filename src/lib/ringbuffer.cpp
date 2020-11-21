#include "owrx/ringbuffer.hpp"
#include <cstdlib>

template <typename T>
Ringbuffer<T>::Ringbuffer(uint32_t new_len) {
    len = new_len;
    buffer = (T*) malloc(sizeof(T) * len);
    write_pos = 0;

    iq_mutex = new std::mutex();
    iq_condition = new std::condition_variable();
}

template <typename T>
uint32_t Ringbuffer<T>::get_write_pos() {
    return write_pos;
}

template <typename T>
T* Ringbuffer<T>::get_write_pointer() {
    return buffer + write_pos;
}

template <typename T>
uint32_t Ringbuffer<T>::get_writeable_samples() {
    return len - write_pos;
}

template <typename T>
uint32_t Ringbuffer<T>::get_writeable_samples(uint32_t requested) {
    uint32_t writeable = get_writeable_samples();
    return writeable > requested ? requested : writeable;
}

template <typename T>
T* Ringbuffer<T>::get_read_pointer(uint32_t read_pos) {
    if (read_pos == write_pos) return nullptr;
    return buffer + read_pos;
}

template <typename T>
void Ringbuffer<T>::advance(uint32_t how_much) {
    write_pos = (write_pos + how_much) % len;

    std::lock_guard<std::mutex> lk(*iq_mutex);
    iq_condition->notify_all();
}

template <typename T>
uint32_t Ringbuffer<T>::available_samples(uint32_t read_pos) {
    if (read_pos < write_pos) {
        return write_pos - read_pos;
    } else {
        return len - read_pos;
    }
}

// modulo that will respect the sign
template <typename T>
unsigned int Ringbuffer<T>::mod(int n, int x) {
    return ((n%x)+x)%x;
}

template <typename T>
uint32_t Ringbuffer<T>::get_length() {
    return len;
}

template <typename T>
void Ringbuffer<T>::wait() {
    std::unique_lock<std::mutex> lk(*iq_mutex);
    iq_condition->wait(lk);
}


template class Ringbuffer<float>;
template class Ringbuffer<uint8_t>;