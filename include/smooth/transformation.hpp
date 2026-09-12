#pragma once

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "smooth/representation_base.hpp"
#include "smooth/smooth_number_base.hpp"

namespace smooth {

// Forward-declared, not included: AtomicTransformation
// (atomic_transformation.hpp) is itself an OffsetTransformation, so the
// dependency can't run the other way too.
class AtomicTransformation;

// One atom, anchored at a specific (i, j) -- what OffsetTransformation::
// atomize() (below) returns a sequence of.
struct AtomApplication {
    std::shared_ptr<const AtomicTransformation> atom;
    int i;
    int j;
};

// A Transformation is any distinct unit of work that doesn't change a
// 3-smooth number's value: check whether it can fire at a given anchor
// (i, j), apply it, and report which cells changed (its "landings") --
// the only cells worth re-examining afterward. Reduction (reduction.hpp)
// is the other half: repeated application until some condition is met.
//
// Deliberately the smallest possible interface, since "distinct unit of
// work" covers genuinely different shapes: most transformations fit one
// specific shape -- a fixed list of input offsets that must all be 1, and
// a fixed list of output offsets that get carry-set to 1 -- see
// OffsetTransformation below. TernaryCarryTransformation
// (transformation_zoo/ternary_carry_transformation.hpp) is the case that
// doesn't: its precondition depends on an entire column's magnitude, not
// a handful of fixed cells, so it implements Transformation directly.
class Transformation {
public:
    virtual ~Transformation() = default;

    // Whether this transformation can fire anchored at (i, j) against
    // `rep`. Must not mutate `rep`.
    virtual bool canApply(const RepresentationBase& rep, int i, int j) const = 0;

    // Applies this transformation, anchored at (i, j), to `rep` in place.
    // Precondition: canApply(rep, i, j). Returns every cell this
    // application actually changed.
    virtual std::vector<std::pair<int, int>> applyAndReportLandings(RepresentationBase& rep, int i, int j) const = 0;

    // Given that cell (i, j) just changed, returns every anchor -- for
    // *this* transformation specifically -- that might now be worth
    // (re-)checking with canApply(). Lets TransformationReduction seed and
    // grow its worklist without knowing anything about the transformation's
    // shape.
    virtual std::vector<std::pair<int, int>> affectedAnchors(int i, int j) const = 0;
};

// The shape most Transformations fit: a fixed list of **input** offsets
// (relative to the anchor), each of which must currently hold a 1 --
// applying always clears them back to 0 -- and a fixed list of **output**
// offsets, each set to 1, ripple-carrying up the row axis (like ordinary
// addition -- see addSingleBitWithCarry() in representation_base.hpp) if
// already occupied. An occupied output never blocks the transformation,
// so canApply() only ever checks the inputs.
//
// canApply()/apply()/applyAndReportLandings() are templated over anything
// that looks like a bit grid (`get(int,int)`/`set(int,int,bool)`), so the
// same object works on a SmoothNumberBase or a bare RepresentationBase.
// The non-template overrides below are what TransformationReduction and
// Plan call, through a `const Transformation&`. SmoothNumberBase::
// applyTransformation() (below) also goes through the RepresentationBase
// path now, working directly against whichever representation is
// currently canonical -- this is what lets a per-representation-
// specialized atom (AtomicTransformation, atomic_transformation.hpp)
// actually get its specialized behavior for ordinary
// n.applyTransformation(t, i, j) calls. Calling canApply()/
// applyAndReportLandings() on some other duck-typed Bits (including a
// bare SmoothNumberBase) still goes through the template -- overload
// resolution prefers a non-template exact match over a template
// instantiation when both are viable, so the two versions serve
// different call sites depending on what's actually passed.
//
// Deliberately concrete, not itself an interface: every fixed-offset
// transformation this library has fits this one shape. Building a custom
// one is just `OffsetTransformation({...input offsets...}, {...output
// offsets...})`. See transformation_zoo/ for the named presets.
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
    // instead, which checks first and throws if it doesn't hold.
    template <typename Bits>
    void apply(Bits& n, int i, int j) const {
        applyAndReportLandings(n, i, j);
    }

    // Same as apply(), but also returns where each output actually landed
    // after carrying -- the only cells worth re-examining for new
    // opportunities after this call.
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
    // one of this transformation's own input offsets.
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
    // inputs equals the same sum over the outputs -- without ever
    // constructing a SmoothNumberBase or calling apply().
    const std::vector<std::pair<int, int>>& inputs() const { return inputs_; }
    const std::vector<std::pair<int, int>>& outputs() const { return outputs_; }

    // Decomposes this transformation, anchored at (i, j), into a sequence
    // of AtomApplications whose combined effect reproduces this
    // transformation's own. Not every OffsetTransformation has one -- the
    // base implementation throws -- since a one-off custom
    // OffsetTransformation has no way to know its own decomposition.
    virtual std::vector<AtomApplication> atomize(int i, int j) const {
        throw std::invalid_argument("OffsetTransformation::atomize(): no decomposition defined for this transformation");
    }

    // Same as apply()/applyAndReportLandings() above, but when `viaAtoms`
    // is true, decomposes via atomize() and applies the resulting atoms in
    // sequence instead of this transformation's own direct logic. Since
    // every atom's own applyAndReportLandings() is specialized per
    // representation (atomic_transformation.hpp), this is the hook for
    // hyper-optimizing a composite transformation later, with no change to
    // canApply() or the direct path. Only meaningful against a
    // RepresentationBase, unlike the templated Bits-based versions above.
    //
    // Declared here but defined in atomic_transformation.hpp, once
    // AtomicTransformation is fully known.
    std::vector<std::pair<int, int>> applyAndReportLandings(RepresentationBase& rep, int i, int j,
                                                             bool viaAtoms) const;

    void apply(RepresentationBase& rep, int i, int j, bool viaAtoms) const {
        applyAndReportLandings(rep, i, j, viaAtoms);
    }

private:
    // Sets (i, j) to 1, ripple-carrying up the row axis whenever a cell is
    // already occupied -- moving a second 1 into an occupied cell is the
    // same as moving that bit up to the next row
    // (2 * 2^i * 3^j = 2^(i+1) * 3^j). Returns where it finally landed.
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

inline void SmoothNumberBase::applyTransformation(const OffsetTransformation& t, int i, int j, bool atomize) {
    RepresentationBase& rep = repFor(canonical_);
    if (!t.canApply(rep, i, j)) {
        throw std::invalid_argument(
            "SmoothNumberBase::applyTransformation: the transformation cannot be applied at that (i, j)");
    }
    t.apply(rep, i, j, atomize);
    invalidateAllExcept(canonical_);
}

}  // namespace smooth
