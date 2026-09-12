#pragma once

// Convenience header pulling in every named Transformation preset:
// MergeTransformation, SplitTransformation, SpreadTransformation,
// CornerSplitTransformation, RowSpreadTransformation (each an
// OffsetTransformation), and TernaryCarryTransformation (which isn't).
#include "smooth/transformation_zoo/corner_split_transformation.hpp"
#include "smooth/transformation_zoo/merge_transformation.hpp"
#include "smooth/transformation_zoo/row_spread_transformation.hpp"
#include "smooth/transformation_zoo/spread_transformation.hpp"
#include "smooth/transformation_zoo/split_transformation.hpp"
#include "smooth/transformation_zoo/ternary_carry_transformation.hpp"
