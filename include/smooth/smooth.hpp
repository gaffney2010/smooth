#pragma once

// Convenience header pulling in all four concrete 3-smooth number types,
// Plan, and Transformation/AtomicTransformation. Named plan_zoo/
// transformation_zoo presets are opt-in -- include those headers
// separately.
#include "smooth/atomic_transformation.hpp"
#include "smooth/plan.hpp"
#include "smooth/signed.hpp"
#include "smooth/smooth_float.hpp"
#include "smooth/smooth_integer.hpp"
#include "smooth/smooth_number_base.hpp"
#include "smooth/transformation.hpp"
