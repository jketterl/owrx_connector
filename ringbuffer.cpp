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

template class Ringbuffer<float>;