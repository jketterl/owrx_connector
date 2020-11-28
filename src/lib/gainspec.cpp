#include "owrx/gainspec.hpp"
#include "owrx/connector.hpp"
#include <algorithm>
#include <stdexcept>

using namespace Owrx;

GainSpec* GainSpec::parse(std::string* input) {
    std::string lower = *input;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });
    if (lower == "auto" || lower == "none") {
        return new AutoGainSpec();
    }
    try {
        return new SimpleGainSpec(stof(*input));
    } catch (std::invalid_argument e) {
        return new MultiGainSpec(*input);
    }
}

SimpleGainSpec::SimpleGainSpec(float new_value) {
    value = new_value;
}

float SimpleGainSpec::getValue() {
    return value;
}

MultiGainSpec::MultiGainSpec(std::map<std::string, std::string> new_gains) {
    gains = new_gains;
}

MultiGainSpec::MultiGainSpec(std::string unparsed) {
    gains = Connector::parseSettings(unparsed);
}

std::map<std::string, std::string> MultiGainSpec::getValue() {
    return gains;
}