#pragma once

#include <memory>
#include <string>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"

namespace smooth {

// A Plan whose blueprint wraps every leaf in Ensure(Sparse) -- so every
// leaf ends up a SparseRepresentation, and combine() (inherited from
// Plan) runs its addInPlace()/multiplyInPlace(). buildBlueprint() is the
// entire strategy; everything else is inherited as-is.
class SparsePlan : public Plan {
public:
    // Forwards to Plan's own constructor explicitly -- see Signed<Base>'s
    // constructor for why, when the base constructor has a default
    // argument.
    explicit SparsePlan(std::shared_ptr<Metrics> metrics = nullptr) : Plan(std::move(metrics)) {}

    std::string name() const override { return "sparse"; }

protected:
    std::unique_ptr<Node> buildBlueprint(const Node& declaration) const override {
        return wrapLeavesWithEnsure(declaration, SmoothNumberBase::Representation::Sparse);
    }
};

}  // namespace smooth
