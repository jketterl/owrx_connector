#ifndef CONNECTOR_PARAMS_H
#define CONNECTOR_PARAMS_H

typedef struct {
    char* device_id;
    uint32_t frequency;
    uint32_t samp_rate;
    bool agc;
    int gain;
    int ppm;
    int directsampling;
    bool biastee;
} connector_params;

#endif
