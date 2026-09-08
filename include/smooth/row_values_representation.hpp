#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "smooth/representation_base.hpp"

namespace smooth {

// One number n_j per column j that actually has a set bit, where
// n_j = sum_i (bit(i,j) ? 2^i : 0), so the total value is sum_j n_j * 3^j.
// Stored as a map keyed by j rather than an array, so it's unbounded and
// only ever holds entries for columns that have something set in them
// (an entry is dropped once its value returns to zero). n_j is an integer
// if the number doesn't allow fractional (negative-index) terms, and may
// be fractional otherwise.
class RowValuesRepresentation : public RepresentationBase {
public:
    explicit RowValuesRepresentation(bool allow_fractional) : fractional_(allow_fractional) {}

    // Individual bits are recovered from n_j by dividing out 2^i and
    // checking parity. This is exact for the row ranges this class is
    // meant for, since every n_j is an exact sum of distinct powers of two,
    // but it is floating-point-based and could get unreliable at very
    // large row counts.
    bool get(int i, int j) const override {
        auto it = values_.find(j);
        double n = (it == values_.end()) ? 0.0 : it->second;
        double scaled = n / std::pow(2.0, i);
        long long whole = static_cast<long long>(std::llround(std::floor(scaled + 1e-9)));
        long long parity = whole % 2;
        if (parity < 0) parity += 2;
        return parity == 1;
    }

    bool set(int i, int j, bool value) override {
        if (get(i, j) == value) return false;
        double delta = std::pow(2.0, i);
        double updated = values_[j] + (value ? delta : -delta);
        if (updated == 0.0) {
            values_.erase(j);
        } else {
            values_[j] = updated;
        }
        return true;
    }

    void reset() override { values_.clear(); }

    // Each n_j already sums its row's contribution, so this is a single
    // pass over the (typically few) columns that have anything set, rather
    // than a full grid walk, with no per-bit decoding needed.
    double value() const override {
        double total = 0.0;
        for (const auto& col : values_) {
            total += col.second * std::pow(3.0, static_cast<double>(col.first));
        }
        return total;
    }

    // One line per column j that has a set bit, showing n_j.
    void print(std::ostream& os) const override {
        for (const auto& col : values_) {
            os << "n[" << col.first << "] = ";
            if (fractional_) {
                os << col.second;
            } else {
                os << static_cast<long long>(std::llround(col.second));
            }
            os << '\n';
        }
    }

    // Decodes each n_j back into individual (i, j) bits: the integer part
    // via ordinary bit shifting, and -- when fractional terms are allowed
    // -- the fractional part via repeated doubling, the standard way to
    // read off a binary fraction's digits.
    void forEachSet(const std::function<void(int, int)>& fn) const override {
        for (const auto& col : values_) {
            int j = col.first;
            double n = col.second;

            long long intPart = static_cast<long long>(std::floor(n + 1e-9));
            for (int i = 0; intPart != 0; ++i, intPart >>= 1) {
                if (intPart & 1) fn(i, j);
            }

            double frac = n - std::floor(n + 1e-9);
            int i = -1;
            while (frac > 1e-9 && i > -64) {
                frac *= 2.0;
                if (frac >= 1.0 - 1e-9) {
                    fn(i, j);
                    frac -= 1.0;
                }
                --i;
            }
        }
    }

    // Storage here already *is* n_j per column, so setting one is a direct
    // O(1), exact assignment -- no bit decomposition, and none of the
    // accumulated floating-point error the default (RepresentationBase's
    // bit-by-bit set() loop) would introduce by adding/subtracting powers
    // of two one at a time.
    void setColumnValue(int j, double n) override {
        if (n == 0.0) {
            values_.erase(j);
        } else {
            values_[j] = n;
        }
    }

    std::unique_ptr<RepresentationBase> clone() const override {
        return std::make_unique<RowValuesRepresentation>(*this);
    }

    // Doesn't need explicit carry handling: `other`'s bits are captured up
    // front (safe even for self-addition), and each one just contributes
    // 2^i to its column's running total -- ordinary floating-point addition
    // already produces the correct combined value (e.g. two contributions
    // of 2^i at the same (i, j) simply sum to 2^(i+1), exactly as if the
    // carry had been handled explicitly), so this is a direct accumulation
    // rather than Sparse/Dynamic's bit-by-bit carry walk.
    void addInPlace(const RepresentationBase& other) override {
        std::vector<std::pair<int, int>> bits;
        other.forEachSet([&bits](int i, int j) { bits.emplace_back(i, j); });
        for (const auto& bit : bits) {
            double delta = std::pow(2.0, bit.first);
            double updated = values_[bit.second] + delta;
            if (updated == 0.0) {
                values_.erase(bit.second);
            } else {
                values_[bit.second] = updated;
            }
        }
    }

private:
    bool fractional_;
    std::map<int, double> values_;
};

}  // namespace smooth
