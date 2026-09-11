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

// A small family of Transformations, greedily applied across an entire
// RepresentationBase until none of them can fire anywhere anymore -- a
// fixed point. This is the generic engine behind every AlgorithmCluster
// (algorithm_cluster.hpp) this library builds directly from
// Transformations, and it lives here in algorithm_cluster_zoo/ -- despite
// being the generic case, not a specific preset -- because nothing outside
// this folder ever needs to name it: MergeCluster and BinaryFormCluster
// (this folder) are its only subclasses, each fixing its own
// Transformation(s)/name/bound as constructor arguments and adding
// nothing else; Plan (plan.hpp) only ever holds the abstract
// AlgorithmCluster it produces.
//
// Whether that fixed point is ever reached depends on the family. A
// family like {MergeTransformation} always terminates on its own: every
// successful application strictly reduces the representation's total
// set-bit count (it clears at least as many bits -- its inputs, plus
// however many occupied cells a carry rippled through -- as it ever sets,
// since a carry ripple clears each occupied cell it passes through before
// finally landing on exactly one clear one), and that count can't go
// negative, so it can't run forever. But a family isn't guaranteed that
// property just by being made of Transformations -- {SplitTransformation}
// alone, for instance, is the exact reverse of a merge (it *grows* the bit
// count) and has no floor of its own: nothing about SplitTransformation
// knows that column 0 is special, so splitting a bit that has already
// reached column 0 just produces column -1, then -2, forever. That's what
// the optional `allowed` constructor parameter below is for -- it lets a
// caller bound the region a fixed-point search is allowed to explore,
// turning a search that would otherwise run forever into one that
// provably terminates at the boundary. See BinaryFormCluster
// (algorithm_cluster_zoo/binary_form_cluster.hpp) for exactly this:
// {SplitTransformation}, bounded to column >= 0. It's a constructor
// parameter, not a per-run() one, because it's a property of *this
// cluster* (which region it's meant to search), fixed for its whole
// lifetime -- and because run(rep, metrics) then matches
// AlgorithmCluster's own signature exactly, with nothing extra to pass at
// the call site.
//
// The interesting part is doing this *without* rescanning the whole
// representation after every single application. Clearing a bit can only
// ever remove an opportunity for some transformation to fire, never create
// one -- new opportunities can only ever appear at a cell that just became
// 1, i.e. one of the previous application's own output landings (see
// Transformation::applyAndReportLandings()). So run() seeds a worklist
// from the representation's own set bits (via forEachSet() -- already just
// the actual 1s, not a full grid scan), and after every application, only
// re-examines the cells right around where that application's outputs
// landed, rather than looking anywhere else.
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
            // A cell at (i, j) just became 1 -- for every transformation
            // and every one of its input offsets, the anchor that would
            // place *that* input exactly on (i, j) is a candidate worth
            // checking (its other inputs might already be satisfied).
            for (std::size_t t = 0; t < transformations_.size(); ++t) {
                for (const auto& offset : transformations_[t]->inputs()) {
                    int ai = i - offset.first;
                    int aj = j - offset.second;
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
