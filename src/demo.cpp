#include <iostream>

#include "smooth/smooth_number.hpp"

int main() {
    // Rows index powers of 2 (0..5), columns index powers of 3 (0..3).
    smooth::SmoothNumber n(6, 4);

    std::cout << "Empty matrix:\n";
    n.printMatrix();
    std::cout << "Value: " << n.value() << "\n\n";

    // 2^0*3^0 = 1, 2^1*3^0 = 2, 2^0*3^2 = 9 -> 1 + 2 + 9 = 12
    n.set(0, 0);
    n.set(1, 0);
    n.set(0, 2);

    std::cout << "After setting (0,0), (1,0), (0,2):\n";
    n.printMatrix();
    std::cout << "Value: " << n.value() << "\n\n";

    // 2^2*3^1 = 12 -> total becomes 12 + 12 = 24
    n.set(2, 1);

    std::cout << "After also setting (2,1):\n";
    n.printMatrix();
    std::cout << "Value: " << n.value() << "\n\n";

    std::cout << "get(2,1) = " << n.get(2, 1) << "\n";
    n.clear(2, 1);
    std::cout << "After clear(2,1), get(2,1) = " << n.get(2, 1) << "\n";
    std::cout << "Value: " << n.value() << "\n\n";

    // Fractional example: 2 whole rows/cols (i, j in [0,1]) plus 2 negative
    // rows/cols (i, j in [-2,-1]), so terms can have negative exponents.
    smooth::SmoothNumber f(2, 2, 2, 2);

    f.set(0, 0);   // 2^0 * 3^0 = 1
    f.set(-1, 0);  // 2^-1 * 3^0 = 0.5
    f.set(0, -1);  // 2^0 * 3^-1 = 1/3

    std::cout << "Fractional matrix (line marks the whole/fractional boundary):\n";
    f.printMatrix();
    std::cout << "Value: " << f.value() << "\n\n";

    // --- Representations demo -------------------------------------------
    // The class picks and tracks its own canonical (trusted) representation
    // internally -- there's no public way to force one. Each of the three
    // print functions converts to its own representation on demand (only if
    // it's outdated) without disturbing which one is canonical, and value()
    // computes using whichever representation is currently canonical.
    std::cout << "--- Representations ---\n";
    smooth::SmoothNumber r(4, 3);
    r.set(1, 0);  // 2^1 = 2
    r.set(0, 2);  // 3^2 = 9

    std::cout << "Same number, viewed through all three representations:\n";
    std::cout << "Matrix:\n";
    r.printMatrix();
    std::cout << "Sparse:\n";
    r.printSparse();
    std::cout << "RowValues:\n";
    r.printRowValues();
    std::cout << "Value: " << r.value() << "\n";

    return 0;
}
