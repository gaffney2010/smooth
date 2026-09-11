#pragma once

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "smooth/metrics.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/transformation.hpp"

namespace smooth {

// A small family of Transformations, greedily applied across an entire
// RepresentationBase until none of them can fire anywhere anymore -- a
// fixed point.
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
// the optional `allowed` predicate below is for -- it lets a caller bound
// the region a fixed-point search is allowed to explore, turning a search
// that would otherwise run forever into one that provably terminates at
// the boundary. See BinaryFormCluster (binary_form_cluster.hpp) for
// exactly this: {SplitTransformation}, bounded to column >= 0.
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
class TransformationAlgorithmCluster {
public:
    // `name` is purely cosmetic -- it's what Plan (plan.hpp) shows for a
    // Cluster blueprint step that runs this cluster, e.g. "cluster(merge)".
    TransformationAlgorithmCluster(std::vector<const Transformation*> transformations, std::string name)
        : transformations_(std::move(transformations)), name_(std::move(name)) {}

    const std::string& name() const { return name_; }

    // Runs every transformation in this cluster, wherever any of them can
    // apply, against `rep`, until none of them can fire anywhere anymore.
    // If `metrics` is given, increments "transformations_applied" once per
    // successful application (Transformation itself doesn't touch Metrics
    // at all -- see transformation.hpp -- so this is the only place that
    // count is available). If `allowed` is given, an anchor is only ever
    // tried when `allowed(i, j)` is true -- see the class comment above for
    // why a family without its own natural floor (or ceiling) needs this to
    // terminate at all.
    void run(RepresentationBase& rep, const std::shared_ptr<Metrics>& metrics = nullptr,
             const std::function<bool(int, int)>& allowed = nullptr) const {
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
                    if (allowed && !allowed(ai, aj)) continue;
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
};

}  // namespace smooth
