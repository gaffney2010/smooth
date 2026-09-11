#pragma once

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

#include "smooth/algorithm_cluster.hpp"
#include "smooth/algorithm_cluster_zoo/binary_form_cluster.hpp"
#include "smooth/metrics.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/representation_zoo/row_values_representation.hpp"

namespace smooth {

// Reduces a number to a form where every column j that has anything in it
// holds exactly one bit -- i.e. every nonzero n_j (RowValuesRepresentation's
// own per-column total, see row_values_representation.hpp) is a single
// power of two, not an arbitrary magnitude.
//
// First runs BinaryFormCluster (binary_form_cluster.hpp), which collapses
// the whole number down into column 0's n_0 -- discarding whatever column
// layout it started with, so the final result only ever depends on the
// number's value, never on how it got there. Then, column by column
// starting at 0: while that column's n_j has more than one bit set,
// subtracts 3 from n_j and adds 1 to n_(j+1) -- value-preserving, since
// 3 * 3^j = 3^(j+1) -- until n_j is down to a single bit (or 0, i.e. gone
// entirely), then moves on to the next column. Since a carry only ever
// flows to a strictly higher column, and every column is visited in
// increasing order exactly once, nothing already finished is ever
// revisited.
//
// This always terminates without ever going negative: the descending
// sequence n_j, n_j - 3, n_j - 6, ... is confined to one residue class mod
// 3, and that class's smallest nonnegative member -- 0, 1, or 2 -- always
// has popcount <= 1. So the loop is guaranteed to stop at or before
// reaching it, never below.
//
// This can only be done directly on a RowValuesRepresentation, not any
// RepresentationBase: "subtract 3 from n_j" is an ordinary magnitude
// subtraction, and in general (n_j not conveniently lined up with 3's own
// bit pattern, 0b11) that needs a borrow across bits -- exactly the kind
// of arbitrary-precision subtraction this library has no bit-grid
// Transformation for (Transformation, transformation.hpp, only ever
// clears/carry-sets a small *fixed* set of bit offsets -- it has no way to
// express "however many bits borrowing this particular subtraction
// touches"). RowValuesRepresentation sidesteps the problem entirely by
// already storing n_j as a single number rather than exploded bits, so
// run() requires one and throws std::invalid_argument otherwise. This is
// also why TernaryFormCluster can't be built on top of
// TransformationAlgorithmCluster at all (unlike MergeCluster/
// BinaryFormCluster): its second phase isn't a Transformation, or any
// family of them, so it holds a BinaryFormCluster for its first phase and
// implements the second directly.
class TernaryFormCluster : public AlgorithmCluster {
public:
    const std::string& name() const override {
        static const std::string kName = "ternary_form";
        return kName;
    }

    // If `metrics` is given, increments "transformations_applied" once per
    // successful subtract-3/add-1 step, same counter name
    // BinaryFormCluster/MergeCluster use for their own per-step counts.
    void run(RepresentationBase& rep, const std::shared_ptr<Metrics>& metrics = nullptr) const override {
        auto* rowValues = dynamic_cast<RowValuesRepresentation*>(&rep);
        if (!rowValues) {
            throw std::invalid_argument("TernaryFormCluster::run() requires a RowValuesRepresentation");
        }

        binaryForm_.run(rep, metrics);

        int j = 0;
        while (rowValues->columnValue(j) != 0.0) {
            while (hasMoreThanOneBit(rowValues->columnValue(j))) {
                rowValues->addToColumnValue(j, -3.0);
                rowValues->addToColumnValue(j + 1, 1.0);
                if (metrics) metrics->increment("transformations_applied");
            }
            ++j;
        }
    }

private:
    static bool hasMoreThanOneBit(double n) {
        long long whole = static_cast<long long>(std::llround(n));
        return (whole & (whole - 1)) != 0;
    }

    BinaryFormCluster binaryForm_;
};

}  // namespace smooth
