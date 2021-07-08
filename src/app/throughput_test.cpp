#include "rtl_tcp_connector.hpp"

#define BUFFER_SIZE 1024

int main (int argc, char** argv) {
    Connector* connector = new RtlTcpConnector();

    uint8_t* input = (uint8_t*) malloc(sizeof(uint8_t) * BUFFER_SIZE);
    float* output = (float*) malloc(sizeof(float) * BUFFER_SIZE);

    FILE* file = fopen("/dev/urandom", "r");

    for (int i = 0; i < 1000000; i++) {
        fread(input, sizeof(uint8_t), BUFFER_SIZE, file);
        connector->convert(input, output, BUFFER_SIZE);
    }

}
