#pragma once

#include <memory>
#include <string>

#include "smooth/metrics.hpp"
#include "smooth/representation_base.hpp"

namespace smooth {

// Common interface for every "value-preserving, run-to-completion
// reduction over a representation" this library has -- run a
// Transformation (or a sequence of them) against a representation, over
// and over, until it reaches a fixed point (none of them can fire
// anywhere anymore). Every reduction lives in reduction_zoo/:
// TransformationReduction, the generic engine, plus named presets built
// from it (MergeReduction/BinaryFormReduction/TernaryCarryReduction); and
// TernaryFormReduction/StaircaseReduction, which each need their own
// bespoke search and implement Reduction directly instead.
//
// Deliberately the smallest interface that covers all of them: "run me
// against a representation, optionally counting steps into a Metrics"
// plus a cosmetic name. This is what lets Plan's Reduce blueprint node
// (plan.hpp) hold a single polymorphic pointer type, splicing in *any*
// reduction without Plan itself needing to know which one.
class Reduction {
public:
    virtual ~Reduction() = default;

    // Purely cosmetic -- what Plan shows for a Reduce step, e.g.
    // "reduce(merge)". Every preset hardcodes its own name; only the
    // generic TransformationReduction engine takes one explicitly.
    virtual const std::string& name() const = 0;

    // Runs this reduction against `rep` in place, to completion. If
    // `metrics` is given, implementations increment
    // "transformations_applied" once per successful step -- the same
    // counter name every reduction here uses, so tallies stay comparable.
    virtual void run(RepresentationBase& rep, const std::shared_ptr<Metrics>& metrics = nullptr) const = 0;
};

}  // namespace smooth
