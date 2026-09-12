#pragma once

#include <stdexcept>
#include <utility>
#include <vector>

#include "smooth/representation_base.hpp"
#include "smooth/smooth_number_base.hpp"

namespace smooth {

// A Transformation is any distinct unit of work that doesn't change a
// 3-smooth number's value: check whether it can fire at a given anchor
// (i, j), apply it, and report which cells its own application changed
// (its "landings") -- the only cells worth re-examining afterward for
// newly created opportunities, since applying a Transformation can only
// ever create new opportunities at cells it touched, never anywhere else.
// Reduction (reduction.hpp) is the other half of this idea: repeated
// application of one or more Transformations until some condition is met
// (typically: none of them can fire anywhere anymore).
//
// This is deliberately the smallest possible interface, because
// "distinct unit of work" covers genuinely different shapes. Every
// Transformation this library had until now fit one very specific shape:
// a fixed list of input offsets that must all be 1 (cleared by applying),
// and a fixed list of output offsets that get carry-set to 1 -- see
// OffsetTransformation below, which is exactly that shape, pulled out as
// its own concrete class once it became clear it was only a special
// case, not the general one. TernaryCarryTransformation
// (transformation_zoo/ternary_carry_transformation.hpp) is the case that
// forced the split: "subtract 3 from column j, add 1 to column j+1" has
// no fixed set of bit offsets at all -- its own precondition ("column j
// has more than one bit set") depends on an entire column's aggregate
// magnitude, not a handful of fixed cells -- so it implements
// Transformation directly instead of going through OffsetTransformation.
class Transformation {
public:
    virtual ~Transformation() = default;

    // Whether this transformation can fire anchored at (i, j) against
    // `rep`. Must not mutate `rep`.
    virtual bool canApply(const RepresentationBase& rep, int i, int j) const = 0;

    // Applies this transformation, anchored at (i, j), to `rep` in place.
    // Precondition: canApply(rep, i, j). Returns every cell this
    // application actually changed -- see the class comment above for why
    // that's exactly what's worth re-examining afterward, and
    // TransformationReduction (reduction_zoo/transformation_reduction.hpp)
    // for the one place that currently relies on it.
    virtual std::vector<std::pair<int, int>> applyAndReportLandings(RepresentationBase& rep, int i, int j) const = 0;

    // Given that cell (i, j) just changed, returns every anchor -- for
    // *this* transformation specifically -- that might now be worth
    // (re-)checking with canApply(). For OffsetTransformation this is
    // purely arithmetic (see its override); for a transformation like
    // TernaryCarryTransformation, whose notion of "input" is a whole
    // column rather than a fixed cell, it can be just as simple (any
    // change anywhere in column j means column j itself is worth
    // rechecking) but for a fundamentally different reason. Either way,
    // this is what lets TransformationReduction seed and grow its
    // worklist without knowing anything about what shape a particular
    // Transformation actually is.
    virtual std::vector<std::pair<int, int>> affectedAnchors(int i, int j) const = 0;
};

// The shape every Transformation this library had until now fits: a fixed
// list of **input** offsets (relative to the anchor), each of which must
// currently hold a 1 -- applying the transformation always clears every
// one of them back to 0 -- and a fixed list of **output** offsets, each
// of which gets set to 1, ripple-carrying up the row axis (exactly like
// ordinary addition -- see addSingleBitWithCarry() in
// representation_base.hpp) if that position is already occupied. Because
// an occupied output never blocks the transformation (it just carries),
// canApply() only ever needs to check the inputs.
//
// canApply()/apply()/applyAndReportLandings() are also templated over
// anything that looks like a bit grid -- `bool get(int, int) const` and
// `set(int, int, bool)` -- so the exact same OffsetTransformation works
// directly on a SmoothNumberBase (get()/set() there correctly invalidate
// every representation but the canonical one, same as any other set()
// call) or directly on a bare RepresentationBase. The non-template
// canApply()/applyAndReportLandings() overrides below (required by
// Transformation) are what TransformationReduction and Plan actually
// call, through a `const Transformation&`/`const Transformation*` -- a
// plain RepresentationBase is all either of them ever has, and all either
// of them ever needs, since a Plan's blueprint execution works with
// representations directly, never a SmoothNumberBase. The two
// non-template overrides just instantiate the template versions with
// Bits = RepresentationBase -- overload resolution always prefers a
// non-template exact match over a template instantiation when both are
// viable, so calling these methods through a concrete OffsetTransformation
// object directly (as SmoothNumberBase::applyTransformation() does, with
// Bits = SmoothNumberBase) is completely unaffected; the two versions
// simply serve two different call sites.
//
// This is deliberately concrete, not itself an interface: every
// fixed-offset transformation this library has fits this one shape
// (clear a fixed set of 1s, carry-set a fixed set of new 1s), so there's
// no further virtual dispatch needed within it. Building a custom one is
// just `OffsetTransformation({...input offsets...}, {...output
// offsets...})`. See transformation_zoo/ for the named presets
// (MergeTransformation/SplitTransformation/SpreadTransformation), built
// exactly that way.
class OffsetTransformation : public Transformation {
public:
    OffsetTransformation(std::vector<std::pair<int, int>> inputs, std::vector<std::pair<int, int>> outputs)
        : inputs_(std::move(inputs)), outputs_(std::move(outputs)) {}

