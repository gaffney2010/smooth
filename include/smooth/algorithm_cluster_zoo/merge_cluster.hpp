#pragma once

#include "smooth/algorithm_cluster_zoo/transformation_algorithm_cluster.hpp"
#include "smooth/transformation_zoo/merge_transformation.hpp"

namespace smooth {

// Greedily combines every (i, j)/(i+1, j) pair of set bits it can find
// into (i, j+1), repeating (since a merge's own carry can create new
// merge opportunities) until none are left -- see
// TransformationAlgorithmCluster's own class comment for why this
// particular family (just MergeTransformation) is guaranteed to terminate
// on its own, no bound required. This is what MergingSparsePlan
// (plan_zoo/merging_sparse_plan.hpp) runs on both of a multiply's
// operands before the multiply itself: fewer set bits in means a cheaper
// multiply (see multiplyBitsWithCarry(), representation_base.hpp).
//
// A MergeCluster *is* a TransformationAlgorithmCluster configured with
// just MergeTransformation and the name "merge" -- it adds no behavior of
// its own, so it's a subclass rather than a wrapper: name()/run() are
// simply inherited, not redeclared. The one thing to watch is what gets
// passed to the base constructor: `mergeTransformation()` below returns a
// reference to a function-local static MergeTransformation rather than an
// instance member, specifically so its address is safe to hand to the
// base class constructor. A plain instance member wouldn't be safe here
// (unlike the sibling members TransformationAlgorithmCluster itself
// combines with its own Transformation -- see MergingSparsePlan, which
// still does exactly that): base classes are always fully constructed
// *before* any of a derived class's own members, so
// `TransformationAlgorithmCluster({&merge_}, ...)` would be capturing the
// address of a MergeCluster instance member, merge_, that doesn't exist
// yet. A function-local static sidesteps the whole ordering question --
// it's guaranteed constructed (once, thread-safely) the first time
// mergeTransformation() is ever called, long before any MergeCluster
// instance needs it -- and since MergeTransformation is stateless once
// built, every MergeCluster sharing the one instance is no different from
// each having its own.
class MergeCluster : public TransformationAlgorithmCluster {
public:
    MergeCluster() : TransformationAlgorithmCluster({&mergeTransformation()}, "merge") {}

private:
    static const MergeTransformation& mergeTransformation() {
        static const MergeTransformation merge;
        return merge;
    }
};

}  // namespace smooth
