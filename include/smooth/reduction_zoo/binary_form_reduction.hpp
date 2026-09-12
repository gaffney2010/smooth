#pragma once

#include "smooth/reduction_zoo/transformation_reduction.hpp"
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
// This can't just be `TransformationReduction({&split}, "split")` run
// unbounded: split's natural fixed point doesn't stop at column 0 --
// nothing about SplitTransformation itself knows that column 0 is
// special, so left alone it just keeps splitting a column-0 bit into
// column -1, then -2, forever (see TransformationReduction's own class
// comment). So the base class constructor call below passes `allowed`,
// which blocks every anchor below column 0 -- every bit that starts
// above column 0 still gets split all the way down to it, and a bit
// already at column 0 is never touched, since producing it would require
// an anchor at column -1, which is disallowed.
//
// Like MergeReduction (merge_reduction.hpp), a BinaryFormReduction *is* a
// TransformationReduction configured a particular way -- just
// SplitTransformation, named "binary_form", bounded to column >= 0 -- so
// name()/run() are simply inherited. See MergeReduction's own class
// comment for why splitTransformation() below is a function-local static
// rather than an instance member: a plain member's address wouldn't be
// safe to hand to the base constructor, since base classes finish
// constructing before any of a derived class's own members even begin.
class BinaryFormReduction : public TransformationReduction {
public:
    BinaryFormReduction()
        : TransformationReduction({&splitTransformation()}, "binary_form", [](int, int j) { return j >= 0; }) {}

private:
    static const SplitTransformation& splitTransformation() {
        static const SplitTransformation split;
        return split;
    }
};

}  // namespace smooth
