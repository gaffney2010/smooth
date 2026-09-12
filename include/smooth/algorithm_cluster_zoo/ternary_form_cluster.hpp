#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include "smooth/algorithm_cluster.hpp"
#include "smooth/algorithm_cluster_zoo/binary_form_cluster.hpp"
#include "smooth/algorithm_cluster_zoo/ternary_carry_cluster.hpp"
#include "smooth/metrics.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/representation_zoo/row_values_representation.hpp"

namespace smooth {

// Reduces a number to a form where every column j that has anything in it
// holds exactly one bit -- i.e. every nonzero n_j (RowValuesRepresentation's
// own per-column total, see row_values_representation.hpp) is a single
// power of two, not an arbitrary magnitude.
//
// Two phases, run in sequence: first BinaryFormCluster, which collapses
// the whole number down into column 0's n_0 -- discarding whatever column
// layout it started with, so the final result only ever depends on the
// number's value, never on how it got there -- then TernaryCarryCluster,
// which works column by column from there, subtracting 3 from a column
// and adding 1 to the next wherever a column still has more than one bit
// set, until none do. See TernaryCarryTransformation
// (transformation_zoo/ternary_carry_transformation.hpp) for the actual
// unit of work each step of the second phase performs, and its own class
// comment for why it's a Transformation but not an OffsetTransformation,
// and why the whole reduction is guaranteed to terminate.
//
// This can only be done directly on a RowValuesRepresentation -- see
// TernaryCarryTransformation's own class comment for why -- so run()
// checks and throws std::invalid_argument immediately, unconditionally,
// rather than relying on TernaryCarryTransformation's own (otherwise
// equivalent) check ever actually being reached: if `rep` happens to have
// nothing set in it yet, BinaryFormCluster's own reduction is a silent
// no-op regardless of representation kind, and TernaryCarryCluster's
// worklist would never examine a single candidate -- so without this
// upfront check, calling run() against the wrong kind of representation
// could easily fail to throw at all, purely by chance of what's currently
// in it.
class TernaryFormCluster : public AlgorithmCluster {
public:
    const std::string& name() const override {
        static const std::string kName = "ternary_form";
        return kName;
    }

    void run(RepresentationBase& rep, const std::shared_ptr<Metrics>& metrics = nullptr) const override {
        if (!dynamic_cast<RowValuesRepresentation*>(&rep)) {
            throw std::invalid_argument("TernaryFormCluster::run() requires a RowValuesRepresentation");
        }
        binaryForm_.run(rep, metrics);
        ternaryCarry_.run(rep, metrics);
    }

private:
    BinaryFormCluster binaryForm_;
    TernaryCarryCluster ternaryCarry_;
};

}  // namespace smooth
