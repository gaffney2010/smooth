#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>

#include "smooth/dynamic_matrix_representation.hpp"
#include "smooth/metrics.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/row_values_representation.hpp"
#include "smooth/sparse_representation.hpp"

namespace smooth {

// Shared engine behind all four concrete 3-smooth number types
// (SmoothInteger, SmoothFloat, and their Signed<> counterparts -- see
// smooth_integer.hpp, smooth_float.hpp, signed.hpp). Represents a 3-smooth
// number (a number of the form 2^i * 3^j summed over a set of (i, j)
// pairs). Row index i is the power of 2, column index j is the power of 3.
//
// Not meant to be used directly: its constructor is protected, since
// whether fractional (negative-index) terms are allowed is meant to be
// fixed by which concrete class you pick, not a runtime flag callers set.
//
// Internally, the same logical bit grid can be held in more than one
// representation (see representation_base.hpp and its implementations:
// SparseRepresentation, RowValuesRepresentation, DynamicMatrixRepresentation).
// None of them need any capacity declared up front -- each is either
// naturally unbounded (Sparse, RowValues) or grows to fit whatever gets set
// (Dynamic). Only one representation is canonical at a time -- it is the
// trusted source of truth. The others are lazily (re)derived from it on
// demand and are otherwise considered outdated/un-built. This class talks
// to representations purely through the RepresentationBase interface, so
// adding a new one means: adding an enumerator to Representation, adding a
// slot to the construction/registration in the constructor below, and
// writing the new class -- no other existing logic needs to change.
class SmoothNumberBase {
public:
    enum class Representation { Sparse, RowValues, Dynamic };

    virtual ~SmoothNumberBase() = default;

    // Deep-copies every representation (via RepresentationBase::clone()),
    // so the copy shares no state with the original. Needed for
    // value-returning addition (operator+, below): it's built out of a
    // copy plus operator+=, rather than duplicating add's logic.
    SmoothNumberBase(const SmoothNumberBase& other)
        : allowFractional_(other.allowFractional_),
          boundsSet_(other.boundsSet_),
          rowBound_(other.rowBound_),
          colBound_(other.colBound_),
          negRowBound_(other.negRowBound_),
          negColBound_(other.negColBound_),
          metrics_(other.metrics_),
          canonical_(other.canonical_) {
        for (std::size_t k = 0; k < kRepresentationCount; ++k) {
            reps_[k] = other.reps_[k]->clone();
            valid_[k] = other.valid_[k];
        }
    }

    SmoothNumberBase& operator=(const SmoothNumberBase& other) {
        if (this == &other) return *this;
        allowFractional_ = other.allowFractional_;
        boundsSet_ = other.boundsSet_;
        rowBound_ = other.rowBound_;
        colBound_ = other.colBound_;
        negRowBound_ = other.negRowBound_;
        negColBound_ = other.negColBound_;
        metrics_ = other.metrics_;
        canonical_ = other.canonical_;
        for (std::size_t k = 0; k < kRepresentationCount; ++k) {
            reps_[k] = other.reps_[k]->clone();
            valid_[k] = other.valid_[k];
        }
        return *this;
    }

    SmoothNumberBase(SmoothNumberBase&&) = default;
    SmoothNumberBase& operator=(SmoothNumberBase&&) = default;

    bool allowsFractional() const { return allowFractional_; }

    // Which representation is currently canonical (trusted). Which one that
    // is, and when (if ever) that changes, is an internal decision -- there
    // is no public way to force it.
    Representation canonical() const { return canonical_; }

    // The optional Metrics this number was constructed with (or later given
    // via setMetrics()), or nullptr if none. Exposed publicly so free
    // functions like operator+ (below) can implement the "keep a's
    // metrics, falling back to b's" rule without being members.
    bool hasMetrics() const { return static_cast<bool>(metrics_); }
    const std::shared_ptr<Metrics>& metricsPtr() const { return metrics_; }
    void setMetricsPtr(std::shared_ptr<Metrics> metrics) { metrics_ = std::move(metrics); }

    // Purely an optional, after-the-fact sanity check: future set()/get()
    // calls outside [-neg_rows, max_rows) x [-neg_cols, max_cols) will
    // throw. It does not preallocate, reserve, or otherwise change how any
    // representation stores data -- every representation is unbounded (or
    // grows to fit) regardless of whether this has ever been called.
    void setBounds(std::size_t max_rows, std::size_t max_cols, std::size_t neg_rows, std::size_t neg_cols) {
        rowBound_ = max_rows;
        colBound_ = max_cols;
        negRowBound_ = neg_rows;
        negColBound_ = neg_cols;
        boundsSet_ = true;
    }

