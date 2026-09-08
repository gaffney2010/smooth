#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

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

    // Deep-copies this representation. Each implementation returns
    // std::make_unique<ThatClass>(*this), using its own (compiler-generated)
    // copy constructor -- this is what lets SmoothNumberBase itself be
    // copied (see its copy constructor) without needing to know which
    // concrete representation types exist.
    virtual std::unique_ptr<RepresentationBase> clone() const = 0;

    // Adds `other`'s bits into this representation in place. Adding a
    // second 1 into an (i, j) cell that already holds one is the same as
    // moving that bit up to (i+1, j) -- since 2 * 2^i * 3^j = 2^(i+1) *
    // 3^j -- so carries only ever propagate up the row (power-of-2) axis,
    // independently within each column j, exactly like ordinary binary
    // addition done once per column.
    //
    // Every representation implements this itself, same as the other
    // per-representation strategies above: Sparse and Dynamic have no more
    // direct way to add a number than walking other's bits one at a time
    // and carrying (see addBitsWithCarry() below, which both share).
    // RowValues doesn't need explicit carry handling at all -- each bit
    // just contributes 2^i to its column's running total, and ordinary
    // floating-point addition already produces the correct combined value.
    virtual void addInPlace(const RepresentationBase& other) = 0;
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

// Shared by any representation that stores raw (i, j) bits directly
// (currently Sparse and Dynamic): adds each of `other`'s set bits into
// `dst` one at a time via plain get()/set(), ripple-carrying up the row
// axis within a column whenever a cell is already set. `other`'s bits are
// captured up front, so this is safe even if `other` and `dst` are the
// same object (i.e. adding a number to itself).
inline void addBitsWithCarry(RepresentationBase& dst, const RepresentationBase& other) {
    std::vector<std::pair<int, int>> bits;
    other.forEachSet([&bits](int i, int j) { bits.emplace_back(i, j); });
    for (const auto& bit : bits) {
        int i = bit.first;
        int j = bit.second;
        while (dst.get(i, j)) {
            dst.set(i, j, false);
            ++i;
        }
        dst.set(i, j, true);
    }
}

}  // namespace smooth
