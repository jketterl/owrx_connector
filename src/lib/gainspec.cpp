#include "owrx/gainspec.hpp"
#include <algorithm>

GainSpec* GainSpec::parse(std::string* input) {
    std::string lower = *input;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });
    if (lower == "auto" || lower == "none") {
        return new AutoGainSpec();
    }
    return new SimpleGainSpec(stof(*input));
}

SimpleGainSpec::SimpleGainSpec(float new_value) {
    value = new_value;
}

float SimpleGainSpec::getValue() {
    return value;
}