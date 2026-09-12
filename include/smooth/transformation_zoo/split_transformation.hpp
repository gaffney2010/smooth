#pragma once

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "smooth/atomic_transformation.hpp"

namespace smooth {

// The reverse of MergeTransformation (merge_transformation.hpp): splits
// the bit at (i, j+1) into the two bits at (i, j) and (i+1, j) -- each
// carrying independently (in that order) if its destination is already
// occupied.
//
// The other half of MergeTransformation's atom (atomic_transformation.hpp)
// -- its own atomize() is just itself.
class SplitTransformation : public AtomicTransformation {
public:
    SplitTransformation() : AtomicTransformation({{0, 1}}, {{0, 0}, {1, 0}}) {}

    std::vector<AtomApplication> atomize(int i, int j) const override {
        return {AtomApplication{std::make_shared<SplitTransformation>(), i, j}};
    }

protected:
    // The reverse of MergeTransformation's own RowValues arithmetic:
    // subtract 2^i from column j+1, add 2^i + 2^(i+1) = 3*2^i to column j.
    std::vector<std::pair<int, int>> applyRowValuesAndReportLandings(RowValuesRepresentation& rep, int i,
                                                                      int j) const override {
        double bit = std::pow(2.0, i);
        rep.addToColumnValue(j + 1, -bit);
        rep.addToColumnValue(j, 3.0 * bit);
        return {{i, j}, {i + 1, j}};
    }
};

}  // namespace smooth
