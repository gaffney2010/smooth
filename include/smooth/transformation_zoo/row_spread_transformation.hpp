#pragma once

#include <stdexcept>
#include <utility>
#include <vector>

#include "smooth/transformation.hpp"

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
    explicit RowSpreadTransformation(int n) : OffsetTransformation(inputOffsets(n), outputOffsets(n)) {}

private:
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
