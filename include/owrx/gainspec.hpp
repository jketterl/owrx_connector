#pragma once
#pragma GCC visibility push(default)

#include <string>

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
}