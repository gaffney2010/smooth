#pragma once

#include "smooth/transformation.hpp"

namespace smooth {

// Merges the two bits at (i, j) and (i+1, j) into the single bit at
// (i, j+1): 2^i*3^j + 2^(i+1)*3^j = 2^i*3^(j+1). If (i, j+1) is already
// set, the merge still succeeds -- it carries into (i+1, j+1),
// (i+2, j+1), ... until it lands on a clear cell, exactly like ordinary
// addition would.
class MergeTransformation : public OffsetTransformation {
public:
    MergeTransformation() : OffsetTransformation({{0, 0}, {1, 0}}, {{0, 1}}) {}
};

}  // namespace smooth
