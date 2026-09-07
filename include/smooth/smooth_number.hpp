#pragma once

#include <array>
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "smooth/matrix_representation.hpp"
#include "smooth/representation_base.hpp"
#include "smooth/row_values_representation.hpp"
#include "smooth/sparse_representation.hpp"

namespace smooth {

// Represents a 3-smooth number (a number of the form 2^i * 3^j summed over a
// set of (i, j) pairs). Row index i is the power of 2, column index j is the
// power of 3. By default both start at 0 (whole numbers only). Constructing
// with negative capacities also allows i and j to go negative, letting the
// number represent fractional values (e.g. i = -1 contributes a factor of
// 1/2).
//
// Internally, the same logical bit grid can be held in more than one
// representation (see representation_base.hpp and its implementations:
// MatrixRepresentation, SparseRepresentation, RowValuesRepresentation).
// Only one representation is canonical at a time -- it is the trusted
// source of truth. The others are lazily (re)derived from it on demand and
// are otherwise considered outdated/un-built. This class talks to
// representations purely through the RepresentationBase interface, so
// adding a new one means: adding an enumerator to Representation, adding a
// slot to the construction/registration in the constructor below, and
// writing the new class -- no other existing logic needs to change.
class SmoothNumber {
public:
    enum class Representation { Matrix, Sparse, RowValues };

    // Whole-number-only matrix: i in [0, max_rows), j in [0, max_cols).
    SmoothNumber(std::size_t max_rows, std::size_t max_cols)
        : SmoothNumber(max_rows, max_cols, 0, 0) {}

    // General matrix: i in [-neg_rows, max_rows), j in [-neg_cols, max_cols).
    SmoothNumber(std::size_t max_rows, std::size_t max_cols, std::size_t neg_rows, std::size_t neg_cols)
        : rows_(max_rows), cols_(max_cols), negRows_(neg_rows), negCols_(neg_cols), canonical_(Representation::Matrix) {
        reps_[index(Representation::Matrix)] =
            std::make_unique<MatrixRepresentation>(max_rows, max_cols, neg_rows, neg_cols);
        reps_[index(Representation::Sparse)] =
            std::make_unique<SparseRepresentation>(max_rows, max_cols, neg_rows, neg_cols);
        reps_[index(Representation::RowValues)] =
            std::make_unique<RowValuesRepresentation>(max_rows, max_cols, neg_rows, neg_cols);
        valid_[index(Representation::Matrix)] = true;
    }

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    std::size_t negRows() const { return negRows_; }
    std::size_t negCols() const { return negCols_; }

    // Which representation is currently canonical (trusted). Which one that
    // is, and when (if ever) that changes, is an internal decision -- there
    // is no public way to force it.
    Representation canonical() const { return canonical_; }

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

    // Converts to Matrix if needed, then prints it.
    void printMatrix(std::ostream& os = std::cout) {
        ensure(Representation::Matrix);
        repFor(Representation::Matrix).print(os);
    }

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

    int firstRow() const { return -static_cast<int>(negRows_); }
    int firstCol() const { return -static_cast<int>(negCols_); }

    void checkBounds(int i, int j) const {
        if (i < firstRow() || i >= static_cast<int>(rows_) || j < firstCol() ||
            j >= static_cast<int>(cols_)) {
            throw std::out_of_range("SmoothNumber: index out of range");
        }
    }

    bool isValid(Representation r) const { return valid_[index(r)]; }
    void markValid(Representation r) { valid_[index(r)] = true; }

    void invalidateAllExcept(Representation keep) {
        for (std::size_t k = 0; k < kRepresentationCount; ++k) {
            if (static_cast<Representation>(k) != keep) valid_[k] = false;
        }
    }

    // Brings `target` up to date by rebuilding it, cell by cell, from the
    // canonical representation -- unless it's already up to date (canonical
    // is always considered up to date, and an already-valid representation
    // is never redundantly reconverted).
    void ensure(Representation target) {
        if (target == canonical_ || isValid(target)) return;
        RepresentationBase& dst = repFor(target);
        const RepresentationBase& src = repFor(canonical_);
        dst.reset();
        for (int i = firstRow(); i < static_cast<int>(rows_); ++i) {
            for (int j = firstCol(); j < static_cast<int>(cols_); ++j) {
                if (src.get(i, j)) dst.set(i, j, true);
            }
        }
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

    std::size_t rows_;
    std::size_t cols_;
    std::size_t negRows_;
    std::size_t negCols_;

    std::array<std::unique_ptr<RepresentationBase>, kRepresentationCount> reps_;
    std::array<bool, kRepresentationCount> valid_{};
    Representation canonical_;
};

}  // namespace smooth
