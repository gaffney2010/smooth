#pragma once

#include <memory>
#include <string>

#include "smooth/metrics.hpp"
#include "smooth/representation_base.hpp"

namespace smooth {

// Common interface for every "value-preserving, run-to-completion
// reduction over a representation" utility this library has. All of them
// live in algorithm_cluster_zoo/: TransformationAlgorithmCluster, the
// generic engine (greedily apply a family of Transformations -- see
// transformation.hpp -- until none can fire anymore); MergeCluster/
// BinaryFormCluster/TernaryCarryCluster, named presets that are subclasses
// of it, each just fixing its own Transformation(s), name, and bound as
// constructor arguments; and TernaryFormCluster, which doesn't fit that
// mold at all -- it composes BinaryFormCluster and TernaryCarryCluster in
// sequence, two separate fixed-point searches rather than one, so it
// implements AlgorithmCluster directly instead.
//
// This is deliberately the smallest interface that covers all of them:
// just "run me against a representation, optionally counting steps into a
// Metrics" plus a cosmetic name. It's what lets Plan's Cluster blueprint
// node (plan.hpp) hold a single polymorphic pointer type -- so a Plan
// subclass's buildBlueprint() can splice in *any* cluster this library
// has (or a bespoke, ad hoc TransformationAlgorithmCluster) via the same
// wrapWithCluster() call, without Plan itself needing to know which one.
class AlgorithmCluster {
public:
    virtual ~AlgorithmCluster() = default;

    // Purely cosmetic -- what Plan (plan.hpp) shows for a Cluster
    // blueprint step that runs this cluster, e.g. "cluster(merge)". Every
    // algorithm_cluster_zoo/ preset hardcodes its own name (no constructor
    // parameter for it, same as MergeTransformation/SplitTransformation
    // hardcode their own offsets) -- only the generic
    // TransformationAlgorithmCluster engine takes one explicitly, since it
    // has no fixed identity of its own.
    virtual const std::string& name() const = 0;

    // Runs this cluster's reduction against `rep` in place, to completion.
    // If `metrics` is given, implementations increment
    // "transformations_applied" once per successful step -- the same
    // counter name every cluster in this library uses, so a Plan's
    // Metrics tally stays comparable regardless of which cluster ran.
    virtual void run(RepresentationBase& rep, const std::shared_ptr<Metrics>& metrics = nullptr) const = 0;
};

}  // namespace smooth
