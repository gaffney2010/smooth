#pragma once

#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace smooth {

// Represents a 3-smooth number (a number of the form 2^i * 3^j summed over a
// set of (i, j) pairs) as a bit matrix. Entry (i, j) being set means the term
// 2^i * 3^j is included in the number.
//
// Row index i is the power of 2, column index j is the power of 3. By
// default both start at 0 (whole numbers only). Constructing with negative
// capacities also allows i and j to go negative, which lets the matrix
// represent fractional values (e.g. i = -1 contributes a factor of 1/2).
class SmoothNumber {
public:
    // Whole-number-only matrix: i in [0, max_rows), j in [0, max_cols).
    SmoothNumber(std::size_t max_rows, std::size_t max_cols)
        : SmoothNumber(max_rows, max_cols, 0, 0) {}

    // General matrix: i in [-neg_rows, max_rows), j in [-neg_cols, max_cols).
    SmoothNumber(std::size_t max_rows, std::size_t max_cols, std::size_t neg_rows, std::size_t neg_cols)
        : rows_(max_rows),
          cols_(max_cols),
          negRows_(neg_rows),
          negCols_(neg_cols),
          bits_((max_rows + neg_rows) * (max_cols + neg_cols), false) {}

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    std::size_t negRows() const { return negRows_; }
    std::size_t negCols() const { return negCols_; }

    void set(int i, int j, bool value = true) {
        checkBounds(i, j);
        bits_[index(i, j)] = value;
    }

    void clear(int i, int j) { set(i, j, false); }

    bool get(int i, int j) const {
        checkBounds(i, j);
        return bits_[index(i, j)];
    }

    // Prints the matrix, ascending row (top to bottom) and column (left to
    // right). If the matrix has negative capacity, a horizontal line marks
    // the boundary between negative-row (fractional) and non-negative-row
    // (whole) entries, and a vertical line does the same for columns.
    void print(std::ostream& os = std::cout) const {
        const int firstRow = -static_cast<int>(negRows_);
        const int firstCol = -static_cast<int>(negCols_);
        for (int i = firstRow; i < static_cast<int>(rows_); ++i) {
            std::string line = rowString(i, firstCol);
            os << line << '\n';
            if (i == -1 && negRows_ > 0) {
                std::string sep(line.size(), '-');
                auto barPos = line.find('|');
                if (barPos != std::string::npos) sep[barPos] = '+';
                os << sep << '\n';
            }
        }
    }

    // Sum of 2^i * 3^j over all set bits. Uses double, so precision degrades
    // for large row/column counts or deeply negative indices.
    double value() const {
        double total = 0.0;
        const int firstRow = -static_cast<int>(negRows_);
        const int firstCol = -static_cast<int>(negCols_);
        for (int i = firstRow; i < static_cast<int>(rows_); ++i) {
            for (int j = firstCol; j < static_cast<int>(cols_); ++j) {
                if (get(i, j)) {
                    total += std::pow(2.0, static_cast<double>(i)) *
                             std::pow(3.0, static_cast<double>(j));
                }
            }
        }
        return total;
    }

private:
    std::string rowString(int i, int firstCol) const {
        std::string s;
        for (int j = firstCol; j < static_cast<int>(cols_); ++j) {
            s += get(i, j) ? '1' : '0';
            if (j == -1 && negCols_ > 0) {
                s += " | ";
            } else if (j + 1 < static_cast<int>(cols_)) {
                s += ' ';
            }
        }
        return s;
    }

    std::size_t index(int i, int j) const {
        std::size_t ri = static_cast<std::size_t>(i + static_cast<int>(negRows_));
        std::size_t rj = static_cast<std::size_t>(j + static_cast<int>(negCols_));
        return ri * (negCols_ + cols_) + rj;
    }

    void checkBounds(int i, int j) const {
        if (i < -static_cast<int>(negRows_) || i >= static_cast<int>(rows_) ||
            j < -static_cast<int>(negCols_) || j >= static_cast<int>(cols_)) {
            throw std::out_of_range("SmoothNumber: index out of range");
        }
    }

    std::size_t rows_;
    std::size_t cols_;
    std::size_t negRows_;
    std::size_t negCols_;
    std::vector<bool> bits_;
};

}  // namespace smooth
