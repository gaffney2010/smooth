#pragma once

#include <iostream>
#include <map>
#include <string>

namespace smooth {

// A small named-counter tracker, optionally attached to a SmoothNumberBase-
// derived number at construction (see smooth_integer.hpp, smooth_float.hpp,
// signed.hpp). For now, SmoothNumberBase uses it to count how many times a
// number converts between internal representations -- one counter per
// (source, target) pair, e.g. "convert_row_values_to_dynamic" -- but it's
// deliberately generic: any named event can be tallied here.
class Metrics {
public:
    void increment(const std::string& name, long long count = 1) { counters_[name] += count; }

    void print(std::ostream& os = std::cout) const {
        for (const auto& counter : counters_) {
            os << counter.first << " = " << counter.second << "\n";
        }
    }

private:
    std::map<std::string, long long> counters_;
};

}  // namespace smooth
