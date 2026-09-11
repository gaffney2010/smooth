#pragma once

// Convenience header pulling in every named Transformation preset in
// transformation_zoo/: MergeTransformation, SplitTransformation, and
// SpreadTransformation. Transformation itself (transformation.hpp) is
// concrete and general -- see its class comment for building a custom one
// directly. Mirrors plan_zoo.hpp/representation_zoo.hpp.
#include "smooth/transformation_zoo/merge_transformation.hpp"
#include "smooth/transformation_zoo/spread_transformation.hpp"
#include "smooth/transformation_zoo/split_transformation.hpp"
