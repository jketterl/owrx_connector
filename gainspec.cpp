#include "gainspec.hpp"

GainSpec* GainSpec::parse(std::string* input) {
    return new SimpleGainSpec(stof(*input));
}

SimpleGainSpec::SimpleGainSpec(float new_value) {
    value = new_value;
}

float SimpleGainSpec::getValue() {
    return value;
}