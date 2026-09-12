#pragma once

#include <memory>

#include "smooth/transformation.hpp"

namespace smooth {

// Tags MergeTransformation, SplitTransformation, and
// CornerSplitTransformation (transformation_zoo/) as "atoms" -- the two
// independent generating families every OffsetTransformation in this
// library reduces to: Merge/Split encode 2^i + 2^(i+1) = 2^i*3 ("1+2=3"),
// CornerSplit encodes 2^(i-1)*3^j + 2^i*3^(j-1) + 2^(i-1)*3^(j-1) =
// 2^i*3^j ("1+2+3=6"). Every other named OffsetTransformation
// (SpreadTransformation, RowSpreadTransformation) can be written as some
// sequence of these -- see atomize() on each.
//
// Purely a label: no behavior beyond OffsetTransformation itself, and no
// new virtual methods -- an AtomicTransformation is still just an
// OffsetTransformation as far as canApply()/applyAndReportLandings() are
// concerned. Distinguishing atoms from composites is only interesting at
// the type level (this class exists at all) and via atomize()'s return
// type (AtomApplication holds one by pointer, below).
class AtomicTransformation : public OffsetTransformation {
public:
    using OffsetTransformation::OffsetTransformation;
};

// One atom, anchored at a specific (i, j) -- what atomize() returns a
// sequence of. `atom` is heap-allocated (rather than, say, a variant over
// the three concrete atom classes) because atomize() needs to hand back a
// genuinely heterogeneous sequence -- a RowSpreadTransformation's
// atomization mixes CornerSplitTransformation and MergeTransformation
// steps -- and every atom is itself stateless and cheap to share, so a
// shared_ptr costs nothing beyond the one allocation per call.
struct AtomApplication {
    std::shared_ptr<const AtomicTransformation> atom;
    int i;
    int j;
};

}  // namespace smooth
