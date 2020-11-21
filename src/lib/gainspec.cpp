#include "owrx/gainspec.hpp"
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

static std::string trim(const std::string &s) {
    std::string out = s;
    while (not out.empty() and std::isspace(out[0])) out = out.substr(1);
    while (not out.empty() and std::isspace(out[out.size()-1])) out = out.substr(0, out.size()-1);
    return out;
}

MultiGainSpec::MultiGainSpec(std::string unparsed) {
    bool inKey = true;
    std::string key, val;
    for (size_t i = 0; i < unparsed.size(); i++) {
        const char ch = unparsed[i];
        if (inKey) {
            if (ch == '=') inKey = false;
            else if (ch == ',') inKey = true;
            else key += ch;
        } else {
            if (ch == ',') inKey = true;
            else val += ch;
        }
        if ((inKey and (not val.empty() or (ch == ','))) or ((i+1) == unparsed.size())) {
            key = trim(key);
            val = trim(val);
            if (not key.empty()) gains[key] = val;
            key = "";
            val = "";
        }
    }
}

std::map<std::string, std::string> MultiGainSpec::getValue() {
    return gains;
}