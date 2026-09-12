#pragma once

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "smooth/atomic_transformation.hpp"
#include "smooth/transformation.hpp"
#include "smooth/transformation_zoo/split_transformation.hpp"

namespace smooth {

// Bridges the two bits at (i, j) and (i, j+n), n columns apart, into
// (i+2, j) plus a "staircase" of bits at (i+1, j+1), (i+1, j+2), ...,
// (i+1, j+n-1):
//
//   2^i*3^j + 2^i*3^(j+n)
//     = 2^i*3^j * (1 + 3^n)
//     = 2^i*3^j * (4 + 2*(3 + 3^2 + ... + 3^(n-1)))            [1 + 3^n = 4 + 2*sum_{k=1}^{n-1} 3^k]
//     = 2^(i+2)*3^j + sum_{k=1}^{n-1} 2^(i+1)*3^(j+k)
//
// For n = 1, the staircase is empty (there's no k with 1 <= k <= 0), so
// this reduces to exactly MergeTransformation applied twice in a row:
// (i, j) and (i, j+1) both feed straight into (i+2, j).
//
// n must be at least 1 -- the constructor throws std::invalid_argument
// otherwise, since n = 0 would need one cell to independently hold two 1s
// at once, and a negative n would break the staircase's ascending order.
class SpreadTransformation : public OffsetTransformation {
public:
    explicit SpreadTransformation(int n) : OffsetTransformation(inputOffsets(n), outputOffsets(n)), n_(n) {}

    // n sequential SplitTransformation applications, walking the far
    // input at (i, j+n) down one column at a time: the k-th split turns
    // whatever landed at (i, j+k) into (i, j+k-1) and (i+1, j+k-1), so
    // after n of them the near copy has reached (i+2, j) and each
    // intermediate step left one bit at (i+1, j+k). Pure Family A -- no
    // CornerSplitTransformation needed here, unlike
    // RowSpreadTransformation's atomize().
    std::vector<AtomApplication> atomize(int i, int j) const override {
        std::vector<AtomApplication> atoms;
        atoms.reserve(n_);
        for (int col = j + n_ - 1; col >= j; --col) {
            atoms.push_back(AtomApplication{std::make_shared<SplitTransformation>(), i, col});
        }
        return atoms;
    }

private:
    int n_;
    static std::vector<std::pair<int, int>> inputOffsets(int n) {
        requirePositive(n);
        return {{0, 0}, {0, n}};
    }

    static std::vector<std::pair<int, int>> outputOffsets(int n) {
        requirePositive(n);
        std::vector<std::pair<int, int>> offsets;
        offsets.emplace_back(2, 0);
        for (int k = 1; k <= n - 1; ++k) {
            offsets.emplace_back(1, k);
        }
        return offsets;
    }

    static void requirePositive(int n) {
        if (n < 1) {
            throw std::invalid_argument("SpreadTransformation: n must be at least 1");
        }
    }
};

}  // namespace smooth