    // The only precondition: every input offset currently holds a 1. An
    // already-occupied output offset is never a reason to reject --
    // apply() carries through it instead.
    template <typename Bits>
    bool canApply(const Bits& n, int i, int j) const {
        for (const auto& offset : inputs_) {
            if (!n.get(i + offset.first, j + offset.second)) return false;
        }
        return true;
    }

    bool canApply(const RepresentationBase& rep, int i, int j) const override {
        return canApply<RepresentationBase>(rep, i, j);
    }

    // Clears every input offset, then carry-sets every output offset, in
    // order. Precondition: canApply(n, i, j). Callers that haven't just
    // checked it should go through SmoothNumberBase::applyTransformation()
    // instead (see smooth_number_base.hpp), which checks first and throws
    // if it doesn't hold.
    template <typename Bits>
    void apply(Bits& n, int i, int j) const {
        applyAndReportLandings(n, i, j);
    }

    // Same as apply(), but also returns where each output actually landed
    // after carrying -- the "frontier" of what changed. Clearing an input
    // can only ever remove an opportunity for some other transformation
    // (never create one), so these landings are the *only* cells worth
    // re-examining for new opportunities after this call.
    template <typename Bits>
    std::vector<std::pair<int, int>> applyAndReportLandings(Bits& n, int i, int j) const {
        for (const auto& offset : inputs_) {
            n.set(i + offset.first, j + offset.second, false);
        }
        std::vector<std::pair<int, int>> landings;
        landings.reserve(outputs_.size());
        for (const auto& offset : outputs_) {
            landings.push_back(carrySet(n, i + offset.first, j + offset.second));
        }
        return landings;
    }

    std::vector<std::pair<int, int>> applyAndReportLandings(RepresentationBase& rep, int i, int j) const override {
        return applyAndReportLandings<RepresentationBase>(rep, i, j);
    }

    // The anchor that would place a just-changed cell at (i, j) exactly on
    // one of this transformation's own input offsets -- one candidate per
    // input offset, same as before this was pulled behind a virtual
    // method.
    std::vector<std::pair<int, int>> affectedAnchors(int i, int j) const override {
        std::vector<std::pair<int, int>> anchors;
        anchors.reserve(inputs_.size());
        for (const auto& offset : inputs_) {
            anchors.emplace_back(i - offset.first, j - offset.second);
        }
        return anchors;
    }

    // Exposed so external code can check a transformation's own
    // well-formedness directly -- e.g. that summing 2^i*3^j over the
    // inputs equals the same sum over the outputs, which is exactly what
    // "value-preserving" means, without needing to construct a
    // SmoothNumberBase or call apply() at all.
    const std::vector<std::pair<int, int>>& inputs() const { return inputs_; }
    const std::vector<std::pair<int, int>>& outputs() const { return outputs_; }

private:
    // Sets (i, j) to 1, ripple-carrying up the row axis within column j
    // whenever a cell is already occupied -- moving a second 1 into an
    // occupied cell is the same as moving that bit up to the next row
    // (2 * 2^i * 3^j = 2^(i+1) * 3^j), the same one-term carry
    // addSingleBitWithCarry() (representation_base.hpp) performs for
    // ordinary addition, just through Bits's own get()/set() instead of a
    // specific RepresentationBase. Returns where it finally landed.
    template <typename Bits>
    static std::pair<int, int> carrySet(Bits& n, int i, int j) {
        while (n.get(i, j)) {
            n.set(i, j, false);
            ++i;
        }
        n.set(i, j, true);
        return {i, j};
    }

    std::vector<std::pair<int, int>> inputs_;
    std::vector<std::pair<int, int>> outputs_;
};

inline void SmoothNumberBase::applyTransformation(const OffsetTransformation& t, int i, int j) {
    if (!t.canApply(*this, i, j)) {
        throw std::invalid_argument(
            "SmoothNumberBase::applyTransformation: the transformation cannot be applied at that (i, j)");
    }
    t.apply(*this, i, j);
}

}  // namespace smooth
