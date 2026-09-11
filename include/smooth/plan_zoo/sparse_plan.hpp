#pragma once

#include <memory>
#include <string>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"

namespace smooth {

// A Plan whose blueprint wraps every leaf in Ensure(Sparse) -- so every
// scalar()/number()/numberVia() leaf ends up as a SparseRepresentation,
// and combine() (inherited, unchanged, from Plan) runs its own
// addInPlace()/multiplyInPlace() (the pairwise-exponent-sum-with-carry
// strategy, see sparse_representation.hpp). buildBlueprint() is the entire
// strategy -- see plan.hpp's wrapLeavesWithEnsure() for what it does and
// Plan's class comment for how the resulting Ensure steps are executed and
// printed. Everything else (building, plan(), calculate(), combine(),
// Metrics, error-handling) is inherited as-is.
class SparsePlan : public Plan {
public:
    // Forwards to Plan's own (metrics) constructor explicitly, rather than
    // relying on `using Plan::Plan;` to inherit it -- see Signed<Base>'s
    // constructor (signed.hpp) for why that's clearer than relying on
    // inherited-constructor rules when the base constructor has a default
    // argument.
    explicit SparsePlan(std::shared_ptr<Metrics> metrics = nullptr) : Plan(std::move(metrics)) {}

    std::string name() const override { return "sparse"; }

protected:
    std::unique_ptr<Node> buildBlueprint(const Node& declaration) const override {
        return wrapLeavesWithEnsure(declaration, SmoothNumberBase::Representation::Sparse);
    }
};

}  // namespace smooth
