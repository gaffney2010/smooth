#pragma once

#include <memory>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/smooth_float.hpp"
#include "smooth/smooth_integer.hpp"

namespace smooth {

// Adds an overall sign to any default-constructible SmoothNumberBase
// subclass, sign-magnitude style: the (i, j) bit grid still only ever
// holds positive terms (2^i * 3^j > 0 always), and this just tracks
// whether the whole number is negated. set()/get()/print* all operate on
// the magnitude and are untouched by sign; only value() and the sign
// accessors below know about it.
//
// This is the shared logic behind both signed types (see the aliases
// below) -- writing it once here, generic over Base, is what lets
// SmoothSignedInteger and SmoothSignedFloat reuse it instead of
// duplicating a sign flag in two separate classes.
//
// value() intentionally hides (doesn't override) Base::value(): these
// classes are always used by their concrete type, never through a
// SmoothNumberBase*, so static hiding is sufficient and avoids making the
// shared engine's value() virtual for a feature only the signed types need.
template <typename Base>
class Signed : public Base {
public:
    // Forwards to Base's own (metrics, ) constructor explicitly, rather
    // than relying on `using Base::Base;` to inherit it -- Base's
    // constructor takes one defaulted argument, which makes it callable
    // with zero arguments too, and the inherited-constructor rules around
    // that overlap with a derived class's own default constructor are
    // subtle enough that spelling it out here is clearer than relying on
    // them.
    explicit Signed(std::shared_ptr<Metrics> metrics = nullptr) : Base(std::move(metrics)) {}

    bool isNegative() const { return negative_; }
    void setNegative(bool negative) { negative_ = negative; }
    void negate() { negative_ = !negative_; }

    double value() const { return (negative_ ? -1.0 : 1.0) * Base::value(); }

    // Splits the sign off of `v`, then hands the non-negative magnitude to
    // Base::setValue() -- which is where the actual bit encoding lives, so
    // it's written exactly once and shared by both signed and unsigned
    // types, same as value() above.
    void setValue(long long v) {
        setNegative(v < 0);
        Base::setValue(v < 0 ? -v : v);
    }

    void setValue(double v) {
        setNegative(v < 0.0);
        Base::setValue(v < 0.0 ? -v : v);
    }

    // Value-returning signed addition: never mutates its two arguments.
    // `result` is a fresh copy of the left-hand argument (taken by value),
    // distinct from whatever the caller passed -- see
    // SmoothInteger::operator+ for the full explanation of that, and for
    // why this is a hidden friend rather than a member or a free function.
    //
    // Same sign: the magnitudes just combine (via
    // result.Base::addMatchingInPlace(b), i.e. SmoothNumberBase's ordinary
    // "add the 1s and handle carries" addition -- which throws unless
    // result.canonical() == b.canonical()), sign unchanged. Different
    // signs: subtract the smaller magnitude from the larger
    // (subtractMagnitudeInPlace(), likewise requiring matching
    // representations) and take the larger argument's sign -- ordinary
    // signed-number addition. When |result| < |b|, that means building the
    // answer from a copy of `b` instead (into `swapped`, below), since
    // subtractMagnitudeInPlace() only ever computes receiver-minus-argument
    // and here it's `b` minus `result` that's needed; `swapped` is then
    // moved into `result` so the rest of the function has one variable to
    // finish up on. A result of exactly zero is normalized back to
    // non-negative, so isNegative() is never true for a zero value.
    //
    // Metrics: keeps the left-hand argument's, falling back to the
    // right-hand one's if the left-hand side has none -- captured once up
    // front (before any branch runs) and stamped onto `result` at the end,
    // since the swap case would otherwise carry `b`'s metrics through
    // (via `swapped`) regardless of what the left-hand argument had.
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

private:
    bool negative_ = false;
};

using SmoothSignedInteger = Signed<SmoothInteger>;
using SmoothSignedFloat = Signed<SmoothFloat>;

}  // namespace smooth
