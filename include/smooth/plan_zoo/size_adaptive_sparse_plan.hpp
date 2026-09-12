#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"
#include "smooth/plan_zoo/sparse_plan.hpp"
#include "smooth/reduction.hpp"
#include "smooth/reduction_zoo/merge_reduction.hpp"
#include "smooth/reduction_zoo/staircase_reduction.hpp"
#include "smooth/representation_zoo/sparse_representation.hpp"

namespace smooth {

// Same idea as MergingSparsePlan -- reduce a multiply's operands
// beforehand -- but which reduction (if any) gets used for a given
// operand depends on how many bits it's estimated to have, rather than
// always being MergeReduction:
//
//   < kMergeThreshold bits                      -- no reduction: too few
//     bits for a reduction's own bookkeeping (rescanning every set bit,
//     once per step) to pay for itself.
//   kMergeThreshold .. kStaircaseThreshold - 1   -- MergeReduction: cheap,
//     and (per its own comment) can only ever shrink the bit count.
//   >= kStaircaseThreshold bits                  -- StaircaseReduction:
//     more work per step, and -- via CornerSplitTransformation -- not
//     even guaranteed to shrink the bit count the way MergeReduction is
//     (see StaircaseReduction's own comment). Worth trying anyway once an
//     operand is big enough that a more thorough normalization has room
//     to pay off.
//
// estimatedBitCount() below necessarily guesses: buildBlueprint() only
// ever sees the blueprint before anything has actually been computed, so
// an Add/Multiply node's own count is approximated as the sum of its
// operands' -- an upper bound on the real, post-carry result, not a
// prediction of it, but enough to decide whether reducing first looks
// worthwhile.
class SizeAdaptiveSparsePlan : public SparsePlan {
public:
    explicit SizeAdaptiveSparsePlan(std::shared_ptr<Metrics> metrics = nullptr) : SparsePlan(std::move(metrics)) {}

    std::string name() const override { return "size_adaptive_sparse"; }

protected:
    std::unique_ptr<Node> buildBlueprint(const Node& declaration) const override {
        return wrapMultiplyOperandsWithReduction(
            SparsePlan::buildBlueprint(declaration),
            [](const Node& operand) -> const Reduction* {
                std::size_t bits = estimatedBitCount(operand);
                if (bits < kMergeThreshold) return nullptr;
                if (bits < kStaircaseThreshold) return &mergeReduction();
                return &staircaseReduction();
            });
    }

private:
    static constexpr std::size_t kMergeThreshold = 4;
    static constexpr std::size_t kStaircaseThreshold = 16;

    // Counts an already-built blueprint node's own set bits directly
    // (Ensure/Reduce leaves, ScalarLeaf/NumberLeaf), or approximates an
    // Add/Multiply node's as the sum of its two operands'.
    static std::size_t estimatedBitCount(const Node& node) {
        switch (node.kind) {
            case Node::Kind::ScalarLeaf: {
                SparseRepresentation probe(/*allow_fractional=*/true);
                probe.setColumnValue(0, node.scalarValue);
                std::size_t count = 0;
                probe.forEachSet([&count](int, int) { ++count; });
                return count;
            }
            case Node::Kind::NumberLeaf: {
                // representationAs(canonical()) is a plain clone, not a
                // conversion (ensure() short-circuits when target ==
                // canonical), so estimating a size costs nothing and
                // leaves the number's own Metrics untouched.
                auto rep = node.numberSource->representationAs(node.numberSource->canonical());
                std::size_t count = 0;
                rep->forEachSet([&count](int, int) { ++count; });
                return count;
            }
            case Node::Kind::Ensure:
            case Node::Kind::Reduce:
                return estimatedBitCount(*node.child);
            case Node::Kind::Add:
            case Node::Kind::Multiply:
                return estimatedBitCount(*node.left) + estimatedBitCount(*node.right);
        }
        return 0;  // unreachable
    }

    static const MergeReduction& mergeReduction() {
        static const MergeReduction reduction;
        return reduction;
    }
    static const StaircaseReduction& staircaseReduction() {
        static const StaircaseReduction reduction;
        return reduction;
    }
};

}  // namespace smooth
