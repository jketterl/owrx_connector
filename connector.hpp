#pragma once

#include "handler.hpp"

class Connector {
    public:
        Connector(Handler* handler);
        int main(int argc, char** argv);
    private:
        Handler* handler;
        int get_arguments(int argc, char** argv);
        void print_usage(char* program);
        void print_version();
};
