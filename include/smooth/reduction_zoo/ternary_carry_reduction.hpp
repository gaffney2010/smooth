#pragma once

#include "smooth/reduction_zoo/transformation_reduction.hpp"
#include "smooth/transformation_zoo/ternary_carry_transformation.hpp"

namespace smooth {

// Runs TernaryCarryTransformation (transformation_zoo/) to a fixed point:
// so long as any column's total has more than one bit set, subtracts 3
// from it and adds 1 to the next column, until every column is left with
// at most one bit. See TernaryCarryTransformation's own class comment for
// why this terminates on its own, no bound required -- unlike
// BinaryFormReduction's SplitTransformation, TernaryCarryTransformation's
// own canApply() is already self-limiting (a column with <= 1 bit set
// simply stops being a candidate), so there's no need for an `allowed`
// predicate here the way BinaryFormReduction needs one.
//
// A TernaryCarryReduction *is* a TransformationReduction configured with
// just TernaryCarryTransformation and the name "ternary_carry" -- it adds
// no behavior of its own, so it's a subclass, same as MergeReduction
// (merge_reduction.hpp) and BinaryFormReduction (binary_form_reduction.hpp);
// see either one's class comment for why the Transformation it's
// configured with is a function-local static rather than an instance
// member.
//
// This only ever does anything useful against a RowValuesRepresentation
// (TernaryCarryTransformation throws std::invalid_argument against
// anything else) -- TernaryFormReduction (ternary_form_reduction.hpp) is
// what combines this with BinaryFormReduction's own reduction into the
// full, usable two-phase pipeline.
class TernaryCarryReduction : public TransformationReduction {
public:
    TernaryCarryReduction() : TransformationReduction({&ternaryCarryTransformation()}, "ternary_carry") {}

private:
    static const TernaryCarryTransformation& ternaryCarryTransformation() {
        static const TernaryCarryTransformation carry;
        return carry;
    }
};

}  // namespace smooth
