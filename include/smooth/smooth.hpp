#pragma once

// Convenience header pulling in all four concrete 3-smooth number types
// (SmoothInteger, SmoothFloat, SmoothSignedInteger, SmoothSignedFloat),
// Plan, the fluent arithmetic-expression builder, and Transformation, the
// value-preserving bit-grid rewrites SmoothNumberBase::applyTransformation()
// applies.
#include "smooth/plan.hpp"
#include "smooth/signed.hpp"
#include "smooth/smooth_float.hpp"
#include "smooth/smooth_integer.hpp"
#include "smooth/smooth_number_base.hpp"
#include "smooth/transformation.hpp"
