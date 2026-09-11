#pragma once

#include <memory>

#include "smooth/metrics.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/transformation_algorithm_cluster.hpp"
#include "smooth/transformation_zoo/split_transformation.hpp"

namespace smooth {

// Reduces a representation to its binary form: repeatedly splits the bit
// at (i, j+1) into (i, j) and (i+1, j) -- see SplitTransformation,
// transformation_zoo/split_transformation.hpp -- until every set bit sits
// in column 0, i.e. until the number is expressed purely as a sum of
// distinct powers of 2 (its ordinary binary representation) with none of
// the powers-of-3 column structure left. The value never changes -- every
// application is a Transformation, and every Transformation is
// value-preserving by construction (see transformation.hpp).
//
// This can't just be `TransformationAlgorithmCluster({&split}, "split")`
// run unbounded: split's natural fixed point doesn't stop at column 0 --
// nothing about SplitTransformation itself knows that column 0 is special,
// so left alone it just keeps splitting a column-0 bit into column -1,
// then -2, forever (see TransformationAlgorithmCluster's own class
// comment). So run() below bounds the search with `allowed`, which blocks
// every anchor below column 0 -- every bit that starts above column 0
// still gets split all the way down to it, and a bit already at column 0
// is never touched, since producing it required an anchor at column -1,
// which is disallowed.
class BinaryFormCluster {
public:
    BinaryFormCluster() : cluster_({&split_}, "binary_form") {}

    const std::string& name() const { return cluster_.name(); }

    void run(RepresentationBase& rep, const std::shared_ptr<Metrics>& metrics = nullptr) const {
        cluster_.run(rep, metrics, [](int, int j) { return j >= 0; });
    }

private:
    SplitTransformation split_;
    TransformationAlgorithmCluster cluster_;
};

}  // namespace smooth
