#pragma once

// Convenience header pulling in everything in reduction_zoo/:
// TransformationReduction, the generic engine, and the named presets
// built from it (MergeReduction, BinaryFormReduction,
// TernaryCarryReduction) or implementing Reduction directly
// (TernaryFormReduction, composing two of them together; StaircaseReduction,
// which needs its own bespoke search). Reduction itself (reduction.hpp) is
// just the shared interface -- see each file's own class comment for
// building a custom reduction directly. Mirrors
// plan_zoo.hpp/transformation_zoo.hpp/representation_zoo.hpp.
#include "smooth/reduction_zoo/binary_form_reduction.hpp"
#include "smooth/reduction_zoo/merge_reduction.hpp"
#include "smooth/reduction_zoo/staircase_reduction.hpp"
#include "smooth/reduction_zoo/ternary_carry_reduction.hpp"
#include "smooth/reduction_zoo/ternary_form_reduction.hpp"
#include "smooth/reduction_zoo/transformation_reduction.hpp"
