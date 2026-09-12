#include <iostream>
#include <memory>
#include <sstream>

#include "smooth/plan_zoo.hpp"
#include "smooth/reduction_zoo.hpp"
#include "smooth/representation_zoo.hpp"
#include "smooth/smooth.hpp"
#include "smooth/transformation_zoo.hpp"

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

    // Beyond conversions, each representation also instruments its own
    // work: "carries" (a ripple-carry step during an add), "bit_operations"
    // (one per (termA, termB) pairing during a multiply -- n bits times m
    // bits is n*m), "scalar_operations" (one per integer/float multiply or
    // add, for RowValues and Scalar), and "bit_iterations" (one per cell
    // visited while looping -- meaning per set bit for Sparse, per grid
    // cell for DynamicMatrix, per column for RowValues).
    std::cout << "\nCounters for a single add + multiply, per representation:\n";
    {
        auto sparseMetrics = std::make_shared<smooth::Metrics>();
        smooth::SparseRepresentation a(/*allow_fractional=*/true, sparseMetrics);
        smooth::SparseRepresentation b(/*allow_fractional=*/true, sparseMetrics);
        a.setColumnValue(0, 7);  // bits at rows 0,1,2
        b.setColumnValue(0, 1);  // bit at row 0
        a.addInPlace(b);         // 7 + 1 = 8: a 3-step carry chain
        a.multiplyInPlace(b);
        std::cout << "sparse:\n";
        sparseMetrics->print();
    }
    {
        auto dynamicMetrics = std::make_shared<smooth::Metrics>();
        smooth::DynamicMatrixRepresentation a(/*allow_fractional=*/true, dynamicMetrics);
        smooth::DynamicMatrixRepresentation b(/*allow_fractional=*/true, dynamicMetrics);
        a.setColumnValue(0, 7);
        b.setColumnValue(0, 1);
        a.addInPlace(b);
        a.multiplyInPlace(b);
        std::cout << "matrix (dynamic):\n";
        dynamicMetrics->print();
    }
    {
        auto rowValuesMetrics = std::make_shared<smooth::Metrics>();
        smooth::RowValuesRepresentation a(/*allow_fractional=*/true, rowValuesMetrics);
        smooth::RowValuesRepresentation b(/*allow_fractional=*/true, rowValuesMetrics);
        a.setColumnValue(0, 7);
        b.setColumnValue(0, 1);
        a.addInPlace(b);
        a.multiplyInPlace(b);
        std::cout << "row_values:\n";
        rowValuesMetrics->print();
    }

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
    smooth::DefaultPlan p1;
    p1.scalar(3).times().left().scalar(4).plus().scalar(2).right();
    std::cout << "3 * (4 + 2):\n";
    p1.plan();
    std::cout << "calculate() = " << p1.calculate() << "\n\n";

    // Without left()/right(), chaining is left-associative, like a simple
    // calculator -- each operator wraps the *entire* accumulated result so
    // far, not just the value immediately before it.
    smooth::DefaultPlan p2;
    p2.scalar(3).times().scalar(4).plus().scalar(2);
    std::cout << "Unbracketed 3 * 4 + 2 (left-associative, i.e. (3*4)+2):\n";
    p2.plan();

    // number() accepts any existing SmoothNumber, using its value() at its
    // own concrete type. Every Plan is magnitude-only (it genuinely encodes
    // each leaf into a RepresentationBase now -- see plan.hpp), so a
    // negative signed number throws rather than silently succeeding as its
    // unsigned magnitude -- which is exactly what resolving T::value() at
    // T's own static type (rather than through a SmoothNumberBase&, where
    // value() intentionally hides, non-virtually) guarantees.
    smooth::SmoothInteger ten;
    ten.setValue(10LL);
    smooth::SmoothSignedInteger negFive;
    negFive.setValue(-5LL);
    std::cout << "\nnumber(ten):\n";
    smooth::DefaultPlan().number(ten).plan();
    try {
        smooth::DefaultPlan().number(negFive).calculate();
    } catch (const std::exception& e) {
        std::cout << "number(negFive) throws (every Plan is magnitude-only): " << e.what() << "\n";
    }

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

    smooth::DefaultPlan(planMetrics).number(tracked).plus().scalar(1).calculate();

    std::cout << "\nMetrics shared between a Plan and a SmoothInteger it reads via number():\n";
    planMetrics->print();

    // --- plan_zoo ----------------------------------------------------------
    // Plan subclasses whose buildBlueprint() wraps every leaf in
    // Ensure(target), routing through a specific RepresentationBase instead
    // of DefaultPlan's Scalar. Same expression, same result, different
    // representation actually doing the addInPlace()/multiplyInPlace() work
    // underneath -- and each shows up under its own name() in the printed
    // plan (each leaf now explicitly showing the Ensure step forcing it
    // there) and the Metrics counters.
    std::cout << "\n--- plan_zoo ---\n";
    smooth::SparsePlan sparsePlan;
    sparsePlan.scalar(3).times().left().scalar(4).plus().scalar(2).right();
    std::cout << sparsePlan.name() << ":\n";
    sparsePlan.plan();

    smooth::MatrixPlan matrixPlan;
    matrixPlan.scalar(3).times().left().scalar(4).plus().scalar(2).right();
    std::cout << "\n" << matrixPlan.name() << ":\n";
    matrixPlan.plan();

    smooth::RowValuesPlan rowValuesPlan;
    rowValuesPlan.scalar(3).times().left().scalar(4).plus().scalar(2).right();
    std::cout << "\n" << rowValuesPlan.name() << ":\n";
    rowValuesPlan.plan();

    // numberVia() keeps a live reference to an existing SmoothNumberBase
    // instead of immediately snapshotting its value the way number() does,
    // so at compile time, every Plan wraps it in an Ensure(target) blueprint
    // step (see Plan::wrapLeavesWithEnsure() in plan.hpp) that forces it
    // through its own real ensure()-driven conversion
    // (SmoothNumberBase::representationAs()), rather than reading a plain
    // value and rebuilding from scratch. Here, feeding the
    // same three numbers into SparsePlan forces each one through Sparse
    // (showing up as convert_dynamic_to_sparse); feeding them into
    // RowValuesPlan instead forces RowValues (convert_dynamic_to_row_values)
    // -- same numbers, same expression, different real conversion,
    // depending entirely on which Plan is asking.
    std::cout << "\nnumberVia(): the same numbers, forced through each Plan's own representation:\n";
    smooth::SmoothInteger three, four, two;
    three.setValue(3LL);
    four.setValue(4LL);
    two.setValue(2LL);

    auto sparseMetrics = std::make_shared<smooth::Metrics>();
    three.setMetricsPtr(sparseMetrics);
    four.setMetricsPtr(sparseMetrics);
    two.setMetricsPtr(sparseMetrics);
    smooth::SparsePlan(sparseMetrics)
        .numberVia(three)
        .times()
        .left()
        .numberVia(four)
        .plus()
        .numberVia(two)
        .right()
        .calculate();
    std::cout << "SparsePlan's Metrics:\n";
    sparseMetrics->print();

    auto rowValuesMetrics = std::make_shared<smooth::Metrics>();
    three.setMetricsPtr(rowValuesMetrics);
    four.setMetricsPtr(rowValuesMetrics);
    two.setMetricsPtr(rowValuesMetrics);
    smooth::RowValuesPlan(rowValuesMetrics)
        .numberVia(three)
        .times()
        .left()
        .numberVia(four)
        .plus()
        .numberVia(two)
        .right()
        .calculate();
    std::cout << "\nRowValuesPlan's Metrics:\n";
    rowValuesMetrics->print();

    // Used polymorphically through a Plan*: name() and calculate() still
    // dispatch to MatrixPlan's overrides, and the Plan* destructs safely
    // (Plan now has a virtual destructor).
    std::unique_ptr<smooth::Plan> polymorphic = std::make_unique<smooth::MatrixPlan>();
    polymorphic->scalar(5).plus().scalar(7);
    std::cout << "\nThrough a Plan*: name() = " << polymorphic->name()
              << ", calculate() = " << polymorphic->calculate() << "\n";

    // --- OffsetTransformation --------------------------------------------------
    // A value-preserving rewrite of a number's bit grid, built from a
    // fixed list of input offsets (each must hold a 1; applying always
    // clears them) and output offsets (each gets carry-set to 1 -- just
    // like ordinary addition -- if already occupied). Since
    // 2^i*3^j + 2^(i+1)*3^j = 2^i*3^(j+1), MergeTransformation trades the
    // two bits at (i, j)/(i+1, j) for the one bit at (i, j+1);
    // SplitTransformation is the exact reverse. applyTransformation()
    // checks canApply() first (only ever the inputs -- an occupied output
    // is never a reason to reject) and throws if it doesn't hold.
    std::cout << "\n--- OffsetTransformation ---\n";
    smooth::SmoothInteger transformed;
    transformed.set(2, 0);  // 2^2 = 4
    transformed.set(3, 0);  // 2^3 = 8
    std::cout << "before merge: value = " << transformed.value() << ", ";
    transformed.printSparse();

    smooth::MergeTransformation merge;
    transformed.applyTransformation(merge, 2, 0);
    std::cout << "after merge(2, 0): value = " << transformed.value() << ", ";
    transformed.printSparse();

    smooth::SplitTransformation split;
    transformed.applyTransformation(split, 2, 0);
    std::cout << "after split(2, 0): value = " << transformed.value() << ", ";
    transformed.printSparse();

    try {
        transformed.applyTransformation(merge, 10, 10);
    } catch (const std::exception& e) {
        std::cout << "merge(10, 10) throws (bits aren't set up for it): " << e.what() << "\n";
    }

    // An occupied output doesn't block a merge -- it carries into it,
    // exactly like ordinary addition would.
    smooth::SmoothInteger collision;
    collision.set(2, 0);  // 4
    collision.set(3, 0);  // 8
    collision.set(2, 1);  // 12  -- already occupies the merge's output
    std::cout << "\nbefore merge with an occupied output: value = " << collision.value() << ", ";
    collision.printSparse();
    collision.applyTransformation(merge, 2, 0);
    std::cout << "after merge(2, 0) (12 + 12 carries to 24 at (3, 1)): value = " << collision.value() << ", ";
    collision.printSparse();

    // OffsetTransformation is concrete, not an interface: a custom one,
    // built directly from its own offset lists, works the same way --
    // 2^i*3^j + 2^(i+1)*3^j + 2^i*3^(j+1) = 2^i*3^j*6 = 2^(i+1)*3^(j+1).
    smooth::OffsetTransformation combineBothAxes({{0, 0}, {1, 0}, {0, 1}}, {{1, 1}});
    smooth::SmoothInteger custom;
    custom.set(0, 0);  // 1
    custom.set(1, 0);  // 2
    custom.set(0, 1);  // 3
    std::cout << "\nbefore a custom OffsetTransformation: value = " << custom.value() << ", ";
    custom.printSparse();
    custom.applyTransformation(combineBothAxes, 0, 0);
    std::cout << "after: value = " << custom.value() << ", ";
    custom.printSparse();

    // SpreadTransformation(n): bridges (i, j) and (i, j+n) -- n columns
    // apart -- into (i+2, j) plus a staircase of bits at
    // (i+1, j+1) .. (i+1, j+n-1). For n = 1 the staircase is empty, so
    // it's just MergeTransformation applied twice into the same cell.
    smooth::SmoothInteger spread;
    spread.set(0, 0);  // 1
    spread.set(0, 4);  // 3^4 = 81
    std::cout << "\nbefore SpreadTransformation(4): value = " << spread.value() << ", ";
    spread.printSparse();
    smooth::SpreadTransformation spread4(4);
    spread.applyTransformation(spread4, 0, 0);
    std::cout << "after: value = " << spread.value() << ", ";
    spread.printSparse();

    // CornerSplitTransformation: splits the bit at (i, j) into its three
    // corner neighbors (i-1, j), (i, j-1), (i-1, j-1) -- all three outputs
    // land at negative indices even anchored at (0, 0), so this needs a
    // fractional type.
    smooth::SmoothFloat corner;
    corner.set(0, 0);  // 1
    std::cout << "\nbefore CornerSplitTransformation at (0, 0): value = " << corner.value() << ", ";
    corner.printSparse();
    smooth::CornerSplitTransformation cornerSplit;
    corner.applyTransformation(cornerSplit, 0, 0);
    std::cout << "after: value = " << corner.value() << ", ";
    corner.printSparse();

    // RowSpreadTransformation(n): the row-axis counterpart to
    // SpreadTransformation -- bridges (i, j) and (i+n, j) -- n rows apart
    // -- into (i, j+1) plus a staircase of bits at (i+1, j) .. (i+n-1, j).
    // For n = 1 this has exactly MergeTransformation's own offsets.
    smooth::SmoothInteger rowSpread;
    rowSpread.set(0, 0);  // 1
    rowSpread.set(4, 0);  // 2^4 = 16
    std::cout << "\nbefore RowSpreadTransformation(4): value = " << rowSpread.value() << ", ";
    rowSpread.printSparse();
    smooth::RowSpreadTransformation rowSpread4(4);
    rowSpread.applyTransformation(rowSpread4, 0, 0);
    std::cout << "after: value = " << rowSpread.value() << ", ";
    rowSpread.printSparse();

    // --- MergingSparsePlan ----------------------------------------------------
    // A TransformationReduction greedily applies a family of
    // Transformations across an entire representation until none of them
    // can fire anywhere anymore -- MergingSparsePlan uses one containing
    // just MergeTransformation, run on both operands before every multiply
    // (never before an add -- merging only helps a multiply's n*m
    // bit_operations blowup). 15 = 1111 binary (4 bits) merges down to just
    // 2 bits first, so 15*15 costs 2*2 = 4 bit_operations instead of
    // SparsePlan's unmerged 4*4 = 16 -- and the printed tree shows exactly
    // where each reduction ran.
    std::cout << "\n--- MergingSparsePlan ---\n";
    smooth::MergingSparsePlan mergingPlan;
    mergingPlan.scalar(15).times().scalar(15);
    mergingPlan.plan();

    auto plainSparseMetrics = std::make_shared<smooth::Metrics>();
    smooth::SparsePlan(plainSparseMetrics).scalar(15).times().scalar(15).calculate();
    auto mergingMetrics = std::make_shared<smooth::Metrics>();
    smooth::MergingSparsePlan(mergingMetrics).scalar(15).times().scalar(15).calculate();
    std::cout << "\nSparsePlan bit_operations for 15*15: " << plainSparseMetrics->get("bit_operations") << "\n";
    std::cout << "MergingSparsePlan bit_operations for 15*15: " << mergingMetrics->get("bit_operations")
              << " (after " << mergingMetrics->get("transformations_applied") << " merges)\n";

    // --- BinaryFormReduction ------------------------------------------------
    // The reverse idea: a TransformationReduction containing just
    // SplitTransformation, run until every bit sits in column 0 -- i.e.
    // until the number is a plain sum of distinct powers of 2, its
    // ordinary binary representation. Unlike the merge reduction above,
    // split alone has no natural floor (nothing about it knows column 0 is
    // special -- left unbounded it would just keep splitting a column-0 bit
    // into column -1, then -2, forever), so BinaryFormReduction bounds its
    // TransformationReduction::run() with an `allowed` predicate that
    // blocks every anchor below column 0.
    std::cout << "\n--- BinaryFormReduction ---\n";
    smooth::SmoothInteger toBinary;
    toBinary.set(1, 2);  // 2^1 * 3^2 = 18
    std::cout << "before: value = " << toBinary.value() << ", ";
    toBinary.printSparse();
    auto binaryRep = toBinary.representationAs(smooth::SmoothInteger::Representation::Sparse);
    smooth::BinaryFormReduction binaryReduction;
    auto binaryMetrics = std::make_shared<smooth::Metrics>();
    binaryReduction.run(*binaryRep, binaryMetrics);
    std::cout << "after: value = " << binaryRep->value() << ", ";
    binaryRep->print(std::cout);
    std::cout << "18 = 16 + 2 = 10010 in binary, reached after "
              << binaryMetrics->get("transformations_applied") << " splits\n";

    // --- TernaryFormReduction -------------------------------------------------
    // First runs BinaryFormReduction (collapsing whatever column layout the
    // number started with down into column 0), then works column by column:
    // so long as a column's total has more than one bit, subtracts 3 from
    // it and adds 1 to the next column -- value-preserving, since
    // 3 * 3^j = 3^(j+1) -- until that column is a single bit (or empty),
    // then moves to the next. This can only run on a RowValuesRepresentation
    // directly, since "subtract 3" needs each column's magnitude as one
    // number, not individual bits with no borrow-subtraction of their own.
    std::cout << "\n--- TernaryFormReduction ---\n";
    smooth::SmoothInteger toTernary;
    toTernary.setValue(13LL);  // 13 = 2^2 + 3^2, not itself a single term
    std::cout << "before: value = " << toTernary.value() << "\n";
    auto ternaryRep = toTernary.representationAs(smooth::SmoothInteger::Representation::RowValues);
    smooth::TernaryFormReduction ternaryReduction;
    auto ternaryMetrics = std::make_shared<smooth::Metrics>();
    ternaryReduction.run(*ternaryRep, ternaryMetrics);
    std::cout << "after: value = " << ternaryRep->value() << "\n";
    ternaryRep->print(std::cout);
    std::cout << "13 = 4*3^0 + 1*3^2, reached after " << ternaryMetrics->get("transformations_applied")
              << " subtract-3/add-1 steps\n";

    // --- TernaryCarryTransformation / TernaryCarryReduction --------------------
    // The Transformation (not OffsetTransformation) TernaryFormReduction's
    // second phase is actually built from: anchored at column j, while n_j
    // has more than one bit set, subtracts 3 from n_j and adds 1 to
    // n_(j+1). Its precondition depends on a whole column's magnitude, not
    // a handful of fixed bit offsets, so it implements Transformation
    // directly rather than going through OffsetTransformation.
    // TernaryCarryReduction runs it to a fixed point, needing no `allowed`
    // bound the way BinaryFormReduction does -- its own canApply() is
    // already self-limiting.
    std::cout << "\n--- TernaryCarryTransformation / TernaryCarryReduction ---\n";
    smooth::RowValuesRepresentation carryRep(/*allow_fractional=*/false);
    carryRep.setColumnValue(0, 9.0);  // 9 = 1001 binary, at column 0
    std::cout << "before: value = " << carryRep.value() << ", ";
    carryRep.print(std::cout);
    smooth::TernaryCarryReduction carryReduction;
    auto carryMetrics = std::make_shared<smooth::Metrics>();
    carryReduction.run(carryRep, carryMetrics);
    std::cout << "after: value = " << carryRep.value() << ", ";
    carryRep.print(std::cout);
    std::cout << "9 = 1*3^2, reached after " << carryMetrics->get("transformations_applied")
              << " subtract-3/add-1 steps -- same result as BinaryFormReduction + TernaryCarryReduction together "
                 "(see TernaryFormReduction above), starting directly from column 0 instead\n";

    // --- StaircaseReduction ---------------------------------------------------
    // So long as two set bits (i1, j1)/(i2, j2) exist with i2 >= i1 and
    // j2 >= j1, combines them: SpreadTransformation for a same-row pair,
    // RowSpreadTransformation for a same-column pair, or
    // CornerSplitTransformation on the dominating bit otherwise. The fixed
    // point is an antichain: sorted by row, columns strictly decrease.
    std::cout << "\n--- StaircaseReduction ---\n";
    smooth::SmoothInteger staircase;
    staircase.set(1, 1);  // 6
    staircase.set(4, 5);  // 3888
    std::cout << "before: value = " << staircase.value() << ", ";
    staircase.printSparse();
    auto staircaseRep = staircase.representationAs(smooth::SmoothInteger::Representation::Sparse);
    smooth::StaircaseReduction staircaseReduction;
    auto staircaseMetrics = std::make_shared<smooth::Metrics>();
    staircaseReduction.run(*staircaseRep, staircaseMetrics);
    std::cout << "after: value = " << staircaseRep->value() << ", ";
    staircaseRep->print(std::cout);
    std::cout << "reached in " << staircaseMetrics->get("transformations_applied") << " steps\n";

    return 0;
}
