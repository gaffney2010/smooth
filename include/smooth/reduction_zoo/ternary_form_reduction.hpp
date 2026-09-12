#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "smooth/metrics.hpp"
#include "smooth/reduction.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/transformation_zoo/row_spread_transformation.hpp"

namespace smooth {

// Reduces a number to a form where every column j that has anything in it
// holds exactly one bit -- i.e. every column's own magnitude (summing 2^i
// over its set rows) is a single power of two, never an arbitrary sum.
//
// So long as some column has more than one set bit, this takes its two
// *smallest* set rows i1 < i2 and applies RowSpreadTransformation(i2-i1)
// anchored at (i1, j) -- folding them into a single bit one column over,
// plus (when i2-i1 > 1) a staircase filling the gap, all within column j
// -- then repeats.
//
// Always resolving the smallest currently-offending column first (ties
// broken by its two smallest rows) is what makes this terminate: within
// one application, column j's magnitude decreases by exactly 3 * 2^i1 (a
// strict decrease, since i1 >= 0); and once a column is driven to at most
// one bit, nothing this reduction does can put a bit below the column
// it's currently working on, so a resolved column stays resolved.
//
// Unlike the two-phase pipeline this replaces (BinaryFormReduction
// collapsing into column 0, then TernaryCarryReduction working with
// RowValuesRepresentation's per-column magnitudes directly), this works
// entirely through a bit-level OffsetTransformation, so it runs against
// *any* RepresentationBase with no special-casing at all. It also needs
// no "collapse into column 0 first" phase -- but that means the result
// can now depend on the *starting* bit layout, not just the value, unlike
// the old pipeline (which always erased any head start first).
//
// Like StaircaseReduction, candidate columns aren't confined to a small
// local neighborhood, so this rescans all of `rep`'s set bits from
// scratch after every application, rather than using
// TransformationReduction's worklist.
class TernaryFormReduction : public Reduction {
public:
    const std::string& name() const override {
        static const std::string kName = "ternary_form";
        return kName;
    }

    void run(RepresentationBase& rep, const std::shared_ptr<Metrics>& metrics = nullptr) const override {
        while (true) {
            std::map<int, std::vector<int>> rowsByColumn;
            rep.forEachSet([&rowsByColumn](int i, int j) { rowsByColumn[j].push_back(i); });

            auto offending = rowsByColumn.begin();
            while (offending != rowsByColumn.end() && offending->second.size() <= 1) ++offending;
            if (offending == rowsByColumn.end()) return;  // fixed point: <= 1 bit per column

            std::vector<int>& rows = offending->second;
            std::sort(rows.begin(), rows.end());
            int i1 = rows[0];
            int i2 = rows[1];
            RowSpreadTransformation(i2 - i1).applyAndReportLandings(rep, i1, offending->first);
            if (metrics) metrics->increment("transformations_applied");
        }
    }
};

}  // namespace smooth
