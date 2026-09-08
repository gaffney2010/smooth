#include <iostream>

#include "smooth/dynamic_matrix_representation.hpp"
#include "smooth/row_values_representation.hpp"
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
    // add()/operator+= mutate in place; the free operator+ (shared by all
    // four types) makes a copy first and adds into that. Both dispatch to
    // the canonical representation's addInPlace(), which -- per
    // representation -- either walks bits with an explicit carry (Sparse,
    // Dynamic) or accumulates column totals directly (RowValues).
    std::cout << "--- Addition ---\n";
    smooth::SmoothInteger a1;
    a1.setValue(12LL);
    smooth::SmoothInteger a2;
    a2.setValue(7LL);
    a1.add(a2);
    std::cout << "SmoothInteger: 12 + 7 = " << a1.value() << "\n";

    a1 += 3LL;
    std::cout << "  += 3 (scalar, converted via setValue) -> " << a1.value() << "\n";

    smooth::SmoothInteger a3 = a1 + a2;
    std::cout << "operator+ (value-returning): (" << a1.value() << ") + (" << a2.value() << ") = " << a3.value()
              << " -- both operands unchanged: " << a1.value() << ", " << a2.value() << "\n\n";

    // Signed addition combines magnitudes when signs match, and otherwise
    // subtracts the smaller magnitude from the larger and takes the larger
    // operand's sign -- ordinary signed-number addition.
    smooth::SmoothSignedInteger s1;
    s1.setValue(5LL);
    smooth::SmoothSignedInteger s2;
    s2.setValue(-3LL);
    s1 += s2;
    std::cout << "SmoothSignedInteger: 5 + (-3) = " << s1.value() << " (isNegative() = " << s1.isNegative()
              << ")\n";

    smooth::SmoothSignedInteger s3;
    s3.setValue(3LL);
    s3 += -10LL;
    std::cout << "SmoothSignedInteger: 3 += -10 -> " << s3.value() << " (isNegative() = " << s3.isNegative()
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

    std::cout << "Same number, viewed through all three representations:\n";
    std::cout << "Dynamic:\n";
    r.printDynamic();
    std::cout << "Sparse:\n";
    r.printSparse();
    std::cout << "RowValues:\n";
    r.printRowValues();
    std::cout << "Value: " << r.value() << "\n\n";

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

    return 0;
}
