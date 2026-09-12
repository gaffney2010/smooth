#pragma once

#include <memory>
#include <string>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"
#include "smooth/plan_zoo/sparse_plan.hpp"
#include "smooth/reduction_zoo/ternary_form_reduction.hpp"

namespace smooth {

// Same shape as MergingSparsePlan (SparsePlan plus a reduction run on both
// of a multiply's operands beforehand), but with TernaryFormReduction
// instead of MergeReduction: rather than greedily combining any
// same-column row pair it happens to find, it drives *every* column down
// to at most one bit, always resolving the smallest offending column
// first.
//
// The two reductions can leave an operand with a different bit count for
// the same starting value -- MergeReduction stops as soon as no
// (i, j)/(i+1, j) pair is left (which can still leave several bits
// scattered across one column, e.g. columns never forced below three
// bits by a single merge), while TernaryFormReduction keeps going until
// every column individually holds at most one. Comparing this plan's
// bit_operations counter against MergingSparsePlan's for the same
// expression (see src/profile_runner.cpp) shows which one actually wins
// for a given shape of number -- neither is strictly better in general.
class TernaryFormSparsePlan : public SparsePlan {
public:
    explicit TernaryFormSparsePlan(std::shared_ptr<Metrics> metrics = nullptr) : SparsePlan(std::move(metrics)) {}

    std::string name() const override { return "ternary_form_sparse"; }

protected:
    std::unique_ptr<Node> buildBlueprint(const Node& declaration) const override {
        return wrapMultiplyOperandsWithReduction(SparsePlan::buildBlueprint(declaration), reduction_);
    }

private:
    TernaryFormReduction reduction_;
};

}  // namespace smooth
