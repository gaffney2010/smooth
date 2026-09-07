#pragma once

#include <cmath>
#include <cstddef>
#include <set>
#include <utility>

#include "smooth/representation_base.hpp"

namespace smooth {

// The set of (i, j) coordinates whose bit is set. Dimensions aren't needed
// for storage, but the constructor still takes them so SmoothNumber can
// build every representation the same way.
class SparseRepresentation : public RepresentationBase {
public:
    SparseRepresentation(std::size_t /*max_rows*/, std::size_t /*max_cols*/, std::size_t /*neg_rows*/,
                          std::size_t /*neg_cols*/) {}

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
            total += std::pow(2.0, static_cast<double>(coord.first)) *
                     std::pow(3.0, static_cast<double>(coord.second));
        }
        return total;
    }

    void print(std::ostream& os) const override {
        os << "{";
        bool first = true;
        for (const auto& coord : coords_) {
            if (!first) os << ", ";
            os << "(" << coord.first << ", " << coord.second << ")";
            first = false;
        }
        os << "}\n";
    }

private:
    std::set<std::pair<int, int>> coords_;
};

}  // namespace smooth
