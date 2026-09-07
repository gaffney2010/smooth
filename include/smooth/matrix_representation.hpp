#pragma once

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "smooth/representation_base.hpp"

namespace smooth {

// The (i, j) grid of bits, stored densely.
class MatrixRepresentation : public RepresentationBase {
public:
    MatrixRepresentation(std::size_t max_rows, std::size_t max_cols, std::size_t neg_rows, std::size_t neg_cols)
        : rows_(max_rows),
          cols_(max_cols),
          negRows_(neg_rows),
          negCols_(neg_cols),
          bits_((max_rows + neg_rows) * (max_cols + neg_cols), false) {}

    bool get(int i, int j) const override { return bits_[index(i, j)]; }

    bool set(int i, int j, bool value) override {
        std::size_t idx = index(i, j);
        bool changed = bits_[idx] != value;
        bits_[idx] = value;
        return changed;
    }

    void reset() override { std::fill(bits_.begin(), bits_.end(), false); }

    // Walks the full grid; there's no shortcut to skip zero cells when
    // they're packed densely like this.
    double value() const override {
        double total = 0.0;
        for (int i = firstRow(); i < static_cast<int>(rows_); ++i) {
            for (int j = firstCol(); j < static_cast<int>(cols_); ++j) {
                if (get(i, j)) {
                    total += std::pow(2.0, static_cast<double>(i)) *
                             std::pow(3.0, static_cast<double>(j));
                }
            }
        }
        return total;
    }

    // Prints the grid as rows of `0`/`1`. If there is negative row/column
    // capacity, a horizontal and/or vertical line marks the boundary
    // between the fractional entries (negative index) and the whole-number
    // entries (non-negative index); the whole-number part is the block
    // below and to the right of the lines.
    void print(std::ostream& os) const override {
        for (int i = firstRow(); i < static_cast<int>(rows_); ++i) {
            std::string line = rowString(i);
            os << line << '\n';
            if (i == -1 && negRows_ > 0) {
                std::string sep(line.size(), '-');
                auto barPos = line.find('|');
                if (barPos != std::string::npos) sep[barPos] = '+';
                os << sep << '\n';
            }
        }
    }

private:
    int firstRow() const { return -static_cast<int>(negRows_); }
    int firstCol() const { return -static_cast<int>(negCols_); }

    std::size_t index(int i, int j) const {
        std::size_t ri = static_cast<std::size_t>(i + static_cast<int>(negRows_));
        std::size_t rj = static_cast<std::size_t>(j + static_cast<int>(negCols_));
        return ri * (negCols_ + cols_) + rj;
    }

    std::string rowString(int i) const {
        std::string s;
        for (int j = firstCol(); j < static_cast<int>(cols_); ++j) {
            s += get(i, j) ? '1' : '0';
            if (j == -1 && negCols_ > 0) {
                s += " | ";
            } else if (j + 1 < static_cast<int>(cols_)) {
                s += ' ';
            }
        }
        return s;
    }

    std::size_t rows_;
    std::size_t cols_;
    std::size_t negRows_;
    std::size_t negCols_;
    std::vector<bool> bits_;
};

}  // namespace smooth
