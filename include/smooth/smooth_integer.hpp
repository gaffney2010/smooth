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

    // Value-returning addition: never mutates its two arguments. The
    // left-hand argument is passed by value, so `result` here is already a
    // fresh copy, distinct from whatever the caller passed as `a` in
    // `a + b` -- mutating `result` in place (via the protected
    // addMatchingInPlace(), which throws unless result.canonical() ==
    // b.canonical()) is what builds the sum, and is exactly as safe as
    // `result = a; result.addMatchingInPlace(b);` would be. A hidden
    // friend (defined inside the class) rather than a member, so `a + b`
    // reads symmetrically; it can reach addMatchingInPlace(), a protected
    // SmoothNumberBase member, because it's a friend of SmoothInteger and
    // `result` is of that derived type.
    //
    // Metrics: keeps the left-hand argument's, falling back to the
    // right-hand one's if the left-hand side has none.
    friend SmoothInteger operator+(SmoothInteger result, const SmoothInteger& b) {
        result.addMatchingInPlace(b);
        if (!result.hasMetrics() && b.hasMetrics()) result.setMetricsPtr(b.metricsPtr());
        return result;
    }
};

}  // namespace smooth
