#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"
#include "smooth/sparse_representation.hpp"

namespace smooth {

// A Plan that performs its arithmetic by converting every value to a
// SparseRepresentation and combining them via its own
// addInPlace()/multiplyInPlace() -- the pairwise-exponent-sum-with-carry
// strategy (see sparse_representation.hpp) -- rather than Plan's default
// plain double arithmetic. Building/printing/Metrics/error-handling are
// all unchanged, inherited as-is from Plan; only name(), convertLeaf(),
// and combine() differ.
//
// Every RepresentationBase implementation, SparseRepresentation included,
// can only ever hold a non-negative magnitude, so a negative leaf throws
// -- this is the same restriction SmoothNumberBase::setValue() has for the
// two unsigned types, for the same underlying reason.
class SparsePlan : public Plan {
public:
    // Forwards to Plan's own (metrics) constructor explicitly, rather than
    // relying on `using Plan::Plan;` to inherit it -- see Signed<Base>'s
    // constructor (signed.hpp) for why that's clearer than relying on
    // inherited-constructor rules when the base constructor has a default
    // argument.
    explicit SparsePlan(std::shared_ptr<Metrics> metrics = nullptr) : Plan(std::move(metrics)) {}

    std::string name() const override { return "sparse"; }

protected:
    double convertLeaf(double raw) const override {
        requireNonNegative(raw);
        SparseRepresentation rep(/*allow_fractional=*/true, metricsPtr());
        rep.setColumnValue(0, raw);
        return rep.value();
    }

    double combine(Op op, double left, double right) const override {
        SparseRepresentation a(/*allow_fractional=*/true, metricsPtr());
        SparseRepresentation b(/*allow_fractional=*/true, metricsPtr());
        a.setColumnValue(0, left);
        b.setColumnValue(0, right);
        if (op == Op::Add) {
            a.addInPlace(b);
        } else {
            a.multiplyInPlace(b);
        }
        return a.value();
    }

private:
    static void requireNonNegative(double value) {
        if (value < 0.0) {
            throw std::invalid_argument(
                "SparsePlan: SparseRepresentation can only hold a non-negative magnitude");
        }
    }
};

}  // namespace smooth
