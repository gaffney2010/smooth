#pragma once

#include <stdexcept>
#include <utility>
#include <vector>

#include "smooth/smooth_number_base.hpp"

namespace smooth {

// A value-preserving rewrite of a 3-smooth number's bit grid, anchored at
// a given (i, j): a fixed list of **input** offsets (relative to the
// anchor), each of which must currently hold a 1 -- applying the
// transformation always clears every one of them back to 0 -- and a fixed
// list of **output** offsets, each of which gets set to 1, ripple-carrying
// up the row axis (exactly like ordinary addition -- see
// addSingleBitWithCarry() in representation_base.hpp) if that position is
// already occupied. Because an occupied output never blocks the
// transformation (it just carries), canApply() only ever needs to check
// the inputs.
//
// canApply()/apply()/applyAndReportLandings() are templated over anything
// that looks like a bit grid -- `bool get(int, int) const` and
// `set(int, int, bool)` -- so the exact same Transformation works directly
// on a SmoothNumberBase (get()/set() there correctly invalidate every
// representation but the canonical one, same as any other set() call) or
// directly on a bare RepresentationBase (which is what
// TransformationAlgorithmCluster --
// algorithm_cluster_zoo/transformation_algorithm_cluster.hpp -- and Plan
// (plan.hpp) need, since a Plan's blueprint execution works with
// representations directly, never a SmoothNumberBase).
//
// This is deliberately concrete, not an interface: every transformation
// this library has fits this one shape (clear a fixed set of 1s, carry-set
// a fixed set of new 1s), so there's no virtual dispatch to speak of.
// Building a custom one is just
// `Transformation({...input offsets...}, {...output offsets...})`. See
// transformation_zoo/ for the named presets (MergeTransformation/
// SplitTransformation/SpreadTransformation), built exactly that way.
class Transformation {
public:
    Transformation(std::vector<std::pair<int, int>> inputs, std::vector<std::pair<int, int>> outputs)
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
    // re-examining for new opportunities after this call -- exactly what
    // TransformationAlgorithmCluster
    // (algorithm_cluster_zoo/transformation_algorithm_cluster.hpp) uses
    // this for, to avoid rescanning an entire representation after every
    // single application.
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

    // Exposed so external code can check a transformation's own
    // well-formedness directly -- e.g. that summing 2^i*3^j over the
    // inputs equals the same sum over the outputs, which is exactly what
    // "value-preserving" means, without needing to construct a
    // SmoothNumberBase or call apply() at all -- and so
    // TransformationAlgorithmCluster can compute candidate anchors from a
    // transformation's own input offsets.
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

inline void SmoothNumberBase::applyTransformation(const Transformation& t, int i, int j) {
    if (!t.canApply(*this, i, j)) {
        throw std::invalid_argument(
            "SmoothNumberBase::applyTransformation: the transformation cannot be applied at that (i, j)");
    }
    t.apply(*this, i, j);
}

}  // namespace smooth
