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

    // Value-returning addition: never mutates a or b. `a` is taken by value
    // (a copy) and combined with `b` via the protected
    // addMatchingInPlace() -- which throws unless a.canonical() ==
    // b.canonical() -- then returned. A hidden friend (defined inside the
    // class) rather than a member, so `a + b` reads symmetrically; it can
    // reach addMatchingInPlace(), a protected SmoothNumberBase member,
    // because it's a friend of SmoothInteger and `a`/`result` are of that
    // derived type.
    //
    // Metrics: keeps a's, falling back to b's if a has none.
    friend SmoothInteger operator+(SmoothInteger a, const SmoothInteger& b) {
        a.addMatchingInPlace(b);
        if (!a.hasMetrics() && b.hasMetrics()) a.setMetricsPtr(b.metricsPtr());
        return a;
    }
};

}  // namespace smooth
