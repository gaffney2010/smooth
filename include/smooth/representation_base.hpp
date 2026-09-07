#pragma once

#include <cmath>
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

    // Sets column j's entire contribution at once to n (i.e. the bits at
    // (i, j) for i = 0, 1, 2, ... and, for a negative n, ... -2, -1 come
    // from n's ordinary binary digits), as if by clearing column j and then
    // calling set(i, j, true) for each of n's bits. Precondition: column j
    // is already all-zero (the one caller, SmoothNumberBase::setValue(),
    // always clears the whole number first) -- this does not clear
    // preexisting bits itself.
    //
    // Every representation implements this itself, same as value() and
    // forEachSet() -- there's no default here, so each one is explicit
    // about its own strategy. Sparse and Dynamic have no more direct way to
    // encode a number than writing its bits one at a time, so both just
    // call decomposeColumnValue() below. RowValues, which already stores
    // exactly this number per column, assigns it directly instead -- O(1)
    // and exact, with no bit decomposition (and no accumulated
    // floating-point error from repeated add/subtract) at all.
    virtual void setColumnValue(int j, double n) = 0;
};

// Shared by any representation whose set() is the only way it knows how to
// encode a number (currently Sparse and Dynamic): decomposes n via
// ordinary bit-shifting (integer part) and repeated doubling (fractional
// part), and writes each resulting bit into `rep` through set().
inline void decomposeColumnValue(RepresentationBase& rep, int j, double n) {
    long long intPart = static_cast<long long>(std::floor(n + 1e-9));
    for (int i = 0; intPart != 0; ++i, intPart >>= 1) {
        if (intPart & 1) rep.set(i, j, true);
    }
    double frac = n - std::floor(n + 1e-9);
    int i = -1;
    while (frac > 1e-9 && i > -64) {
        frac *= 2.0;
        if (frac >= 1.0 - 1e-9) {
            rep.set(i, j, true);
            frac -= 1.0;
        }
        --i;
    }
}

}  // namespace smooth
