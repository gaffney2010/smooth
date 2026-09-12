#include <iostream>

#include "smooth/plan_zoo.hpp"
#include "smooth/reduction_zoo.hpp"
#include "smooth/smooth.hpp"
#include "smooth/transformation_zoo.hpp"

// A quick tour of the library's main pieces. See the test suite for
// exhaustive coverage of each one.
int main() {
    // --- Basics: set/get/clear, value() = sum of 2^i * 3^j over set bits ---
    smooth::SmoothInteger n;
    n.set(0, 0);  // 2^0*3^0 = 1
    n.set(1, 0);  // 2^1*3^0 = 2
    n.set(0, 2);  // 2^0*3^2 = 9
    std::cout << "1 + 2 + 9 = " << n.value() << "\n";
    n.clear(1, 0);
    std::cout << "after clear(1,0): " << n.value() << "\n\n";

    // --- SmoothFloat: same bit grid, negative indices allowed ---
    smooth::SmoothFloat f;
    f.set(0, 0);   // 1
    f.set(-1, 0);  // 0.5
    f.set(0, -1);  // 1/3
    std::cout << "SmoothFloat: 1 + 0.5 + 1/3 = " << f.value() << "\n\n";

    // --- Signed variants and setValue() ---
    smooth::SmoothSignedInteger s;
    s.setValue(5LL);
    s.negate();
    std::cout << "SmoothSignedInteger: negate(5) = " << s.value() << "\n\n";

    // --- Addition and multiplication (never mutate their operands) ---
    smooth::SmoothInteger a, b;
    a.setValue(12LL);
    b.setValue(7LL);
    std::cout << "12 + 7 = " << (a + b).value() << ", 12 * 7 = " << (a * b).value() << "\n\n";

    // --- Representations: same number, different internal storage ---
    smooth::SmoothInteger r;
    r.set(1, 0);  // 2
    r.set(0, 2);  // 9
    std::cout << "Same value (" << r.value() << ") through Dynamic/Sparse/RowValues:\n";
    r.printDynamic();
    r.printSparse();
    r.printRowValues();
    std::cout << "\n";

    // --- Plan: a fluent scalar-expression builder ---
    std::cout << "3 * (4 + 2) via DefaultPlan:\n";
    smooth::DefaultPlan().scalar(3).times().left().scalar(4).plus().scalar(2).right().plan();
    std::cout << "\n" << smooth::SparsePlan().name() << " runs the same expression through Sparse instead:\n";
    smooth::SparsePlan sparsePlan;
    sparsePlan.scalar(3).times().left().scalar(4).plus().scalar(2).right();
    sparsePlan.plan();
    std::cout << "\n";

    // --- Transformations: value-preserving bit-grid rewrites ---
    smooth::SmoothInteger t;
    t.set(2, 0);  // 4
    t.set(3, 0);  // 8
    std::cout << "before merge: value = " << t.value() << ", ";
    t.printSparse();
    t.applyTransformation(smooth::MergeTransformation(), 2, 0);  // 2^2*3^0 + 2^3*3^0 = 2^2*3^1
    std::cout << "after merge(2,0): value = " << t.value() << ", ";
    t.printSparse();
    std::cout << "\n";

    // --- Reduction: repeatedly apply transformations to a fixed point ---
    smooth::SmoothInteger staircase;
    staircase.set(1, 1);  // 6
    staircase.set(4, 5);  // 3888
    auto rep = staircase.representationAs(smooth::SmoothInteger::Representation::Sparse);
    smooth::StaircaseReduction().run(*rep);
    std::cout << "StaircaseReduction fixed point for " << staircase.value() << ": value = " << rep->value() << ", ";
    rep->print(std::cout);

    return 0;
}
