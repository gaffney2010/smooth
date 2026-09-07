#pragma once

#include <iostream>

namespace smooth {

// Interface implemented by each internal storage strategy for a
// SmoothNumber's bit grid (Matrix, Sparse, RowValues, ...). SmoothNumber
// talks to whichever representation is canonical purely through this
// interface, so adding a new representation means writing one new class
// that implements it -- no existing code needs to change.
class RepresentationBase {
public:
    virtual ~RepresentationBase() = default;

    virtual bool get(int i, int j) const = 0;

    // Sets the bit at (i, j) to `value`. Returns true if the bit's value
    // actually changed.
    virtual bool set(int i, int j, bool value) = 0;

    // Clears every bit back to the initial (all-zero) state.
    virtual void reset() = 0;

    // Sum of 2^i * 3^j over all set bits, computed however is natural for
    // this representation's storage.
    virtual double value() const = 0;

    virtual void print(std::ostream& os) const = 0;
};

}  // namespace smooth
