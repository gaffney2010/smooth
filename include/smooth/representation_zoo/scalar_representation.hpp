#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "smooth/representation_base.hpp"

namespace smooth {

// Stores the number as a single plain value rather than an (i, j) bit
// grid: every term this can hold lives in column j = 0, so its value is
// just that one number. Still a RepresentationBase, purely so a number
// stored this way can convert to and from the others.
//
// get()/set() on any column other than 0 throw std::invalid_argument,
// including indirectly when converting a non-scalar value into this one.
class ScalarRepresentation : public RepresentationBase {
public:
    explicit ScalarRepresentation(bool allow_fractional, std::shared_ptr<Metrics> metrics = nullptr)
        : fractional_(allow_fractional), value_(0.0), metrics_(std::move(metrics)) {}

    // Individual bits are recovered from the stored value by dividing out
    // 2^i and checking parity, same approach as RowValuesRepresentation.
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
        if (metrics_) metrics_->increment("scalar_operations");
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

    // Decodes the stored value into individual (i, 0) bits: integer part
    // via bit shifting, fractional part (when allowed) via repeated
    // doubling.
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

    void setMetricsPtr(std::shared_ptr<Metrics> metrics) override { metrics_ = std::move(metrics); }

    // When `other` is also Scalar, a direct addition -- no bit
    // decomposition needed. Otherwise falls back to reading other's bits
    // via forEachSet(); each must be in column 0.
    void addInPlace(const RepresentationBase& other) override {
        if (const auto* scalar = dynamic_cast<const ScalarRepresentation*>(&other)) {
            if (metrics_) metrics_->increment("scalar_operations");
            value_ += scalar->value_;
            return;
        }
        std::vector<std::pair<int, int>> bits;
        other.forEachSet([&bits](int i, int j) { bits.emplace_back(i, j); });
        for (const auto& bit : bits) {
            requireColumnZero(bit.second);
            if (metrics_) metrics_->increment("scalar_operations");
            value_ += std::pow(2.0, bit.first);
        }
    }

    // Two plain numbers multiply directly. When `other` isn't Scalar, its
    // column j contributes a term at column j, representable here only if
    // j == 0 -- otherwise this throws, unless the stored value is exactly
    // 0 (0 * anything is always 0, regardless of other's shape).
    void multiplyInPlace(const RepresentationBase& other) override {
        if (const auto* scalar = dynamic_cast<const ScalarRepresentation*>(&other)) {
            if (metrics_) metrics_->increment("scalar_operations");
            value_ *= scalar->value_;
            return;
        }
        if (value_ == 0.0) return;
        std::map<int, double> otherTotals;
        other.forEachSet([&otherTotals](int i, int j) { otherTotals[j] += std::pow(2.0, i); });
        double product = 0.0;
        for (const auto& col : otherTotals) {
            requireColumnZero(col.first);
            if (metrics_) metrics_->increment("scalar_operations", 2);  // one multiply, one add
            product += value_ * col.second;
        }
        value_ = product;
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
    std::shared_ptr<Metrics> metrics_;
};

}  // namespace smooth
