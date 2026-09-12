#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"
#include "smooth/reduction.hpp"
#include "smooth/reduction_zoo/merge_reduction.hpp"
#include "smooth/reduction_zoo/ternary_carry_reduction.hpp"

namespace smooth {

// Every other Plan in plan_zoo/ forces *every* leaf into one fixed target
// representation (see wrapLeavesWithEnsure() in plan.hpp). This one
// instead lets each numberVia() leaf keep whatever representation its own
// number is *actually already sitting in* -- not just canonical():
// SmoothNumberBase::isRepresentationCached() (smooth_number_base.hpp) is
// true for canonical() always, but also for any other representation a
// previous printSparse()/valueAs(RowValues)/etc. call left cached on that
// same number. Reusing one of those needs no conversion at all, real or
// counted -- Ensure(target) on an already-cached target is a genuine
// no-op (see Plan::compileBlueprintNode()'s Ensure case). Checked in a
// fixed order (Sparse, RowValues, Scalar, Dynamic); since canonical()
// itself is always cached and every fresh number starts out canonical()
// == Dynamic (and nothing in this library currently changes that -- see
// canonical()'s own comment), Dynamic is always this order's fallback,
// same as every leaf would get without this Plan at all -- the adaptive
// choice only ever actually *differs* once some other representation has
// already been materialized on that particular number by something else.
// A bare scalar() leaf has no existing number to check, so it defaults to
// Dynamic directly.
//
// Since combine() builds a multiply/add's result via
// `left.clone()->addInPlace(right)`/`multiplyInPlace(right)` -- and every
// representation's addInPlace()/multiplyInPlace() falls back to reading
// `other` through forEachSet() whenever it isn't the exact same concrete
// type (see e.g. RowValuesRepresentation::addInPlace() or
// SparseRepresentation's addBitsWithCarry()/multiplyBitsWithCarry()) --
// mixing representations across one expression's leaves is already safe;
// this Plan is just the first one in plan_zoo/ to actually do it on
// purpose.
//
// The reduction wrapped around each multiply operand (see
// wrapMultiplyOperandsWithReduction() in plan.hpp) is likewise chosen by
// that operand's own resulting representation, via representationOf()
// below:
//   - Sparse or Dynamic: MergeReduction -- a raw bit grid either way, so
//     the same reasoning as MergingSparsePlan applies.
//   - RowValues: TernaryCarryReduction -- the one reduction that only
//     ever works against a RowValuesRepresentation to begin with (its
//     transformation throws otherwise, see ternary_carry_transformation.
//     hpp), so it's only ever chosen here when the representation
//     actually is one.
//   - Scalar: no reduction. ScalarRepresentation only tolerates column
//     j == 0 (see ScalarRepresentation::requireColumnZero()); every
//     reduction here works by rewriting bits at other columns, so running
//     any of them against a Scalar-backed operand would just throw.
class RepresentationAwarePlan : public Plan {
public:
    explicit RepresentationAwarePlan(std::shared_ptr<Metrics> metrics = nullptr) : Plan(std::move(metrics)) {}

    std::string name() const override { return "representation_aware"; }

protected:
    std::unique_ptr<Node> buildBlueprint(const Node& declaration) const override {
        std::unique_ptr<Node> blueprint =
            wrapLeavesWithEnsure(declaration, [](const Node& leaf) { return chooseLeafTarget(leaf); });
        return wrapMultiplyOperandsWithReduction(std::move(blueprint), [](const Node& operand) -> const Reduction* {
            switch (representationOf(operand)) {
                case SmoothNumberBase::Representation::Sparse:
                case SmoothNumberBase::Representation::Dynamic:
                    return &mergeReduction();
                case SmoothNumberBase::Representation::RowValues:
                    return &ternaryCarryReduction();
                case SmoothNumberBase::Representation::Scalar:
                    return nullptr;
            }
            return nullptr;  // unreachable
        });
    }

private:
    // A numberVia() leaf reuses whatever representation its number
    // already has cached, preferring (in this fixed, arbitrary order)
    // Sparse, then RowValues, then Scalar, then -- always available,
    // since it's always cached -- Dynamic. A bare scalar() leaf has no
    // number to check, so it goes straight to Dynamic.
    static SmoothNumberBase::Representation chooseLeafTarget(const Node& leaf) {
        if (leaf.kind != Node::Kind::NumberLeaf) return SmoothNumberBase::Representation::Dynamic;
        SmoothNumberBase& n = *leaf.numberSource;
        for (auto candidate : {SmoothNumberBase::Representation::Sparse, SmoothNumberBase::Representation::RowValues,
                                SmoothNumberBase::Representation::Scalar}) {
            if (n.isRepresentationCached(candidate)) return candidate;
        }
        return SmoothNumberBase::Representation::Dynamic;
    }

    // The concrete representation an already-built blueprint node's own
    // step will end up computed in: an Ensure node's own target; an
    // Add/Multiply node's left operand's (matching combine()'s own
    // `left.clone()` -- see the class comment above); or, through a
    // Reduce node, whatever its wrapped child resolves to (a reduction
    // never changes which representation a node computes into).
    static SmoothNumberBase::Representation representationOf(const Node& node) {
        switch (node.kind) {
            case Node::Kind::Ensure:
                return node.ensureTarget;
            case Node::Kind::Reduce:
                return representationOf(*node.child);
            case Node::Kind::Add:
            case Node::Kind::Multiply:
                return representationOf(*node.left);
            case Node::Kind::ScalarLeaf:
            case Node::Kind::NumberLeaf:
                throw std::invalid_argument(
                    "RepresentationAwarePlan: expected a fully Ensure-wrapped blueprint node");
        }
        throw std::invalid_argument("RepresentationAwarePlan: unreachable node kind");
    }

    static const MergeReduction& mergeReduction() {
        static const MergeReduction reduction;
        return reduction;
    }
    static const TernaryCarryReduction& ternaryCarryReduction() {
        static const TernaryCarryReduction reduction;
        return reduction;
    }
};

}  // namespace smooth
