#pragma once

#include <cmath>
#include <functional>
#include <memory>
#include <set>
#include <utility>

#include "smooth/representation_base.hpp"

namespace smooth {

// The set of (i, j) coordinates whose bit is set -- inherently unbounded,
// since it only stores coordinates that are actually on. Every loop over
// coords_ visits only 1s, each incrementing "bit_iterations" on metrics_,
// if given.
class SparseRepresentation : public RepresentationBase {
public:
    explicit SparseRepresentation(bool /*allow_fractional*/, std::shared_ptr<Metrics> metrics = nullptr)
        : metrics_(std::move(metrics)) {}

    bool get(int i, int j) const override { return coords_.count({i, j}) > 0; }

    bool set(int i, int j, bool value) override {
        auto key = std::make_pair(i, j);
        bool present = coords_.count(key) > 0;
        if (present == value) return false;
        if (value) {
            coords_.insert(key);
        } else {
            coords_.erase(key);
        }
        return true;
    }

    void reset() override { coords_.clear(); }

    // Only visits set bits, skipping the (typically many) zero cells that a
    // full grid walk would waste time on.
    double value() const override {
        double total = 0.0;
        for (const auto& coord : coords_) {
            bumpIteration();
            total += std::pow(2.0, static_cast<double>(coord.first)) *
                     std::pow(3.0, static_cast<double>(coord.second));
        }
        return total;
    }

    void print(std::ostream& os) const override {
        os << "{";
        bool first = true;
        for (const auto& coord : coords_) {
            bumpIteration();
            if (!first) os << ", ";
            os << "(" << coord.first << ", " << coord.second << ")";
            first = false;
        }
        os << "}\n";
    }

    void forEachSet(const std::function<void(int, int)>& fn) const override {
        for (const auto& coord : coords_) {
            bumpIteration();
            fn(coord.first, coord.second);
        }
    }

    // No more direct way to encode a number than writing its bits one at
    // a time, so this defers to the shared helper.
    void setColumnValue(int j, double n) override { decomposeColumnValue(*this, j, n); }

    std::unique_ptr<RepresentationBase> clone() const override {
        return std::make_unique<SparseRepresentation>(*this);
    }

    void setMetricsPtr(std::shared_ptr<Metrics> metrics) override { metrics_ = std::move(metrics); }

    // No more direct way to add than walking other's bits and carrying, so
    // this defers to the shared helper.
    void addInPlace(const RepresentationBase& other) override { addBitsWithCarry(*this, other, metrics_); }

    // Likewise, defers to the shared helper for multiplying. `*this` is
    // passed as both destination and left-hand operand --
    // multiplyBitsWithCarry() snapshots both operands' bits before
    // resetting the destination, so this is safe.
    void multiplyInPlace(const RepresentationBase& other) override {
        multiplyBitsWithCarry(*this, *this, other, metrics_);
    }

private:
    void bumpIteration() const {
        if (metrics_) metrics_->increment("bit_iterations");
    }

    std::set<std::pair<int, int>> coords_;
    std::shared_ptr<Metrics> metrics_;
};

}  // namespace smooth
