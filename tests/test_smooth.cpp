// Lightweight, dependency-free test suite for the smooth library. No test
// framework is linked in (see CMakeLists.txt) -- CHECK() below just records
// pass/fail and prints failures as they happen; main() prints a summary and
// returns non-zero if anything failed, so this also works as a `ctest`.

#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "smooth/dynamic_matrix_representation.hpp"
#include "smooth/plan_zoo.hpp"
#include "smooth/row_values_representation.hpp"
#include "smooth/scalar_representation.hpp"
#include "smooth/smooth.hpp"
#include "smooth/sparse_representation.hpp"

using namespace smooth;

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool condition, const std::string& description) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << description << "\n";
    }
}

void checkNear(double actual, double expected, const std::string& description, double epsilon = 1e-9) {
    check(std::abs(actual - expected) < epsilon,
          description + " (expected " + std::to_string(expected) + ", got " + std::to_string(actual) + ")");
}

template <typename Fn>
void checkThrows(Fn&& fn, const std::string& description) {
    bool threw = false;
    try {
        fn();
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, description);
}

// Reads a single named counter's current value out of a Metrics, without
// depending on where it falls among the other counters' lines. Missing
// counters (nothing incremented it yet) read as 0, same as Metrics itself
// would report if asked directly.
long long counterValue(const std::shared_ptr<Metrics>& metrics, const std::string& name) {
    std::ostringstream out;
    metrics->print(out);
    std::string s = out.str();
    std::string prefix = name + " = ";
    auto pos = s.find(prefix);
    if (pos == std::string::npos) return 0;
    return std::stoll(s.substr(pos + prefix.size()));
}

// ---------------------------------------------------------------------
// Basic set/get/clear/value, per concrete type
// ---------------------------------------------------------------------
void testBasicSetGetClear() {
    SmoothInteger n;
    check(!n.get(0, 0), "SmoothInteger starts with every bit clear");
    checkNear(n.value(), 0.0, "empty SmoothInteger has value 0");

    n.set(0, 0);  // 2^0*3^0 = 1
    n.set(1, 0);  // 2^1*3^0 = 2
    n.set(0, 2);  // 2^0*3^2 = 9
    checkNear(n.value(), 12.0, "1 + 2 + 9 = 12 after setting three bits");
    check(n.get(0, 0) && n.get(1, 0) && n.get(0, 2), "all three set bits read back true");

    n.set(2, 1);  // 2^2*3^1 = 12
    checkNear(n.value(), 24.0, "12 + 12 = 24 after also setting (2,1)");

    n.clear(2, 1);
    check(!n.get(2, 1), "clear(2,1) turns the bit back off");
    checkNear(n.value(), 12.0, "value back to 12 after clearing (2,1)");

    n.set(1, 0, false);  // set() with value=false behaves like clear()
    check(!n.get(1, 0), "set(i, j, false) clears the bit");
}

// ---------------------------------------------------------------------
// Fractional (negative-index) support on SmoothFloat, and the
// non-fractional restriction on SmoothInteger.
// ---------------------------------------------------------------------
void testFractionalRestriction() {
    SmoothInteger whole;
    checkThrows([&] { whole.set(-1, 0); }, "SmoothInteger.set with negative row throws");
    checkThrows([&] { whole.set(0, -1); }, "SmoothInteger.set with negative column throws");
    check(!whole.allowsFractional(), "SmoothInteger.allowsFractional() is false");

    SmoothFloat frac;
    check(frac.allowsFractional(), "SmoothFloat.allowsFractional() is true");
    frac.set(0, 0);   // 1
    frac.set(-1, 0);  // 1/2
    frac.set(0, -1);  // 1/3
    checkNear(frac.value(), 1.0 + 0.5 + 1.0 / 3.0, "1 + 1/2 + 1/3 on a SmoothFloat");
}

// ---------------------------------------------------------------------
// setBounds(): an optional, after-the-fact guard that doesn't affect
// storage, only future set()/get() calls.
// ---------------------------------------------------------------------
void testSetBounds() {
    SmoothInteger n;
    n.set(5, 3);  // fine before any bounds are set
    checkNear(n.value(), std::pow(2.0, 5) * std::pow(3.0, 3), "value set before setBounds() is unaffected by it");

    n.setBounds(6, 4, 0, 0);  // rows in [0,6), cols in [0,4) -- (5,3) is still in range
    checkThrows([&] { n.set(9, 0); }, "set() outside bounds set via setBounds() throws");
    check(n.get(5, 3), "a bit set before setBounds() still reads back fine afterward");
    checkNear(n.value(), std::pow(2.0, 5) * std::pow(3.0, 3), "setBounds() doesn't retroactively touch existing bits");
}

// ---------------------------------------------------------------------
// setValue(): converting a plain integer/float directly into a number.
// ---------------------------------------------------------------------
void testSetValue() {
    SmoothInteger n;
    n.setValue(42LL);
    checkNear(n.value(), 42.0, "SmoothInteger.setValue(42)");

    n.setValue(0LL);
    checkNear(n.value(), 0.0, "setValue(0) after a nonzero value fully replaces it");

    checkThrows([&] { n.setValue(-1LL); }, "SmoothInteger.setValue(negative) throws");

    SmoothFloat f;
    f.setValue(3.75);
    checkNear(f.value(), 3.75, "SmoothFloat.setValue(3.75)");

    SmoothInteger wholeOnly;
    checkThrows([&] { wholeOnly.setValue(3.75); },
                "SmoothInteger.setValue(fractional double) throws (no fractional terms allowed)");
    wholeOnly.setValue(3.0);
    checkNear(wholeOnly.value(), 3.0, "SmoothInteger.setValue(3.0) is fine (integral double)");
}

// ---------------------------------------------------------------------
// Signed types: isNegative/setNegative/negate, and setValue() splitting
// off the sign automatically.
// ---------------------------------------------------------------------
void testSignedSetValueAndSign() {
    SmoothSignedInteger s;
    check(!s.isNegative(), "SmoothSignedInteger starts non-negative");

    s.set(1, 0);  // 2
    s.set(0, 2);  // 9
    checkNear(s.value(), 11.0, "SmoothSignedInteger magnitude 11 before negate()");
    s.negate();
    checkNear(s.value(), -11.0, "value() is negated after negate()");
    check(s.isNegative(), "isNegative() true after negate()");
    s.setNegative(false);
    checkNear(s.value(), 11.0, "setNegative(false) flips back to positive");

    SmoothSignedInteger fromNeg;
    fromNeg.setValue(-7LL);
    checkNear(fromNeg.value(), -7.0, "SmoothSignedInteger.setValue(-7)");
    check(fromNeg.isNegative(), "setValue(-7) sets isNegative()");

    SmoothSignedFloat sf;
    sf.setValue(-1.5);
    checkNear(sf.value(), -1.5, "SmoothSignedFloat.setValue(-1.5)");
    check(sf.isNegative(), "setValue(-1.5) sets isNegative()");
}

// ---------------------------------------------------------------------
// Internal representations: canonical() defaults to Dynamic, and the
// same number's value agrees across all three views.
// ---------------------------------------------------------------------
void testRepresentationsAgree() {
    SmoothInteger n;
    check(n.canonical() == SmoothNumberBase::Representation::Dynamic, "canonical() defaults to Dynamic");

    n.set(1, 0);  // 2
    n.set(0, 2);  // 9

    std::ostringstream sparseOut, rowValuesOut, dynamicOut;
    n.printSparse(sparseOut);
    n.printRowValues(rowValuesOut);
    n.printDynamic(dynamicOut);

    check(sparseOut.str() == "{(0, 2), (1, 0)}\n", "printSparse() output matches expected coordinate set");
    check(rowValuesOut.str() == "n[0] = 2\nn[2] = 1\n", "printRowValues() output matches expected n_j entries");
    check(!dynamicOut.str().empty(), "printDynamic() produces output");
    checkNear(n.value(), 11.0, "value() still correct after converting through all three representations");

    // n has a bit in column 2, which Scalar structurally can't represent.
    std::ostringstream discard;
    checkThrows([&] { n.printScalar(discard); },
                "printScalar() throws when the number has a term outside column 0");

    SmoothInteger plain;
    plain.setValue(17LL);
    std::ostringstream scalarOut;
    plain.printScalar(scalarOut);
    check(scalarOut.str() == "17\n", "printScalar() succeeds and prints the plain value when column 0 is all it has");
}

