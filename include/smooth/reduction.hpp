#pragma once

#include <memory>
#include <string>

#include "smooth/metrics.hpp"
#include "smooth/representation_base.hpp"

namespace smooth {

// Common interface for every "value-preserving, run-to-completion
// reduction over a representation" utility this library has -- run a
// Transformation (or a sequence of them) against a representation, over
// and over, until it reaches some canonical/normal form (a fixed point:
// none of them can fire anywhere anymore). All of them live in
// reduction_zoo/: TransformationReduction, the generic engine (greedily
// apply a family of Transformations -- see transformation.hpp -- until
// none can fire anymore); MergeReduction/BinaryFormReduction/
// TernaryCarryReduction, named presets that are subclasses of it, each
// just fixing its own Transformation(s), name, and bound as constructor
// arguments; and TernaryFormReduction, which doesn't fit that mold at all
// -- it composes BinaryFormReduction and TernaryCarryReduction in
// sequence, two separate fixed-point searches rather than one, so it
// implements Reduction directly instead.
//
// This is deliberately the smallest interface that covers all of them:
// just "run me against a representation, optionally counting steps into a
// Metrics" plus a cosmetic name. It's what lets Plan's Reduce blueprint
// node (plan.hpp) hold a single polymorphic pointer type -- so a Plan
// subclass's buildBlueprint() can splice in *any* reduction this library
// has (or a bespoke, ad hoc TransformationReduction) via the same
// wrapWithReduction() call, without Plan itself needing to know which one.
class Reduction {
public:
    virtual ~Reduction() = default;

    // Purely cosmetic -- what Plan (plan.hpp) shows for a Reduce
    // blueprint step that runs this reduction, e.g. "reduce(merge)". Every
    // reduction_zoo/ preset hardcodes its own name (no constructor
    // parameter for it, same as MergeTransformation/SplitTransformation
    // hardcode their own offsets) -- only the generic
    // TransformationReduction engine takes one explicitly, since it has no
    // fixed identity of its own.
    virtual const std::string& name() const = 0;

    // Runs this reduction against `rep` in place, to completion. If
    // `metrics` is given, implementations increment
    // "transformations_applied" once per successful step -- the same
    // counter name every reduction in this library uses, so a Plan's
    // Metrics tally stays comparable regardless of which reduction ran.
    virtual void run(RepresentationBase& rep, const std::shared_ptr<Metrics>& metrics = nullptr) const = 0;
};

}  // namespace smooth
