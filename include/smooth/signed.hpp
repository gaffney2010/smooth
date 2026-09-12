#pragma once

#include <memory>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/smooth_float.hpp"
#include "smooth/smooth_integer.hpp"

namespace smooth {

// Adds an overall sign to any default-constructible SmoothNumberBase
// subclass, sign-magnitude style: the (i, j) bit grid still only holds
// positive terms, and this just tracks whether the whole number is
// negated. set()/get()/print* all operate on the magnitude, untouched by
// sign; only value() and the sign accessors below know about it. Writing
// this once here, generic over Base, is what lets SmoothSignedInteger and
// SmoothSignedFloat share it instead of duplicating a sign flag.
//
// value() intentionally hides (doesn't override) Base::value(): these
// classes are always used by their concrete type, never through a
// SmoothNumberBase*, so static hiding is enough.
template <typename Base>
class Signed : public Base {
public:
    // Forwards to Base's own constructor explicitly rather than `using
    // Base::Base;` -- Base's constructor takes one defaulted argument,
    // and the inherited-constructor rules around that overlap with a
    // derived class's own default constructor subtly enough that spelling
    // it out is clearer.
    explicit Signed(std::shared_ptr<Metrics> metrics = nullptr) : Base(std::move(metrics)) {}

    bool isNegative() const { return negative_; }
    void setNegative(bool negative) { negative_ = negative; }
    void negate() { negative_ = !negative_; }

    double value() const { return (negative_ ? -1.0 : 1.0) * Base::value(); }

    // Splits the sign off of `v`, then hands the non-negative magnitude to
    // Base::setValue() -- the actual bit encoding, written once and shared
    // by both signed and unsigned types.
    void setValue(long long v) {
        setNegative(v < 0);
        Base::setValue(v < 0 ? -v : v);
    }

    void setValue(double v) {
        setNegative(v < 0.0);
        Base::setValue(v < 0.0 ? -v : v);
    }

    // Value-returning signed addition: never mutates its two arguments.
    // `result` is a fresh copy of the left-hand argument (see
    // SmoothInteger::operator+ for why this is a hidden friend).
    //
    // Same sign: magnitudes just combine (addMatchingInPlace(), sign
    // unchanged). Different signs: subtract the smaller magnitude from the
    // larger and take the larger argument's sign. When |result| < |b|,
    // that means building the answer from a copy of `b` instead (into
    // `swapped`), since subtractMagnitudeInPlace() only computes
    // receiver-minus-argument and here it's `b` minus `result` that's
    // needed. A result of exactly zero is normalized to non-negative.
    //
    // Metrics: keeps the left-hand argument's, falling back to the
    // right-hand one's -- captured once up front, since the swap case
    // would otherwise carry `b`'s metrics through regardless.
    friend Signed<Base> operator+(Signed<Base> result, const Signed<Base>& b) {
        auto keepMetrics = result.metricsPtr();
        if (!keepMetrics && b.hasMetrics()) keepMetrics = b.metricsPtr();

        if (result.negative_ == b.negative_) {
            result.Base::addMatchingInPlace(b);
        } else if (result.Base::value() >= b.Base::value()) {
            result.Base::subtractMagnitudeInPlace(b);
        } else {
            Signed<Base> swapped(b);
            swapped.subtractMagnitudeInPlace(result);
            swapped.negative_ = b.negative_;
            result = std::move(swapped);
        }

        result.setMetricsPtr(std::move(keepMetrics));
        if (result.Base::value() == 0.0) result.negative_ = false;
        return result;
    }

    // Value-returning signed multiplication: simpler than addition --
    // magnitudes multiply regardless of sign, and the result's sign is
    // just the usual XOR rule, normalized to non-negative for zero.
    friend Signed<Base> operator*(Signed<Base> result, const Signed<Base>& b) {
        bool negativeProduct = (result.negative_ != b.negative_);
        result.Base::multiplyMatchingInPlace(b);
        result.negative_ = negativeProduct;
        if (result.Base::value() == 0.0) result.negative_ = false;
        if (!result.hasMetrics() && b.hasMetrics()) result.setMetricsPtr(b.metricsPtr());
        return result;
    }

private:
    bool negative_ = false;
};

using SmoothSignedInteger = Signed<SmoothInteger>;
using SmoothSignedFloat = Signed<SmoothFloat>;

}  // namespace smooth
