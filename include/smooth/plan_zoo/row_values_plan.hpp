#pragma once

#include <memory>
#include <string>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"

namespace smooth {

// A Plan whose blueprint wraps every leaf in Ensure(RowValues) -- so every
// scalar()/number()/numberVia() leaf ends up as a RowValuesRepresentation,
// and combine() (inherited, unchanged, from Plan) runs its own
// addInPlace()/multiplyInPlace() (direct column-total accumulation for
// addition, and convolution -- long multiplication in base 3 -- for
// multiplication, see row_values_representation.hpp). buildBlueprint() is
// the entire strategy -- see plan.hpp's wrapLeavesWithEnsure() for what it
// does. Everything else (building, plan(), calculate(), combine(),
// Metrics, error-handling) is inherited as-is.
class RowValuesPlan : public Plan {
public:
    // See SparsePlan's constructor for why this is spelled out explicitly
    // rather than via `using Plan::Plan;`.
    explicit RowValuesPlan(std::shared_ptr<Metrics> metrics = nullptr) : Plan(std::move(metrics)) {}

    std::string name() const override { return "row_values"; }

protected:
    std::unique_ptr<Node> buildBlueprint(const Node& declaration) const override {
        return wrapLeavesWithEnsure(declaration, SmoothNumberBase::Representation::RowValues);
    }
};

}  // namespace smooth
