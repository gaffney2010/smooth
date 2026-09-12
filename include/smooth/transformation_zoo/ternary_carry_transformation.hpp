#pragma once

#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

#include "smooth/representation_zoo/row_values_representation.hpp"
#include "smooth/transformation.hpp"

namespace smooth {

// Anchored at column j (the row half of the anchor, i, is unused --
// always 0 by convention): while column j's total, n_j
// (RowValuesRepresentation's own per-column magnitude -- see
// row_values_representation.hpp), has more than one bit set, subtracts 3
// from n_j and adds 1 to n_(j+1) -- value-preserving, since
// 3 * 3^j = 3^(j+1).
//
// This is a Transformation (transformation.hpp) that isn't an
// OffsetTransformation: its precondition ("does column j have more than
// one bit set") depends on an entire column's aggregate magnitude, not a
// fixed, small set of grid cells, and applying it doesn't clear a fixed
// set of 1s either -- it's an ordinary magnitude subtraction, which in
// general needs a borrow across bits that OffsetTransformation has no way
// to express. RowValuesRepresentation sidesteps the problem by already
// storing n_j as a single number rather than exploded bits, which is why
// canApply()/applyAndReportLandings() below require one and throw
// std::invalid_argument otherwise.
//
// Unlike an OffsetTransformation's inputs (always fully cleared, so only
// the outputs are worth re-examining afterward -- see
// OffsetTransformation::applyAndReportLandings()), one application here
// only *decrements* column j -- it may still have more than one bit set
// afterward, needing further applications at the same anchor. So
// applyAndReportLandings() reports column j itself as a landing, right
// alongside column j+1, unlike any OffsetTransformation.
//
// This always terminates without ever going negative, run repeatedly at
// a fixed column: the descending sequence n_j, n_j - 3, n_j - 6, ... is
// confined to one residue class mod 3, and that class's smallest
// nonnegative member -- 0, 1, or 2 -- always has popcount <= 1. So it's
// guaranteed to stop at or before reaching it, never below -- see
// TernaryCarryCluster (algorithm_cluster_zoo/ternary_carry_cluster.hpp),
// which runs this to a fixed point the same way MergeCluster runs
// MergeTransformation, needing no bound at all.
class TernaryCarryTransformation : public Transformation {
public:
    bool canApply(const RepresentationBase& rep, int /*i*/, int j) const override {
        return hasMoreThanOneBit(requireRowValues(rep).columnValue(j));
    }

    std::vector<std::pair<int, int>> applyAndReportLandings(RepresentationBase& rep, int /*i*/,
                                                              int j) const override {
        RowValuesRepresentation& rowValues = requireRowValues(rep);
        rowValues.addToColumnValue(j, -3.0);
        rowValues.addToColumnValue(j + 1, 1.0);
        return {{0, j}, {0, j + 1}};
    }

    // Any change anywhere in column j means column j -- the only anchor
    // this transformation ever cares about -- is worth rechecking, no
    // matter which row within it actually changed.
    std::vector<std::pair<int, int>> affectedAnchors(int /*i*/, int j) const override { return {{0, j}}; }

private:
    static bool hasMoreThanOneBit(double n) {
        long long whole = static_cast<long long>(std::llround(n));
        return (whole & (whole - 1)) != 0;
    }

    static RowValuesRepresentation& requireRowValues(RepresentationBase& rep) {
        auto* rowValues = dynamic_cast<RowValuesRepresentation*>(&rep);
        if (!rowValues) {
            throw std::invalid_argument("TernaryCarryTransformation: requires a RowValuesRepresentation");
        }
        return *rowValues;
    }

    static const RowValuesRepresentation& requireRowValues(const RepresentationBase& rep) {
        const auto* rowValues = dynamic_cast<const RowValuesRepresentation*>(&rep);
        if (!rowValues) {
            throw std::invalid_argument("TernaryCarryTransformation: requires a RowValuesRepresentation");
        }
        return *rowValues;
    }
};

}  // namespace smooth
