#pragma once

#include <memory>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/smooth_number_base.hpp"

namespace smooth {

// A 3-smooth whole number: i and j must be non-negative, so every term
// 2^i * 3^j is itself a whole number.
class SmoothInteger : public SmoothNumberBase {
public:
    // `metrics`, if given, is shared (not copied) -- see Metrics and
    // SmoothNumberBase's counters for representation conversions.
    explicit SmoothInteger(std::shared_ptr<Metrics> metrics = nullptr)
        : SmoothNumberBase(/*allow_fractional=*/false, std::move(metrics)) {}
};

}  // namespace smooth
