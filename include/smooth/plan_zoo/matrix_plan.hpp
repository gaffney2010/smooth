#pragma once

#include <memory>
#include <string>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"

namespace smooth {

// A Plan whose blueprint wraps every leaf in Ensure(Dynamic) -- referred to
// as "matrix" here, matching what this library calls its grid/matrix-
// shaped representation now that the old fixed-size MatrixRepresentation
// has been superseded by DynamicMatrixRepresentation -- so every
// scalar()/number()/numberVia() leaf ends up as a DynamicMatrixRepresentation,
// and combine() (inherited, unchanged, from Plan) runs its own
// addInPlace()/multiplyInPlace() (the same pairwise-exponent-sum-with-
// carry strategy Sparse uses, since a raw bit grid needs the same approach
// regardless of whether it's backed by a std::set or an array).
// buildBlueprint() is the entire strategy -- see plan.hpp's
// wrapLeavesWithEnsure() for what it does. Everything else (building,
// plan(), calculate(), combine(), Metrics, error-handling) is inherited
// as-is.
//
// Ensure(Dynamic) on a numberVia() leaf is typically a no-op conversion-
// wise: every fresh number's canonical representation already *is*
// Dynamic, so SmoothNumberBase::representationAs(Dynamic) just clones it
// directly -- no convert_dynamic_to_dynamic counter exists, since ensure()
// never fires it when target == canonical.
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
