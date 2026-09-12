#pragma once

#include "smooth/reduction_zoo/transformation_reduction.hpp"
#include "smooth/transformation_zoo/split_transformation.hpp"

namespace smooth {

// Reduces a representation to its binary form: repeatedly splits the bit
// at (i, j+1) into (i, j) and (i+1, j) until every set bit sits in column
// 0 -- its ordinary binary representation, with none of the powers-of-3
// column structure left.
//
// This can't just be `TransformationReduction({&split}, "split")` run
// unbounded: nothing about SplitTransformation knows column 0 is special,
// so left alone it keeps splitting a column-0 bit into -1, -2, forever
// (see TransformationReduction's own class comment). So the base
// constructor call passes `allowed`, blocking every anchor below column
// 0 -- a bit above column 0 still gets split all the way down, but one
// already there is never touched.
//
// Like MergeReduction, this *is* a TransformationReduction configured a
// particular way -- name()/run() are simply inherited. See
// MergeReduction's own comment for why splitTransformation() is a
// function-local static rather than an instance member.
class BinaryFormReduction : public TransformationReduction {
public:
    // `slack`, if given, is passed straight through to TransformationReduction
    // -- see its own class comment.
    explicit BinaryFormReduction(std::size_t slack = 0)
        : TransformationReduction({&splitTransformation()}, "binary_form", [](int, int j) { return j >= 0; },
                                   slack) {}

private:
    static const SplitTransformation& splitTransformation() {
        static const SplitTransformation split;
        return split;
    }
};

}  // namespace smooth