// ---------------------------------------------------------------------
// Each RepresentationBase implementation, exercised directly: get/set/
// reset/value/forEachSet/setColumnValue/clone.
// ---------------------------------------------------------------------
void testSparseRepresentationDirectly() {
    SparseRepresentation rep(true);
    check(rep.set(0, 0, true), "Sparse.set() on a fresh bit returns true (changed)");
    check(!rep.set(0, 0, true), "Sparse.set() to the same value returns false (unchanged)");
    check(rep.get(0, 0), "Sparse.get() reads back the set bit");
    checkNear(rep.value(), 1.0, "Sparse.value() after setting (0,0)");

    rep.set(-1, 0, true);  // 0.5
    checkNear(rep.value(), 1.5, "Sparse.value() after also setting (-1,0)");

    int count = 0;
    rep.forEachSet([&count](int, int) { ++count; });
    check(count == 2, "Sparse.forEachSet() visits exactly the 2 set bits");

    rep.setColumnValue(2, 4.0);  // n_2 = 4 = bits at i=2 -> 2^2*3^2 = 36
    checkNear(rep.value(), 1.5 + 36.0, "Sparse.setColumnValue(2, 4) adds 4*3^2 = 36");

    auto cloned = rep.clone();
    checkNear(cloned->value(), rep.value(), "Sparse.clone() has the same value as the original");
    rep.set(0, 0, false);
    check(cloned->get(0, 0) && !rep.get(0, 0), "Sparse.clone() is independent of later mutation to the original");

    rep.reset();
    checkNear(rep.value(), 0.0, "Sparse.reset() clears everything");
}

void testRowValuesRepresentationDirectly() {
    RowValuesRepresentation rep(false);
    rep.setColumnValue(0, 5.0);
    checkNear(rep.value(), 5.0, "RowValues.setColumnValue(0, 5)");
    check(rep.get(0, 0) && rep.get(2, 0) && !rep.get(1, 0), "RowValues decodes n_0=5 (binary 101) bit by bit");

    rep.set(1, 0, true);  // += 2 -> n_0 = 7
    checkNear(rep.value(), 7.0, "RowValues.set() accumulates onto the existing column value");

    rep.setColumnValue(1, 3.0);  // n_1 = 3 -> +9
    checkNear(rep.value(), 7.0 + 9.0, "RowValues with two nonzero columns");

    int columns = 0;
    rep.forEachSet([&columns](int, int) { ++columns; });
    check(columns > 0, "RowValues.forEachSet() visits at least one bit");

    auto cloned = rep.clone();
    rep.reset();
    checkNear(rep.value(), 0.0, "RowValues.reset() clears everything");
    checkNear(cloned->value(), 7.0 + 9.0, "RowValues.clone() is unaffected by resetting the original");
}

void testDynamicRepresentationGrowth() {
    DynamicMatrixRepresentation rep(true);
    check(!rep.get(0, 0), "Dynamic starts empty (get on unallocated cell is false, not a throw)");

    rep.set(0, 0, true);
    rep.set(3, 1, true);   // forces row capacity to grow past its initial size
    rep.set(-2, 5, true);  // forces negative-row and column capacity to grow too
    check(rep.get(0, 0) && rep.get(3, 1) && rep.get(-2, 5), "all three bits read back true after growth");
    checkNear(rep.value(), std::pow(2.0, 0) + std::pow(2.0, 3) * 3.0 + std::pow(2.0, -2) * std::pow(3.0, 5),
              "Dynamic.value() correct after multiple growth steps");

    auto cloned = rep.clone();
    rep.reset();
    checkNear(rep.value(), 0.0, "Dynamic.reset() drops back to empty");
    check(cloned->get(3, 1), "Dynamic.clone() retains the bits from before reset()");
}

void testScalarRepresentationDirectly() {
    ScalarRepresentation rep(true);
    rep.setColumnValue(0, 5.5);
    checkNear(rep.value(), 5.5, "Scalar.setColumnValue(0, 5.5)");
    check(rep.get(0, 0) && rep.get(2, 0) && rep.get(-1, 0), "Scalar decodes 5.5 (101.1 in binary) bit by bit");
    check(!rep.get(0, 1), "Scalar.get() on any column other than 0 is false");

    checkThrows([&] { rep.set(0, 1, true); }, "Scalar.set() on column != 0 throws");
    checkThrows([&] { rep.setColumnValue(2, 1.0); }, "Scalar.setColumnValue() on column != 0 throws");

    int columns = 0;
    rep.forEachSet([&columns](int, int j) {
        ++columns;
        check(j == 0, "Scalar.forEachSet() only ever reports column 0");
    });
    check(columns > 0, "Scalar.forEachSet() visits at least one bit");

    auto cloned = rep.clone();
    rep.reset();
    checkNear(rep.value(), 0.0, "Scalar.reset() clears the stored value");
    checkNear(cloned->value(), 5.5, "Scalar.clone() is unaffected by resetting the original");

    ScalarRepresentation a(false), b(false);
    a.setColumnValue(0, 42.0);
    b.setColumnValue(0, 8.0);
    a.addInPlace(b);
    checkNear(a.value(), 50.0, "Scalar+Scalar addInPlace: direct scalar addition, 42 + 8 = 50");

    // Falls back to decomposing other's bits when it isn't also a Scalar,
    // but still refuses anything outside column 0.
    RowValuesRepresentation notScalar(false);
    notScalar.setColumnValue(1, 2.0);  // 2 * 3^1 -- column 1, not representable as a scalar
    checkThrows([&] { a.addInPlace(notScalar); },
                "Scalar.addInPlace() rejects a term from another representation's column != 0");
}

// ---------------------------------------------------------------------
// addInPlace(): exact bit-level carry for Sparse/Dynamic, direct
// accumulation for RowValues -- each representation's own strategy.
// ---------------------------------------------------------------------
void testAddInPlacePerRepresentation() {
    {
        SparseRepresentation a(false), b(false);
        a.set(0, 0, true);  // 1
        b.set(0, 0, true);  // 1 -> should carry into (1, 0)
        a.addInPlace(b);
        check(!a.get(0, 0) && a.get(1, 0), "Sparse addInPlace carries 1+1 up to bit (1,0)");
        checkNear(a.value(), 2.0, "Sparse addInPlace: 1 + 1 = 2");
    }
    {
        DynamicMatrixRepresentation a(false), b(false);
        a.set(1, 0, true);  // 2
        a.set(0, 0, true);  // +1 = 3
        b.set(1, 0, true);  // +2
        a.addInPlace(b);
        checkNear(a.value(), 5.0, "Dynamic addInPlace: 3 + 2 = 5");
    }
    {
        RowValuesRepresentation a(false), b(false);
        a.setColumnValue(0, 3.0);
        b.setColumnValue(0, 2.0);
        a.addInPlace(b);
        checkNear(a.value(), 5.0, "RowValues addInPlace: 3 + 2 = 5 (direct accumulation, no carry)");
    }
    {
        // RowValues + RowValues: columns should add directly (n_j + n_j),
        // with no need to decompose either side into individual bits --
        // including a column where the two operands cancel out exactly to
        // zero, and one present in only one of the two operands.
        RowValuesRepresentation a(false), b(false);
        a.setColumnValue(0, 4.0);
        a.setColumnValue(1, 5.0);
        b.setColumnValue(0, -4.0);
        b.setColumnValue(2, 6.0);
        a.addInPlace(b);
        checkNear(a.value(), 5.0 * 3.0 + 6.0 * 9.0,
                  "RowValues+RowValues fast path: column 0 cancels to zero, column 1 untouched, "
                  "column 2 introduced");
        check(!a.get(0, 0), "RowValues+RowValues fast path drops a column that cancels to exactly zero");
    }
    {
        // Self-addition (doubling) must be safe for every representation,
        // since addInPlace() snapshots the other operand's bits up front.
        SparseRepresentation s(false);
        s.set(0, 0, true);
        s.addInPlace(s);
        checkNear(s.value(), 2.0, "Sparse self-addition doubles the value safely");

        RowValuesRepresentation r(false);
        r.setColumnValue(0, 5.0);
        r.addInPlace(r);
        checkNear(r.value(), 10.0, "RowValues self-addition doubles the value safely");
    }
}

