#pragma once

#include <memory>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/smooth_number_base.hpp"

namespace smooth {

// A 3-smooth number that allows fractional terms: i and j may be negative,
// so a term like i = -1 contributes a factor of 1/2.
class SmoothFloat : public SmoothNumberBase {
public:
    // `metrics`, if given, is shared (not copied) -- see Metrics and
    // SmoothNumberBase's counters for representation conversions.
    explicit SmoothFloat(std::shared_ptr<Metrics> metrics = nullptr)
        : SmoothNumberBase(/*allow_fractional=*/true, std::move(metrics)) {}

    // Value-returning addition: never mutates its two arguments -- see
    // SmoothInteger::operator+ for why this is a hidden friend. Throws
    // unless result.canonical() == b.canonical().
    friend SmoothFloat operator+(SmoothFloat result, const SmoothFloat& b) {
        result.addMatchingInPlace(b);
        if (!result.hasMetrics() && b.hasMetrics()) result.setMetricsPtr(b.metricsPtr());
        return result;
    }

    // Value-returning multiplication: same shape as operator+ above, but
    // builds the product via the protected multiplyMatchingInPlace()
    // instead.
    friend SmoothFloat operator*(SmoothFloat result, const SmoothFloat& b) {
        result.multiplyMatchingInPlace(b);
        if (!result.hasMetrics() && b.hasMetrics()) result.setMetricsPtr(b.metricsPtr());
        return result;
    }
};

}  // namespace smooth
