#pragma once

#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

#include "smooth/representation_zoo/row_values_representation.hpp"
#include "smooth/transformation.hpp"

namespace smooth {

// Anchored at column j (the row half of the anchor, i, is unused, always
// 0 by convention): while column j's total, n_j (RowValuesRepresentation's
// per-column magnitude), has more than one bit set, subtracts 3 from n_j
// and adds 1 to n_(j+1) -- value-preserving since 3 * 3^j = 3^(j+1).
//
// This is a Transformation that isn't an OffsetTransformation: its
// precondition depends on a whole column's aggregate magnitude, and
// applying it is an ordinary subtraction needing a borrow across bits
// that OffsetTransformation has no way to express. RowValuesRepresentation
// sidesteps this by storing n_j as a single number, which is why
// canApply()/applyAndReportLandings() require one and throw
// std::invalid_argument otherwise.
//
// One application only decrements column j -- it may still need further
// applications at the same anchor -- so applyAndReportLandings() reports
// column j itself as a landing alongside column j+1, unlike any
// OffsetTransformation.
//
// This always terminates without going negative: the descending sequence
// n_j, n_j - 3, n_j - 6, ... is confined to one residue class mod 3,
// whose smallest nonnegative member (0, 1, or 2) always has popcount <= 1
// -- see TernaryCarryReduction, which runs this to a fixed point with no
// bound needed.
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

    // Any change anywhere in column j means column j is worth rechecking,
    // no matter which row within it actually changed.
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
