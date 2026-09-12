#pragma once

#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "smooth/representation_zoo/row_values_representation.hpp"
#include "smooth/representation_zoo/scalar_representation.hpp"
#include "smooth/transformation.hpp"

namespace smooth {

// Tags MergeTransformation, SplitTransformation, and
// CornerSplitTransformation (transformation_zoo/) as "atoms" -- the two
// independent generating families every OffsetTransformation in this
// library reduces to: Merge/Split encode 2^i + 2^(i+1) = 2^i*3 ("1+2=3"),
// CornerSplit encodes 2^(i-1)*3^j + 2^i*3^(j-1) + 2^(i-1)*3^(j-1) =
// 2^i*3^j ("1+2+3=6"). Every other named OffsetTransformation
// (SpreadTransformation, RowSpreadTransformation) can be written as some
// sequence of these -- see atomize() on each (transformation.hpp).
//
// Unlike a plain OffsetTransformation, an atom's canApply()/
// applyAndReportLandings() are specialized per representation, rather than
// going through the generic get()/set() bit grid interface for everything:
//
// - Against a RowValuesRepresentation, applying an atom is ordinary
//   arithmetic on the affected column(s)' own magnitudes -- add/subtract
//   the net delta directly (addToColumnValue()), the same way
//   TernaryCarryTransformation (transformation_zoo/ternary_carry_transformation.hpp)
//   already works -- rather than clearing/carry-setting bit by bit through
//   get()/set(), which for RowValues would mean repeatedly decoding and
//   re-encoding the very same column. Each concrete atom below overrides
//   applyRowValuesAndReportLandings() with its own deltas.
// - Against a ScalarRepresentation, atoms don't apply at all: it holds the
//   whole number as one plain value with no independent per-(i, j)
//   structure to rewrite a handful of cells within, so both canApply() and
//   applyAndReportLandings() throw std::invalid_argument unconditionally.
// - Against anything else (Sparse, Dynamic, or some future representation),
//   the ordinary get()/set() bit-grid logic (OffsetTransformation's own
//   template, instantiated with Bits = RepresentationBase) is exactly
//   right, so canApply() and the non-RowValues branch of
//   applyAndReportLandings() just fall back to it.
//
// This is what makes OffsetTransformation::atomize()'s `viaAtoms` apply
// path (transformation.hpp) actually able to "hyper-optimize" a composite
// transformation: SpreadTransformation/RowSpreadTransformation's own direct
// apply still goes through the generic bit-grid path (unchanged), but
// applying either *through* atomize() dispatches each step to whichever of
// these representation-specific implementations actually fits.
//
// canApply() itself is not specialized per representation -- get() is
// already exactly as cheap for RowValues as for any other representation
// (a single decode), so there's nothing to gain by special-casing it; only
// applyAndReportLandings() benefits from skipping the bit-by-bit dance.
class AtomicTransformation : public OffsetTransformation {
public:
    using OffsetTransformation::OffsetTransformation;
    using OffsetTransformation::apply;
    using OffsetTransformation::applyAndReportLandings;
    using OffsetTransformation::canApply;

    bool canApply(const RepresentationBase& rep, int i, int j) const override {
        requireNotScalar(rep);
        return OffsetTransformation::canApply(rep, i, j);
    }

    std::vector<std::pair<int, int>> applyAndReportLandings(RepresentationBase& rep, int i, int j) const override {
        requireNotScalar(rep);
        if (auto* rowValues = dynamic_cast<RowValuesRepresentation*>(&rep)) {
            return applyRowValuesAndReportLandings(*rowValues, i, j);
        }
        return OffsetTransformation::applyAndReportLandings(rep, i, j);
    }

protected:
    // This atom's own column-magnitude arithmetic against a
    // RowValuesRepresentation directly -- see each concrete atom's own
    // override (transformation_zoo/) for its specific deltas.
    virtual std::vector<std::pair<int, int>> applyRowValuesAndReportLandings(RowValuesRepresentation& rep, int i,
                                                                              int j) const = 0;

private:
    static void requireNotScalar(const RepresentationBase& rep) {
        if (dynamic_cast<const ScalarRepresentation*>(&rep)) {
            throw std::invalid_argument(
                "AtomicTransformation: atoms don't support ScalarRepresentation directly -- convert to another "
                "representation first");
        }
    }
};

// Defined here (not in transformation.hpp) since it needs
// AtomicTransformation's full definition, just above -- see AtomApplication
// and OffsetTransformation::applyAndReportLandings()'s own `viaAtoms`
// overload in transformation.hpp for why that declaration had to be split
// from this definition in the first place.
inline std::vector<std::pair<int, int>> OffsetTransformation::applyAndReportLandings(RepresentationBase& rep, int i,
                                                                                      int j, bool viaAtoms) const {
    if (!viaAtoms) return applyAndReportLandings(rep, i, j);
    std::vector<std::pair<int, int>> landings;
    for (const auto& application : atomize(i, j)) {
        auto atomLandings = application.atom->applyAndReportLandings(rep, application.i, application.j);
        landings.insert(landings.end(), atomLandings.begin(), atomLandings.end());
    }
    return landings;
}

}  // namespace smooth
