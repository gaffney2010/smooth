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
        accumulate(j, value ? delta : -delta);
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

    // Doesn't need explicit carry handling: each contribution just adds
    // onto its column's running total, and ordinary floating-point
    // addition already produces the correct combined value (e.g. two
    // contributions of 2^i at the same (i, j) simply sum to 2^(i+1),
    // exactly as if a carry had been handled explicitly).
    //
    // When `other` is also a RowValuesRepresentation, its columns already
    // *are* the n_j totals we want to add -- so this adds them directly,
    // column by column, with no need to decompose either side into
    // individual bits at all (not even to reconstruct `other`'s). Only
    // when `other` is some other representation (Sparse, Dynamic) does
    // this fall back to reading its bits via forEachSet() and accumulating
    // each one's 2^i.
    void addInPlace(const RepresentationBase& other) override {
        if (const auto* rowValues = dynamic_cast<const RowValuesRepresentation*>(&other)) {
            // Snapshot first (not strictly required here, since we only
            // ever update -- never insert or erase -- an already-visited
            // column, but this keeps the safety argument the same as the
            // fallback below regardless of aliasing).
            std::vector<std::pair<int, double>> columns(rowValues->values_.begin(), rowValues->values_.end());
            for (const auto& col : columns) {
                accumulate(col.first, col.second);
            }
            return;
        }

        std::vector<std::pair<int, int>> bits;
        other.forEachSet([&bits](int i, int j) { bits.emplace_back(i, j); });
        for (const auto& bit : bits) {
            accumulate(bit.second, std::pow(2.0, bit.first));
        }
    }

    // Convolves this's column totals with other's: since value() =
    // sum_j n_j * 3^j, multiplying two of these together is exactly
    // multiplying two polynomials in the variable 3 (or long
    // multiplication in base 3, if you allow a "digit" n_j to be any
    // magnitude rather than just 0..2) -- the product's column j1+j2 gets
    // n_j1 * n_j2 added in, for every pair of columns (j1, j2). Building
    // the whole result in a fresh map first (rather than writing into
    // values_ as the pairs are found) is what makes this safe even when
    // `other` is `*this` (squaring): `otherTotals` is captured as an
    // independent snapshot before values_ is touched at all.
    void multiplyInPlace(const RepresentationBase& other) override {
        std::map<int, double> otherTotals;
        if (const auto* rowValues = dynamic_cast<const RowValuesRepresentation*>(&other)) {
            otherTotals = rowValues->values_;
        } else {
            other.forEachSet([&otherTotals](int i, int j) { otherTotals[j] += std::pow(2.0, i); });
        }

        std::map<int, double> product;
        for (const auto& colA : values_) {
            for (const auto& colB : otherTotals) {
                product[colA.first + colB.first] += colA.second * colB.second;
            }
        }
        for (auto it = product.begin(); it != product.end();) {
            if (it->second == 0.0) {
                it = product.erase(it);
            } else {
                ++it;
            }
        }
        values_ = std::move(product);
    }

private:
    // Adds delta onto column j's total, dropping the entry if that brings
    // it back to exactly zero (keeping the invariant that values_ only
    // ever holds nonzero columns).
    void accumulate(int j, double delta) {
        double updated = values_[j] + delta;
        if (updated == 0.0) {
            values_.erase(j);
        } else {
            values_[j] = updated;
        }
    }

    bool fractional_;
    std::map<int, double> values_;
};

}  // namespace smooth
