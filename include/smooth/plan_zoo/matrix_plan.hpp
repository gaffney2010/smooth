#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "smooth/dynamic_matrix_representation.hpp"
#include "smooth/metrics.hpp"
#include "smooth/plan.hpp"

namespace smooth {

// A Plan that performs its arithmetic by converting every value to a
// DynamicMatrixRepresentation -- referred to as "matrix" here, matching
// what this library calls its grid/matrix-shaped representation now that
// the old fixed-size MatrixRepresentation has been superseded by it (see
// dynamic_matrix_representation.hpp) -- and combining them via its own
// addInPlace()/multiplyInPlace() (the same pairwise-exponent-sum-with-
// carry strategy Sparse uses, since a raw bit grid needs the same
// approach regardless of whether it's backed by a std::set or an array).
// Building/printing/Metrics/error-handling are all unchanged, inherited
// as-is from Plan; only name(), convertLeaf(), and combine() differ.
//
// Every RepresentationBase implementation, DynamicMatrixRepresentation
// included, can only ever hold a non-negative magnitude, so a negative
// leaf throws -- the same restriction SmoothNumberBase::setValue() has
// for the two unsigned types, for the same underlying reason.
class MatrixPlan : public Plan {
public:
    // See SparsePlan's constructor for why this is spelled out explicitly
    // rather than via `using Plan::Plan;`.
    explicit MatrixPlan(std::shared_ptr<Metrics> metrics = nullptr) : Plan(std::move(metrics)) {}

    std::string name() const override { return "matrix"; }

protected:
    double convertLeaf(double raw) const override {
        requireNonNegative(raw);
        DynamicMatrixRepresentation rep(/*allow_fractional=*/true);
        rep.setColumnValue(0, raw);
        return rep.value();
    }

    double combine(Op op, double left, double right) const override {
        DynamicMatrixRepresentation a(/*allow_fractional=*/true);
        DynamicMatrixRepresentation b(/*allow_fractional=*/true);
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
                "MatrixPlan: DynamicMatrixRepresentation can only hold a non-negative magnitude");
        }
    }
};

}  // namespace smooth
