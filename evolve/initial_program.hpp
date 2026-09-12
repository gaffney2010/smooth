// Seed program for openevolve: a smooth::Plan strategy (see
// include/smooth/plan.hpp and include/smooth/plan_zoo/*.hpp for the
// primitives available here, and evolve/README.md for how this gets scored).
//
// Only the EVOLVE-BLOCK below is mutated by the LLM. Everything outside it
// stays fixed so evolve/harness.cpp can always find `smooth::EvolvedPlan`.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"
#include "smooth/reduction.hpp"
#include "smooth/reduction_zoo.hpp"
#include "smooth/representation_zoo/sparse_representation.hpp"
#include "smooth/smooth_number_base.hpp"

namespace smooth {

// # EVOLVE-BLOCK-START

// A starting point copied from plan_zoo/size_adaptive_sparse_plan.hpp:
// every leaf goes to Sparse, and a multiply's operands get reduced first
// (by bit-count-estimated thresholds) so the multiply itself is cheaper.
// This is a reasonable, already-decent strategy -- evolve it into a better
// one. Nothing about the harness cares *how* you pick targets/reductions,
// only that buildBlueprint() still returns a blueprint validateBlueprint()
// accepts (see plan.hpp: same shape and leaf contents as the declaration,
// only Ensure/Reduce nodes added) and that the result stays correct.
class EvolvedPlan : public Plan {
public:
    explicit EvolvedPlan(std::shared_ptr<Metrics> metrics = nullptr) : Plan(std::move(metrics)) {}

    std::string name() const override { return "evolved"; }

protected:
    std::unique_ptr<Node> buildBlueprint(const Node& declaration) const override {
        std::unique_ptr<Node> blueprint =
            wrapLeavesWithEnsure(declaration, SmoothNumberBase::Representation::Sparse);
        return wrapMultiplyOperandsWithReduction(
            std::move(blueprint), [](const Node& operand) -> const Reduction* {
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
    // (Ensure/Reduce leaves), or approximates an Add/Multiply node's as the
    // sum of its two operands' -- an upper bound on the real, post-carry
    // result, not a prediction of it, but enough to pick a reduction.
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

// # EVOLVE-BLOCK-END

}  // namespace smooth
