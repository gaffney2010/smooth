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
    // left-hand argument is passed by value, so `result` is already a
    // fresh copy -- mutating it in place (via the protected
    // addMatchingInPlace(), which throws unless result.canonical() ==
    // b.canonical()) is what builds the sum. A hidden friend (rather than
    // a member) so `a + b` reads symmetrically, while still reaching a
    // protected SmoothNumberBase member as a friend of the derived type.
    //
    // Metrics: keeps the left-hand argument's, falling back to the
    // right-hand one's if the left-hand side has none.
    friend SmoothInteger operator+(SmoothInteger result, const SmoothInteger& b) {
        result.addMatchingInPlace(b);
        if (!result.hasMetrics() && b.hasMetrics()) result.setMetricsPtr(b.metricsPtr());
        return result;
    }

    // Value-returning multiplication: same shape as operator+ above, but
    // builds the product via multiplyMatchingInPlace() instead.
    friend SmoothInteger operator*(SmoothInteger result, const SmoothInteger& b) {
        result.multiplyMatchingInPlace(b);
        if (!result.hasMetrics() && b.hasMetrics()) result.setMetricsPtr(b.metricsPtr());
        return result;
    }
};

}  // namespace smooth
