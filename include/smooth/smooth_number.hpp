#pragma once

#include <array>
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "smooth/dynamic_matrix_representation.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/row_values_representation.hpp"
#include "smooth/sparse_representation.hpp"

namespace smooth {

// Represents a 3-smooth number (a number of the form 2^i * 3^j summed over a
// set of (i, j) pairs). Row index i is the power of 2, column index j is the
// power of 3.
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
class SmoothNumber {
public:
    enum class Representation { Sparse, RowValues, Dynamic };

    // Whole numbers only: i and j must be non-negative.
    SmoothNumber() : SmoothNumber(false) {}

    // allow_fractional lets i and j go negative, so the number can
    // represent fractional values (e.g. i = -1 contributes a factor of
    // 1/2). This has to be decided up front because it's the one thing no
    // representation can discover on its own: it affects how RowValues
    // formats and decodes its numbers, and it's the only structural
    // restriction any representation still enforces.
    explicit SmoothNumber(bool allow_fractional)
        : allowFractional_(allow_fractional), canonical_(Representation::Dynamic) {
        reps_[index(Representation::Sparse)] = std::make_unique<SparseRepresentation>(allow_fractional);
        reps_[index(Representation::RowValues)] = std::make_unique<RowValuesRepresentation>(allow_fractional);
        reps_[index(Representation::Dynamic)] = std::make_unique<DynamicMatrixRepresentation>(allow_fractional);
        valid_[index(Representation::Dynamic)] = true;
    }

    bool allowsFractional() const { return allowFractional_; }

    // Which representation is currently canonical (trusted). Which one that
    // is, and when (if ever) that changes, is an internal decision -- there
    // is no public way to force it.
    Representation canonical() const { return canonical_; }

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
    // row/column counts or deeply negative indices.
    double value() const { return repFor(canonical_).value(); }

private:
    static constexpr std::size_t kRepresentationCount = 3;

    static std::size_t index(Representation r) { return static_cast<std::size_t>(r); }

    RepresentationBase& repFor(Representation r) { return *reps_[index(r)]; }
    const RepresentationBase& repFor(Representation r) const { return *reps_[index(r)]; }

    void checkBounds(int i, int j) const {
        if (!allowFractional_ && (i < 0 || j < 0)) {
            throw std::out_of_range("SmoothNumber: negative index requires allow_fractional");
        }
        if (boundsSet_) {
            if (i < -static_cast<int>(negRowBound_) || i >= static_cast<int>(rowBound_) ||
                j < -static_cast<int>(negColBound_) || j >= static_cast<int>(colBound_)) {
                throw std::out_of_range("SmoothNumber: index outside the bounds set via setBounds()");
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

    bool allowFractional_;

    bool boundsSet_ = false;
    std::size_t rowBound_ = 0;
    std::size_t colBound_ = 0;
    std::size_t negRowBound_ = 0;
    std::size_t negColBound_ = 0;

    std::array<std::unique_ptr<RepresentationBase>, kRepresentationCount> reps_;
    std::array<bool, kRepresentationCount> valid_{};
    Representation canonical_;
};

}  // namespace smooth