    // Reads/writes go through the canonical representation. A set() that
    // actually changes the value invalidates every other representation;
    // one that doesn't change anything leaves them as they are.
    bool get(int i, int j) const {
        checkBounds(i, j);
        return repFor(canonical_).get(i, j);
    }

    void set(int i, int j, bool value = true) {
        checkBounds(i, j);
        bool changed = repFor(canonical_).set(i, j, value);
        if (changed) invalidateAllExcept(canonical_);
    }

    void clear(int i, int j) { set(i, j, false); }

    // Converts to Sparse if needed, then prints it.
    void printSparse(std::ostream& os = std::cout) {
        ensure(Representation::Sparse);
        repFor(Representation::Sparse).print(os);
    }

    // Converts to RowValues if needed, then prints it.
    void printRowValues(std::ostream& os = std::cout) {
        ensure(Representation::RowValues);
        repFor(Representation::RowValues).print(os);
    }

    // Converts to Dynamic if needed, then prints it (including its current
    // grown capacity -- see DynamicMatrixRepresentation).
    void printDynamic(std::ostream& os = std::cout) {
        ensure(Representation::Dynamic);
        repFor(Representation::Dynamic).print(os);
    }

    // Sum of 2^i * 3^j over all set bits, computed by whichever
    // representation is canonical (see each representation's value() for
    // its strategy). Uses double, so precision degrades for large
    // row/column counts or deeply negative indices. Not virtual: Signed<>
    // hides rather than overrides this (see signed.hpp) since these classes
    // are always used by their concrete type, never through a
    // SmoothNumberBase*.
    double value() const { return repFor(canonical_).value(); }

    // Replaces whatever this number currently holds with `v`, encoded
    // entirely in row j = 0: since value() = sum_j n_j * 3^j, putting
    // everything in column 0 means the total is just n_0 * 3^0 = n_0 = v.
    // Delegates the actual encoding to the canonical representation's own
    // setColumnValue() (see RepresentationBase), so e.g. RowValues -- which
    // already stores exactly n_0 -- assigns it directly in O(1) rather than
    // going through a bit-by-bit decomposition.
    //
    // Throws std::invalid_argument for a negative v -- the bit grid can
    // only ever hold positive magnitude; use a Signed<> type (which
    // overrides this to split off the sign first) for negative values.
    void setValue(long long v) {
        if (v < 0) {
            throw std::invalid_argument("SmoothNumberBase::setValue: value must be non-negative for this type");
        }
        clearAllBits();
        repFor(canonical_).setColumnValue(0, static_cast<double>(v));
    }

    // Same idea, but v may have a fractional part. Throws
    // std::invalid_argument if v is negative, or if it has a fractional
    // part but this type doesn't allow fractional (negative-index) terms.
    void setValue(double v) {
        if (v < 0.0) {
            throw std::invalid_argument("SmoothNumberBase::setValue: value must be non-negative for this type");
        }
        double frac = v - std::floor(v);
        if (frac > 1e-9 && !allowFractional_) {
            throw std::invalid_argument(
                "SmoothNumberBase::setValue: value has a fractional part but this type doesn't allow "
                "fractional terms");
        }
        clearAllBits();
        repFor(canonical_).setColumnValue(0, v);
    }

    // Adds `other`'s value into this number in place, through the
    // canonical representation's addInPlace() (see RepresentationBase) --
    // each representation adds the 1s and handles carries however is
    // natural for its own storage. Throws std::invalid_argument if `other`
    // has fractional terms but this type doesn't allow them.
    void add(const SmoothNumberBase& other) {
        if (!allowFractional_ && other.allowFractional_) {
            bool otherHasFractional = false;
            other.repFor(other.canonical_).forEachSet([&otherHasFractional](int i, int j) {
                if (i < 0 || j < 0) otherHasFractional = true;
            });
            if (otherHasFractional) {
                throw std::invalid_argument(
                    "SmoothNumberBase::add: other has fractional terms but this type doesn't allow them");
            }
        }
        repFor(canonical_).addInPlace(other.repFor(other.canonical_));
        invalidateAllExcept(canonical_);
    }

    // Adds a plain integer/float into this number in place, by converting
    // it the same way setValue() does -- placing it entirely in column 0 --
    // into a scratch representation, then adding that in. Throws
    // std::invalid_argument for a negative scalar (see setValue()) or (for
    // the double overload) a fractional scalar on a non-fractional type.
    void add(long long scalar) {
        if (scalar < 0) {
            throw std::invalid_argument("SmoothNumberBase::add: scalar must be non-negative for this type");
        }
        RowValuesRepresentation delta(allowFractional_);
        delta.setColumnValue(0, static_cast<double>(scalar));
        repFor(canonical_).addInPlace(delta);
        invalidateAllExcept(canonical_);
    }

