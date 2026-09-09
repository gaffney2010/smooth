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

    // Value-returning signed addition: never mutates a or b. Same sign:
    // the magnitudes just combine (via Base::addMatchingInPlace(), i.e.
    // SmoothNumberBase's ordinary "add the 1s and handle carries"
    // addition -- which throws unless a.canonical() == b.canonical()),
    // sign unchanged. Different signs: subtract the smaller magnitude from
    // the larger (Base::subtractMagnitudeInPlace(), likewise requiring
    // matching representations) and take the larger operand's sign --
    // ordinary signed-number addition. A result of exactly zero is
    // normalized back to non-negative, so isNegative() is never true for a
    // zero value. A hidden friend, same as SmoothInteger/SmoothFloat's
    // operator+ -- see SmoothInteger::operator+ for why.
    //
    // Metrics: keeps a's, falling back to b's if a has none -- captured
    // once up front (before any branch runs) and stamped on at the end,
    // since the "different magnitudes, |a| < |b|" branch below builds its
    // result out of a copy of b, which would otherwise carry b's metrics
    // through regardless of what a's were.
    friend Signed<Base> operator+(Signed<Base> a, const Signed<Base>& b) {
        auto keepMetrics = a.metricsPtr();
        if (!keepMetrics && b.hasMetrics()) keepMetrics = b.metricsPtr();

        if (a.negative_ == b.negative_) {
            a.Base::addMatchingInPlace(b);
        } else if (a.Base::value() >= b.Base::value()) {
            a.Base::subtractMagnitudeInPlace(b);
        } else {
            Signed<Base> result(b);
            result.subtractMagnitudeInPlace(a);
            result.negative_ = b.negative_;
            a = std::move(result);
        }

        a.setMetricsPtr(std::move(keepMetrics));
        if (a.Base::value() == 0.0) a.negative_ = false;
        return a;
    }

private:
    bool negative_ = false;
};

using SmoothSignedInteger = Signed<SmoothInteger>;
using SmoothSignedFloat = Signed<SmoothFloat>;

}  // namespace smooth