// ---------------------------------------------------------------------
// multiplyInPlace(): pairwise exponent-sum with carry for Sparse/Dynamic
// (the same helper, shared, as neither needs to convert to the other to
// multiply), convolution of column totals for RowValues, direct scalar
// multiplication for Scalar.
// ---------------------------------------------------------------------
void testMultiplyInPlacePerRepresentation() {
    {
        // A = 3 (bits (0,0),(1,0)), B = 3 (bits (0,0),(1,0)) -> 9, which
        // requires a genuine carry collision at (1,0) partway through the
        // pairwise sums (two different pairs land there before the final
        // carry resolves it).
        SparseRepresentation a(false), b(false);
        a.set(0, 0, true);
        a.set(1, 0, true);
        b.set(0, 0, true);
        b.set(1, 0, true);
        a.multiplyInPlace(b);
        checkNear(a.value(), 9.0, "Sparse multiplyInPlace: 3 * 3 = 9 (carry collision handled)");
    }
    {
        DynamicMatrixRepresentation a(false), b(false);
        a.set(0, 0, true);
        a.set(1, 0, true);  // 3
        b.set(0, 1, true);  // 3
        a.multiplyInPlace(b);
        checkNear(a.value(), 9.0, "Dynamic multiplyInPlace: 3 * 3 = 9 (same shared helper as Sparse)");
    }
    {
        // Convolution: (3 + 2*3^1) * (1 + 4*3^2) = 9 * 37 = 333.
        RowValuesRepresentation a(false), b(false);
        a.setColumnValue(0, 3.0);
        a.setColumnValue(1, 2.0);
        b.setColumnValue(0, 1.0);
        b.setColumnValue(2, 4.0);
        a.multiplyInPlace(b);
        checkNear(a.value(), 333.0, "RowValues multiplyInPlace: 9 * 37 = 333 (convolution)");
    }
    {
        // Fallback path: other isn't a RowValuesRepresentation.
        RowValuesRepresentation a(false);
        a.setColumnValue(0, 2.0);
        SparseRepresentation b(false);
        b.set(0, 1, true);  // 3
        a.multiplyInPlace(b);
        checkNear(a.value(), 6.0, "RowValues.multiplyInPlace(Sparse) fallback: 2 * 3 = 6");
    }
    {
        ScalarRepresentation a(false), b(false);
        a.setColumnValue(0, 6.0);
        b.setColumnValue(0, 7.0);
        a.multiplyInPlace(b);
        checkNear(a.value(), 42.0, "Scalar multiplyInPlace: 6 * 7 = 42 (direct scalar multiplication)");
    }
    {
        // 0 * anything is always 0, even a term Scalar couldn't otherwise
        // represent -- this never throws. A nonzero Scalar times such a
        // term does throw, same restriction as everywhere else in Scalar.
        ScalarRepresentation zero(false);
        SparseRepresentation notScalar(false);
        notScalar.set(0, 2, true);  // column 2 -- not representable as a scalar
        zero.multiplyInPlace(notScalar);
        checkNear(zero.value(), 0.0, "Scalar 0 * (non-column-0 term) = 0, no throw");

        ScalarRepresentation nonzero(false);
        nonzero.setColumnValue(0, 5.0);
        checkThrows([&] { nonzero.multiplyInPlace(notScalar); },
                    "Scalar (nonzero) * (non-column-0 term) throws");
    }
    {
        // Self-multiplication (squaring) must be safe for every
        // representation, since multiplyInPlace() snapshots both operands'
        // bits/totals before touching the destination.
        SparseRepresentation s(false);
        s.set(0, 0, true);
        s.set(0, 1, true);  // 1 + 3 = 4
        s.multiplyInPlace(s);
        checkNear(s.value(), 16.0, "Sparse self-multiply (squaring) is safe: 4^2 = 16");

        RowValuesRepresentation r(false);
        r.setColumnValue(0, 5.0);
        r.multiplyInPlace(r);
        checkNear(r.value(), 25.0, "RowValues self-multiply (squaring) is safe: 5^2 = 25");
    }
}

// ---------------------------------------------------------------------
// operator+ (the only way to add -- there is no add()/operator+= anymore)
// for the two unsigned types. Always value-returning: never mutates
// either operand.
// ---------------------------------------------------------------------
void testUnsignedAddition() {
    SmoothInteger a, b;
    a.setValue(12LL);
    b.setValue(7LL);
    SmoothInteger c = a + b;
    checkNear(c.value(), 19.0, "SmoothInteger operator+: 12 + 7 = 19");
    checkNear(a.value(), 12.0, "operator+ does not mutate its left-hand operand");
    checkNear(b.value(), 7.0, "operator+ does not mutate its right-hand operand");

    SmoothFloat f1, f2;
    f1.setValue(1.25);
    f2.setValue(2.5);
    SmoothFloat f3 = f1 + f2;
    checkNear(f3.value(), 3.75, "SmoothFloat operator+: 1.25 + 2.5 = 3.75");
}

// ---------------------------------------------------------------------
// Addition requires matching representations (SmoothNumberBase::
// addMatchingInPlace()/subtractMagnitudeInPlace(), used internally by
// every concrete type's operator+, throw std::invalid_argument otherwise).
// This can't directly be tested by forcing a mismatch through the public
// API, though: canonical() always starts out (and, for now, stays)
// Dynamic for every freshly constructed number, and there's no public way
// to change it -- so any two independently constructed numbers always
// match trivially, which is what this actually verifies.
// ---------------------------------------------------------------------
void testAdditionRequiresMatchingRepresentation() {
    SmoothInteger a, b;
    check(a.canonical() == b.canonical(),
          "two freshly constructed numbers always start with the same (Dynamic) canonical representation");
    a.setValue(3LL);
    b.setValue(4LL);
    SmoothInteger c = a + b;
    checkNear(c.value(), 7.0, "addition succeeds when representations match, which they always currently do");
}

// ---------------------------------------------------------------------
// Signed addition: same sign, differing sign in both directions, a tie
// that lands on zero, and the cross-column (mixed-radix) borrow case.
// Always value-returning, same as the unsigned case.
// ---------------------------------------------------------------------
void testSignedAddition() {
    SmoothSignedInteger a, b;
    a.setValue(5LL);
    b.setValue(3LL);
    SmoothSignedInteger sum1 = a + b;
    checkNear(sum1.value(), 8.0, "signed same-sign (+,+): 5 + 3 = 8");
    check(!sum1.isNegative(), "result of (+,+) addition is not negative");
    checkNear(a.value(), 5.0, "operator+ does not mutate the signed left-hand operand");
    checkNear(b.value(), 3.0, "operator+ does not mutate the signed right-hand operand");

    SmoothSignedInteger c, d;
    c.setValue(-5LL);
    d.setValue(-3LL);
    SmoothSignedInteger sum2 = c + d;
    checkNear(sum2.value(), -8.0, "signed same-sign (-,-): -5 + -3 = -8");
    check(sum2.isNegative(), "result of (-,-) addition is negative");

    SmoothSignedInteger e, f;
    e.setValue(5LL);
    f.setValue(-3LL);
    SmoothSignedInteger sum3 = e + f;
    checkNear(sum3.value(), 2.0, "signed diff-sign, |a| >= |b|: 5 + (-3) = 2");
    check(!sum3.isNegative(), "5 + (-3) is not negative");

    SmoothSignedInteger g, h;
    g.setValue(3LL);
    h.setValue(-5LL);
    SmoothSignedInteger sum4 = g + h;
    checkNear(sum4.value(), -2.0, "signed diff-sign, |a| < |b|: 3 + (-5) = -2");
    check(sum4.isNegative(), "3 + (-5) is negative");
    checkNear(g.value(), 3.0, "operator+'s swap branch does not mutate a");
    checkNear(h.value(), -5.0, "operator+'s swap branch does not mutate b");

    SmoothSignedInteger tieA, tieB;
    tieA.setValue(5LL);
    tieB.setValue(-5LL);
    SmoothSignedInteger tieSum = tieA + tieB;
    checkNear(tieSum.value(), 0.0, "signed diff-sign tie: 5 + (-5) = 0");
    check(!tieSum.isNegative(), "a zero result is normalized to non-negative, never a signed zero");

    SmoothSignedFloat sf1, sf2;
    sf1.setValue(-1.5);
    sf2.setValue(2.25);
    SmoothSignedFloat sf3 = sf1 + sf2;
    checkNear(sf3.value(), 0.75, "SmoothSignedFloat: -1.5 + 2.25 = 0.75");
    check(!sf3.isNegative(), "-1.5 + 2.25 is not negative");

    // Cross-column (mixed-radix) borrow: 3 (stored as n_1=1, i.e. bit
    // (0,1)) minus 2 (stored as n_0=2, i.e. bit (1,0)) must borrow a unit
    // from column 1 into column 0, since neither operand has a bit to
    // borrow from within column 0 alone.
    SmoothSignedInteger x, y;
    x.set(0, 1, true);  // 2^0 * 3^1 = 3
    y.set(1, 0, true);  // 2^1 * 3^0 = 2
    y.negate();
    SmoothSignedInteger borrowResult = x + y;
    checkNear(borrowResult.value(), 1.0, "cross-column borrow: 3 (column 1) - 2 (column 0) = 1");
    check(!borrowResult.isNegative(), "3 - 2 = 1 is not negative");
}

