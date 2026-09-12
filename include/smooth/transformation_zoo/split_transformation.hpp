#pragma once

#include <memory>
#include <vector>

#include "smooth/atomic_transformation.hpp"

namespace smooth {

// The reverse of MergeTransformation (merge_transformation.hpp): splits
// the bit at (i, j+1) into the two bits at (i, j) and (i+1, j) -- each
// carrying independently (in that order) if its destination is already
// occupied.
//
// The other half of MergeTransformation's atom (atomic_transformation.hpp)
// -- its own atomize() is just itself.
class SplitTransformation : public AtomicTransformation {
public:
    SplitTransformation() : AtomicTransformation({{0, 1}}, {{0, 0}, {1, 0}}) {}

    std::vector<AtomApplication> atomize(int i, int j) const {
        return {AtomApplication{std::make_shared<SplitTransformation>(), i, j}};
    }
};

}  // namespace smooth
