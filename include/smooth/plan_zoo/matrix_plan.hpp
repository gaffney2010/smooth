#pragma once

#include <memory>
#include <string>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"

namespace smooth {

// A Plan whose blueprint wraps every leaf in Ensure(Dynamic) -- called
// "matrix" here, matching what this library calls its grid-shaped
// representation. Every leaf ends up a DynamicMatrixRepresentation, and
// combine() runs its addInPlace()/multiplyInPlace() (the same strategy
// Sparse uses, since a raw bit grid works the same regardless of whether
// it's backed by a std::set or an array).
//
// Ensure(Dynamic) on a numberVia() leaf is typically a no-op: every fresh
// number's canonical representation already *is* Dynamic, so
// representationAs(Dynamic) just clones it directly -- no
// convert_dynamic_to_dynamic counter exists.
class MatrixPlan : public Plan {
public:
    // See SparsePlan's constructor for why this is spelled out explicitly
    // rather than via `using Plan::Plan;`.
    explicit MatrixPlan(std::shared_ptr<Metrics> metrics = nullptr) : Plan(std::move(metrics)) {}

    std::string name() const override { return "matrix"; }

protected:
    std::unique_ptr<Node> buildBlueprint(const Node& declaration) const override {
        return wrapLeavesWithEnsure(declaration, SmoothNumberBase::Representation::Dynamic);
    }
};

}  // namespace smooth
