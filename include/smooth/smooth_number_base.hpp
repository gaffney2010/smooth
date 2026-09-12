#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "smooth/metrics.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/representation_zoo.hpp"

namespace smooth {

// Forward-declared, not included: OffsetTransformation (transformation.hpp)
// needs SmoothNumberBase's full definition, so the dependency can't run
// the other way too -- applyTransformation() below is declared here but
// defined in transformation.hpp, once OffsetTransformation is fully known.
class OffsetTransformation;

// Shared engine behind all four concrete 3-smooth number types
// (SmoothInteger, SmoothFloat, and their Signed<> counterparts). Represents
// a 3-smooth number (a number of the form 2^i * 3^j summed over a set of
// (i, j) pairs). Row index i is the power of 2, column index j is the
// power of 3.
//
// Not meant to be used directly: its constructor is protected, since
// whether fractional (negative-index) terms are allowed is fixed by which
// concrete class you pick, not a runtime flag.
//
// Internally, the same logical bit grid can be held in more than one
// representation (representation_base.hpp: SparseRepresentation,
// RowValuesRepresentation, DynamicMatrixRepresentation,
// ScalarRepresentation). Only one representation is canonical at a time --
// the trusted source of truth; the others are lazily (re)derived from it
// on demand. This class talks to representations purely through the
// RepresentationBase interface, so adding a new one means: adding an
// enumerator, registering it in the constructor, and writing the class --
// no other existing logic needs to change.
class SmoothNumberBase {
public:
    enum class Representation { Sparse, RowValues, Dynamic, Scalar };

    virtual ~SmoothNumberBase() = default;

    // Deep-copies every representation, so the copy shares no state with
    // the original. Needed for value-returning addition (operator+,
    // below): built out of a copy plus operator+=, rather than
    // duplicating add's logic.
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

    // Which representation is currently canonical. There is no public way
    // to force it.
    Representation canonical() const { return canonical_; }

    // The optional Metrics this number was constructed with, or nullptr.
    // Exposed so free functions like operator+ can implement "keep a's
    // metrics, falling back to b's" without being members.
    bool hasMetrics() const { return static_cast<bool>(metrics_); }
    const std::shared_ptr<Metrics>& metricsPtr() const { return metrics_; }
    void setMetricsPtr(std::shared_ptr<Metrics> metrics) { metrics_ = std::move(metrics); }

    // Purely an optional, after-the-fact sanity check: future set()/get()
    // calls outside [-neg_rows, max_rows) x [-neg_cols, max_cols) throw.
    // Doesn't preallocate or reserve anything -- every representation is
    // unbounded regardless of whether this has ever been called.
    void setBounds(std::size_t max_rows, std::size_t max_cols, std::size_t neg_rows, std::size_t neg_cols) {
        rowBound_ = max_rows;
        colBound_ = max_cols;
        negRowBound_ = neg_rows;
        negColBound_ = neg_cols;
        boundsSet_ = true;
    }

    // Reads/writes go through the canonical representation. A set() that
    // actually changes the value invalidates every other representation.
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

    // Checks that `t` can be applied at (i, j) and, if so, applies it;
    // throws std::invalid_argument otherwise. Works directly against
    // whichever representation is currently canonical (not through this
    // class's own get()/set()), so a per-representation-specialized atom
    // (atomic_transformation.hpp) gets its specialized behavior; the other
    // representations are invalidated once afterward instead of per bit.
    // When `atomize` is true, applies `t` via its own atomize()
    // decomposition instead of directly. Defined out-of-line in
    // transformation.hpp, once OffsetTransformation is fully defined.
    void applyTransformation(const OffsetTransformation& t, int i, int j, bool atomize = false);

    // Converts to Sparse if needed, then prints it.
    void printSparse(std::ostream& os = std::cout) { ensured(Representation::Sparse).print(os); }

    // Converts to RowValues if needed, then prints it.
    void printRowValues(std::ostream& os = std::cout) { ensured(Representation::RowValues).print(os); }

    // Converts to Dynamic if needed, then prints it (including its current
    // grown capacity).
    void printDynamic(std::ostream& os = std::cout) { ensured(Representation::Dynamic).print(os); }

    // Converts to Scalar if needed, then prints it. Throws
    // std::invalid_argument if the value isn't representable as a plain
    // number (a term with column j != 0).
    void printScalar(std::ostream& os = std::cout) { ensured(Representation::Scalar).print(os); }

    // Converts to `target` if needed, then returns that representation's
    // value -- lets external code request a specific representation
    // without needing to know or care which one is already canonical.
    double valueAs(Representation target) { return ensured(target).value(); }

    // Same idea, but hands back an independent clone of the representation
    // itself instead of just its value. Mutating the clone never touches
    // this number's own state.
    std::unique_ptr<RepresentationBase> representationAs(Representation target) {
        return ensured(target).clone();
    }

    // Sum of 2^i * 3^j over all set bits. Not virtual: Signed<> hides
    // rather than overrides this (signed.hpp), since these classes are
    // always used by their concrete type, never through a
    // SmoothNumberBase*.
    double value() const { return repFor(canonical_).value(); }

    // Replaces whatever this number currently holds with `v`, encoded
    // entirely in row j = 0 (since value() = sum_j n_j * 3^j, n_0 = v
    // reproduces it exactly). Throws std::invalid_argument for a negative
    // v -- use a Signed<> type for negative values.
    void setValue(long long v) { setValue(static_cast<double>(v)); }

