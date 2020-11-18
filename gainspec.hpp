#pragma once

#include <string>

class GainSpec {
    public:
        virtual ~GainSpec() = default;
        static GainSpec* parse(std::string* input);
};

class AutoGainSpec: public GainSpec {
};

class SimpleGainSpec: public GainSpec {
    public:
        SimpleGainSpec(float value);
        float getValue();
    private:
        float value;
};