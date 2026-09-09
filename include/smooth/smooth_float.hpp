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

    // Value-returning addition: never mutates a or b -- see
    // SmoothInteger::operator+ for why this is a hidden friend rather than
    // a member or a free function. Throws unless a.canonical() ==
    // b.canonical(). Metrics: keeps a's, falling back to b's if a has none.
    friend SmoothFloat operator+(SmoothFloat a, const SmoothFloat& b) {
        a.addMatchingInPlace(b);
        if (!a.hasMetrics() && b.hasMetrics()) a.setMetricsPtr(b.metricsPtr());
        return a;
    }
};

}  // namespace smooth
