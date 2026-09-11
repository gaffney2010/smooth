#pragma once

#include <functional>
#include <string>
#include <vector>

#include "smooth/plan.hpp"
#include "smooth/smooth_integer.hpp"

namespace smooth {

// A named, fixed arithmetic expression to run against a Plan (or any Plan
// subclass -- see plan_zoo/), purely so the same expression can be measured
// identically across every representation. run() is written directly in
// terms of Plan's own builder methods (scalar()/number()/numberVia()/
// plus()/times()/left()/right()), so it works unchanged against a Plan&
// bound to a DefaultPlan, SparsePlan, MatrixPlan, RowValuesPlan, etc. --
// calculate() dispatches to the right representation's convertLeaf()/
// convertNumberLeaf()/targetRepresentation() virtually regardless of which
// concrete type the reference is bound to. run() builds *and* calls
// calculate() itself (rather than just building, leaving calculate() to
// the caller), so a profile that constructs its own SmoothNumberBase
// objects for numberVia() can keep them alive for exactly as long as
// they're needed, entirely within its own local scope.
//
// These are meant to look like ordinary, everyday arithmetic -- not edge
// cases (no zeros, no negatives, no single-leaf expressions) -- so the
// counters they produce reflect typical usage. A couple intentionally push
// toward the upper end of "typical" (6-10 numbers) to see how the counters
// scale, without going further into deliberately pathological territory.
struct Profile {
    std::string name;
    std::function<double(Plan&)> run;
};

// Ordered roughly from smallest/simplest to largest/most structurally
// involved.
inline std::vector<Profile> allProfiles() {
    std::vector<Profile> profiles;

    // 2 numbers: the simplest possible non-trivial expression.
    profiles.push_back({"two_number_sum", [](Plan& p) { return p.scalar(47).plus().scalar(35).calculate(); }});
    profiles.push_back(
        {"two_number_product", [](Plan& p) { return p.scalar(12).times().scalar(9).calculate(); }});

    // 3 numbers: a small mixed-operator expression, like a line-item total
    // (price * quantity) + shipping.
    profiles.push_back({"price_quantity_plus_shipping", [](Plan& p) {
                             return p.left().scalar(25).times().scalar(3).right().plus().scalar(8).calculate();
                         }});

    // 4 numbers: an unbracketed left-associative chain, like summing four
    // quarterly totals.
    profiles.push_back({"quarterly_totals", [](Plan& p) {
                             return p.scalar(120)
                                 .plus()
                                 .scalar(95)
                                 .plus()
                                 .scalar(130)
                                 .plus()
                                 .scalar(110)
                                 .calculate();
                         }});

    // 6 numbers: a small basket of (price * quantity) line items summed
    // together -- a realistic mix of both operators.
    profiles.push_back({"weighted_basket", [](Plan& p) {
                             return p.left()
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
                                 .right()
                                 .calculate();
                         }});

    // 6 numbers: a longer unbracketed sum, e.g. six monthly totals.
    profiles.push_back({"half_year_totals", [](Plan& p) {
                             return p.scalar(40)
                                 .plus()
                                 .scalar(35)
                                 .plus()
                                 .scalar(50)
                                 .plus()
                                 .scalar(45)
                                 .plus()
                                 .scalar(60)
                                 .plus()
                                 .scalar(55)
                                 .calculate();
                         }});

    // 8 numbers: two levels of nested groups -- (a+b)*(c+d) + (e+f)*(g+h) --
    // like combining two independently-scored halves of a game.
    profiles.push_back({"nested_score_totals", [](Plan& p) {
                             return p.left()
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
                                 .right()
                                 .calculate();
                         }});

    // 10 numbers: the upper end of "typical" -- a flat sum, e.g. ten daily
    // readings totaled at the end of the week-plus.
    profiles.push_back({"ten_day_totals", [](Plan& p) {
                             return p.scalar(7)
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
                                 .scalar(1)
                                 .calculate();
                         }});

    // 3 numbers, via numberVia() instead of scalar(): three real
    // SmoothInteger objects (not just plain doubles), summed. Sharing the
    // Plan's own Metrics with each number (via setMetricsPtr()) means that,
    // since every Plan forces a numberVia() leaf through its own real
    // conversion into its own target representation (see
    // Plan::convertNumberLeaf()/targetRepresentation() in plan.hpp), each
    // number's own conversion counter (convert_dynamic_to_<target>) lands
    // in the same Metrics row as the rest of that Plan's counters -- a
    // different one per plan kind (e.g. convert_dynamic_to_sparse for
    // SparsePlan, convert_dynamic_to_row_values for RowValuesPlan), except
    // MatrixPlan, whose target (Dynamic) is already every fresh number's
    // canonical representation, so nothing there needs converting.
    profiles.push_back({"converted_number_sum", [](Plan& p) {
                             SmoothInteger a, b, c;
                             a.setMetricsPtr(p.metricsPtr());
                             b.setMetricsPtr(p.metricsPtr());
                             c.setMetricsPtr(p.metricsPtr());
                             a.setValue(30LL);
                             b.setValue(45LL);
                             c.setValue(20LL);
                             return p.numberVia(a).plus().numberVia(b).plus().numberVia(c).calculate();
                         }});

    // 3 numbers, mixing numberVia() and scalar(): (price * quantity) +
    // shipping, where price and quantity are real SmoothInteger objects and
    // shipping is a plain scalar -- showing the two leaf kinds combine
    // freely in the same expression.
    profiles.push_back({"converted_price_quantity_plus_shipping", [](Plan& p) {
                             SmoothInteger price, quantity;
                             price.setMetricsPtr(p.metricsPtr());
                             quantity.setMetricsPtr(p.metricsPtr());
                             price.setValue(18LL);
                             quantity.setValue(4LL);
                             return p.left()
                                 .numberVia(price)
                                 .times()
                                 .numberVia(quantity)
                                 .right()
                                 .plus()
                                 .scalar(6)
                                 .calculate();
                         }});

    return profiles;
}

}  // namespace smooth
