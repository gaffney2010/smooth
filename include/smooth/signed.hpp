#pragma once

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
    using Base::Base;

    bool isNegative() const { return negative_; }
    void setNegative(bool negative) { negative_ = negative; }
    void negate() { negative_ = !negative_; }

    double value() const { return (negative_ ? -1.0 : 1.0) * Base::value(); }

private:
    bool negative_ = false;
};

using SmoothSignedInteger = Signed<SmoothInteger>;
using SmoothSignedFloat = Signed<SmoothFloat>;

}  // namespace smooth
