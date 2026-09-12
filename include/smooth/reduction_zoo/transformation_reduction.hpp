#pragma once

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "smooth/metrics.hpp"
#include "smooth/reduction.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/transformation.hpp"

namespace smooth {

// A small family of Transformations, greedily applied across an entire
// RepresentationBase until none of them can fire anywhere anymore -- a
// fixed point. This is the generic engine behind every Reduction this
// library builds directly from Transformations; MergeReduction/
// BinaryFormReduction/TernaryCarryReduction (this folder) are its only
// subclasses, each fixing its own Transformation(s)/name/bound. Nothing
// about this engine cares what *kind* of Transformation it's holding --
// transformations_ is a vector of the abstract base, so an
// OffsetTransformation-based preset and something built an entirely
// different way (TernaryCarryTransformation) can even be mixed together.
//
// Whether that fixed point is ever reached depends on the family. A
// family like {MergeTransformation} always terminates on its own: every
// application strictly reduces total set-bit count (a carry clears each
// occupied cell it ripples through before landing on a clear one), which
// can't go negative. {TernaryCarryTransformation} also terminates, but
// for a different reason (see its own class comment). But neither
// property is guaranteed just by being made of Transformations --
// {SplitTransformation} alone grows the bit count and has no floor:
// nothing about it knows column 0 is special, so it splits into column
// -1, -2, forever. That's what the optional `allowed` constructor
// parameter is for -- bounding the region a search may explore. See
// BinaryFormReduction for exactly this: {SplitTransformation} bounded to
// column >= 0. It's a constructor parameter (a fixed property of this
// reduction) rather than a run() one, so run() matches Reduction's own
// signature exactly.
//
// The interesting part is doing this without rescanning the whole
// representation after every application. A new opportunity can only
// appear at a cell some earlier application touched -- each
// transformation's own affectedAnchors() says exactly which anchors a
// changed cell could newly affect. So run() seeds a worklist from the
// representation's own set bits, and after every application, only
// re-examines what affectedAnchors() says is worth another look.
class TransformationReduction : public Reduction {
public:
    // `name` is purely cosmetic -- what Plan shows for a Reduce step, e.g.
    // "reduce(merge)". `allowed`, if given, bounds every anchor this
    // reduction will ever try, e.g. `[](int, int j){ return j >= 0; }`.
    TransformationReduction(std::vector<const Transformation*> transformations, std::string name,
                             std::function<bool(int, int)> allowed = nullptr)
        : transformations_(std::move(transformations)), name_(std::move(name)), allowed_(std::move(allowed)) {}

    const std::string& name() const override { return name_; }

    // Runs every transformation in this reduction, wherever any of them
    // can apply (and `allowed` permits), against `rep`, until none can
    // fire anywhere. If `metrics` is given, increments
    // "transformations_applied" once per successful application.
    void run(RepresentationBase& rep, const std::shared_ptr<Metrics>& metrics = nullptr) const override {
        // (transformation index, i, j): a candidate anchor worth checking.
        using Candidate = std::tuple<std::size_t, int, int>;
        std::vector<Candidate> worklist;
        std::set<Candidate> queued;

        auto enqueueCandidatesAt = [&](int i, int j) {
            // A cell at (i, j) just changed -- ask each transformation,
            // via its own affectedAnchors(), which anchors that might
            // newly affect.
            for (std::size_t t = 0; t < transformations_.size(); ++t) {
                for (const auto& anchor : transformations_[t]->affectedAnchors(i, j)) {
                    int ai = anchor.first;
                    int aj = anchor.second;
                    if (allowed_ && !allowed_(ai, aj)) continue;
                    Candidate candidate{t, ai, aj};
                    if (queued.insert(candidate).second) worklist.push_back(candidate);
                }
            }
        };

        rep.forEachSet([&](int i, int j) { enqueueCandidatesAt(i, j); });

        while (!worklist.empty()) {
            Candidate candidate = worklist.back();
            worklist.pop_back();
            queued.erase(candidate);

            const auto& [t, i, j] = candidate;
            const Transformation& transformation = *transformations_[t];
            if (!transformation.canApply(rep, i, j)) continue;  // stale -- an earlier step consumed an input

            if (metrics) metrics->increment("transformations_applied");
            for (const auto& landing : transformation.applyAndReportLandings(rep, i, j)) {
                enqueueCandidatesAt(landing.first, landing.second);
            }
        }
    }

private:
    std::vector<const Transformation*> transformations_;
    std::string name_;
    std::function<bool(int, int)> allowed_;
};

}  // namespace smooth
