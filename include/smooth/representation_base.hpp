#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "smooth/metrics.hpp"

namespace smooth {

// Interface implemented by each internal storage strategy for a
// SmoothNumber's bit grid (Sparse, RowValues, Dynamic, ...). SmoothNumber
// talks to whichever representation is canonical purely through this
// interface, so adding a new one means writing one class -- no existing
// code needs to change. None require any capacity declared up front: each
// grows (or is simply unbounded) to fit whatever gets set.
class RepresentationBase {
public:
    virtual ~RepresentationBase() = default;

    virtual bool get(int i, int j) const = 0;

    // Sets the bit at (i, j) to `value`. Returns true if it actually
    // changed.
    virtual bool set(int i, int j, bool value) = 0;

    // Clears every bit back to the initial (all-zero) state.
    virtual void reset() = 0;

    // Sum of 2^i * 3^j over all set bits, however is natural for this
    // representation's storage.
    virtual double value() const = 0;

    virtual void print(std::ostream& os) const = 0;

    // Invokes fn(i, j) once for every set bit. This is how one
    // representation is rebuilt from another -- conversion asks the
    // source to enumerate exactly the cells it has set.
    virtual void forEachSet(const std::function<void(int, int)>& fn) const = 0;

    // Sets column j's entire contribution at once to n (its bits for
    // i = 0, 1, 2, ..., and, for a negative n, ... -2, -1). Precondition:
    // column j is already all-zero -- this doesn't clear preexisting bits.
    //
    // Every representation implements this itself: Sparse/Dynamic just
    // call decomposeColumnValue() below; RowValues, which already stores
    // exactly this number per column, assigns it directly -- O(1), exact,
    // with no bit decomposition or accumulated floating-point error.
    virtual void setColumnValue(int j, double n) = 0;

    // Deep-copies this representation (each implementation returns
    // std::make_unique<ThatClass>(*this)) -- what lets SmoothNumberBase
    // itself be copied without knowing which concrete types exist. Also
    // copies metrics_ as-is, so a clone starts out instrumented under the
    // same Metrics its source was -- see setMetricsPtr() for re-pointing.
    virtual std::unique_ptr<RepresentationBase> clone() const = 0;

    // Re-points this representation's own Metrics to a different one, or
    // none. Exists for Plan::compileBlueprintNode()'s Ensure case
    // (plan.hpp): a numberVia() leaf's cloned representation starts out
    // carrying that number's own Metrics, which this re-points at the
    // Plan's own instead, so its counters land in the same place.
    virtual void setMetricsPtr(std::shared_ptr<Metrics> metrics) = 0;

    // The read-only counterpart to setMetricsPtr() above, or nullptr if
    // none was ever given. Exposed so code that only has a
    // RepresentationBase& to work with -- not the concrete number or Plan
    // that attached the Metrics in the first place -- can still tally
    // against it. AtomicTransformation::applyAndReportLandings()
    // (atomic_transformation.hpp) is the one caller today: every atomic
    // transform funnels through there regardless of which representation
    // or reduction triggered it, so that's where "atomic_transforms" is
    // counted, reading the Metrics straight off `rep` rather than needing
    // one threaded through the whole Transformation interface.
    virtual std::shared_ptr<Metrics> metricsPtr() const = 0;

    // Adds `other`'s bits into this representation in place. Adding a
    // second 1 into an occupied (i, j) is the same as moving that bit up
    // to (i+1, j) (2 * 2^i * 3^j = 2^(i+1) * 3^j), so carries only ever
    // propagate up the row axis, independently per column.
    //
    // Sparse/Dynamic walk other's bits one at a time and carry (see
    // addBitsWithCarry() below, shared by both). RowValues needs no
    // explicit carry handling -- each bit just adds 2^i to its column's
    // running total.
    virtual void addInPlace(const RepresentationBase& other) = 0;

    // Multiplies this representation by `other`, in place: this = this *
    // other. Since (2^i1*3^j1)*(2^i2*3^j2) = 2^(i1+i2)*3^(j1+j2),
    // multiplying means pairing every term of one with every term of the
    // other and adding exponents.
    //
    // Sparse/Dynamic form every such pairwise sum and carry each in (see
    // multiplyBitsWithCarry() below, shared by both). RowValues instead
    // convolves column totals -- the same as multiplying two polynomials
    // in the variable 3: the result's column j1+j2 gets n_j1*n_j2 added
    // in, for every pair of columns.
    virtual void multiplyInPlace(const RepresentationBase& other) = 0;
};

// Shared by any representation whose set() is the only way it knows how to
// encode a number (Sparse and Dynamic): decomposes n via bit-shifting
// (integer part) and repeated doubling (fractional part), writing each
// resulting bit via set().
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
// whenever a cell is already occupied. The one-term core both
// addBitsWithCarry() and multiplyBitsWithCarry() below repeat for however
// many terms they're combining. Each ripple step increments the "carries"
// counter on `metrics`, if given.
inline void addSingleBitWithCarry(RepresentationBase& dst, int i, int j,
                                   const std::shared_ptr<Metrics>& metrics = nullptr) {
    while (dst.get(i, j)) {
        if (metrics) metrics->increment("carries");
        dst.set(i, j, false);
        ++i;
    }
    dst.set(i, j, true);
}

// Shared by representations that store raw (i, j) bits directly (Sparse
// and Dynamic): adds each of `other`'s set bits into `dst` one at a time.
// `other`'s bits are captured up front, so this is safe even when `other`
// and `dst` are the same object.
inline void addBitsWithCarry(RepresentationBase& dst, const RepresentationBase& other,
                              const std::shared_ptr<Metrics>& metrics = nullptr) {
    std::vector<std::pair<int, int>> bits;
    other.forEachSet([&bits](int i, int j) { bits.emplace_back(i, j); });
    for (const auto& bit : bits) {
        addSingleBitWithCarry(dst, bit.first, bit.second, metrics);
    }
}

// Shared by representations that store raw (i, j) bits directly: sets
// `dst` to a * b by pairing every term of `a` with every term of `b` and
// carrying each in. Both operands' bits are captured up front, before
// `dst` is reset, so this is safe even if `dst` aliases `a`/`b` (e.g.
// squaring). Every (termA, termB) pairing increments "bit_operations".
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

// Subtracts other's magnitude from dst's, in place: dst = dst - other.
// Precondition: dst's value >= other's. Generic rather than per-
// representation -- no representation has a cheaper way to do this borrow
// directly, so this is the one shared implementation every representation
// gets, the same as decomposeColumnValue()/addBitsWithCarry() above.
//
// Converts both operands to per-column totals via forEachSet(), subtracts
// column by column, and resolves any negative column by borrowing from the
// next one up (3^(j+1) = 3 * 3^j), before writing the result back via
// setColumnValue().
inline void subtractMagnitudeInPlace(RepresentationBase& dst, const RepresentationBase& other) {
    std::map<int, double> totals;
    dst.forEachSet([&totals](int i, int j) { totals[j] += std::pow(2.0, i); });
    other.forEachSet([&totals](int i, int j) { totals[j] -= std::pow(2.0, i); });

    for (auto it = totals.begin(); it != totals.end(); ++it) {
        if (it->second < -1e-9) {
            double borrowUnits = std::ceil((-it->second) / 3.0 - 1e-9);
            totals[it->first + 1] -= borrowUnits;
            it->second += borrowUnits * 3.0;
        }
    }

    dst.reset();
    for (const auto& col : totals) {
        if (std::abs(col.second) > 1e-9) dst.setColumnValue(col.first, col.second);
    }
}

}  // namespace smooth
