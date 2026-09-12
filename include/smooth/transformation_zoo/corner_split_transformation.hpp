#pragma once

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "smooth/atomic_transformation.hpp"

namespace smooth {

// Splits the bit at (i, j) into its three "corner" neighbors: (i-1, j),
// (i, j-1), and (i-1, j-1). Value-preserving:
//
//   2^(i-1)*3^j + 2^i*3^(j-1) + 2^(i-1)*3^(j-1)
//     = 2^(i-1)*3^(j-1) * (3 + 2 + 1)
//     = 2^(i-1)*3^(j-1) * 6
//     = 2^i * 3^j
//
// This library's second atom (atomic_transformation.hpp), independent of
// Merge/Split -- see its own atomize(), which is just itself.
class CornerSplitTransformation : public AtomicTransformation {
public:
    CornerSplitTransformation() : AtomicTransformation({{0, 0}}, {{-1, 0}, {0, -1}, {-1, -1}}) {}

    std::vector<AtomApplication> atomize(int i, int j) const override {
        return {AtomApplication{std::make_shared<CornerSplitTransformation>(), i, j}};
    }

protected:
    // Unlike Merge/Split, this one touches two columns at once: column j
    // loses 2^i but gains 2^(i-1) (from output (i-1,j)) -- a net change of
    // -2^(i-1) -- while column j-1 gains 2^i + 2^(i-1) = 3*2^(i-1) (from
    // outputs (i,j-1) and (i-1,j-1)). i-1 can be negative (e.g. anchored at
    // i=0), same as the generic bit-grid path already allows on a
    // fractional-capable representation.
    std::vector<std::pair<int, int>> applyRowValuesAndReportLandings(RowValuesRepresentation& rep, int i,
                                                                      int j) const override {
        double half = std::pow(2.0, i - 1);
        rep.addToColumnValue(j, -half);
        rep.addToColumnValue(j - 1, 3.0 * half);
        return {{i - 1, j}, {i, j - 1}, {i - 1, j - 1}};
    }
};

}  // namespace smooth
