#pragma once

#include <cmath>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "smooth/representation_base.hpp"

namespace smooth {

// Stores the number as a single plain value (an integer or a float,
// depending on whether fractional terms are allowed) rather than as a
// (i, j) bit grid: every term this can hold lives in column j = 0, so its
// value is just that one number (3^0 = 1). It's still a RepresentationBase,
// like Sparse/RowValues/Dynamic, purely so a number stored this way can
// still convert to and from the others through the ordinary
// ensure()/forEachSet()/setColumnValue() machinery -- it's not a
// standalone type of its own.
//
// get()/set() on any column other than 0 -- a term this representation
// structurally cannot hold -- throw std::invalid_argument, including
// (indirectly) when converting some other representation's non-scalar
// value into this one.
class ScalarRepresentation : public RepresentationBase {
public:
    explicit ScalarRepresentation(bool allow_fractional) : fractional_(allow_fractional), value_(0.0) {}

    // Individual bits are recovered from the stored value by dividing out
    // 2^i and checking parity, the same approach RowValuesRepresentation
    // uses for its column totals.
    bool get(int i, int j) const override {
        if (j != 0) return false;
        double scaled = value_ / std::pow(2.0, i);
        long long whole = static_cast<long long>(std::llround(std::floor(scaled + 1e-9)));
        long long parity = whole % 2;
        if (parity < 0) parity += 2;
        return parity == 1;
    }

    bool set(int i, int j, bool value) override {
        requireColumnZero(j);
        if (get(i, 0) == value) return false;
        double delta = std::pow(2.0, i);
        value_ += value ? delta : -delta;
        return true;
    }

    void reset() override { value_ = 0.0; }

    // The stored value already *is* the total (3^0 = 1) -- nothing to sum.
    double value() const override { return value_; }

    void print(std::ostream& os) const override {
        if (fractional_) {
            os << value_;
        } else {
            os << static_cast<long long>(std::llround(value_));
        }
        os << '\n';
    }

    // Decodes the stored value into individual (i, 0) bits: the integer
    // part via ordinary bit shifting, and -- when fractional terms are
    // allowed -- the fractional part via repeated doubling.
    void forEachSet(const std::function<void(int, int)>& fn) const override {
        long long intPart = static_cast<long long>(std::floor(value_ + 1e-9));
        for (int i = 0; intPart != 0; ++i, intPart >>= 1) {
            if (intPart & 1) fn(i, 0);
        }
        double frac = value_ - std::floor(value_ + 1e-9);
        int i = -1;
        while (frac > 1e-9 && i > -64) {
            frac *= 2.0;
            if (frac >= 1.0 - 1e-9) {
                fn(i, 0);
                frac -= 1.0;
            }
            --i;
        }
    }

    // O(1), exact assignment -- the stored value already is n_j for j = 0.
    void setColumnValue(int j, double n) override {
        requireColumnZero(j);
        value_ = n;
    }

    std::unique_ptr<RepresentationBase> clone() const override {
        return std::make_unique<ScalarRepresentation>(*this);
    }

    // When `other` is also a ScalarRepresentation, this is a direct scalar
    // addition -- no bit decomposition needed on either side. Otherwise,
    // falls back to reading other's bits via forEachSet(); each one must
    // be in column 0, since that's the only term this representation can
    // hold.
    void addInPlace(const RepresentationBase& other) override {
        if (const auto* scalar = dynamic_cast<const ScalarRepresentation*>(&other)) {
            value_ += scalar->value_;
            return;
        }
        std::vector<std::pair<int, int>> bits;
        other.forEachSet([&bits](int i, int j) { bits.emplace_back(i, j); });
        for (const auto& bit : bits) {
            requireColumnZero(bit.second);
            value_ += std::pow(2.0, bit.first);
        }
    }

private:
    static void requireColumnZero(int j) {
        if (j != 0) {
            throw std::invalid_argument(
                "ScalarRepresentation: can only represent a plain number (column j = 0), not a term with j != 0");
        }
    }

    bool fractional_;
    double value_;
};

}  // namespace smooth
