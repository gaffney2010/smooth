#pragma once

#include "smooth/reduction_zoo/transformation_reduction.hpp"
#include "smooth/transformation_zoo/ternary_carry_transformation.hpp"

namespace smooth {

// Runs TernaryCarryTransformation to a fixed point: so long as any
// column's total has more than one bit set, subtracts 3 from it and adds
// 1 to the next column, until every column has at most one bit. Needs no
// `allowed` predicate the way BinaryFormReduction does -- the
// transformation's own canApply() is already self-limiting.
//
// A TernaryCarryReduction *is* a TransformationReduction configured with
// just TernaryCarryTransformation and the name "ternary_carry" -- same
// shape as MergeReduction/BinaryFormReduction; see either for why the
// Transformation is a function-local static rather than an instance
// member.
//
// Only ever useful against a RowValuesRepresentation (the transformation
// throws otherwise). TernaryFormReduction (ternary_form_reduction.hpp)
// solves the same problem a different way, working against any
// representation.
class TernaryCarryReduction : public TransformationReduction {
public:
    // `slack`, if given, is passed straight through to TransformationReduction
    // -- see its own class comment.
    explicit TernaryCarryReduction(std::size_t slack = 0)
        : TransformationReduction({&ternaryCarryTransformation()}, "ternary_carry", nullptr, slack) {}

private:
    static const TernaryCarryTransformation& ternaryCarryTransformation() {
        static const TernaryCarryTransformation carry;
        return carry;
    }
};

}  // namespace smooth
