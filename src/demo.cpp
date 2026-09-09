#include <iostream>
#include <memory>
#include <sstream>

#include "smooth/dynamic_matrix_representation.hpp"
#include "smooth/row_values_representation.hpp"
#include "smooth/scalar_representation.hpp"
#include "smooth/smooth.hpp"
#include "smooth/sparse_representation.hpp"

int main() {
    smooth::SmoothInteger n;  // whole numbers only, no capacity to declare

    std::cout << "Empty number:\n";
    n.printDynamic();
    std::cout << "Value: " << n.value() << "\n\n";

    // 2^0*3^0 = 1, 2^1*3^0 = 2, 2^0*3^2 = 9 -> 1 + 2 + 9 = 12
    n.set(0, 0);
    n.set(1, 0);
    n.set(0, 2);

    std::cout << "After setting (0,0), (1,0), (0,2):\n";
    n.printDynamic();
    std::cout << "Value: " << n.value() << "\n\n";

    // 2^2*3^1 = 12 -> total becomes 12 + 12 = 24
    n.set(2, 1);

    std::cout << "After also setting (2,1):\n";
    n.printDynamic();
    std::cout << "Value: " << n.value() << "\n\n";

    std::cout << "get(2,1) = " << n.get(2, 1) << "\n";
    n.clear(2, 1);
    std::cout << "After clear(2,1), get(2,1) = " << n.get(2, 1) << "\n";
    std::cout << "Value: " << n.value() << "\n\n";

    // setBounds() is just an optional after-the-fact sanity check -- it
    // doesn't reserve or preallocate anything. n already has bits set at
    // row 2, but that's fine; it only affects *future* set()/get() calls.
    n.setBounds(6, 4, 0, 0);
    std::cout << "After setBounds(6, 4, 0, 0), set(9, 0) throws:\n";
    try {
        n.set(9, 0);
    } catch (const std::exception& e) {
        std::cout << "  caught: " << e.what() << "\n";
    }
    std::cout << "\n";

    // Fractional example: SmoothFloat permits negative indices.
    smooth::SmoothFloat f;

    f.set(0, 0);   // 2^0 * 3^0 = 1
    f.set(-1, 0);  // 2^-1 * 3^0 = 0.5
    f.set(0, -1);  // 2^0 * 3^-1 = 1/3

    std::cout << "Fractional number (line marks the whole/fractional boundary):\n";
    f.printDynamic();
    std::cout << "Value: " << f.value() << "\n\n";

    // --- Signed variants ---------------------------------------------------
    // SmoothSignedInteger and SmoothSignedFloat are Signed<SmoothInteger>
    // and Signed<SmoothFloat>: the same sign-flag logic, written once in
    // the Signed<> template, reused by both. set()/get()/print* still only
    // ever deal with the (always-positive) magnitude; only value() and the
    // sign accessors know the number is negative.
    std::cout << "--- Signed variants ---\n";
    smooth::SmoothSignedInteger s;
    s.set(1, 0);  // 2^1 = 2
    s.set(0, 2);  // 3^2 = 9
    std::cout << "SmoothSignedInteger before negate(): " << s.value() << "\n";
    s.negate();
    std::cout << "After negate(): " << s.value() << " (isNegative() = " << s.isNegative() << ")\n\n";

    smooth::SmoothSignedFloat sf;
    sf.set(-1, 0);  // 2^-1 = 0.5
    sf.setNegative(true);
    std::cout << "SmoothSignedFloat with setNegative(true): " << sf.value() << "\n\n";

    // --- setValue() ---------------------------------------------------------
    // Converts a plain integer/float straight into a smooth number by
    // placing it entirely in row j = 0 (n[0] in the RowValues view), since
    // value() = sum_j n_j * 3^j and 3^0 = 1. Replaces whatever was there.
    std::cout << "--- setValue() ---\n";
    smooth::SmoothInteger fromInt;
    fromInt.setValue(42LL);
    std::cout << "SmoothInteger.setValue(42): value = " << fromInt.value() << "\n";
    fromInt.printRowValues();

    smooth::SmoothFloat fromFloat;
    fromFloat.setValue(3.75);
    std::cout << "SmoothFloat.setValue(3.75): value = " << fromFloat.value() << "\n";
    fromFloat.printRowValues();

    smooth::SmoothSignedInteger fromNegInt;
    fromNegInt.setValue(-7LL);
    std::cout << "SmoothSignedInteger.setValue(-7): value = " << fromNegInt.value()
              << " (isNegative() = " << fromNegInt.isNegative() << ")\n\n";

    // --- Addition --------------------------------------------------------
    // operator+ is the only way to add: it never mutates either operand --
    // it copies the left-hand side and adds the right-hand side into that
    // copy, dispatching to the canonical representation's addInPlace(),
    // which -- per representation -- either walks bits with an explicit
    // carry (Sparse, Dynamic) or accumulates column totals directly
    // (RowValues). Both operands must have the same canonical
    // representation -- there's no implicit reconciliation -- or it
    // throws std::invalid_argument. Every freshly constructed number
    // starts out canonical() == Dynamic, so two of them always match.
    std::cout << "--- Addition ---\n";
    smooth::SmoothInteger a1, a2;
    a1.setValue(12LL);
    a2.setValue(7LL);
    smooth::SmoothInteger a3 = a1 + a2;
    std::cout << "SmoothInteger: 12 + 7 = " << a3.value() << " -- both operands unchanged: " << a1.value() << ", "
              << a2.value() << "\n\n";

    // Signed addition combines magnitudes when signs match, and otherwise
    // subtracts the smaller magnitude from the larger and takes the larger
    // operand's sign -- ordinary signed-number addition.
    smooth::SmoothSignedInteger s1, s2;
    s1.setValue(5LL);
    s2.setValue(-3LL);
    smooth::SmoothSignedInteger s3 = s1 + s2;
    std::cout << "SmoothSignedInteger: 5 + (-3) = " << s3.value() << " (isNegative() = " << s3.isNegative()
              << ")\n\n";

    // --- Multiplication ----------------------------------------------------
    // operator* gets exactly the same treatment as operator+: value-
    // returning only, representations must match, no scalars (build a
    // plain number with setValue()/Scalar and multiply that instead).
    // (2^i1*3^j1) * (2^i2*3^j2) = 2^(i1+i2)*3^(j1+j2), so multiplying means
    // pairing up every term of one operand with every term of the other
    // and adding exponents -- Sparse/Dynamic do this directly (carrying
    // collisions the same way addition does), RowValues convolves its
    // column totals (long multiplication in base 3), and Scalar just
    // multiplies its one stored number by the other's.
    std::cout << "--- Multiplication ---\n";
    smooth::SmoothInteger mul1, mul2;
    mul1.setValue(6LL);
    mul2.setValue(7LL);
    smooth::SmoothInteger mul3 = mul1 * mul2;
    std::cout << "SmoothInteger: 6 * 7 = " << mul3.value() << " -- both operands unchanged: " << mul1.value()
              << ", " << mul2.value() << "\n";

    // Signed multiplication is the usual sign-XOR rule: same signs give a
    // positive result, differing signs give a negative one.
    smooth::SmoothSignedInteger sm1, sm2;
    sm1.setValue(6LL);
    sm2.setValue(-7LL);
    smooth::SmoothSignedInteger sm3 = sm1 * sm2;
    std::cout << "SmoothSignedInteger: 6 * (-7) = " << sm3.value() << " (isNegative() = " << sm3.isNegative()
              << ")\n\n";

    // --- Representations demo -------------------------------------------
    // The class picks and tracks its own canonical (trusted) representation
    // internally -- there's no public way to force one. Each print function
    // converts to its own representation on demand (only if it's outdated)
    // without disturbing which one is canonical, and value() computes using
    // whichever representation is currently canonical.
    std::cout << "--- Representations ---\n";
    smooth::SmoothInteger r;
    r.set(1, 0);  // 2^1 = 2
    r.set(0, 2);  // 3^2 = 9

    std::cout << "Same number, viewed through Dynamic/Sparse/RowValues:\n";
    std::cout << "Dynamic:\n";
    r.printDynamic();
    std::cout << "Sparse:\n";
    r.printSparse();
    std::cout << "RowValues:\n";
    r.printRowValues();
    std::cout << "Value: " << r.value() << "\n";

    // Scalar can only hold a plain number (everything in column j = 0), so
    // converting this one -- which has a bit in column 2 -- throws.
    std::cout << "printScalar() on that same number throws (it has a term in column 2):\n";
    try {
        r.printScalar();
    } catch (const std::exception& e) {
        std::cout << "  caught: " << e.what() << "\n";
    }
    std::cout << "\n";

    // --- Dynamic growth, step by step -------------------------------------
    // DynamicMatrixRepresentation isn't gated by any capacity -- it starts
    // at 0x0 and grows purely from what gets set() into it. Setting bits
    // far apart forces repeated doublings; each print below shows the
    // capacity after that doubling.
    std::cout << "--- Dynamic growth, step by step ---\n";
    smooth::DynamicMatrixRepresentation d(/*allow_fractional=*/true);

    std::cout << "Initial (0x0, empty):\n";
    d.print(std::cout);

    d.set(0, 0, true);
    std::cout << "\nAfter set(0,0): grows to fit row/col 0:\n";
    d.print(std::cout);

    d.set(3, 1, true);
    std::cout << "\nAfter set(3,1): row capacity doubles (1 -> 2 -> 4) to fit row 3:\n";
    d.print(std::cout);

    d.set(-2, 5, true);
    std::cout << "\nAfter set(-2,5): negative row capacity grows to fit row -2,\n";
    std::cout << "column capacity doubles to fit column 5:\n";
    d.print(std::cout);

    // --- addInPlace() per representation ------------------------------------
    // A SmoothNumber's canonical representation always starts out (and, for
    // now, stays) Dynamic, so the only way to see Sparse's and RowValues'
    // own addInPlace() strategies run is to exercise those representations
    // directly, the same way the growth walkthrough above does for Dynamic.
    std::cout << "\n--- addInPlace() per representation ---\n";
    smooth::SparseRepresentation sparseA(false), sparseB(false);
    sparseA.set(0, 0, true);  // 1
    sparseB.set(0, 0, true);  // 1 -> carries into (1, 0) since the cell's full
    sparseA.addInPlace(sparseB);
    std::cout << "Sparse: 1 + 1, carried up to bit (1,0): ";
    sparseA.print(std::cout);
    std::cout << "  value = " << sparseA.value() << "\n";

    smooth::RowValuesRepresentation rowA(false), rowB(false);
    rowA.setColumnValue(0, 3.0);
    rowB.setColumnValue(0, 2.0);
    rowA.addInPlace(rowB);
    std::cout << "RowValues: n[0]=3 + n[0]=2, direct accumulation (no bit carry needed): ";
    rowA.print(std::cout);

    // --- multiplyInPlace() per representation --------------------------------
    // Sparse and Dynamic share the exact same pairwise-exponent-sum-with-
    // carry strategy addition uses (being a raw bit grid rather than a
    // std::set doesn't change anything), so there's no need to convert
    // either to the other just to multiply. RowValues instead convolves
    // column totals -- long multiplication in base 3.
    std::cout << "\n--- multiplyInPlace() per representation ---\n";
    smooth::SparseRepresentation mulA(false), mulB(false);
    mulA.set(0, 0, true);
    mulA.set(1, 0, true);  // 3
    mulB.set(0, 0, true);
    mulB.set(1, 0, true);  // 3
    mulA.multiplyInPlace(mulB);
    std::cout << "Sparse: 3 * 3 = 9, via a carry collision mid-multiplication: ";
    mulA.print(std::cout);
    std::cout << "  value = " << mulA.value() << "\n";

    smooth::RowValuesRepresentation mulRowA(false), mulRowB(false);
    mulRowA.setColumnValue(0, 3.0);
    mulRowA.setColumnValue(1, 2.0);  // 3 + 2*3 = 9
    mulRowB.setColumnValue(0, 1.0);
    mulRowB.setColumnValue(2, 4.0);  // 1 + 4*9 = 37
    mulRowA.multiplyInPlace(mulRowB);
    std::cout << "RowValues: 9 * 37 = 333, via convolution of column totals: ";
    mulRowA.print(std::cout);

    // --- ScalarRepresentation ------------------------------------------------
    // Stores the number as a single plain int/float rather than a bit grid:
    // every term it can hold lives in column j = 0, so its value is just
    // that one number. It's still a RepresentationBase, purely so a number
    // using it can still convert to/from the others via the usual
    // ensure()/forEachSet() machinery -- it's not a standalone type.
    std::cout << "\n--- ScalarRepresentation ---\n";
    smooth::ScalarRepresentation scalarA(false), scalarB(false);
    scalarA.setColumnValue(0, 42.0);
    scalarB.setColumnValue(0, 8.0);
    scalarA.addInPlace(scalarB);
    std::cout << "Scalar: 42 + 8, direct scalar addition (no bit decomposition at all): ";
    scalarA.print(std::cout);

    smooth::ScalarRepresentation scalarC(false), scalarD(false);
    scalarC.setColumnValue(0, 6.0);
    scalarD.setColumnValue(0, 7.0);
    scalarC.multiplyInPlace(scalarD);
    std::cout << "Scalar: 6 * 7, direct scalar multiplication: ";
    scalarC.print(std::cout);

    smooth::SmoothInteger plain;
    plain.setValue(17LL);
    std::cout << "SmoothInteger.setValue(17), printScalar(): ";
    plain.printScalar();

    std::cout << "That same number after also setting bit (0,2) [not column 0], printScalar() throws:\n";
    plain.set(0, 2);
    try {
        plain.printScalar();
    } catch (const std::exception& e) {
        std::cout << "  caught: " << e.what() << "\n";
    }

    // --- Metrics -------------------------------------------------------------
    // Optional, shared via the constructor: every representation conversion
    // (via ensure(), triggered here by the print*() calls) increments a
    // counter named convert_<from>_to_<to>. a + b keeps a's metrics unless
    // a has none, in which case it falls back to b's.
    std::cout << "\n--- Metrics ---\n";
    auto metrics = std::make_shared<smooth::Metrics>();
    smooth::SmoothInteger m1(metrics);
    m1.set(1, 0);
    m1.set(0, 2);

    std::ostringstream discard;
    m1.printSparse(discard);     // dynamic -> sparse
    m1.printRowValues(discard);  // dynamic -> row_values
    m1.printSparse(discard);     // sparse already valid: no new conversion

    std::cout << "Counters after two distinct conversions (a redundant third print didn't recount):\n";
    metrics->print();

    smooth::SmoothInteger m2;  // no metrics
    smooth::SmoothInteger m3 = m1 + m2;
    std::cout << "m1 + m2 -> result keeps m1's metrics (m1 has some, m2 doesn't): "
              << (m3.metricsPtr() == metrics ? "same metrics object as m1" : "different") << "\n";

    smooth::SmoothInteger m4 = m2 + m1;
    std::cout << "m2 + m1 -> result falls back to m1's metrics (m2 has none): "
              << (m4.metricsPtr() == metrics ? "same metrics object as m1" : "different") << "\n";

    // --- Plan ------------------------------------------------------------
    // A fluent builder for a scalar arithmetic expression. Building never
    // computes anything -- it just records a tree; left()/right() open and
    // close an explicit nested group (like `(` and `)`), overriding the
    // default left-associative chaining a plain, unbracketed sequence of
    // operators would otherwise get. plan()/calculate() lazily compile that
    // tree into a concrete sequence of steps (for now: convert every leaf
    // to a plain scalar and evaluate with ordinary arithmetic) the first
    // time either is called.
    std::cout << "\n--- Plan ---\n";
    smooth::Plan p1;
    p1.scalar(3).times().left().scalar(4).plus().scalar(2).right();
    std::cout << "3 * (4 + 2):\n";
    p1.plan();
    std::cout << "calculate() = " << p1.calculate() << "\n\n";

    // Without left()/right(), chaining is left-associative, like a simple
    // calculator -- each operator wraps the *entire* accumulated result so
    // far, not just the value immediately before it.
    smooth::Plan p2;
    p2.scalar(3).times().scalar(4).plus().scalar(2);
    std::cout << "Unbracketed 3 * 4 + 2 (left-associative, i.e. (3*4)+2):\n";
    p2.plan();

    // number() accepts any existing SmoothNumber, using its value() at its
    // own concrete type -- correctly capturing a signed number's sign,
    // which value() (being intentionally non-virtual) wouldn't survive
    // being read through a SmoothNumberBase&.
    smooth::SmoothInteger ten;
    ten.setValue(10LL);
    smooth::SmoothSignedInteger negFive;
    negFive.setValue(-5LL);
    std::cout << "\nnumber(ten) + number(negFive):\n";
    smooth::Plan().number(ten).plus().number(negFive).plan();

    // Plan's constructor accepts and propagates an optional shared
    // Metrics, same as every concrete SmoothNumber type: compiling
    // increments one counter per step (convert_to_scalar/add/multiply),
    // mirroring how SmoothNumberBase counts each representation
    // conversion -- so a Metrics shared between a Plan and the numbers
    // feeding it (via number()) tallies both under the same counters.
    auto planMetrics = std::make_shared<smooth::Metrics>();
    smooth::SmoothInteger tracked(planMetrics);
    tracked.set(1, 0);
    tracked.printSparse(discard);  // dynamic -> sparse

    smooth::Plan(planMetrics).number(tracked).plus().scalar(1).calculate();

    std::cout << "\nMetrics shared between a Plan and a SmoothInteger it reads via number():\n";
    planMetrics->print();

    return 0;
}
