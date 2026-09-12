#pragma once

#include <memory>
#include <string>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"

namespace smooth {

// A Plan whose blueprint wraps every leaf in Ensure(RowValues) -- so every
// leaf ends up a RowValuesRepresentation, and combine() runs its
// addInPlace()/multiplyInPlace() (direct column-total accumulation, and
// base-3 convolution, respectively).
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
