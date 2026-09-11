#pragma once

#include <memory>
#include <string>
#include <utility>

#include "smooth/algorithm_cluster_zoo/merge_cluster.hpp"
#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"
#include "smooth/plan_zoo/sparse_plan.hpp"

namespace smooth {

// Same strategy as SparsePlan (every leaf ends up as a SparseRepresentation
// -- see plan.hpp's wrapLeavesWithEnsure()), but with one addition: before
// every multiply, both operands are first run through MergeCluster
// (algorithm_cluster_zoo/merge_cluster.hpp) -- greedily combining every
// (i, j)/(i+1, j) pair of set bits it can find into (i, j+1), and
// repeating (since a merge's own carry can create new merge opportunities)
// until neither operand has any left.
//
// Multiplying two SparseRepresentations costs one bit_operation per
// (termA, termB) pair -- n*m for an n-bit by m-bit multiply (see
// multiplyBitsWithCarry(), representation_base.hpp) -- so reducing each
// operand's bit count first directly reduces the multiply's cost. This
// doesn't help addition at all (it's already just carrying, with no
// pairwise blowup to shrink), so only a Multiply node's operands get
// wrapped -- see buildBlueprint() below.
//
// Which specific (i, j) pairs actually get merged can only be discovered
// at runtime, from the real bits -- unlike Ensure, a Cluster blueprint
// node's *shape* (that one runs here, wrapping this particular operand) is
// still decided statically by buildBlueprint(), same as always; only what
// it actually does, once running, is inherently data-dependent.
class MergingSparsePlan : public SparsePlan {
public:
    explicit MergingSparsePlan(std::shared_ptr<Metrics> metrics = nullptr) : SparsePlan(std::move(metrics)) {}

    std::string name() const override { return "merging_sparse"; }

protected:
    std::unique_ptr<Node> buildBlueprint(const Node& declaration) const override {
        return wrapMultiplyOperandsWithCluster(SparsePlan::buildBlueprint(declaration));
    }

private:
    // Walks an already-built blueprint (from SparsePlan::buildBlueprint()),
    // wrapping both operands of every Multiply node -- however deeply
    // nested, and regardless of whether an operand is a simple leaf or
    // itself the result of a nested Add/Multiply -- in a Cluster node
    // running cluster_. Add nodes are left alone.
    std::unique_ptr<Node> wrapMultiplyOperandsWithCluster(std::unique_ptr<Node> node) const {
        if (node->kind == Node::Kind::Add || node->kind == Node::Kind::Multiply) {
            node->left = wrapMultiplyOperandsWithCluster(std::move(node->left));
            node->right = wrapMultiplyOperandsWithCluster(std::move(node->right));
            if (node->kind == Node::Kind::Multiply) {
                node->left = wrapWithCluster(std::move(node->left), cluster_);
                node->right = wrapWithCluster(std::move(node->right), cluster_);
            }
        }
        return node;
    }

    MergeCluster cluster_;
};

}  // namespace smooth
