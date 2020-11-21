#pragma once
#pragma GCC visibility push(default)

#include <string>
#include <map>

namespace Owrx {
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

    class MultiGainSpec: public GainSpec {
        public:
            MultiGainSpec(std::map<std::string, std::string> gains);
            MultiGainSpec(std::string unparsed);
            std::map<std::string, std::string> getValue();
        private:
            std::map<std::string, std::string> gains;
    };
}