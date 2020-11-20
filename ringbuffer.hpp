#pragma once
#pragma GCC visibility push(default)

#include <stdint.h>
#include <mutex>
#include <condition_variable>

template <typename T>
class Ringbuffer {
    public:
        Ringbuffer(uint32_t len);
        uint32_t get_write_pos();
        T* get_write_pointer();
        T* get_read_pointer(uint32_t read_pos);
        void advance(uint32_t how_much);
        uint32_t available_samples(uint32_t read_pos);
        uint32_t get_length();
        void wait();
    private:
        T* buffer;
        uint32_t len;
        uint32_t write_pos;
        std::mutex* iq_mutex;
        std::condition_variable* iq_condition;

        unsigned int mod(int n, int x);
};