// ---------------------------------------------------------------------
// operator* (the only way to multiply -- same "no in-place, no scalars,
// representations must match" treatment as operator+) for the unsigned
// types, and the sign-XOR rule for signed multiplication.
// ---------------------------------------------------------------------
void testMultiplication() {
    SmoothInteger a, b;
    a.setValue(6LL);
    b.setValue(7LL);
    SmoothInteger c = a * b;
    checkNear(c.value(), 42.0, "SmoothInteger operator*: 6 * 7 = 42");
    checkNear(a.value(), 6.0, "operator* does not mutate its left-hand operand");
    checkNear(b.value(), 7.0, "operator* does not mutate its right-hand operand");

    SmoothFloat f1, f2;
    f1.setValue(1.5);
    f2.setValue(2.0);
    SmoothFloat f3 = f1 * f2;
    checkNear(f3.value(), 3.0, "SmoothFloat operator*: 1.5 * 2 = 3");

    // Same representations-must-match note as addition: every freshly
    // constructed number starts out canonical() == Dynamic, so this always
    // succeeds in practice; see testAdditionRequiresMatchingRepresentation.
    SmoothInteger d, e;
    d.setValue(3LL);
    e.setValue(4LL);
    SmoothInteger prod = d * e;
    checkNear(prod.value(), 12.0, "multiplication succeeds when representations match");

    // Signed: sign is the usual XOR rule (same signs -> positive, mixed ->
    // negative), and a zero product is normalized back to non-negative.
    SmoothSignedInteger p, q;
    p.setValue(6LL);
    q.setValue(7LL);
    SmoothSignedInteger pos = p * q;
    checkNear(pos.value(), 42.0, "signed (+)*(+) = (+): 6 * 7 = 42");
    check(!pos.isNegative(), "result of (+)*(+) is not negative");

    SmoothSignedInteger np, nq;
    np.setValue(-6LL);
    nq.setValue(-7LL);
    SmoothSignedInteger negTimesNeg = np * nq;
    checkNear(negTimesNeg.value(), 42.0, "signed (-)*(-) = (+): -6 * -7 = 42");
    check(!negTimesNeg.isNegative(), "result of (-)*(-) is not negative");

    SmoothSignedInteger mixed = p * nq;
    checkNear(mixed.value(), -42.0, "signed (+)*(-) = (-): 6 * -7 = -42");
    check(mixed.isNegative(), "result of (+)*(-) is negative");
    checkNear(p.value(), 6.0, "operator* does not mutate the signed left-hand operand");
    checkNear(nq.value(), -7.0, "operator* does not mutate the signed right-hand operand");

    SmoothSignedInteger zero;
    zero.setValue(0LL);
    SmoothSignedInteger zeroProduct = zero * nq;
    checkNear(zeroProduct.value(), 0.0, "0 * anything = 0");
    check(!zeroProduct.isNegative(), "a zero product is normalized to non-negative, never a signed zero");
}

// ---------------------------------------------------------------------
// Copy/move semantics: a copy is a deep, independent clone; a move
// leaves the source usable (defaulted, so no specific end state is
// asserted beyond "doesn't crash and the destination is correct").
// ---------------------------------------------------------------------
void testCopyAndMoveSemantics() {
    SmoothInteger one;
    one.setValue(1LL);

    SmoothInteger original;
    original.setValue(100LL);

    SmoothInteger copy(original);
    copy = copy + one;
    checkNear(copy.value(), 101.0, "mutating a copy after copy-construction affects the copy");
    checkNear(original.value(), 100.0, "...but not the original (deep copy, no shared state)");

    SmoothInteger assigned;
    assigned.setValue(1LL);
    assigned = original;
    checkNear(assigned.value(), 100.0, "copy assignment replaces the destination's value");
    assigned = assigned + one;
    checkNear(original.value(), 100.0, "copy assignment is also a deep copy, not aliased");

    SmoothInteger moveSource;
    moveSource.setValue(55LL);
    SmoothInteger moved(std::move(moveSource));
    checkNear(moved.value(), 55.0, "move construction transfers the value");

    // Signed types compose Base's copy/move with their own sign flag.
    SmoothSignedInteger signedOriginal;
    signedOriginal.setValue(-9LL);
    SmoothSignedInteger signedCopy(signedOriginal);
    checkNear(signedCopy.value(), -9.0, "copying a Signed<Base> preserves its magnitude");
    check(signedCopy.isNegative(), "copying a Signed<Base> preserves its sign");
    signedCopy.negate();
    check(signedOriginal.isNegative() && !signedCopy.isNegative(),
          "negating a signed copy doesn't affect the original's sign");
}

// ---------------------------------------------------------------------
// Transformation: value-preserving bit-grid rewrites, built from a fixed
// list of input offsets (each must hold a 1; applying always clears them)
// and output offsets (each gets carry-set to 1 -- ripple-carrying up the
// row axis, exactly like ordinary addition, if already occupied).
// canApply() only ever checks the inputs, since an occupied output is
// never a reason to reject -- apply() carries through it instead.
// 2^i*3^j + 2^(i+1)*3^j = 2^i*3^(j+1), so MergeTransformation/
// SplitTransformation trade the two bits at (i, j)/(i+1, j) for the one
// bit at (i, j+1), and back, without changing value() at all.
// SmoothNumberBase::applyTransformation() checks canApply() first and
// throws if it doesn't hold, rather than applying regardless.
// ---------------------------------------------------------------------
void testTransformation() {
    // MergeTransformation: applicable exactly when both input bits are
    // set -- the output bit's own state doesn't matter.
    {
        SmoothInteger n;
        n.set(0, 0);  // 2^0*3^0 = 1
        n.set(1, 0);  // 2^1*3^0 = 2
        MergeTransformation merge;
        check(merge.canApply(n, 0, 0), "MergeTransformation: applicable when both input bits are set");
        checkNear(n.value(), 3.0, "before merge: 1 + 2 = 3");

        n.applyTransformation(merge, 0, 0);
        checkNear(n.value(), 3.0, "MergeTransformation preserves value(): still 3");
        check(!n.get(0, 0) && !n.get(1, 0) && n.get(0, 1),
              "MergeTransformation moves the two input bits into the one output bit");
    }
    {
        // Not applicable: only one of the two input bits is set.
        SmoothInteger n;
        n.set(0, 0);
        MergeTransformation merge;
        check(!merge.canApply(n, 0, 0), "MergeTransformation: not applicable with only one input bit set");
        checkThrows([&] { n.applyTransformation(merge, 0, 0); },
                    "applyTransformation() throws when canApply() is false (missing input bit)");
    }
    {
        // The output bit already being set doesn't block canApply() at
        // all -- apply() carries into it instead of failing.
        SmoothInteger n;
        n.set(2, 0);  // 2^2 = 4       )  inputs: merge these two
        n.set(3, 0);  // 2^3 = 8       )
        n.set(2, 1);  // 2^2*3 = 12    -- pre-existing bit at the output
        MergeTransformation merge;
        check(merge.canApply(n, 2, 0),
              "MergeTransformation: still applicable even when the output bit is already set");
        checkNear(n.value(), 24.0, "before merge (with output collision): 4 + 8 + 12 = 24");

        n.applyTransformation(merge, 2, 0);
        checkNear(n.value(), 24.0, "MergeTransformation preserves value() even when it has to carry");
        check(!n.get(2, 0) && !n.get(3, 0) && !n.get(2, 1) && n.get(3, 1),
              "an occupied output bit ripple-carries up the row axis (12 + 12 = 24 = 2^3*3^1) instead of "
              "blocking the merge");
    }

    // SplitTransformation: the reverse -- applicable exactly when the
    // input bit is set; the output bits' own states don't matter.
    {
        SmoothInteger n;
        n.set(0, 1);  // 2^0*3^1 = 3
        SplitTransformation split;
        check(split.canApply(n, 0, 0), "SplitTransformation: applicable when the input bit is set");
        checkNear(n.value(), 3.0, "before split: 3");

        n.applyTransformation(split, 0, 0);
        checkNear(n.value(), 3.0, "SplitTransformation preserves value(): still 3");
        check(n.get(0, 0) && n.get(1, 0) && !n.get(0, 1),
              "SplitTransformation moves the one input bit into the two output bits");
    }
    {
        SplitTransformation split;
        SmoothInteger n;
        check(!split.canApply(n, 0, 0), "SplitTransformation: not applicable when the input bit is clear");
        checkThrows([&] { n.applyTransformation(split, 0, 0); },
                    "applyTransformation() throws when canApply() is false (input bit clear)");
    }
    {
        // Both output bits already occupied -- still applicable, and both
        // carry (in order), still preserving the total value.
        SmoothInteger n;
        n.set(2, 1);  // 2^2*3 = 12  -- input, to be split
        n.set(2, 0);  // 2^2 = 4     -- pre-existing, collides with the first output
        SplitTransformation split;
        check(split.canApply(n, 2, 0),
              "SplitTransformation: still applicable even when an output bit is already set");
        checkNear(n.value(), 16.0, "before split (with output collision): 12 + 4 = 16");

        n.applyTransformation(split, 2, 0);
        checkNear(n.value(), 16.0, "SplitTransformation preserves value() even when it has to carry");
        check(!n.get(2, 0) && !n.get(3, 0) && !n.get(2, 1) && n.get(4, 0),
              "the first output's carry (4 + 4 = 8, landing at row 3) collides with the second output's own "
              "target, carrying again (8 + 8 = 16, landing at row 4)");
    }

    // Merge then split is a round trip: back to the original bit layout.
    {
        SmoothInteger n;
        n.set(2, 0);
        n.set(3, 0);
        n.applyTransformation(MergeTransformation(), 2, 0);
        n.applyTransformation(SplitTransformation(), 2, 0);
        check(n.get(2, 0) && n.get(3, 0) && !n.get(2, 1), "merge then split round-trips to the original bits");
    }

    // A transformation that actually changes a bit invalidates every
    // representation but the canonical one, the same as any other set()
    // call -- printRowValues() (a different representation entirely)
    // reflects the post-merge layout correctly.
    {
        SmoothInteger n;
        n.set(2, 0);
        n.set(3, 0);
        n.applyTransformation(MergeTransformation(), 2, 0);
        std::ostringstream out;
        n.printRowValues(out);
        check(out.str() == "n[1] = 4\n",
              "after a merge, a different representation (RowValues) reflects the new layout: only column 1 "
              "(n_1 = 4, i.e. 4*3^1 = 12) has anything set");
        checkNear(n.value(), 12.0, "value is unchanged by re-deriving a different representation");
    }

    // Transformation itself is concrete and general: a custom one, built
    // directly from its own input/output offset lists (not one of the two
    // named presets), works exactly the same way. This one combines three
    // bits across both axes at once: 2^i*3^j + 2^(i+1)*3^j + 2^i*3^(j+1) =
    // 2^i*3^j*(1+2+3) = 2^i*3^j*6 = 2^(i+1)*3^(j+1).
    {
        Transformation combineBothAxes({{0, 0}, {1, 0}, {0, 1}}, {{1, 1}});
        SmoothInteger n;
        n.set(0, 0);  // 1
        n.set(1, 0);  // 2
        n.set(0, 1);  // 3
        checkNear(n.value(), 6.0, "before custom transformation: 1 + 2 + 3 = 6");
        check(combineBothAxes.canApply(n, 0, 0), "a custom Transformation's canApply() checks its own input list");
        n.applyTransformation(combineBothAxes, 0, 0);
        checkNear(n.value(), 6.0, "a custom Transformation still preserves value()");
        check(!n.get(0, 0) && !n.get(1, 0) && !n.get(0, 1) && n.get(1, 1),
              "a custom Transformation clears all of its inputs and carry-sets all of its outputs");
    }

    // Works for negative indices on a fractional type too -- the identity
    // 2^i*3^j + 2^(i+1)*3^j = 2^i*3^(j+1) doesn't care about the sign of i
    // or j.
    {
        SmoothFloat n;
        n.set(-2, 0);  // 2^-2 = 0.25
        n.set(-1, 0);  // 2^-1 = 0.5
        checkNear(n.value(), 0.75, "before merge (fractional): 0.25 + 0.5 = 0.75");
        n.applyTransformation(MergeTransformation(), -2, 0);
        checkNear(n.value(), 0.75, "MergeTransformation preserves value() for negative indices too");
        check(!n.get(-2, 0) && !n.get(-1, 0) && n.get(-2, 1),
              "MergeTransformation moves bits correctly for negative i");
    }
}

