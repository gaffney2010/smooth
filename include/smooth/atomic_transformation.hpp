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
// independent generating families every OffsetTransformation reduces to:
// Merge/Split encode 2^i + 2^(i+1) = 2^i*3 ("1+2=3"), CornerSplit encodes
// 2^(i-1)*3^j + 2^i*3^(j-1) + 2^(i-1)*3^(j-1) = 2^i*3^j ("1+2+3=6"). Every
// other named OffsetTransformation can be written as a sequence of these --
// see atomize() on each (transformation.hpp).
//
// Unlike a plain OffsetTransformation, an atom's canApply()/
// applyAndReportLandings() are specialized per representation:
//
// - Against RowValuesRepresentation, applying an atom is ordinary
//   arithmetic on the affected column(s)' magnitude (addToColumnValue()),
//   the same way TernaryCarryTransformation works, rather than
//   clearing/carry-setting bit by bit -- which for RowValues would mean
//   repeatedly decoding and re-encoding the same column. Each concrete
//   atom overrides applyRowValuesAndReportLandings() with its own deltas.
// - Against ScalarRepresentation, atoms don't apply at all: no
//   independent per-(i,j) structure to rewrite, so both throw
//   std::invalid_argument unconditionally.
// - Against anything else (Sparse, Dynamic), the ordinary get()/set()
//   logic is exactly right, so both fall back to it.
//
// This is what makes OffsetTransformation::atomize()'s `viaAtoms` path
// able to "hyper-optimize" a composite transformation: its own direct
// apply is unchanged, but applying *through* atomize() dispatches each
// step to whichever representation-specific implementation fits.
//
// canApply() isn't specialized beyond the Scalar check -- get() is
// already just as cheap for RowValues as anywhere else.
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
// AtomicTransformation's full definition, just above.
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
