#pragma once

#include <memory>
#include <vector>

#include "smooth/atomic_transformation.hpp"

namespace smooth {

// Splits the bit at (i, j) into its three "corner" neighbors: (i-1, j),
// (i, j-1), and (i-1, j-1). Value-preserving:
//
//   2^(i-1)*3^j + 2^i*3^(j-1) + 2^(i-1)*3^(j-1)
//     = 2^(i-1)*3^(j-1) * (3 + 2 + 1)
//     = 2^(i-1)*3^(j-1) * 6
//     = 2^i * 3^j
//
// This library's second atom (atomic_transformation.hpp), independent of
// Merge/Split -- see its own atomize(), which is just itself.
class CornerSplitTransformation : public AtomicTransformation {
public:
    CornerSplitTransformation() : AtomicTransformation({{0, 0}}, {{-1, 0}, {0, -1}, {-1, -1}}) {}

    std::vector<AtomApplication> atomize(int i, int j) const {
        return {AtomApplication{std::make_shared<CornerSplitTransformation>(), i, j}};
    }
};

}  // namespace smooth