// ---------------------------------------------------------------------
// Metrics: optional per-number counter tracking, incremented once per
// representation conversion, and the a+=b / a+b metrics-inheritance
// rules.
// ---------------------------------------------------------------------
void testMetrics() {
    {
        Metrics m;
        m.increment("foo");
        m.increment("foo");
        m.increment("bar");
        std::ostringstream out;
        m.print(out);
        check(out.str() == "bar = 1\nfoo = 2\n", "Metrics::increment/print");
    }

    // Conversions are counted once per actual (re)conversion, not once per
    // print call -- an already-valid representation isn't reconverted.
    {
        auto metrics = std::make_shared<Metrics>();
        SmoothInteger n(metrics);
        n.set(1, 0);
        n.set(0, 2);

        std::ostringstream discard;
        n.printSparse(discard);     // dynamic -> sparse
        n.printRowValues(discard);  // dynamic -> row_values
        n.printSparse(discard);     // already valid: no new conversion

        std::ostringstream out;
        metrics->print(out);
        check(out.str() ==
                  "bit_iterations = 24\n"
                  "convert_dynamic_to_row_values = 1\n"
                  "convert_dynamic_to_sparse = 1\n"
                  "scalar_operations = 2\n",
              "each representation conversion is counted once, redundant prints don't recount");

        n.set(2, 2);              // invalidates sparse/row_values again
        n.printSparse(discard);   // dynamic -> sparse, again
        std::ostringstream out2;
        metrics->print(out2);
        check(out2.str() ==
                  "bit_iterations = 51\n"
                  "convert_dynamic_to_row_values = 1\n"
                  "convert_dynamic_to_sparse = 2\n"
                  "scalar_operations = 2\n",
              "re-converting after invalidation increments the counter again");
    }

    // No metrics attached: nothing tracked, nothing throws.
    {
        SmoothInteger n;
        check(!n.hasMetrics(), "SmoothInteger() has no metrics by default");
        std::ostringstream discard;
        n.set(0, 0);
        n.printSparse(discard);
        check(true, "converting/printing without metrics attached doesn't crash");
    }

    // a + b: a's metrics win if present; otherwise fall back to b's, if any.
    {
        auto metricsA = std::make_shared<Metrics>();
        auto metricsB = std::make_shared<Metrics>();
        SmoothInteger a(metricsA), bWithMetrics(metricsB), bWithout;
        a.setValue(5LL);
        bWithMetrics.setValue(3LL);
        bWithout.setValue(3LL);

        check((a + bWithout).metricsPtr() == metricsA, "a+b: only a has metrics -> result keeps a's");
        check((a + bWithMetrics).metricsPtr() == metricsA, "a+b: both have metrics -> result keeps a's, not b's");

        SmoothInteger aWithout;
        aWithout.setValue(5LL);
        check((aWithout + bWithMetrics).metricsPtr() == metricsB,
              "a+b: a has none, b has metrics -> result adopts b's");
        check(!(aWithout + bWithout).hasMetrics(), "a+b: neither has metrics -> result has none");
    }

    // Signed types: the swap branch of operator+ (|a| < |b|, differing
    // signs) builds its result out of a copy of b internally -- metrics
    // must still end up as a's, not b's.
    {
        auto metricsA = std::make_shared<Metrics>();
        auto metricsB = std::make_shared<Metrics>();
        SmoothSignedInteger a(metricsA), b(metricsB);
        a.setValue(3LL);
        b.setValue(-10LL);  // |b| > |a|, differing signs -> takes the swap branch
        SmoothSignedInteger result = a + b;
        checkNear(result.value(), -7.0, "signed swap-branch arithmetic still correct: 3 + (-10) = -7");
        check(result.metricsPtr() == metricsA, "signed swap-branch still keeps a's metrics, not b's");
    }
}