    // Same idea, but v may have a fractional part. Throws
    // std::invalid_argument if v is negative, or fractional on a
    // non-fractional type.
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

protected:
    // allow_fractional lets i and j go negative. Decided up front since
    // it's the one thing no representation can discover on its own.
    explicit SmoothNumberBase(bool allow_fractional, std::shared_ptr<Metrics> metrics = nullptr)
        : allowFractional_(allow_fractional), metrics_(std::move(metrics)), canonical_(Representation::Dynamic) {
        reps_[index(Representation::Sparse)] = std::make_unique<SparseRepresentation>(allow_fractional, metrics_);
        reps_[index(Representation::RowValues)] =
            std::make_unique<RowValuesRepresentation>(allow_fractional, metrics_);
        reps_[index(Representation::Dynamic)] =
            std::make_unique<DynamicMatrixRepresentation>(allow_fractional, metrics_);
        reps_[index(Representation::Scalar)] = std::make_unique<ScalarRepresentation>(allow_fractional, metrics_);
        valid_[index(Representation::Dynamic)] = true;
    }

    // Adds other's value into this one's in place, through the canonical
    // representation's addInPlace(). Protected: only used internally to
    // build value-returning operator+ (smooth_integer.hpp, smooth_float.hpp,
    // signed.hpp).
    //
    // Throws std::invalid_argument if this->canonical() != other.canonical()
    // (no implicit reconciliation, unlike ensure()), or if `other` has
    // fractional terms this type doesn't allow.
    void addMatchingInPlace(const SmoothNumberBase& other) {
        requireMatchingRepresentation(other);
        requireCompatibleFractional(other, "addMatchingInPlace");
        repFor(canonical_).addInPlace(other.repFor(other.canonical_));
        invalidateAllExcept(canonical_);
    }

    // Multiplies this one's value by other's, in place. Same reasoning
    // and same throwing conditions as addMatchingInPlace() -- except
    // multiplying two non-negative exponents can never produce a negative
    // one, so only `other` can be the source of a new fractional term.
    void multiplyMatchingInPlace(const SmoothNumberBase& other) {
        requireMatchingRepresentation(other);
        requireCompatibleFractional(other, "multiplyMatchingInPlace");
        repFor(canonical_).multiplyInPlace(other.repFor(other.canonical_));
        invalidateAllExcept(canonical_);
    }

    // Subtracts other's magnitude from this one's, in place: this - other.
    // Precondition: this->value() >= other.value(), same concrete type as
    // this. Protected: plain subtraction has no meaning for the two
    // unsigned types, so it's only exposed as a building block for signed
    // addition (Signed<Base>::operator+, combining operands with
    // different signs).
    //
    // The actual per-column borrow algorithm is generic across every
    // representation (see subtractMagnitudeInPlace() in
    // representation_base.hpp), so this just enforces the matching-
    // representation precondition and invalidates the rest afterward.
    void subtractMagnitudeInPlace(const SmoothNumberBase& other) {
        requireMatchingRepresentation(other);
        smooth::subtractMagnitudeInPlace(repFor(canonical_), other.repFor(other.canonical_));
        invalidateAllExcept(canonical_);
    }

private:
    static constexpr std::size_t kRepresentationCount = 4;

    static std::size_t index(Representation r) { return static_cast<std::size_t>(r); }

    static const char* representationName(Representation r) {
        switch (r) {
            case Representation::Sparse:
                return "sparse";
            case Representation::RowValues:
                return "row_values";
            case Representation::Dynamic:
                return "dynamic";
            case Representation::Scalar:
                return "scalar";
        }
        return "unknown";
    }

    RepresentationBase& repFor(Representation r) { return *reps_[index(r)]; }
    const RepresentationBase& repFor(Representation r) const { return *reps_[index(r)]; }

    // Shared by addMatchingInPlace(), multiplyMatchingInPlace(), and
    // subtractMagnitudeInPlace(): all three require matching canonical
    // representations, unlike ensure()'s automatic reconciliation.
    void requireMatchingRepresentation(const SmoothNumberBase& other) const {
        if (canonical_ != other.canonical_) {
            throw std::invalid_argument(
                "SmoothNumberBase: representations must match for this operation (convert one to match the "
                "other first)");
        }
    }

    // Shared by addMatchingInPlace() and multiplyMatchingInPlace(): both
    // throw if `other` has a fractional term this type doesn't allow.
    void requireCompatibleFractional(const SmoothNumberBase& other, const char* caller) const {
        if (allowFractional_ || !other.allowFractional_) return;
        bool otherHasFractional = false;
        other.repFor(other.canonical_).forEachSet([&otherHasFractional](int i, int j) {
            if (i < 0 || j < 0) otherHasFractional = true;
        });
        if (otherHasFractional) {
            throw std::invalid_argument(std::string("SmoothNumberBase::") + caller +
                                         ": other has fractional terms but this type doesn't allow them");
        }
    }

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

    // Brings `target` up to date by enumerating the canonical
    // representation's bits and replaying each into `target`, unless
    // `target` is already valid (canonical is always considered up to
    // date).
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

    // ensure(target) followed by a reference to that now-up-to-date
    // representation -- what every read-only "give me representation X"
    // accessor below (printX()/valueAs()/representationAs()) needs.
    RepresentationBase& ensured(Representation target) {
        ensure(target);
        return repFor(target);
    }

    // Wipes the canonical representation back to empty and every other
    // representation back to outdated -- used by setValue() so it fully
    // replaces this number's value rather than adding to it.
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

// There is deliberately no operator+=/add()/operator*=/multiply() here:
// arithmetic never mutates in place. Each concrete type defines its own
// value-returning operator+/operator* as hidden friends (smooth_integer.hpp,
// smooth_float.hpp, signed.hpp), built from a copy plus the protected
// ...MatchingInPlace()/subtractMagnitudeInPlace() above -- a hidden friend,
// rather than a shared free template, because those building blocks are
// protected: a plain free function couldn't reach them, but a friend
// defined inside a derived class can.

}  // namespace smooth
