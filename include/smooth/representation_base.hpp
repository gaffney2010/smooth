#pragma once

#include <functional>
#include <iostream>

namespace smooth {

// Interface implemented by each internal storage strategy for a
// SmoothNumber's bit grid (Sparse, RowValues, Dynamic, ...). SmoothNumber
// talks to whichever representation is canonical purely through this
// interface, so adding a new representation means writing one new class
// that implements it -- no existing code needs to change. None of the
// representations require any capacity to be declared up front: each grows
// (or is simply unbounded) to fit whatever gets set into it.
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

    // Invokes fn(i, j) once for every (i, j) whose bit is set. This is how
    // one representation is rebuilt from another -- since no representation
    // has (or needs) any global bounds to walk, conversion asks the source
    // to enumerate exactly the cells it actually has set.
    virtual void forEachSet(const std::function<void(int, int)>& fn) const = 0;
};

}  // namespace smooth