// ---------------------------------------------------------------------
// The four counters instrumented directly on the representations
// (carries, bit_operations, scalar_operations, bit_iterations -- see
// representation_base.hpp, sparse_representation.hpp,
// dynamic_matrix_representation.hpp, row_values_representation.hpp,
// scalar_representation.hpp). Checked against each representation
// directly, with a fresh Metrics per case, so each counter's exact
// semantics are pinned down independently of the others.
// ---------------------------------------------------------------------
void testInstrumentationCounters() {
    // carries: one ripple step per cell that was already occupied and had
    // to move up a row. 1 + 1 (both at row 0) needs exactly one carry, to
    // produce 2 at row 1.
    {
        auto metrics = std::make_shared<Metrics>();
        SparseRepresentation a(true, metrics), b(true, metrics);
        a.set(0, 0, true);
        b.set(0, 0, true);
        a.addInPlace(b);
        checkNear(a.value(), 2.0, "carries: 1 + 1 = 2");
        check(counterValue(metrics, "carries") == 1, "carries: a single carry is counted once");
    }
    // 7 (bits at rows 0,1,2) + 1 (bit at row 0) = 8 ripples through three
    // occupied cells before landing on the empty row 3.
    {
        auto metrics = std::make_shared<Metrics>();
        SparseRepresentation a(true, metrics), b(true, metrics);
        a.set(0, 0, true);
        a.set(1, 0, true);
        a.set(2, 0, true);
        b.set(0, 0, true);
        a.addInPlace(b);
        checkNear(a.value(), 8.0, "carries: 7 + 1 = 8");
        check(counterValue(metrics, "carries") == 3, "carries: a 3-step ripple chain is counted as 3");
    }

    // bit_operations: an n-bit by m-bit multiplication is n*m pairings, one
    // per (termA, termB) pair -- regardless of how those pairings interact
    // via carrying.
    {
        auto metrics = std::make_shared<Metrics>();
        SparseRepresentation a(true, metrics), b(true, metrics);
        a.set(0, 0, true);
        a.set(5, 0, true);
        b.set(0, 0, true);
        b.set(5, 0, true);
        b.set(10, 0, true);
        a.multiplyInPlace(b);
        check(counterValue(metrics, "bit_operations") == 6,
              "bit_operations: 2-bit times 3-bit multiplication is 2*3 = 6 bit operations");
    }

    // bit_iterations, Sparse: only ever visits the 1s actually stored, not
    // any notion of empty space, since coords_ only ever holds set bits.
    {
        auto metrics = std::make_shared<Metrics>();
        SparseRepresentation a(true, metrics);
        a.set(0, 0, true);
        a.set(1, 0, true);
        a.set(0, 1, true);
        std::ostringstream discard;
        a.value();                      // +3 (one per stored coordinate)
        a.print(discard);               // +3
        a.forEachSet([](int, int) {});  // +3
        check(counterValue(metrics, "bit_iterations") == 9,
              "bit_iterations (sparse): each of 3 full walks over 3 set bits adds 3, total 9");
    }

    // bit_iterations, DynamicMatrix: visits every cell in the currently
    // allocated capacity, 1s and 0s alike -- growToFit()'s copy loop and
    // value()/print()/forEachSet()'s grid walks all count every cell they
    // check, not just the set ones.
    {
        auto metrics = std::make_shared<Metrics>();
        DynamicMatrixRepresentation a(true, metrics);
        a.set(0, 0, true);  // grows from empty (0 cells) to a 1x1 grid
        a.set(2, 0, true);  // grows the 1x1 grid to 4x1 (rows 0..3)
        long long before = counterValue(metrics, "bit_iterations");
        checkNear(a.value(), 5.0, "bit_iterations (dynamic): 2^0 + 2^2 = 5");
        long long after = counterValue(metrics, "bit_iterations");
        check(after - before == 4,
              "bit_iterations (dynamic): value() walks every cell of the 4x1 capacity, not just the set ones");
    }

    // bit_iterations, RowValues: one per column entry visited, i.e. once
    // per row in the "list of rows" sense -- regardless of how large that
    // row's magnitude n_j is.
    {
        auto metrics = std::make_shared<Metrics>();
        RowValuesRepresentation a(true, metrics);
        a.set(0, 0, true);
        a.set(0, 2, true);
        long long before = counterValue(metrics, "bit_iterations");
        a.value();
        long long after = counterValue(metrics, "bit_iterations");
        check(after - before == 2,
              "bit_iterations (row values): value() visits one entry per column, 2 columns -> 2");
    }

    // scalar_operations, RowValues: accumulate() (the shared add primitive
    // behind set() and addInPlace()) counts one operation per call.
    {
        auto metrics = std::make_shared<Metrics>();
        RowValuesRepresentation a(true, metrics);
        a.set(0, 0, true);
        a.set(1, 0, true);
        check(counterValue(metrics, "scalar_operations") == 2,
              "scalar_operations (row values): two set() calls accumulate twice");
    }
    // multiplyInPlace's convolution counts 2 scalar operations (one
    // multiply, one add) per (columnA, columnB) pair; a single-column by
    // single-column multiply is exactly one pair.
    {
        auto metrics = std::make_shared<Metrics>();
        RowValuesRepresentation a(true, metrics), b(true, metrics);
        a.setColumnValue(0, 3);  // direct assignment: not counted
        b.setColumnValue(0, 4);
        a.multiplyInPlace(b);
        checkNear(a.value(), 12.0, "scalar_operations (row values): 3 * 4 = 12");
        check(counterValue(metrics, "scalar_operations") == 2,
              "scalar_operations (row values): one column pair multiplied is 2 ops (multiply + add)");
    }

    // scalar_operations, Scalar: set()'s value_ += delta is one operation.
    {
        auto metrics = std::make_shared<Metrics>();
        ScalarRepresentation a(true, metrics);
        a.set(0, 0, true);
        check(counterValue(metrics, "scalar_operations") == 1,
              "scalar_operations (scalar): one set() call is one op");
    }
    // addInPlace/multiplyInPlace's scalar-to-scalar fast paths are also one
    // operation each.
    {
        auto metrics = std::make_shared<Metrics>();
        ScalarRepresentation a(true, metrics), b(true, metrics);
        a.setColumnValue(0, 3);  // direct assignment: not counted
        b.setColumnValue(0, 4);
        a.addInPlace(b);
        checkNear(a.value(), 7.0, "scalar_operations (scalar): 3 + 4 = 7");
        a.multiplyInPlace(b);
        checkNear(a.value(), 28.0, "scalar_operations (scalar): 7 * 4 = 28");
        check(counterValue(metrics, "scalar_operations") == 2,
              "scalar_operations (scalar): addInPlace then multiplyInPlace is 1 op each, 2 total");
    }
}

// ---------------------------------------------------------------------
// Plan: the fluent scalar-arithmetic expression builder. For now it just
// converts every leaf to a plain double and evaluates with ordinary
// arithmetic -- these tests check the tree-building mechanics (left()/
// right() as an open/close bracket pair, default left-associative
// chaining without them), number()'s use of value() at T's static type,
// plan()'s tree rendering, and the usage-error cases.
// ---------------------------------------------------------------------
void testPlan() {
    // The confirmed example: 3 * (4 + 2) = 18.
    checkNear(DefaultPlan().scalar(3).times().left().scalar(4).plus().scalar(2).right().calculate(), 18.0,
              "DefaultPlan: 3 * (4 + 2) = 18");

    // Without brackets, chaining is left-associative, like a simple
    // calculator: each operator wraps the *entire* accumulated result so
    // far. left()/right() (used above) are how you override that default
    // with explicit grouping.
    checkNear(DefaultPlan().scalar(3).times().scalar(4).plus().scalar(2).calculate(), (3.0 * 4.0) + 2.0,
              "DefaultPlan: unbracketed 3 * 4 + 2 is left-associative: (3*4)+2 = 14");

    // A single leaf, with no operator at all.
    checkNear(DefaultPlan().scalar(5).calculate(), 5.0, "DefaultPlan: a single scalar leaf");

    // number(): uses T::value() at its static type, not a virtual dispatch
    // -- required for a signed number, whose value() intentionally hides
    // (isn't a virtual override of) SmoothNumberBase::value() (see
    // SmoothNumberBase's docs). Since every Plan is magnitude-only (it now
    // genuinely encodes every leaf into a RepresentationBase -- see
    // plan.hpp -- rather than doing raw double arithmetic), a negative
    // signed number must throw, not silently succeed with the wrong
    // (unsigned-magnitude) value: this is exactly what static resolution
    // guarantees. If number() instead took a plain SmoothNumberBase& (and
    // so read the hidden, magnitude-only value() through it), a negative
    // signed number would silently be treated as its positive magnitude
    // instead of throwing.
    {
        SmoothInteger pos;
        pos.setValue(10LL);
        SmoothSignedInteger neg;
        neg.setValue(-5LL);
        checkNear(DefaultPlan().number(pos).calculate(), 10.0,
                  "DefaultPlan.number(): a non-negative operand computes correctly");
        checkThrows([&] { DefaultPlan().number(neg).calculate(); },
                    "DefaultPlan.number(): a negative signed operand throws (its true, signed value is "
                    "correctly resolved via T::value(), not silently read as its unsigned magnitude)");
    }

    // Nested groups two levels deep: 2 * (3 + (4 * 5)) = 2 * 23 = 46.
    {
        double result = DefaultPlan()
                             .scalar(2)
                             .times()
                             .left()
                             .scalar(3)
                             .plus()
                             .left()
                             .scalar(4)
                             .times()
                             .scalar(5)
                             .right()
                             .right()
                             .calculate();
        checkNear(result, 2.0 * (3.0 + 4.0 * 5.0), "DefaultPlan: two levels of nested left()/right() groups");
    }

    // plan() renders the compiled steps as a tree.
    {
        DefaultPlan p;
        p.scalar(3).times().left().scalar(4).plus().scalar(2).right();
        std::ostringstream out;
        p.plan(out);
        check(out.str() ==
                  "multiply\n"
                  "├─ ensure(scalar)\n"
                  "│  └─ scalar: 3\n"
                  "└─ add\n"
                  "   ├─ ensure(scalar)\n"
                  "   │  └─ scalar: 4\n"
                  "   └─ ensure(scalar)\n"
                  "      └─ scalar: 2\n"
                  "= 18\n",
              "DefaultPlan.plan() renders the expected tree, with the Ensure step it forces (Scalar) shown "
              "explicitly above each leaf");
    }
    {
        DefaultPlan p;
        p.scalar(5);
        std::ostringstream out;
        p.plan(out);
        check(out.str() == "ensure(scalar)\n└─ scalar: 5\n= 5\n",
              "DefaultPlan.plan() for a single leaf has no add/multiply branches, but still shows its Ensure "
              "step");
    }

    // Usage errors.
    checkThrows([] { DefaultPlan().right(); }, "DefaultPlan.right() with no open left() group throws");
    checkThrows([] { DefaultPlan().scalar(3).scalar(4); },
                "DefaultPlan: two scalars in a row with no operator throws");
    checkThrows([] { DefaultPlan().plus(); }, "DefaultPlan.plus() with nothing built yet throws");
    checkThrows([] { DefaultPlan().scalar(3).times().left().scalar(4).calculate(); },
                "DefaultPlan.calculate() with an unclosed left() group throws");
    checkThrows([] { DefaultPlan().scalar(3).times().calculate(); },
                "DefaultPlan.calculate() with a dangling operator (missing right-hand value) throws");
    checkThrows([] { DefaultPlan().scalar(-1.0); },
                "DefaultPlan.scalar(): a negative value throws (every RepresentationBase is magnitude-only)");

    // Metrics: the constructor accepts and propagates an optional shared
    // Metrics, the same as every concrete SmoothNumberBase-derived type.
    // Compiling increments one counter per step (mirroring how
    // SmoothNumberBase counts each representation conversion), and a Plan
    // sharing a Metrics with a SmoothNumber tallies onto the same counters.
    {
        DefaultPlan p;
        check(!p.hasMetrics(), "DefaultPlan() has no metrics by default");
    }
    {
        auto metrics = std::make_shared<Metrics>();
        checkNear(
            DefaultPlan(metrics).scalar(3).times().left().scalar(4).plus().scalar(2).right().calculate(), 18.0,
            "DefaultPlan(metrics) still computes correctly");
        std::ostringstream out;
        metrics->print(out);
        check(out.str() == "add = 1\nconvert_to_scalar = 3\nmultiply = 1\nscalar_operations = 6\n",
              "DefaultPlan(metrics) increments one counter per compiled step, plus ScalarRepresentation's own "
              "scalar_operations: each Ensure(scalar) step walks its leaf's bits into a fresh Scalar rep (one "
              "set() per bit -- 2 for 3's two bits, 1 each for 4 and 2's single bit) and combine()'s add/multiply "
              "each contribute one more (2+1+1+1+1=6)");
    }
    {
        // Compiling is memoized: calculate() then plan() doesn't recount.
        auto metrics = std::make_shared<Metrics>();
        DefaultPlan p(metrics);
        p.scalar(1).plus().scalar(2);
        p.calculate();
        std::ostringstream discard;
        p.plan(discard);
        std::ostringstream out;
        metrics->print(out);
        check(out.str() == "add = 1\nconvert_to_scalar = 2\nscalar_operations = 3\n",
              "DefaultPlan compiling only happens once: calculate() then plan() doesn't recount");
    }
    {
        // Propagation: a Plan and a SmoothNumber sharing one Metrics tally
        // onto the same counters.
        auto metrics = std::make_shared<Metrics>();
        SmoothInteger n(metrics);
        n.set(1, 0);
        std::ostringstream discard;
        n.printSparse(discard);  // dynamic -> sparse

        DefaultPlan(metrics).number(n).plus().scalar(1).calculate();

        std::ostringstream out;
        metrics->print(out);
        check(out.str() ==
                  "add = 1\n"
                  "bit_iterations = 5\n"
                  "convert_dynamic_to_sparse = 1\n"
                  "convert_to_scalar = 2\n"
                  "scalar_operations = 3\n",
              "a Plan and a SmoothNumber sharing one Metrics tally onto the same counters");
    }
}