    void add(double scalar) {
        if (scalar < 0.0) {
            throw std::invalid_argument("SmoothNumberBase::add: scalar must be non-negative for this type");
        }
        double frac = scalar - std::floor(scalar);
        if (frac > 1e-9 && !allowFractional_) {
            throw std::invalid_argument(
                "SmoothNumberBase::add: scalar has a fractional part but this type doesn't allow fractional "
                "terms");
        }
        RowValuesRepresentation delta(allowFractional_);
        delta.setColumnValue(0, scalar);
        repFor(canonical_).addInPlace(delta);
        invalidateAllExcept(canonical_);
    }

    // Operator sugar over add(); returns *this so the usual +=/chained-call
    // idioms work. Not virtual -- like value()/setValue(), Signed<> hides
    // rather than overrides these (see signed.hpp).
    SmoothNumberBase& operator+=(const SmoothNumberBase& other) {
        add(other);
        return *this;
    }

    SmoothNumberBase& operator+=(long long scalar) {
        add(scalar);
        return *this;
    }

    SmoothNumberBase& operator+=(double scalar) {
        add(scalar);
        return *this;
    }

protected:
    // allow_fractional lets i and j go negative, so the number can
    // represent fractional values (e.g. i = -1 contributes a factor of
    // 1/2). This has to be decided up front because it's the one thing no
    // representation can discover on its own: it affects how RowValues
    // formats and decodes its numbers, and it's the only structural
    // restriction any representation still enforces.
    explicit SmoothNumberBase(bool allow_fractional, std::shared_ptr<Metrics> metrics = nullptr)
        : allowFractional_(allow_fractional), metrics_(std::move(metrics)), canonical_(Representation::Dynamic) {
        reps_[index(Representation::Sparse)] = std::make_unique<SparseRepresentation>(allow_fractional);
        reps_[index(Representation::RowValues)] = std::make_unique<RowValuesRepresentation>(allow_fractional);
        reps_[index(Representation::Dynamic)] = std::make_unique<DynamicMatrixRepresentation>(allow_fractional);
        valid_[index(Representation::Dynamic)] = true;
    }

    // Subtracts other's magnitude from this one's, in place: this - other.
    // Precondition: this->value() >= other.value(), and `other` is the same
    // concrete type as `this` (so their allowFractional_ agree) -- callers
    // (currently only Signed<Base>::operator+=, for combining operands with
    // different signs) are responsible for both. Protected rather than
    // public: unlike addition, plain subtraction has no meaning for the two
    // unsigned types (their bit grid can't hold a negative result), so it's
    // only exposed as a building block for signed addition.
    //
    // Unlike addInPlace(), this isn't dispatched per representation: it
    // works by converting both operands to per-column totals (n_j, as in
    // RowValues) via forEachSet(), subtracting column by column, and
    // resolving any column that goes negative by borrowing from the next
    // column up -- one unit of n_(j+1) is worth exactly 3 units of n_j,
    // since 3^(j+1) = 3 * 3^j -- before writing the result back through
    // setColumnValue(). That borrow step has no natural per-representation
    // variation the way carrying during addition does (nothing here is
    // cheaper for RowValues to do directly), so one shared implementation
    // covers every representation.
    void subtractMagnitudeInPlace(const SmoothNumberBase& other) {
        std::map<int, double> totals;
        repFor(canonical_).forEachSet([&totals](int i, int j) { totals[j] += std::pow(2.0, i); });
        other.repFor(other.canonical_).forEachSet([&totals](int i, int j) { totals[j] -= std::pow(2.0, i); });

        for (auto it = totals.begin(); it != totals.end(); ++it) {
            if (it->second < -1e-9) {
                double borrowUnits = std::ceil((-it->second) / 3.0 - 1e-9);
                totals[it->first + 1] -= borrowUnits;
                it->second += borrowUnits * 3.0;
            }
        }

        RepresentationBase& canon = repFor(canonical_);
        canon.reset();
        for (const auto& col : totals) {
            if (std::abs(col.second) > 1e-9) canon.setColumnValue(col.first, col.second);
        }
        invalidateAllExcept(canonical_);
    }

private:
    static constexpr std::size_t kRepresentationCount = 3;

    static std::size_t index(Representation r) { return static_cast<std::size_t>(r); }

