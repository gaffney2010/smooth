#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "smooth/metrics.hpp"

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
    // concrete representation types exist. Note that the copy constructor
    // also copies metrics_ (a shared_ptr) as-is, so a clone starts out
    // instrumented under the *same* Metrics its source was -- see
    // setMetricsPtr() below for re-pointing that.
    virtual std::unique_ptr<RepresentationBase> clone() const = 0;

    // Re-points this representation's own Metrics (used to instrument
    // carries/bit_operations/scalar_operations/bit_iterations -- see
    // metrics.hpp and each concrete class) to a different one, or to none
    // (nullptr). Exists for exactly one purpose: Plan::convertNumberLeaf()'s
    // default (see plan.hpp) clones a numberVia() leaf's number's own
    // representation via SmoothNumberBase::representationAs() -- that clone
    // starts out carrying *that number's* Metrics (or none, if it has none),
    // which is almost never what a Plan wants once the clone becomes one of
    // its own Steps; this lets it re-point the clone at its own Metrics
    // instead, so the representation-level counters this Plan's own
    // combine() produces land in the same place as everything else it does.
    virtual void setMetricsPtr(std::shared_ptr<Metrics> metrics) = 0;

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

    // Multiplies this representation by `other`, in place: this = this *
    // other. Since (2^i1 * 3^j1) * (2^i2 * 3^j2) = 2^(i1+i2) * 3^(j1+j2),
    // multiplying two smooth numbers means pairing up every term of one
    // with every term of the other and adding exponents -- i.e. every bit
    // (i1, j1) of this paired with every bit (i2, j2) of other contributes
    // a term at (i1+i2, j1+j2).
    //
    // Every representation implements this itself, same as the other
    // per-representation strategies above: Sparse and Dynamic have no more
    // direct way to multiply than forming every such pairwise sum of
    // exponents and carrying each one in, one term at a time (see
    // multiplyBitsWithCarry() below, which both share -- the same
    // structural fact that makes them share addBitsWithCarry() for
    // addition holds here too, so there's no need for either to convert to
    // the other just to multiply). RowValues instead convolves its column
    // totals -- the same operation as multiplying two polynomials in the
    // variable 3, or long multiplication in base 3 (except a "digit" n_j
    // can be any magnitude, not just 0..2): the result's column j1+j2 gets
    // n_j1 * n_j2 added in, for every pair of columns (j1, j2).
    virtual void multiplyInPlace(const RepresentationBase& other) = 0;
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

// Adds a single term (i, j) into `dst`, ripple-carrying up the row axis
// within column j whenever a cell is already occupied: adding a second 1
// into a cell that already holds one is the same as moving that bit up to
// the next row (2 * 2^i * 3^j = 2^(i+1) * 3^j). This is the one-term core
// that both addBitsWithCarry() and multiplyBitsWithCarry() below repeat
// for however many terms they're combining. Each ripple step -- finding a
// cell already occupied and having to move up -- increments the "carries"
// counter on `metrics`, if one was given.
inline void addSingleBitWithCarry(RepresentationBase& dst, int i, int j,
                                   const std::shared_ptr<Metrics>& metrics = nullptr) {
    while (dst.get(i, j)) {
        if (metrics) metrics->increment("carries");
        dst.set(i, j, false);
        ++i;
    }
    dst.set(i, j, true);
}

// Shared by any representation that stores raw (i, j) bits directly
// (currently Sparse and Dynamic): adds each of `other`'s set bits into
// `dst` one at a time via addSingleBitWithCarry(). `other`'s bits are
// captured up front, so this is safe even if `other` and `dst` are the
// same object (i.e. adding a number to itself).
inline void addBitsWithCarry(RepresentationBase& dst, const RepresentationBase& other,
                              const std::shared_ptr<Metrics>& metrics = nullptr) {
    std::vector<std::pair<int, int>> bits;
    other.forEachSet([&bits](int i, int j) { bits.emplace_back(i, j); });
    for (const auto& bit : bits) {
        addSingleBitWithCarry(dst, bit.first, bit.second, metrics);
    }
}

// Shared by any representation that stores raw (i, j) bits directly
// (currently Sparse and Dynamic): sets `dst` to a * b by pairing up every
// term of `a` with every term of `b` -- (i1, j1) with (i2, j2) contributes
// a term at (i1+i2, j1+j2) -- and carrying each one in via
// addSingleBitWithCarry(). Both operands' bits are captured up front,
// before `dst` is reset, so this is safe even if `dst` aliases `a` and/or
// `b` (e.g. an in-place `x.multiplyInPlace(x)`, squaring x, passes `x` as
// dst, a, and b all at once).
//
// Every (termA, termB) pairing increments the "bit_operations" counter on
// `metrics`, if one was given -- an n-term by m-term multiplication is
// n*m pairings, hence n*m bit operations.
inline void multiplyBitsWithCarry(RepresentationBase& dst, const RepresentationBase& a,
                                   const RepresentationBase& b,
                                   const std::shared_ptr<Metrics>& metrics = nullptr) {
    std::vector<std::pair<int, int>> aBits, bBits;
    a.forEachSet([&aBits](int i, int j) { aBits.emplace_back(i, j); });
    b.forEachSet([&bBits](int i, int j) { bBits.emplace_back(i, j); });
    dst.reset();
    for (const auto& termA : aBits) {
        for (const auto& termB : bBits) {
            if (metrics) metrics->increment("bit_operations");
            addSingleBitWithCarry(dst, termA.first + termB.first, termA.second + termB.second, metrics);
        }
    }
}

}  // namespace smooth