// ---------------------------------------------------------------------
// plan_zoo/: Plan subclasses whose buildBlueprint() wraps every leaf in
// Ensure(target), routing through a specific RepresentationBase instead of
// DefaultPlan's Scalar. Checked generically (template helper) against all
// three, since they should all behave identically except for name(),
// which representation actually does the work, and (for MatrixPlan only)
// the Ensure label not matching name() -- "matrix" is this library's
// display name for what SmoothNumberBase::Representation calls "dynamic".
// ---------------------------------------------------------------------
template <typename PlanType>
void checkPlanZooVariant(const std::string& expectedName, const std::string& ensureLabel,
                          const std::string& expectedMetrics) {
    PlanType p;
    check(p.name() == expectedName, expectedName + ": name() matches");

    checkNear(PlanType().scalar(3).times().left().scalar(4).plus().scalar(2).right().calculate(), 18.0,
              expectedName + ": 3 * (4 + 2) = 18, computed via its own representation");

    PlanType printed;
    printed.scalar(3).times().left().scalar(4).plus().scalar(2).right();
    std::ostringstream out;
    printed.plan(out);
    std::string expected = "multiply\n├─ ensure(" + ensureLabel +
                            ")\n"
                            "│  └─ scalar: 3\n"
                            "└─ add\n"
                            "   ├─ ensure(" +
                            ensureLabel +
                            ")\n"
                            "   │  └─ scalar: 4\n"
                            "   └─ ensure(" +
                            ensureLabel + ")\n      └─ scalar: 2\n= 18\n";
    check(out.str() == expected, expectedName + ": plan() shows the Ensure step it forces above each leaf");

    checkThrows([] { PlanType().scalar(-1.0); },
                expectedName + ": a negative leaf throws (representation is magnitude-only)");

    auto metrics = std::make_shared<Metrics>();
    PlanType(metrics).scalar(1).plus().scalar(2).calculate();
    std::ostringstream metricsOut;
    metrics->print(metricsOut);
    check(metricsOut.str() == expectedMetrics, expectedName + ": Metrics counter is convert_to_" + expectedName);
}

// ---------------------------------------------------------------------
// Plan::validateBlueprint(): a misbehaving buildBlueprint() override
// should never be able to silently change what's actually being computed
// -- only decorate it with Ensure steps. These three deliberately broken
// Plan subclasses each corrupt the blueprint one way; testBlueprintValidation()
// (below) confirms each one is caught.
// ---------------------------------------------------------------------
class TamperedValuePlan : public Plan {
public:
    explicit TamperedValuePlan(std::shared_ptr<Metrics> metrics = nullptr) : Plan(std::move(metrics)) {}
    std::string name() const override { return "tampered_value"; }

protected:
    // Wraps every leaf in Ensure(Scalar) like DefaultPlan, but then
    // secretly changes a scalar() leaf's value -- exactly the kind of
    // corruption validateBlueprint() exists to catch.
    std::unique_ptr<Node> buildBlueprint(const Node& declaration) const override {
        std::unique_ptr<Node> blueprint = wrapLeavesWithEnsure(declaration, SmoothNumberBase::Representation::Scalar);
        Node* n = blueprint.get();
        while (n->kind == Node::Kind::Ensure) n = n->child.get();
        if (n->kind == Node::Kind::ScalarLeaf) n->scalarValue += 1000.0;
        return blueprint;
    }
};

class WrongShapePlan : public Plan {
public:
    explicit WrongShapePlan(std::shared_ptr<Metrics> metrics = nullptr) : Plan(std::move(metrics)) {}
    std::string name() const override { return "wrong_shape"; }

protected:
    // Ignores the declaration's actual shape and always returns a single
    // scalar leaf -- silently dropping an add()/times() the caller asked
    // for.
    std::unique_ptr<Node> buildBlueprint(const Node& /*declaration*/) const override {
        auto leaf = std::make_unique<Node>();
        leaf->kind = Node::Kind::ScalarLeaf;
        leaf->scalarValue = 0.0;
        auto ensure = std::make_unique<Node>();
        ensure->kind = Node::Kind::Ensure;
        ensure->ensureTarget = SmoothNumberBase::Representation::Scalar;
        ensure->child = std::move(leaf);
        return ensure;
    }
};

class NullChildEnsurePlan : public Plan {
public:
    explicit NullChildEnsurePlan(std::shared_ptr<Metrics> metrics = nullptr) : Plan(std::move(metrics)) {}
    std::string name() const override { return "null_child_ensure"; }

protected:
    // Produces a malformed Ensure node with no child at all.
    std::unique_ptr<Node> buildBlueprint(const Node& /*declaration*/) const override {
        auto ensure = std::make_unique<Node>();
        ensure->kind = Node::Kind::Ensure;
        ensure->ensureTarget = SmoothNumberBase::Representation::Scalar;
        return ensure;
    }
};

void testBlueprintValidation() {
    checkThrows([] { TamperedValuePlan().scalar(5).calculate(); },
                "validateBlueprint(): a buildBlueprint() that changes a scalar() leaf's value throws");
    checkThrows([] { WrongShapePlan().scalar(3).plus().scalar(4).calculate(); },
                "validateBlueprint(): a buildBlueprint() that changes the declaration's shape throws");
    checkThrows([] { NullChildEnsurePlan().scalar(5).calculate(); },
                "validateBlueprint(): a buildBlueprint() that produces a childless Ensure node throws");

    // A well-behaved buildBlueprint() (the normal case) is completely
    // unaffected by any of this -- validateBlueprint() runs on every
    // compile, silently, whenever nothing is actually wrong.
    checkNear(DefaultPlan().scalar(3).plus().scalar(4).calculate(), 7.0,
              "validateBlueprint() doesn't interfere with a correct buildBlueprint()");
}

