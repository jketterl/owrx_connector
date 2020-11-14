#include "connector.hpp"
#include <cstdio>

Connector::Connector(Handler* new_handler) {
    handler = new_handler;
}

int Connector::main(int argc, char** argv) {
    handler->setup_and_read();
    return 0;
}
