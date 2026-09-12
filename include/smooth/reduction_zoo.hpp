#pragma once

// Convenience header pulling in everything in reduction_zoo/:
// TransformationReduction, the generic engine, and the named presets
// built from it (MergeReduction, BinaryFormReduction,
// TernaryCarryReduction) or implementing Reduction directly
// (TernaryFormReduction, StaircaseReduction -- each needs its own
// bespoke search). Reduction itself (reduction.hpp) is just the shared
// interface.
#include "smooth/reduction_zoo/binary_form_reduction.hpp"
#include "smooth/reduction_zoo/merge_reduction.hpp"
#include "smooth/reduction_zoo/staircase_reduction.hpp"
#include "smooth/reduction_zoo/ternary_carry_reduction.hpp"
#include "smooth/reduction_zoo/ternary_form_reduction.hpp"
#include "smooth/reduction_zoo/transformation_reduction.hpp"
