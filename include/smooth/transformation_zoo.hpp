#pragma once

// Convenience header pulling in every named Transformation preset in
// transformation_zoo/: MergeTransformation, SplitTransformation,
// SpreadTransformation, CornerSplitTransformation, RowSpreadTransformation
// (each an OffsetTransformation, transformation.hpp -- see its class
// comment for building a custom one directly), and
// TernaryCarryTransformation (which isn't -- see its own class comment).
// Mirrors plan_zoo.hpp/representation_zoo.hpp.
#include "smooth/transformation_zoo/corner_split_transformation.hpp"
#include "smooth/transformation_zoo/merge_transformation.hpp"
#include "smooth/transformation_zoo/row_spread_transformation.hpp"
#include "smooth/transformation_zoo/spread_transformation.hpp"
#include "smooth/transformation_zoo/split_transformation.hpp"
#include "smooth/transformation_zoo/ternary_carry_transformation.hpp"
