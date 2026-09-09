#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"
#include "smooth/row_values_representation.hpp"

namespace smooth {

// A Plan that performs its arithmetic by converting every value to a
// RowValuesRepresentation and combining them via its own
// addInPlace()/multiplyInPlace() -- direct column-total accumulation for
// addition, and convolution (long multiplication in base 3) for
// multiplication (see row_values_representation.hpp) -- rather than
// Plan's default plain double arithmetic. Building/printing/Metrics/
// error-handling are all unchanged, inherited as-is from Plan; only
// name(), convertLeaf(), and combine() differ.
//
// Every RepresentationBase implementation, RowValuesRepresentation
// included, can only ever hold a non-negative magnitude, so a negative
// leaf throws -- the same restriction SmoothNumberBase::setValue() has
// for the two unsigned types, for the same underlying reason.
class RowValuesPlan : public Plan {
public:
    // See SparsePlan's constructor for why this is spelled out explicitly
    // rather than via `using Plan::Plan;`.
    explicit RowValuesPlan(std::shared_ptr<Metrics> metrics = nullptr) : Plan(std::move(metrics)) {}

    std::string name() const override { return "row_values"; }

protected:
    double convertLeaf(double raw) const override {
        requireNonNegative(raw);
        RowValuesRepresentation rep(/*allow_fractional=*/true);
        rep.setColumnValue(0, raw);
        return rep.value();
    }

    double combine(Op op, double left, double right) const override {
        RowValuesRepresentation a(/*allow_fractional=*/true);
        RowValuesRepresentation b(/*allow_fractional=*/true);
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
                "RowValuesPlan: RowValuesRepresentation can only hold a non-negative magnitude");
        }
    }
};

}  // namespace smooth
