#pragma once

#include <functional>
#include <string>
#include <vector>

#include "smooth/plan.hpp"

namespace smooth {

// A named, fixed arithmetic expression to run through a Plan (or any Plan
// subclass -- see plan_zoo/), purely so the same expression can be measured
// identically across every representation. build() is written directly in
// terms of Plan's own builder methods (scalar()/plus()/times()/left()/
// right()), so it works unchanged against a Plan&, a SparsePlan&, a
// MatrixPlan&, or a RowValuesPlan& -- calculate()/plan() dispatch to the
// right representation's convertLeaf()/combine() virtually regardless of
// which concrete type the reference is bound to.
//
// These are meant to look like ordinary, everyday arithmetic -- not edge
// cases (no zeros, no negatives, no single-leaf expressions) -- so the
// counters they produce reflect typical usage. A couple intentionally push
// toward the upper end of "typical" (6-10 numbers) to see how the counters
// scale, without going further into deliberately pathological territory.
struct Profile {
    std::string name;
    std::function<void(Plan&)> build;
};

// Ordered roughly from smallest/simplest to largest/most structurally
// involved.
inline std::vector<Profile> allProfiles() {
    std::vector<Profile> profiles;

    // 2 numbers: the simplest possible non-trivial expression.
    profiles.push_back({"two_number_sum", [](Plan& p) { p.scalar(47).plus().scalar(35); }});
    profiles.push_back({"two_number_product", [](Plan& p) { p.scalar(12).times().scalar(9); }});

    // 3 numbers: a small mixed-operator expression, like a line-item total
    // (price * quantity) + shipping.
    profiles.push_back({"price_quantity_plus_shipping", [](Plan& p) {
                             p.left().scalar(25).times().scalar(3).right().plus().scalar(8);
                         }});

    // 4 numbers: an unbracketed left-associative chain, like summing four
    // quarterly totals.
    profiles.push_back({"quarterly_totals", [](Plan& p) {
                             p.scalar(120).plus().scalar(95).plus().scalar(130).plus().scalar(110);
                         }});

    // 6 numbers: a small basket of (price * quantity) line items summed
    // together -- a realistic mix of both operators.
    profiles.push_back({"weighted_basket", [](Plan& p) {
                             p.left()
                                 .scalar(12)
                                 .times()
                                 .scalar(2)
                                 .right()
                                 .plus()
                                 .left()
                                 .scalar(5)
                                 .times()
                                 .scalar(4)
                                 .right()
                                 .plus()
                                 .left()
                                 .scalar(9)
                                 .times()
                                 .scalar(3)
                                 .right();
                         }});

    // 6 numbers: a longer unbracketed sum, e.g. six monthly totals.
    profiles.push_back({"half_year_totals", [](Plan& p) {
                             p.scalar(40)
                                 .plus()
                                 .scalar(35)
                                 .plus()
                                 .scalar(50)
                                 .plus()
                                 .scalar(45)
                                 .plus()
                                 .scalar(60)
                                 .plus()
                                 .scalar(55);
                         }});

    // 8 numbers: two levels of nested groups -- (a+b)*(c+d) + (e+f)*(g+h) --
    // like combining two independently-scored halves of a game.
    profiles.push_back({"nested_score_totals", [](Plan& p) {
                             p.left()
                                 .left()
                                 .scalar(3)
                                 .plus()
                                 .scalar(5)
                                 .right()
                                 .times()
                                 .left()
                                 .scalar(2)
                                 .plus()
                                 .scalar(4)
                                 .right()
                                 .right()
                                 .plus()
                                 .left()
                                 .left()
                                 .scalar(6)
                                 .plus()
                                 .scalar(1)
                                 .right()
                                 .times()
                                 .left()
                                 .scalar(4)
                                 .plus()
                                 .scalar(3)
                                 .right()
                                 .right();
                         }});

    // 10 numbers: the upper end of "typical" -- a flat sum, e.g. ten daily
    // readings totaled at the end of the week-plus.
    profiles.push_back({"ten_day_totals", [](Plan& p) {
                             p.scalar(7)
                                 .plus()
                                 .scalar(3)
                                 .plus()
                                 .scalar(9)
                                 .plus()
                                 .scalar(2)
                                 .plus()
                                 .scalar(8)
                                 .plus()
                                 .scalar(5)
                                 .plus()
                                 .scalar(6)
                                 .plus()
                                 .scalar(4)
                                 .plus()
                                 .scalar(10)
                                 .plus()
                                 .scalar(1);
                         }});

    return profiles;
}

}  // namespace smooth
