#ifndef CONTROL_WORKER_ARGS_H
#define CONTROL_WORKER_ARGS_H

#include <pthread.h>

typedef struct {
    void* params;
    int socket;
    pthread_mutex_t modification_mutex;
    uint16_t* modified;
} control_worker_args;

#define MODIFIED_SAMPLE_RATE 1
#define MODIFIED_FREQUENCY 2
#define MODIFIED_PPM 4
#define MODIFIED_GAIN 8
#define MODIFIED_ANTENNA 16
#define MODIFIED_SETTINGS 32

#endif