void testPlanZoo() {
    // Beyond "add"/"convert_to_X", each representation's own instrumentation
    // (bit_iterations, scalar_operations -- see representation_base.hpp,
    // row_values_representation.hpp) also fires while converting (via
    // forEachSet()/set(), from convertLeaf() -- see plan.hpp) and combining
    // two single-bit leaves (1 + 2), so the exact counters differ per
    // variant.
    checkPlanZooVariant<SparsePlan>("sparse", "sparse", "add = 1\nbit_iterations = 3\nconvert_to_sparse = 2\n");
    checkPlanZooVariant<MatrixPlan>("matrix", "dynamic", "add = 1\nbit_iterations = 5\nconvert_to_matrix = 2\n");
    checkPlanZooVariant<RowValuesPlan>(
        "row_values", "row_values", "add = 1\nbit_iterations = 1\nconvert_to_row_values = 2\nscalar_operations = 3\n");

    // DefaultPlan itself is unaffected by any of this.
    check(DefaultPlan().name() == "scalar", "DefaultPlan's name() is still \"scalar\"");

    // Polymorphic usage: name() and calculate() dispatch virtually through
    // a Plan*, and the Plan* destructs safely (virtual destructor).
    {
        std::unique_ptr<Plan> p = std::make_unique<MatrixPlan>();
        p->scalar(2).plus().scalar(3);
        check(p->name() == "matrix", "name() dispatches virtually through a Plan*");
        checkNear(p->calculate(), 5.0, "calculate() works the same through a Plan*");
    }
}

// ---------------------------------------------------------------------
// SmoothNumberBase::valueAs()/representationAs() and Plan::numberVia(): the
// mechanism that lets a Plan-driven computation force an existing number
// through its own real ensure()-driven conversion into that Plan's own
// target representation (and thus its convert_<canonical>_to_<target>
// Metrics counter), rather than reading a snapshot value and rebuilding
// from a double. Every Plan does this for numberVia() leaves now -- it's
// not a special case any one variant opts into.
// ---------------------------------------------------------------------
void testNumberViaForcesConversion() {
    // valueAs(): converts to the requested representation only if it isn't
    // already valid, same as printSparse()/etc. do internally -- a second
    // call for the same target doesn't reconvert.
    {
        auto metrics = std::make_shared<Metrics>();
        SmoothInteger n(metrics);
        n.setValue(11LL);
        checkNear(n.valueAs(SmoothNumberBase::Representation::Sparse), 11.0,
                  "valueAs(Sparse) returns the correct value");
        checkNear(n.valueAs(SmoothNumberBase::Representation::Sparse), 11.0,
                  "valueAs(Sparse) again still returns the correct value");
        check(metrics->get("convert_dynamic_to_sparse") == 1,
              "valueAs(Sparse) converts exactly once, not once per call");
    }

    // numberVia() against SparsePlan forces each fed-in number through its
    // own real conversion into Sparse -- computing correctly, and each of
    // the 3 numbers showing up once under convert_dynamic_to_sparse.
    {
        auto metrics = std::make_shared<Metrics>();
        SparsePlan p(metrics);
        SmoothInteger a, b, c;
        a.setMetricsPtr(metrics);
        b.setMetricsPtr(metrics);
        c.setMetricsPtr(metrics);
        a.setValue(3LL);
        b.setValue(4LL);
        c.setValue(2LL);

        checkNear(p.numberVia(a).times().left().numberVia(b).plus().numberVia(c).right().calculate(), 18.0,
                  "SparsePlan via numberVia(): 3 * (4 + 2) = 18");
        check(metrics->get("convert_dynamic_to_sparse") == 3,
              "SparsePlan forces each of the 3 fed-in numbers through Sparse exactly once");
        // The clone convertNumberLeaf() gets back from n.representationAs()
        // is re-pointed at the Plan's own Metrics (setMetricsPtr()), so
        // combine()'s own carry/bit-operation work on it is instrumented
        // too -- not silently lost just because it came from an existing
        // number rather than a scalar() leaf.
        check(metrics->get("carries") == 2 && metrics->get("bit_operations") == 4,
              "the forced clone's own carries/bit_operations land under the Plan's Metrics too");
    }

    // scalar() leaves are untouched by any of this -- convertNumberLeaf()
    // only applies to numberVia() leaves.
    {
        auto metrics = std::make_shared<Metrics>();
        checkNear(SparsePlan(metrics).scalar(5).plus().scalar(7).calculate(), 12.0,
                  "a scalar-only SparsePlan expression is unaffected");
        check(metrics->get("convert_dynamic_to_sparse") == 0,
              "a scalar-only SparsePlan expression never touches convert_dynamic_to_sparse");
    }

    // Mixing numberVia() and scalar() leaves in the same expression: only
    // the numberVia() leaves force a conversion.
    {
        auto metrics = std::make_shared<Metrics>();
        SparsePlan p(metrics);
        SmoothInteger price, quantity;
        price.setMetricsPtr(metrics);
        quantity.setMetricsPtr(metrics);
        price.setValue(18LL);
        quantity.setValue(4LL);
        checkNear(p.left().numberVia(price).times().numberVia(quantity).right().plus().scalar(6).calculate(),
                  78.0, "mixing numberVia() and scalar() leaves: (18 * 4) + 6 = 78");
        check(metrics->get("convert_dynamic_to_sparse") == 2,
              "only the 2 numberVia() leaves force a conversion, not the scalar() leaf");
    }

    // Different Plans force different target representations for the exact
    // same numbers: DefaultPlan forces Scalar, RowValuesPlan forces
    // RowValues.
    {
        auto metrics = std::make_shared<Metrics>();
        DefaultPlan p(metrics);
        SmoothInteger a, b;
        a.setMetricsPtr(metrics);
        b.setMetricsPtr(metrics);
        a.setValue(4LL);
        b.setValue(9LL);
        checkNear(p.numberVia(a).plus().numberVia(b).calculate(), 13.0,
                  "DefaultPlan via numberVia(): 4 + 9 = 13");
        check(metrics->get("convert_dynamic_to_scalar") == 2,
              "DefaultPlan forces numberVia() leaves through Scalar");
    }
    {
        auto metrics = std::make_shared<Metrics>();
        RowValuesPlan p(metrics);
        SmoothInteger a, b;
        a.setMetricsPtr(metrics);
        b.setMetricsPtr(metrics);
        a.setValue(4LL);
        b.setValue(9LL);
        checkNear(p.numberVia(a).plus().numberVia(b).calculate(), 13.0,
                  "RowValuesPlan via numberVia(): 4 + 9 = 13");
        check(metrics->get("convert_dynamic_to_row_values") == 2,
              "RowValuesPlan forces numberVia() leaves through RowValues");
    }
    // MatrixPlan's target *is* Dynamic -- every fresh number's canonical
    // representation already -- so numberVia() never needs to convert
    // anything (and convert_dynamic_to_dynamic can't exist: ensure() never
    // fires its counter when target == canonical).
    {
        auto metrics = std::make_shared<Metrics>();
        MatrixPlan p(metrics);
        SmoothInteger a, b;
        a.setMetricsPtr(metrics);
        b.setMetricsPtr(metrics);
        a.setValue(4LL);
        b.setValue(9LL);
        checkNear(p.numberVia(a).plus().numberVia(b).calculate(), 13.0,
                  "MatrixPlan via numberVia(): 4 + 9 = 13");
        check(metrics->get("convert_dynamic_to_sparse") == 0 &&
                  metrics->get("convert_dynamic_to_row_values") == 0 &&
                  metrics->get("convert_dynamic_to_scalar") == 0,
              "MatrixPlan's target representation is already every number's canonical one, so numberVia() "
              "never triggers a conversion counter");
    }
}

}  // namespace

int main() {
    testBasicSetGetClear();
    testFractionalRestriction();
    testSetBounds();
    testSetValue();
    testSignedSetValueAndSign();
    testRepresentationsAgree();
    testSparseRepresentationDirectly();
    testRowValuesRepresentationDirectly();
    testDynamicRepresentationGrowth();
    testScalarRepresentationDirectly();
    testAddInPlacePerRepresentation();
    testMultiplyInPlacePerRepresentation();
    testUnsignedAddition();
    testAdditionRequiresMatchingRepresentation();
    testSignedAddition();
    testMultiplication();
    testCopyAndMoveSemantics();
    testTransformation();
    testMetrics();
    testInstrumentationCounters();
    testPlan();
    testPlanZoo();
    testBlueprintValidation();
    testNumberViaForcesConversion();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed.\n";
    if (g_failures > 0) {
        std::cout << g_failures << " FAILURE(S).\n";
        return 1;
    }
    return 0;
}
