#pragma once

#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace smooth {

// Represents a 3-smooth number (a number of the form 2^i * 3^j summed over a
// set of (i, j) pairs) as a bit matrix. Entry (i, j) being set means the term
// 2^i * 3^j is included in the number.
class SmoothNumber {
public:
    SmoothNumber(std::size_t max_rows, std::size_t max_cols)
        : rows_(max_rows), cols_(max_cols), bits_(max_rows * max_cols, false) {}

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }

    void set(std::size_t i, std::size_t j, bool value = true) {
        checkBounds(i, j);
        bits_[index(i, j)] = value;
    }

    void clear(std::size_t i, std::size_t j) { set(i, j, false); }

    bool get(std::size_t i, std::size_t j) const {
        checkBounds(i, j);
        return bits_[index(i, j)];
    }

    void print(std::ostream& os = std::cout) const {
        for (std::size_t i = 0; i < rows_; ++i) {
            for (std::size_t j = 0; j < cols_; ++j) {
                os << (get(i, j) ? '1' : '0');
                if (j + 1 < cols_) os << ' ';
            }
            os << '\n';
        }
    }

    // Sum of 2^i * 3^j over all set bits. Uses double, so precision degrades
    // for large row/column counts.
    double value() const {
        double total = 0.0;
        for (std::size_t i = 0; i < rows_; ++i) {
            for (std::size_t j = 0; j < cols_; ++j) {
                if (get(i, j)) {
                    total += std::pow(2.0, static_cast<double>(i)) *
                             std::pow(3.0, static_cast<double>(j));
                }
            }
        }
        return total;
    }

private:
    std::size_t index(std::size_t i, std::size_t j) const { return i * cols_ + j; }

    void checkBounds(std::size_t i, std::size_t j) const {
        if (i >= rows_ || j >= cols_) {
            throw std::out_of_range("SmoothNumber: index out of range");
        }
    }

    std::size_t rows_;
    std::size_t cols_;
    std::vector<bool> bits_;
};

}  // namespace smooth
