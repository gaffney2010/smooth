#pragma once

#include "smooth/reduction_zoo/transformation_reduction.hpp"
#include "smooth/transformation_zoo/merge_transformation.hpp"

namespace smooth {

// Greedily combines every (i, j)/(i+1, j) pair of set bits into (i, j+1),
// repeating until none are left -- see TransformationReduction's own
// class comment for why {MergeTransformation} is guaranteed to terminate
// on its own. This is what MergingSparsePlan runs on both of a multiply's
// operands beforehand: fewer set bits means a cheaper multiply.
//
// A MergeReduction *is* a TransformationReduction configured with just
// MergeTransformation and the name "merge" -- name()/run() are simply
// inherited. `mergeTransformation()` returns a function-local static
// rather than an instance member, since base classes finish constructing
// before any derived class's own members exist -- `{&merge_}` in the base
// constructor call would capture a not-yet-existing member's address. A
// function-local static sidesteps that; since MergeTransformation is
// stateless, every MergeReduction sharing the one instance is no
// different from each having its own.
class MergeReduction : public TransformationReduction {
public:
    MergeReduction() : TransformationReduction({&mergeTransformation()}, "merge") {}

private:
    static const MergeTransformation& mergeTransformation() {
        static const MergeTransformation merge;
        return merge;
    }
};

}  // namespace smooth