    static const char* representationName(Representation r) {
        switch (r) {
            case Representation::Sparse:
                return "sparse";
            case Representation::RowValues:
                return "row_values";
            case Representation::Dynamic:
                return "dynamic";
        }
        return "unknown";
    }

    RepresentationBase& repFor(Representation r) { return *reps_[index(r)]; }
    const RepresentationBase& repFor(Representation r) const { return *reps_[index(r)]; }

    void checkBounds(int i, int j) const {
        if (!allowFractional_ && (i < 0 || j < 0)) {
            throw std::out_of_range("SmoothNumberBase: negative index requires a fractional type");
        }
        if (boundsSet_) {
            if (i < -static_cast<int>(negRowBound_) || i >= static_cast<int>(rowBound_) ||
                j < -static_cast<int>(negColBound_) || j >= static_cast<int>(colBound_)) {
                throw std::out_of_range("SmoothNumberBase: index outside the bounds set via setBounds()");
            }
        }
    }

    bool isValid(Representation r) const { return valid_[index(r)]; }
    void markValid(Representation r) { valid_[index(r)] = true; }

    void invalidateAllExcept(Representation keep) {
        for (std::size_t k = 0; k < kRepresentationCount; ++k) {
            if (static_cast<Representation>(k) != keep) valid_[k] = false;
        }
    }

    // Brings `target` up to date by asking the canonical representation to
    // enumerate every cell it has set, and replaying each into `target` --
    // unless `target` is already up to date (canonical is always considered
    // up to date, and an already-valid representation is never redundantly
    // reconverted). No global bounds are needed for this, since
    // forEachSet() is each representation's own responsibility.
    void ensure(Representation target) {
        if (target == canonical_ || isValid(target)) return;
        if (metrics_) {
            metrics_->increment(std::string("convert_") + representationName(canonical_) + "_to_" +
                                 representationName(target));
        }
        RepresentationBase& dst = repFor(target);
        const RepresentationBase& src = repFor(canonical_);
        dst.reset();
        src.forEachSet([&dst](int i, int j) { dst.set(i, j, true); });
        markValid(target);
    }

    // Converts to `target` if needed and makes it canonical. Private:
    // representation choice is an internal decision, not something callers
    // dictate. Currently unused -- it's the hook for future internal
    // heuristics that pick the best representation for a given workload.
    void convertTo(Representation target) {
        ensure(target);
        canonical_ = target;
    }

    // Wipes the canonical representation back to empty and every other
    // representation back to outdated, in one step -- used by setValue()
    // so it fully replaces this number's value rather than adding to it.
    void clearAllBits() {
        repFor(canonical_).reset();
        invalidateAllExcept(canonical_);
    }

    bool allowFractional_;

    bool boundsSet_ = false;
    std::size_t rowBound_ = 0;
    std::size_t colBound_ = 0;
    std::size_t negRowBound_ = 0;
    std::size_t negColBound_ = 0;

    std::shared_ptr<Metrics> metrics_;

    std::array<std::unique_ptr<RepresentationBase>, kRepresentationCount> reps_;
    std::array<bool, kRepresentationCount> valid_{};
    Representation canonical_;
};

// Value-returning addition (a + b), built from a copy plus operator+= --
// this is what the copy constructor above exists for. Templated once and
// shared by every concrete type (SmoothInteger, SmoothFloat, and both
// Signed<> types), each of which supplies its own operator+= (see
// smooth_number_base.hpp's own SmoothNumberBase::operator+= and
// signed.hpp's Signed<Base>::operator+=); T's own overload is always
// chosen over the inherited SmoothNumberBase one when T = Signed<Base>,
// since Signed<Base> declares operator+= itself and so hides (rather than
// overloads-with) the base's version.
//
// Metrics: `lhs` is a copy of `a`, and `lhs += rhs` (per operator+='s own
// contract) keeps that copy's -- i.e. a's -- metrics no matter what. This
// is the one place that differs from plain operator+=: if a had no
// metrics to begin with, the result falls back to adopting b's (if b has
// any), rather than staying without one.
template <typename T>
T operator+(T lhs, const T& rhs) {
    lhs += rhs;
    if (!lhs.hasMetrics() && rhs.hasMetrics()) {
        lhs.setMetricsPtr(rhs.metricsPtr());
    }
    return lhs;
}

template <typename T>
T operator+(T lhs, long long rhs) {
    lhs += rhs;
    return lhs;
}

template <typename T>
T operator+(T lhs, double rhs) {
    lhs += rhs;
    return lhs;
}

}  // namespace smooth
