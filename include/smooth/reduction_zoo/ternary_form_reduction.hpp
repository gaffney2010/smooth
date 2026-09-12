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
// over its set rows) is a single power of two, never an arbitrary sum of
// several.
//
// So long as some column has more than one set bit, this takes that
// column's two *smallest* set rows i1 < i2 and applies
// RowSpreadTransformation(i2 - i1) (transformation_zoo/row_spread_transformation.hpp)
// anchored at (i1, j) -- folding them into a single bit one column over,
// plus (whenever i2 - i1 > 1) a staircase filling the gap between them,
// all still within column j -- then repeats, until no column has more
// than one bit left.
//
// Always resolving the *smallest* currently-offending column first
// (breaking ties within it by taking its two smallest rows) is what makes
// this terminate. Within a single application: replacing 2^i1 + 2^i2 with
// the staircase sum 2^(i1+1) + ... + 2^(i2-1) = 2^i2 - 2^(i1+1) changes
// column j's own magnitude by exactly -(2^i1 + 2^(i1+1)) = -3 * 2^i1 -- a
// strict decrease, since i1 >= 0 -- so a column being worked on can't be
// touched forever. And once a column is driven down to at most one bit,
// nothing this reduction ever does can put a bit into a column lower than
// the one it's currently working on (RowSpreadTransformation's own outputs
// never land below its anchor's column), so a resolved column stays
// resolved and the "smallest offending column" only ever moves up.
//
// Unlike the two-phase pipeline this replaces (BinaryFormReduction
// collapsing everything into column 0, then TernaryCarryReduction working
// directly with RowValuesRepresentation's per-column magnitudes -- see
// transformation_zoo/ternary_carry_transformation.hpp), this works
// entirely through a bit-level OffsetTransformation, so it runs against
// *any* RepresentationBase -- Sparse, Dynamic, RowValues, Scalar -- with
// no special-casing and no upfront representation check at all. It also
// needs no separate "collapse into column 0 first" phase: a bit that
// starts in whatever column already either satisfies the one-bit-per-
// column property or gets carried rightward from wherever it is, and (like
// the old pipeline) the final result only ever depends on the number's
// value, never on how it started out distributed across columns.
//
// Like StaircaseReduction (staircase_reduction.hpp), candidate columns
// aren't confined to a small local neighborhood the way a single
// Transformation's own affectedAnchors() are, so this rescans all of
// `rep`'s set bits from scratch after every application, rather than using
// TransformationReduction's worklist approach.
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
