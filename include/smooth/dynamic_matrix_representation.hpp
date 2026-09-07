#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "smooth/representation_base.hpp"

namespace smooth {

// A (i, j) bit grid that starts empty and grows on demand: whenever set()
// is asked to turn on a bit outside the currently allocated range, the
// array is grown by doubling -- independently in whichever of the four
// directions (more positive rows, more negative rows, more positive
// columns, more negative columns) ran out -- and the old contents are
// copied into the new, larger array. It never shrinks back down except via
// reset(). Its capacity is discovered purely from what gets set into it --
// it takes no capacity up front and needs none.
class DynamicMatrixRepresentation : public RepresentationBase {
public:
    explicit DynamicMatrixRepresentation(bool /*allow_fractional*/) {}

    bool get(int i, int j) const override {
        if (!inCapacity(i, j)) return false;
        return bits_[index(i, j)];
    }

    bool set(int i, int j, bool value) override {
        if (value) {
            growToFit(i, j);
            std::size_t idx = index(i, j);
            bool changed = !bits_[idx];
            bits_[idx] = true;
            return changed;
        }
        if (!inCapacity(i, j)) return false;  // already implicitly false
        std::size_t idx = index(i, j);
        bool changed = bits_[idx];
        bits_[idx] = false;
        return changed;
    }

    // Drops back to zero capacity; the next set() calls will regrow it
    // from scratch.
    void reset() override {
        posRowCap_ = negRowCap_ = posColCap_ = negColCap_ = 0;
        bits_.clear();
        bits_.shrink_to_fit();
    }

    double value() const override {
        double total = 0.0;
        for (int i = firstRow(); i < static_cast<int>(posRowCap_); ++i) {
            for (int j = firstCol(); j < static_cast<int>(posColCap_); ++j) {
                if (get(i, j)) {
                    total += std::pow(2.0, static_cast<double>(i)) *
                             std::pow(3.0, static_cast<double>(j));
                }
            }
        }
        return total;
    }

    // Prints the current allocated capacity, then the grid within it (with
    // a horizontal/vertical line marking the negative/non-negative
    // boundary, same as before) so growth is visible across successive
    // prints.
    void print(std::ostream& os) const override {
        os << "[capacity: rows " << firstRow() << ".." << (static_cast<int>(posRowCap_) - 1) << ", cols "
           << firstCol() << ".." << (static_cast<int>(posColCap_) - 1) << "]\n";
        for (int i = firstRow(); i < static_cast<int>(posRowCap_); ++i) {
            std::string line = rowString(i);
            os << line << '\n';
            if (i == -1 && negRowCap_ > 0) {
                std::string sep(line.size(), '-');
                auto barPos = line.find('|');
                if (barPos != std::string::npos) sep[barPos] = '+';
                os << sep << '\n';
            }
        }
    }

    // Only walks its own currently allocated capacity (not any global
    // bound), which after a conversion is sized just large enough to cover
    // the bits that were actually set.
    void forEachSet(const std::function<void(int, int)>& fn) const override {
        for (int i = firstRow(); i < static_cast<int>(posRowCap_); ++i) {
            for (int j = firstCol(); j < static_cast<int>(posColCap_); ++j) {
                if (get(i, j)) fn(i, j);
            }
        }
    }

    // A bit grid has no more direct way to encode a number than writing
    // its bits one at a time (each set() call growing the array as
    // needed), so this defers to the shared helper.
    void setColumnValue(int j, double n) override { decomposeColumnValue(*this, j, n); }

private:
    int firstRow() const { return -static_cast<int>(negRowCap_); }
    int firstCol() const { return -static_cast<int>(negColCap_); }

    bool inCapacity(int i, int j) const {
        return i >= firstRow() && i < static_cast<int>(posRowCap_) && j >= firstCol() &&
               j < static_cast<int>(posColCap_);
    }

    std::size_t index(int i, int j) const {
        std::size_t ri = static_cast<std::size_t>(i + static_cast<int>(negRowCap_));
        std::size_t rj = static_cast<std::size_t>(j + static_cast<int>(negColCap_));
        return ri * (negColCap_ + posColCap_) + rj;
    }

    std::string rowString(int i) const {
        std::string s;
        for (int j = firstCol(); j < static_cast<int>(posColCap_); ++j) {
            s += get(i, j) ? '1' : '0';
            if (j == -1 && negColCap_ > 0) {
                s += " | ";
            } else if (j + 1 < static_cast<int>(posColCap_)) {
                s += ' ';
            }
        }
        return s;
    }

    // Smallest power-of-two-ish capacity, starting from `current` (0 counts
    // as needing a first allocation of 1), that is at least `needed`.
    static std::size_t growCapacity(std::size_t current, std::size_t needed) {
        std::size_t cap = current == 0 ? 1 : current;
        while (cap < needed) cap *= 2;
        return cap;
    }

    // Doubles whichever of the four capacities (more positive/negative
    // rows/columns) are too small to hold (i, j), then copies the old grid
    // into a freshly allocated, larger one.
    void growToFit(int i, int j) {
        std::size_t newPosRowCap = posRowCap_;
        std::size_t newNegRowCap = negRowCap_;
        std::size_t newPosColCap = posColCap_;
        std::size_t newNegColCap = negColCap_;

        if (i >= 0 && static_cast<std::size_t>(i) >= posRowCap_) {
            newPosRowCap = growCapacity(posRowCap_, static_cast<std::size_t>(i) + 1);
        } else if (i < 0 && static_cast<std::size_t>(-i) > negRowCap_) {
            newNegRowCap = growCapacity(negRowCap_, static_cast<std::size_t>(-i));
        }
        if (j >= 0 && static_cast<std::size_t>(j) >= posColCap_) {
            newPosColCap = growCapacity(posColCap_, static_cast<std::size_t>(j) + 1);
        } else if (j < 0 && static_cast<std::size_t>(-j) > negColCap_) {
            newNegColCap = growCapacity(negColCap_, static_cast<std::size_t>(-j));
        }

        if (newPosRowCap == posRowCap_ && newNegRowCap == negRowCap_ && newPosColCap == posColCap_ &&
            newNegColCap == negColCap_) {
            return;  // already fits
        }

        std::size_t newRows = newNegRowCap + newPosRowCap;
        std::size_t newCols = newNegColCap + newPosColCap;
        std::vector<bool> newBits(newRows * newCols, false);

        std::size_t oldRows = negRowCap_ + posRowCap_;
        std::size_t oldCols = negColCap_ + posColCap_;
        for (std::size_t oi = 0; oi < oldRows; ++oi) {
            int origI = static_cast<int>(oi) - static_cast<int>(negRowCap_);
            std::size_t ni = static_cast<std::size_t>(origI + static_cast<int>(newNegRowCap));
            for (std::size_t oj = 0; oj < oldCols; ++oj) {
                if (!bits_[oi * oldCols + oj]) continue;
                int origJ = static_cast<int>(oj) - static_cast<int>(negColCap_);
                std::size_t nj = static_cast<std::size_t>(origJ + static_cast<int>(newNegColCap));
                newBits[ni * newCols + nj] = true;
            }
        }

        bits_ = std::move(newBits);
        posRowCap_ = newPosRowCap;
        negRowCap_ = newNegRowCap;
        posColCap_ = newPosColCap;
        negColCap_ = newNegColCap;
    }

    std::size_t posRowCap_ = 0;
    std::size_t negRowCap_ = 0;
    std::size_t posColCap_ = 0;
    std::size_t negColCap_ = 0;
    std::vector<bool> bits_;
};

}  // namespace smooth
