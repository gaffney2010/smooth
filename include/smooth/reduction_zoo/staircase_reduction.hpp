#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "smooth/metrics.hpp"
#include "smooth/reduction.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/transformation_zoo/corner_split_transformation.hpp"
#include "smooth/transformation_zoo/row_spread_transformation.hpp"
#include "smooth/transformation_zoo/spread_transformation.hpp"

namespace smooth {

// So long as there exist two distinct set bits (i1, j1) and (i2, j2) with
// i2 >= i1 and j2 >= j1 (i.e. (i2, j2) weakly dominates (i1, j1)),
// combines them: SpreadTransformation(j2-j1) anchored at (i1,j1) for a
// same-row pair; RowSpreadTransformation(i2-i1) anchored at (i1,j1) for a
// same-column pair; otherwise CornerSplitTransformation anchored at
// (i2,j2), pulling the dominating bit one step toward (i1,j1).
//
// The fixed point -- no such pair exists -- is exactly an antichain under
// the product order. Every 3-smooth number has such a "staircase" form.
//
// Termination isn't free: naively picking *any* dominating pair each step
// can run for tens of thousands of steps without converging. What works
// reliably is always picking the *first* dominating pair found while
// scanning set bits in sorted (i, j) order -- in practice converging in
// well under a hundred steps even for a few dozen starting bits.
//
// Since candidate pairs aren't confined to a small local neighborhood,
// TransformationReduction's worklist approach doesn't apply here -- this
// rescans all of `rep`'s set bits from scratch after every application.
//
// CornerSplitTransformation only ever fires here on a bit that strictly
// dominates some other non-negative bit, so its anchor is always at row
// >= 1 and column >= 1 -- its outputs can never go negative. Starting
// from an all-non-negative representation, this never needs a
// fractional-capable one.
//
// `slack`, if nonzero, stops short of the fixed point: once at most
// `slack` dominating pairs remain among the current set bits, run()
// returns rather than resolving the rest. Counting every pair costs the
// same O(bits^2) scan either way (see above), so `slack` == 0 isn't any
// cheaper than finding just the first pair.
class StaircaseReduction : public Reduction {
public:
    explicit StaircaseReduction(std::size_t slack = 0) : slack_(slack) {}

    const std::string& name() const override {
        static const std::string kName = "staircase";
        return kName;
    }

    void run(RepresentationBase& rep, const std::shared_ptr<Metrics>& metrics = nullptr) const override {
        while (true) {
            std::vector<std::pair<int, int>> bits;
            rep.forEachSet([&bits](int i, int j) { bits.emplace_back(i, j); });
            std::sort(bits.begin(), bits.end());

            std::pair<int, int> first{}, second{};
            bool found = false;
            std::size_t dominatingPairs = 0;
            for (std::size_t a = 0; a < bits.size(); ++a) {
                for (std::size_t b = 0; b < bits.size(); ++b) {
                    if (a == b) continue;
                    if (bits[b].first >= bits[a].first && bits[b].second >= bits[a].second) {
                        ++dominatingPairs;
                        if (!found) {
                            first = bits[a];
                            second = bits[b];
                            found = true;
                        }
                    }
                }
            }
            if (dominatingPairs <= slack_) return;  // <= slack dominating pairs left

            const auto& [i1, j1] = first;
            const auto& [i2, j2] = second;
            if (i1 == i2) {
                SpreadTransformation(j2 - j1).applyAndReportLandings(rep, i1, j1);
            } else if (j1 == j2) {
                RowSpreadTransformation(i2 - i1).applyAndReportLandings(rep, i1, j1);
            } else {
                CornerSplitTransformation().applyAndReportLandings(rep, i2, j2);
            }
            if (metrics) metrics->increment("transformations_applied");
        }
    }

private:
    std::size_t slack_;
};

}  // namespace smooth
