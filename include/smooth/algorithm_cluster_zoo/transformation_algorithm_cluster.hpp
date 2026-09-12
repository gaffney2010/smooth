#pragma once

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "smooth/algorithm_cluster.hpp"
#include "smooth/metrics.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/transformation.hpp"

namespace smooth {

// A small family of Transformations (transformation.hpp), greedily
// applied across an entire RepresentationBase until none of them can fire
// anywhere anymore -- a fixed point. This is the generic engine behind
// every AlgorithmCluster (algorithm_cluster.hpp) this library builds
// directly from Transformations, and it lives here in
// algorithm_cluster_zoo/ -- despite being the generic case, not a
// specific preset -- because nothing outside this folder ever needs to
// name it: MergeCluster, BinaryFormCluster, and TernaryCarryCluster (this
// folder) are its only subclasses, each fixing its own
// Transformation(s)/name/bound as constructor arguments and adding
// nothing else; Plan (plan.hpp) only ever holds the abstract
// AlgorithmCluster it produces. Nothing about this engine cares what
// *kind* of Transformation it's holding, or whether all of them are even
// the same kind -- transformations_ is a vector of the abstract
// Transformation base, so an OffsetTransformation-based preset and one
// built some entirely other way (see TernaryCarryTransformation,
// transformation_zoo/ternary_carry_transformation.hpp) can even be mixed
// in the same cluster.
//
// Whether that fixed point is ever reached depends on the family. A
// family like {MergeTransformation} always terminates on its own: every
// successful application strictly reduces the representation's total
// set-bit count (it clears at least as many bits -- its inputs, plus
// however many occupied cells a carry rippled through -- as it ever sets,
// since a carry ripple clears each occupied cell it passes through before
// finally landing on exactly one clear one), and that count can't go
// negative, so it can't run forever. {TernaryCarryTransformation} also
// terminates on its own, but for a different reason -- see its own class
// comment -- since it doesn't shrink the bit count at all. But neither
// property is guaranteed just by being made of Transformations --
// {SplitTransformation} alone, for instance, is the exact reverse of a
// merge (it *grows* the bit count) and has no floor of its own: nothing
// about SplitTransformation knows that column 0 is special, so splitting a
// bit that has already reached column 0 just produces column -1, then -2,
// forever. That's what the optional `allowed` constructor parameter below
// is for -- it lets a caller bound the region a fixed-point search is
// allowed to explore, turning a search that would otherwise run forever
// into one that provably terminates at the boundary. See BinaryFormCluster
// (algorithm_cluster_zoo/binary_form_cluster.hpp) for exactly this:
// {SplitTransformation}, bounded to column >= 0. It's a constructor
// parameter, not a per-run() one, because it's a property of *this
// cluster* (which region it's meant to search), fixed for its whole
// lifetime -- and because run(rep, metrics) then matches
// AlgorithmCluster's own signature exactly, with nothing extra to pass at
// the call site.
//
// The interesting part is doing this *without* rescanning the whole
// representation after every single application. A new opportunity for
// some transformation to fire can only ever appear at a cell some earlier
// application actually touched -- each transformation's own
// affectedAnchors() (transformation.hpp) says exactly which anchors a
// changed cell could newly affect, for that transformation specifically.
// So run() seeds a worklist
// from the representation's own set bits (via forEachSet() -- already just
// the actual 1s, not a full grid scan), and after every application, only
// re-examines the cells right around where that application's own
// affectedAnchors() says are worth another look, rather than looking
// anywhere else.
class TransformationAlgorithmCluster : public AlgorithmCluster {
public:
    // `name` is purely cosmetic -- it's what Plan (plan.hpp) shows for a
    // Cluster blueprint step that runs this cluster, e.g. "cluster(merge)".
    // `allowed`, if given, bounds every anchor this cluster will ever try
    // (see the class comment above) -- e.g. `[](int, int j){ return j >= 0; }`
    // to never explore below column 0.
    TransformationAlgorithmCluster(std::vector<const Transformation*> transformations, std::string name,
                                    std::function<bool(int, int)> allowed = nullptr)
        : transformations_(std::move(transformations)), name_(std::move(name)), allowed_(std::move(allowed)) {}

    const std::string& name() const override { return name_; }

    // Runs every transformation in this cluster, wherever any of them can
    // apply (and `allowed`, if given at construction, permits), against
    // `rep`, until none of them can fire anywhere anymore. If `metrics` is
    // given, increments "transformations_applied" once per successful
    // application (Transformation itself doesn't touch Metrics at all --
    // see transformation.hpp -- so this is the only place that count is
    // available).
    void run(RepresentationBase& rep, const std::shared_ptr<Metrics>& metrics = nullptr) const override {
        // (transformation index, i, j): a candidate anchor worth checking.
        using Candidate = std::tuple<std::size_t, int, int>;
        std::vector<Candidate> worklist;
        std::set<Candidate> queued;

        auto enqueueCandidatesAt = [&](int i, int j) {
            // A cell at (i, j) just changed -- ask each transformation,
            // via its own affectedAnchors(), which of its anchors that
            // might newly affect (its other conditions might already be
            // satisfied). What "affected" means is entirely up to each
            // transformation -- see Transformation::affectedAnchors()
            // (transformation.hpp).
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
