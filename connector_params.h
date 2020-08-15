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
} rtl_connector_params;

typedef struct {
    char* device_id;
    double frequency;
    double samp_rate;
    char* gain;
    int ppm;
    char* antenna;
    char* settings;
} soapy_connector_params;

#endif
