#include "ringbuffer.hpp"
#include <cstdlib>

template <typename T>
Ringbuffer<T>::Ringbuffer(uint32_t new_len) {
    len = new_len;
    buffer = (T*) malloc(sizeof(T) * len);
    write_pos = 0;
}

template <typename T>
T* Ringbuffer<T>::get_write_pointer() {
    return buffer + write_pos;
}

template <typename T>
void Ringbuffer<T>::advance(uint32_t how_much) {
    write_pos = (write_pos + how_much) % len;
}

template <typename T>
int Ringbuffer<T>::available_bytes(int read_pos) {
    return mod(write_pos - read_pos, len);
}

// modulo that will respect the sign
template <typename T>
unsigned int Ringbuffer<T>::mod(int n, int x) {
    return ((n%x)+x)%x;
}


template class Ringbuffer<float>;