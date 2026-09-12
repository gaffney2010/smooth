#pragma once

#include "smooth/transformation.hpp"

namespace smooth {

// Splits the bit at (i, j) into its three "corner" neighbors: (i-1, j),
// (i, j-1), and (i-1, j-1). Value-preserving:
//
//   2^(i-1)*3^j + 2^i*3^(j-1) + 2^(i-1)*3^(j-1)
//     = 2^(i-1)*3^(j-1) * (3 + 2 + 1)
//     = 2^(i-1)*3^(j-1) * 6
//     = 2^i * 3^j
class CornerSplitTransformation : public OffsetTransformation {
public:
    CornerSplitTransformation() : OffsetTransformation({{0, 0}}, {{-1, 0}, {0, -1}, {-1, -1}}) {}
};

}  // namespace smooth
