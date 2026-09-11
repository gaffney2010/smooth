#pragma once

#include <stdexcept>

#include "smooth/smooth_number_base.hpp"

namespace smooth {

// A value-preserving rewrite of a 3-smooth number's bit grid. The same
// value can be held by more than one bit grid: since
// 2^i * 3^j + 2^(i+1) * 3^j = 2^i * 3^j * (1 + 2) = 2^i * 3^(j+1), the two
// bits at (i, j) and (i+1, j) can be traded for the single bit at
// (i, j+1) (see MergeTransformation/SplitTransformation below) without
// changing value() at all. A Transformation is one such trade, checked and
// applied at a specific (i, j):
//
// - canApply(n, i, j): whether every bit this transformation would touch
//   is currently in the exact state it needs to start in -- both the bits
//   being cleared (must be 1) and the bits being set (must be 0), so a
//   transformation never has "nothing to do" or silently overwrites a bit
//   that was already there.
// - apply(n, i, j): performs the trade. Precondition: canApply(n, i, j).
//   Callers that haven't just checked it should go through
//   SmoothNumberBase::applyTransformation() instead (see
//   smooth_number_base.hpp), which checks first and throws
//   std::invalid_argument if the transformation doesn't apply, rather than
//   applying it regardless.
//
// Both operate directly through SmoothNumberBase's own get()/set(), not
// RepresentationBase -- so a transformation that actually changes a bit
// correctly invalidates every representation but the canonical one, the
// same as any other set() call would.
class Transformation {
public:
    virtual ~Transformation() = default;

    virtual bool canApply(const SmoothNumberBase& n, int i, int j) const = 0;

    virtual void apply(SmoothNumberBase& n, int i, int j) const = 0;
};

// Merges the two bits at (i, j) and (i+1, j) into the single bit at
// (i, j+1): 2^i*3^j + 2^(i+1)*3^j = 2^i*3^(j+1). Applicable only when both
// source bits are set and the destination bit is clear.
class MergeTransformation : public Transformation {
public:
    bool canApply(const SmoothNumberBase& n, int i, int j) const override {
        return n.get(i, j) && n.get(i + 1, j) && !n.get(i, j + 1);
    }

    void apply(SmoothNumberBase& n, int i, int j) const override {
        n.set(i, j, false);
        n.set(i + 1, j, false);
        n.set(i, j + 1, true);
    }
};

// The reverse of MergeTransformation: splits the bit at (i, j+1) into the
// two bits at (i, j) and (i+1, j). Applicable only when the source bit is
// set and both destination bits are clear.
class SplitTransformation : public Transformation {
public:
    bool canApply(const SmoothNumberBase& n, int i, int j) const override {
        return n.get(i, j + 1) && !n.get(i, j) && !n.get(i + 1, j);
    }

    void apply(SmoothNumberBase& n, int i, int j) const override {
        n.set(i, j + 1, false);
        n.set(i, j, true);
        n.set(i + 1, j, true);
    }
};

inline void SmoothNumberBase::applyTransformation(const Transformation& t, int i, int j) {
    if (!t.canApply(*this, i, j)) {
        throw std::invalid_argument(
            "SmoothNumberBase::applyTransformation: the transformation cannot be applied at that (i, j)");
    }
    t.apply(*this, i, j);
}

}  // namespace smooth
