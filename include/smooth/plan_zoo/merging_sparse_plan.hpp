#pragma once

#include <memory>
#include <string>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"
#include "smooth/plan_zoo/sparse_plan.hpp"
#include "smooth/reduction_zoo/merge_reduction.hpp"

namespace smooth {

// Same strategy as SparsePlan, but with one addition: before every
// multiply, both operands are first run through MergeReduction --
// greedily combining every (i, j)/(i+1, j) pair into (i, j+1) until
// neither operand has any left.
//
// Multiplying two SparseRepresentations costs one bit_operation per
// (termA, termB) pair, so reducing each operand's bit count first
// directly reduces the multiply's cost. This doesn't help addition at
// all (no pairwise blowup to shrink), so only Multiply operands are
// wrapped -- see buildBlueprint() below.
class MergingSparsePlan : public SparsePlan {
public:
    explicit MergingSparsePlan(std::shared_ptr<Metrics> metrics = nullptr) : SparsePlan(std::move(metrics)) {}

    std::string name() const override { return "merging_sparse"; }

protected:
    std::unique_ptr<Node> buildBlueprint(const Node& declaration) const override {
        return wrapMultiplyOperandsWithReduction(SparsePlan::buildBlueprint(declaration), reduction_);
    }

private:
    MergeReduction reduction_;
};

}  // namespace smooth
