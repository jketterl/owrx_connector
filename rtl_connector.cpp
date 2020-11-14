#include "connector.hpp"
#include "rtl_handler.hpp"
#include <cstdio>

int main (int argc, char** argv) {
    RtlHandler* handler = new RtlHandler();
    Connector* connector = new Connector(handler);
    return connector->main(argc, argv);
}