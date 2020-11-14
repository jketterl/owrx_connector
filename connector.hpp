#pragma once

#include "handler.hpp"

class Connector {
    public:
        Connector(Handler* handler);
        int main(int argc, char** argv);
    private:
        Handler* handler;
};
