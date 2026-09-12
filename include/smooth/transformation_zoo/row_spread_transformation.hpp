#pragma once

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "smooth/atomic_transformation.hpp"
#include "smooth/transformation.hpp"
#include "smooth/transformation_zoo/corner_split_transformation.hpp"
#include "smooth/transformation_zoo/merge_transformation.hpp"

namespace smooth {

// Bridges the two bits at (i, j) and (i+n, j), n rows apart, into
// (i, j+1) plus a "staircase" of bits at (i+1, j), (i+2, j), ...,
// (i+n-1, j) -- the row-axis counterpart to SpreadTransformation, which
// bridges two column-separated bits instead:
//
//   2^i*3^j + 2^(i+n)*3^j
//     = 2^i*3^j * (1 + 2^n)
//     = 2^i*3^j * (3 + (2^n - 2))                      [2^n - 2 = sum_{k=1}^{n-1} 2^k]
//     = 2^i*3^(j+1) + sum_{k=1}^{n-1} 2^(i+k)*3^j
//
// For n = 1 the staircase is empty (there's no k with 1 <= k <= 0), so
// this has exactly the same input/output offsets as MergeTransformation:
// (i, j) and (i+1, j) both feed into (i, j+1).
//
// n must be at least 1 -- the constructor throws std::invalid_argument
// otherwise, same reasoning as SpreadTransformation's own n < 1 check.
class RowSpreadTransformation : public OffsetTransformation {
public:
    explicit RowSpreadTransformation(int n) : OffsetTransformation(inputOffsets(n), outputOffsets(n)), n_(n) {}

    // Unlike SpreadTransformation, this isn't reachable from Merge/Split
    // alone -- CornerSplitTransformation is required. n = 1 is
    // MergeTransformation itself; n = 2 is CornerSplit(i+2, j) (leaving
    // (i+1,j)/(i+1,j-1)/(i+2,j-1)) followed by Merge(i, j) then
    // Merge(i+1, j-1) -- safe only because (i+1, j) was already consumed
    // by the first merge.
    //
    // For n >= 3 that "consume the collision before it happens" trick has
    // to repeat: CornerSplit(i+n, j) leaves a fresh (n-1)-gap pair plus a
    // leftover pair one column down that would undo the split if merged
    // immediately. So instead, corner-split the leftover's own far cell,
    // pushing the same problem one column further down, and repeat until
    // n has counted down to 2, where the base case finishes it off.
    // Verified computationally (Python simulation, n up to 25) before
    // writing this -- the loop below is this recursion unrolled, costing
    // 4n - 5 atoms for n >= 2 (1 for n = 1), linear in n.
    std::vector<AtomApplication> atomize(int i, int j) const override {
        auto cornerSplit = [] { return std::make_shared<CornerSplitTransformation>(); };
        auto merge = [] { return std::make_shared<MergeTransformation>(); };

        std::vector<AtomApplication> atoms;
        if (n_ == 1) {
            atoms.push_back(AtomApplication{merge(), i, j});
            return atoms;
        }
        if (n_ == 2) {
            atoms.push_back(AtomApplication{cornerSplit(), i + 2, j});
            atoms.push_back(AtomApplication{merge(), i, j});
            atoms.push_back(AtomApplication{merge(), i + 1, j - 1});
            return atoms;
        }
        atoms.push_back(AtomApplication{cornerSplit(), i + n_, j});
        atoms.push_back(AtomApplication{cornerSplit(), i + n_, j - 1});
        for (int k = n_ - 1; k >= 3; --k) {
            atoms.push_back(AtomApplication{cornerSplit(), i + k, j});
            atoms.push_back(AtomApplication{merge(), i + k, j - 1});
            atoms.push_back(AtomApplication{merge(), i + k, j - 2});
            atoms.push_back(AtomApplication{cornerSplit(), i + k, j - 1});
        }
        atoms.push_back(AtomApplication{cornerSplit(), i + 2, j});
        atoms.push_back(AtomApplication{merge(), i + 2, j - 1});
        atoms.push_back(AtomApplication{merge(), i + 2, j - 2});
        atoms.push_back(AtomApplication{merge(), i, j});
        atoms.push_back(AtomApplication{merge(), i + 1, j - 1});
        return atoms;
    }

private:
    int n_;

    static std::vector<std::pair<int, int>> inputOffsets(int n) {
        requirePositive(n);
        return {{0, 0}, {n, 0}};
    }

    static std::vector<std::pair<int, int>> outputOffsets(int n) {
        requirePositive(n);
        std::vector<std::pair<int, int>> offsets;
        offsets.emplace_back(0, 1);
        for (int k = 1; k <= n - 1; ++k) {
            offsets.emplace_back(k, 0);
        }
        return offsets;
    }

    static void requirePositive(int n) {
        if (n < 1) {
            throw std::invalid_argument("RowSpreadTransformation: n must be at least 1");
        }
    }
};

}  // namespace smooth
