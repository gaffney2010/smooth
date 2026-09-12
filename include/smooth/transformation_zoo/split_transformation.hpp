#pragma once

#include "smooth/transformation.hpp"

namespace smooth {

// The reverse of MergeTransformation (merge_transformation.hpp): splits
// the bit at (i, j+1) into the two bits at (i, j) and (i+1, j) -- each
// carrying independently (in that order) if its destination is already
// occupied.
class SplitTransformation : public OffsetTransformation {
public:
    SplitTransformation() : OffsetTransformation({{0, 1}}, {{0, 0}, {1, 0}}) {}
};

}  // namespace smooth
