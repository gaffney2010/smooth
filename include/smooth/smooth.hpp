#pragma once

// Convenience header pulling in all four concrete 3-smooth number types
// (SmoothInteger, SmoothFloat, SmoothSignedInteger, SmoothSignedFloat),
// Plan, the fluent arithmetic-expression builder, and Transformation, the
// value-preserving bit-grid rewrites SmoothNumberBase::applyTransformation()
// applies. Named plan_zoo/transformation_zoo presets (SparsePlan,
// MergeTransformation, ...) are opt-in -- include
// smooth/plan_zoo.hpp/smooth/transformation_zoo.hpp separately for those.
#include "smooth/plan.hpp"
#include "smooth/signed.hpp"
#include "smooth/smooth_float.hpp"
#include "smooth/smooth_integer.hpp"
#include "smooth/smooth_number_base.hpp"
#include "smooth/transformation.hpp"
