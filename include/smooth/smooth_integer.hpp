#pragma once

#include "smooth/smooth_number_base.hpp"

namespace smooth {

// A 3-smooth whole number: i and j must be non-negative, so every term
// 2^i * 3^j is itself a whole number.
class SmoothInteger : public SmoothNumberBase {
public:
    SmoothInteger() : SmoothNumberBase(/*allow_fractional=*/false) {}
};

}  // namespace smooth
