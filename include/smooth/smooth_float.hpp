#pragma once

#include "smooth/smooth_number_base.hpp"

namespace smooth {

// A 3-smooth number that allows fractional terms: i and j may be negative,
// so a term like i = -1 contributes a factor of 1/2.
class SmoothFloat : public SmoothNumberBase {
public:
    SmoothFloat() : SmoothNumberBase(/*allow_fractional=*/true) {}
};

}  // namespace smooth
