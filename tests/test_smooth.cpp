// Lightweight, dependency-free test suite for the smooth library. No test
// framework is linked in (see CMakeLists.txt) -- CHECK() below just records
// pass/fail and prints failures as they happen; main() prints a summary and
// returns non-zero if anything failed, so this also works as a `ctest`.

#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "smooth/dynamic_matrix_representation.hpp"
#include "smooth/row_values_representation.hpp"
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
// SmoothNumberBase::add()/operator+=/operator+ for the two unsigned types.
// ---------------------------------------------------------------------
void testUnsignedAddition() {
    SmoothInteger a, b;
    a.setValue(12LL);
    b.setValue(7LL);
    a.add(b);
    checkNear(a.value(), 19.0, "SmoothInteger.add(SmoothInteger): 12 + 7 = 19");

    a.add(3LL);
    checkNear(a.value(), 22.0, "SmoothInteger.add(long long): 19 + 3 = 22");

    SmoothFloat f;
    f.setValue(1.25);
    f.add(2.5);
    checkNear(f.value(), 3.75, "SmoothFloat.add(double): 1.25 + 2.5 = 3.75");

    SmoothInteger c, d;
    c.setValue(10LL);
    d.setValue(5LL);
    c += d;
    checkNear(c.value(), 15.0, "SmoothInteger operator+=: 10 + 5 = 15");

    SmoothInteger e = c + d;
    checkNear(e.value(), 20.0, "SmoothInteger operator+ (value-returning): 15 + 5 = 20");
    checkNear(c.value(), 15.0, "operator+ does not mutate its left-hand operand");
    checkNear(d.value(), 5.0, "operator+ does not mutate its right-hand operand");

    checkThrows([&] { c.add(-1LL); }, "SmoothInteger.add(negative scalar) throws");

    SmoothInteger whole;
    whole.setValue(5LL);
    SmoothFloat frac;
    frac.setValue(1.5);
    checkThrows([&] { whole.add(frac); },
                "SmoothInteger.add(SmoothFloat with an actual fractional part) throws");

    SmoothFloat wholeFloat;
    wholeFloat.setValue(2.0);  // no fractional bits, even though the type allows them
    whole.add(wholeFloat);
    checkNear(whole.value(), 7.0, "SmoothInteger.add(SmoothFloat with no fractional bits set) is allowed");
}

// ---------------------------------------------------------------------
// Signed addition: same sign, differing sign in both directions, a tie
// that lands on zero, scalar addition with a sign flip, and the
// cross-column (mixed-radix) borrow case.
// ---------------------------------------------------------------------
void testSignedAddition() {
    SmoothSignedInteger a, b;
    a.setValue(5LL);
    b.setValue(3LL);
    a += b;
    checkNear(a.value(), 8.0, "signed same-sign (+,+): 5 + 3 = 8");
    check(!a.isNegative(), "result of (+,+) addition is not negative");

    SmoothSignedInteger c, d;
    c.setValue(-5LL);
    d.setValue(-3LL);
    c += d;
    checkNear(c.value(), -8.0, "signed same-sign (-,-): -5 + -3 = -8");
    check(c.isNegative(), "result of (-,-) addition is negative");

    SmoothSignedInteger e, f;
    e.setValue(5LL);
    f.setValue(-3LL);
    e += f;
    checkNear(e.value(), 2.0, "signed diff-sign, |this| >= |other|: 5 + (-3) = 2");
    check(!e.isNegative(), "5 + (-3) is not negative");

    SmoothSignedInteger g, h;
    g.setValue(3LL);
    h.setValue(-5LL);
    g += h;
    checkNear(g.value(), -2.0, "signed diff-sign, |this| < |other|: 3 + (-5) = -2");
    check(g.isNegative(), "3 + (-5) is negative");

    SmoothSignedInteger tieA, tieB;
    tieA.setValue(5LL);
    tieB.setValue(-5LL);
    tieA += tieB;
    checkNear(tieA.value(), 0.0, "signed diff-sign tie: 5 + (-5) = 0");
    check(!tieA.isNegative(), "a zero result is normalized to non-negative, never a signed zero");

    SmoothSignedInteger scalarCase;
    scalarCase.setValue(4LL);
    scalarCase += -10LL;
    checkNear(scalarCase.value(), -6.0, "signed operator+=(long long) with a sign flip: 4 + (-10) = -6");
    check(scalarCase.isNegative(), "4 + (-10) is negative");

    SmoothSignedFloat sf;
    sf.setValue(-1.5);
    sf += 2.25;
    checkNear(sf.value(), 0.75, "SmoothSignedFloat: -1.5 + 2.25 = 0.75");
    check(!sf.isNegative(), "-1.5 + 2.25 is not negative");

    // Cross-column (mixed-radix) borrow: 3 (stored as n_1=1, i.e. bit
    // (0,1)) minus 2 (stored as n_0=2, i.e. bit (1,0)) must borrow a unit
    // from column 1 into column 0, since neither operand has a bit to
    // borrow from within column 0 alone.
    SmoothSignedInteger x, y;
    x.set(0, 1, true);  // 2^0 * 3^1 = 3
    y.set(1, 0, true);  // 2^1 * 3^0 = 2
    y.negate();
    x += y;
    checkNear(x.value(), 1.0, "cross-column borrow: 3 (column 1) - 2 (column 0) = 1");
    check(!x.isNegative(), "3 - 2 = 1 is not negative");

    // operator+ (value-returning) for a signed type.
    SmoothSignedInteger p, q;
    p.setValue(10LL);
    q.setValue(-4LL);
    SmoothSignedInteger sum = p + q;
    checkNear(sum.value(), 6.0, "SmoothSignedInteger operator+ (value-returning): 10 + (-4) = 6");
    checkNear(p.value(), 10.0, "operator+ does not mutate the signed left-hand operand");
    checkNear(q.value(), -4.0, "operator+ does not mutate the signed right-hand operand");
}

// ---------------------------------------------------------------------
// Copy/move semantics: a copy is a deep, independent clone; a move
// leaves the source usable (defaulted, so no specific end state is
// asserted beyond "doesn't crash and the destination is correct").
// ---------------------------------------------------------------------
void testCopyAndMoveSemantics() {
    SmoothInteger original;
    original.setValue(100LL);

    SmoothInteger copy(original);
    copy.add(1LL);
    checkNear(copy.value(), 101.0, "mutating a copy after copy-construction affects the copy");
    checkNear(original.value(), 100.0, "...but not the original (deep copy, no shared state)");

    SmoothInteger assigned;
    assigned.setValue(1LL);
    assigned = original;
    checkNear(assigned.value(), 100.0, "copy assignment replaces the destination's value");
    assigned.add(1LL);
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
    testAddInPlacePerRepresentation();
    testUnsignedAddition();
    testSignedAddition();
    testCopyAndMoveSemantics();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed.\n";
    if (g_failures > 0) {
        std::cout << g_failures << " FAILURE(S).\n";
        return 1;
    }
    return 0;
}
