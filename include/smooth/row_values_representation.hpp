#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

#include "smooth/representation_base.hpp"

namespace smooth {

// One number n_j per column j, where n_j = sum_i (bit(i,j) ? 2^i : 0), so
// the total value is sum_j n_j * 3^j. n_j is an integer if there's no
// negative row capacity, and may be fractional otherwise.
class RowValuesRepresentation : public RepresentationBase {
public:
    RowValuesRepresentation(std::size_t /*max_rows*/, std::size_t max_cols, std::size_t neg_rows,
                             std::size_t neg_cols)
        : cols_(max_cols), negRows_(neg_rows), negCols_(neg_cols), values_(neg_cols + max_cols, 0.0) {}

    // Individual bits are recovered from n_j by dividing out 2^i and
    // checking parity. This is exact for the row/column ranges this class
    // is meant for, since every n_j is an exact sum of distinct powers of
    // two, but it is floating-point-based and could get unreliable at very
    // large row counts.
    bool get(int i, int j) const override {
        double n = values_[colIndex(j)];
        double scaled = n / std::pow(2.0, i);
        long long whole = static_cast<long long>(std::llround(std::floor(scaled + 1e-9)));
        long long parity = whole % 2;
        if (parity < 0) parity += 2;
        return parity == 1;
    }

    bool set(int i, int j, bool value) override {
        if (get(i, j) == value) return false;
        double delta = std::pow(2.0, i);
        values_[colIndex(j)] += value ? delta : -delta;
        return true;
    }

    void reset() override { std::fill(values_.begin(), values_.end(), 0.0); }

    // Each n_j already sums its row's contribution, so this is a single
    // O(cols) pass rather than O(rows * cols), with no per-bit decoding.
    double value() const override {
        double total = 0.0;
        for (int j = firstCol(); j < static_cast<int>(cols_); ++j) {
            total += values_[colIndex(j)] * std::pow(3.0, static_cast<double>(j));
        }
        return total;
    }

    // One line per column j, showing n_j.
    void print(std::ostream& os) const override {
        const bool fractional = negRows_ > 0;
        for (int j = firstCol(); j < static_cast<int>(cols_); ++j) {
            double n = values_[colIndex(j)];
            os << "n[" << j << "] = ";
            if (fractional) {
                os << n;
            } else {
                os << static_cast<long long>(std::llround(n));
            }
            os << '\n';
        }
    }

private:
    int firstCol() const { return -static_cast<int>(negCols_); }

    std::size_t colIndex(int j) const { return static_cast<std::size_t>(j + static_cast<int>(negCols_)); }

    std::size_t cols_;
    std::size_t negRows_;
    std::size_t negCols_;
    std::vector<double> values_;
};

}  // namespace smooth
