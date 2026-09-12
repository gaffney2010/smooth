#pragma once

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "smooth/atomic_transformation.hpp"

namespace smooth {

// Merges the two bits at (i, j) and (i+1, j) into the single bit at
// (i, j+1): 2^i*3^j + 2^(i+1)*3^j = 2^i*3^(j+1). If (i, j+1) is already
// set, the merge still succeeds -- it carries into (i+1, j+1),
// (i+2, j+1), ... until it lands on a clear cell, exactly like ordinary
// addition would.
//
// One of this library's two atoms (atomic_transformation.hpp) -- it
// isn't a composition of anything smaller, so its own atomize() is just
// itself.
class MergeTransformation : public AtomicTransformation {
public:
    MergeTransformation() : AtomicTransformation({{0, 0}, {1, 0}}, {{0, 1}}) {}

    std::vector<AtomApplication> atomize(int i, int j) const override {
        return {AtomApplication{std::make_shared<MergeTransformation>(), i, j}};
    }

protected:
    // Against RowValues, clearing (i, j) and (i+1, j) then carry-setting
    // (i, j+1) is exactly "subtract 2^i + 2^(i+1) = 3*2^i from column j,
    // add 2^i to column j+1" -- ordinary column arithmetic, no bit-by-bit
    // carry chase needed (RowValuesRepresentation's own magnitude already
    // absorbs whatever was there).
    std::vector<std::pair<int, int>> applyRowValuesAndReportLandings(RowValuesRepresentation& rep, int i,
                                                                      int j) const override {
        double bit = std::pow(2.0, i);
        rep.addToColumnValue(j, -3.0 * bit);
        rep.addToColumnValue(j + 1, bit);
        return {{i, j + 1}};
    }
};

}  // namespace smooth
