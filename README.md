# smooth

A little library to play with 3-smooth numbers.

A 3-smooth number is one whose only prime factors are 2 and 3, i.e. it can be
written as a sum of terms of the form `2^i * 3^j`. This library represents
such a number as a set of `(i, j)` bits: `(i, j)` being set means the term
`2^i * 3^j` is included in the number. The number's value is the sum of
`2^i * 3^j` over every set bit.

## Four concrete types

```cpp
#include "smooth/smooth.hpp"  // pulls in all four

smooth::SmoothInteger        // whole numbers only, always >= 0
smooth::SmoothFloat          // fractional terms allowed, always >= 0
smooth::SmoothSignedInteger  // whole numbers only, may be negative
smooth::SmoothSignedFloat    // fractional terms allowed, may be negative
```

Whether a number allows fractional (negative-index) terms and whether it
can be negative are both fixed by which class you pick, not by a runtime
flag. All four are default-constructible, need no capacity declared up
front, and take an optional `std::shared_ptr<Metrics>` as their one
constructor argument (see "Metrics" below).

Every type shares `get`/`set`/`clear`/`value`/`print*`/`setBounds`/
`operator+`/`operator*` (see `SmoothNumberBase` and "Addition"/
"Multiplication" below); the two signed types add sign-related methods on
top (see "Signed types"). All four are copyable (deep-cloning the
underlying representation) and movable.

### `SmoothNumberBase`

Header-only class in `include/smooth/smooth_number_base.hpp`, the shared
engine behind all four concrete types above (not meant to be constructed
directly — its constructor is `protected`). It owns several internal
representation objects (see below) and delegates to whichever one is
canonical through the `RepresentationBase` interface
(`include/smooth/representation_base.hpp`).

- `set(i, j, value = true)` / `clear(i, j)` / `get(i, j)` — `i`/`j` are
  signed so negative indices can be addressed on a fractional type.
- `value()` — sum of `2^i * 3^j` over all set bits, as a `double`. Uses
  whichever strategy suits the currently canonical representation (below)
  rather than always walking a full grid. Not virtual — see "Signed types"
  for why.
- `allowsFractional()` — whether this type permits negative indices.
- `setValue(long long)` / `setValue(double)` — replaces the number's value
  entirely, by putting it directly into row `j = 0` via the canonical
  representation's `setColumnValue(0, v)` (since `3^0 = 1`). Throws
  `std::invalid_argument` for a negative value on an unsigned type, or a
  fractional value on a non-fractional type.

A negative index on a non-fractional type throws `std::out_of_range`.

### `setBounds` — an optional, non-binding sanity check

```cpp
void setBounds(std::size_t max_rows, std::size_t max_cols,
               std::size_t neg_rows, std::size_t neg_cols);
```

Every representation is unbounded or grows to fit, so there's nothing to
preallocate. `setBounds` is a pure sanity check: once called, any future
`set()`/`get()` outside `[-neg_rows, max_rows) x [-neg_cols, max_cols)`
throws `std::out_of_range`. It reserves nothing and doesn't retroactively
validate anything already set.

### Internal representations

The same number can be held in one of several internal representations,
each implementing `RepresentationBase` (`representation_base.hpp`) and
living in `include/smooth/representation_zoo/`
(`representation_zoo.hpp` pulls in all four):

- **Sparse** — the `std::set` of `(i, j)` coordinates whose bit is set.
- **RowValues** — one number `n_j` per column `j` that has anything set,
  where `n_j = sum_i (bit(i,j) ? 2^i : 0)`, so the total value is
  `sum_j n_j * 3^j`. Stored as a `std::map<int, double>` keyed by `j`
  (dropped once it returns to zero). `n_j` is an integer unless the type
  allows fractional terms.
- **Dynamic** — a bit grid that starts at 0x0 and doubles in whichever
  direction (more positive/negative rows or columns) ran out, whenever
  `set()` reaches outside the currently allocated range.
- **Scalar** — a single plain value (every term lives in column `j = 0`,
  so its value is just that one number). Still a `RepresentationBase`,
  purely so it can convert to and from the others; `set()`/
  `setColumnValue()` on any column but 0 throws `std::invalid_argument`,
  as does converting a number with a genuine multi-column value into it.

Exactly one representation is **canonical** (`Dynamic` by default) — the
trusted, up-to-date copy that `set`/`get`/`value()` operate through. A
`set()` that actually changes a bit invalidates the others; which
representation is canonical, and when that changes, is an internal
decision with no public override.

- `canonical()` — which `Representation` is currently canonical.
- `printSparse()`/`printRowValues()`/`printDynamic()`/`printScalar()` —
  convert (if needed) and print. `printDynamic()` marks the
  fractional/whole-number boundary with a line; `printScalar()` throws
  `std::invalid_argument` if the value isn't representable as a plain
  number.
- `valueAs(Representation target)` — the same "convert if needed" logic,
  generalized to any representation and handed back as a `double`.
- `representationAs(Representation target)` — the same idea, but hands
  back an independent `std::unique_ptr<RepresentationBase>` clone instead
  of just a value — what `Plan::numberVia()` (see "Plan" below) uses to
  force a fed-in number through a real conversion.

`value()` calls straight through to the canonical representation's own
strategy: Sparse sums only its set coordinates; RowValues does one pass
over its (typically few) columns; Dynamic walks its own allocated
capacity; Scalar just returns its one stored number.

Converting one representation from another goes through `forEachSet(fn)`,
which asks the source to invoke `fn(i, j)` once per set bit, however is
natural for its own storage — this is what makes conversion possible
without any global bounds. `setColumnValue(j, n)` is the same
per-representation-strategy idea for *writing* a whole column at once
(used by `setValue()`): Sparse/Dynamic decompose `n` into bits via the
shared `decomposeColumnValue()` helper; RowValues just assigns `n`
directly, in O(1).

This is a Strategy pattern: `SmoothNumberBase` only ever talks to
representations through the `RepresentationBase` interface, so adding a
new one means writing one class, adding an enumerator, and registering it
— no existing logic changes. `clone()` is what lets a `SmoothNumberBase`
be copied without knowing which concrete representation types exist.

## Addition

Addition is purely value-returning — no `add()`, no `operator+=`, only
`operator+`, which never mutates either operand. Each concrete type
defines its own, as a **hidden friend** (needed since the in-place
building block it calls, `SmoothNumberBase::addMatchingInPlace()`, is
`protected` — a plain free function couldn't reach it, but a friend
defined inside a derived class can, through an object of that type):

```cpp
friend SmoothInteger operator+(SmoothInteger a, const SmoothInteger& b);
friend SmoothFloat operator+(SmoothFloat a, const SmoothFloat& b);
friend Signed<Base> operator+(Signed<Base> a, const Signed<Base>& b);  // both signed types
```

**Representations must match.** `addMatchingInPlace()` throws
`std::invalid_argument` unless `a.canonical() == b.canonical()` — there's
no implicit reconciliation the way `ensure()` provides elsewhere. In
practice every freshly constructed number starts out (and stays)
canonical on `Dynamic`, so this only matters once representation choice
becomes more dynamic.

**Unsigned types'** `addMatchingInPlace()` dispatches to the canonical
representation's `addInPlace(other)`: Sparse/Dynamic walk `other`'s bits
and carry (moving a second 1 into an occupied cell up to the next row,
via the shared `addBitsWithCarry()` helper); RowValues just adds column
totals directly, needing no explicit carry handling at all; Scalar adds
the two stored values directly, or decomposes `other`'s bits (throwing if
any fall outside column 0).

**Signed types** need actual signed arithmetic, since the bit grid is
magnitude-only and the sign lives in `Signed<Base>`'s own flag:
`Signed<Base>::operator+` combines magnitudes directly when signs match,
and otherwise subtracts the smaller magnitude from the larger and takes
the larger operand's sign (zero is normalized to non-negative), via a
second protected primitive, `subtractMagnitudeInPlace()`. That
subtraction has no natural per-representation variation, so instead of
dispatching through `RepresentationBase` it converts both operands to
per-column totals, subtracts column by column, and resolves any negative
column by borrowing from the next one up (`3^(j+1) = 3 * 3^j`).

## Multiplication

`operator*` gets the same treatment as `operator+` — value-returning,
hidden-friend, matching-representation — so this section only covers what
differs: `(2^i1*3^j1) * (2^i2*3^j2) = 2^(i1+i2) * 3^(j1+j2)`, so
multiplying means pairing up every term of one operand with every term of
the other and adding exponents.
`SmoothNumberBase::multiplyMatchingInPlace()` dispatches to the canonical
representation's `multiplyInPlace(other)`:

- Sparse/Dynamic form every pairwise sum of exponents and carry each one
  in, via the shared `multiplyBitsWithCarry()` helper (capturing both
  operands' terms up front, so squaring is safe).
- RowValues convolves column totals instead — the same operation as
  multiplying two polynomials in the variable 3: the product's column
  `j1 + j2` gets `n_j1 * n_j2` added in, for every pair of columns.
- Scalar multiplies its stored value directly when `other` is also
  Scalar; otherwise it throws unless every one of `other`'s columns is 0
  — except when this Scalar's own value is exactly 0, since `0 * anything
  = 0` regardless of shape.

Signed multiplication is simpler than signed addition: no subtraction, the
result's sign is just the usual sign-XOR rule, with a zero product
normalized the same way a zero sum is.

## Transformation

```cpp
#include "smooth/transformation.hpp"

// The shared interface every transformation in this library implements:
class Transformation {
public:
    virtual bool canApply(const RepresentationBase& rep, int i, int j) const = 0;
    virtual std::vector<std::pair<int, int>> applyAndReportLandings(RepresentationBase& rep, int i, int j) const = 0;
    virtual std::vector<std::pair<int, int>> affectedAnchors(int i, int j) const = 0;
};
```

A `Transformation` is any distinct unit of work that doesn't change a
3-smooth number's value: check whether it can fire at an anchor `(i, j)`,
apply it, and report which cells changed — its "landings," the only cells
worth re-examining afterward. `Reduction` (below) is the other half:
repeated application of one or more `Transformation`s until some condition
is met.

This is deliberately the smallest interface that covers every
transformation here, because "distinct unit of work" covers genuinely
different shapes: most transformations fit one very specific shape (a
fixed list of input offsets, all `1`, cleared by applying; a fixed list of
output offsets, carry-set to `1`) — `OffsetTransformation` (below) is
exactly that shape. `TernaryCarryTransformation` (`reduction_zoo` below)
is the case that doesn't fit it at all: "subtract 3 from column `j`, add 1
to column `j+1`" has no fixed set of bit offsets, since its precondition
depends on a whole column's aggregate magnitude, not a handful of cells.

### OffsetTransformation

```cpp
#include "smooth/smooth.hpp"
#include "smooth/transformation_zoo.hpp"  // for MergeTransformation/SplitTransformation

smooth::SmoothInteger n;
n.set(2, 0);  // 2^2 = 4
n.set(3, 0);  // 2^3 = 8

smooth::MergeTransformation merge;
n.applyTransformation(merge, 2, 0);
n.printSparse();  // {(2, 1)}  -- still 12, just represented differently
```

The same value can be held by more than one bit grid: since
`2^i*3^j + 2^(i+1)*3^j = 2^i*3^(j+1)`, the two bits at `(i, j)` and
`(i+1, j)` can be traded for the single bit at `(i, j+1)` without changing
`value()`. `OffsetTransformation` implements `Transformation` with one
such value-preserving trade, anchored at `(i, j)`, built from two fixed
offset lists:

- **input** offsets — each must currently hold a `1`; applying always
  clears them.
- **output** offsets — each gets set to `1`, ripple-carrying up the row
  axis (the same carry ordinary addition uses) if already occupied, rather
  than blocking the transformation.

So `canApply()` only ever needs to check the inputs. `OffsetTransformation`
also keeps a templated `canApply()`/`apply()`/`applyAndReportLandings()`
over anything that looks like a bit grid (`get(int,int)`/`set(int,int,bool)`),
so the same object works directly on a `SmoothNumberBase` or a bare
`RepresentationBase`.

`SmoothNumberBase::applyTransformation(const OffsetTransformation& t, int i, int j, bool atomize = false)`
is the usual way to use one directly on a number: checks `canApply()`,
throwing `std::invalid_argument` if it fails, then applies — directly
against whichever representation is currently canonical (not through this
class's own templated `get()`/`set()`), invalidating the others once
afterward instead of per bit. This is what lets a per-representation-
specialized atom (see "Per-representation atom dispatch" below) actually
reach its specialized behavior through this everyday call. `atomize`
(default `false`) is threaded through to `apply()`'s own `viaAtoms`
parameter — see "Applying a transformation via its atoms" below.

`OffsetTransformation` is deliberately **concrete, not itself an
interface**. Building a custom one is just handing its constructor the two
offset lists:

```cpp
// 2^i*3^j + 2^(i+1)*3^j + 2^i*3^(j+1) = 2^i*3^j*6 = 2^(i+1)*3^(j+1)
smooth::OffsetTransformation combineBothAxes({{0, 0}, {1, 0}, {0, 1}}, {{1, 1}});
```

`inputs()`/`outputs()` hand back those two lists directly — enough to
check a transformation's own well-formedness (that summing `2^i*3^j` over
inputs matches outputs) without ever calling `apply()`;
`checkTransformationPreservesValue()` in the test suite does exactly this
for every preset below.

Presets live in `include/smooth/transformation_zoo/`, each just an
`OffsetTransformation` built from a fixed offset pair
(`transformation_zoo.hpp` pulls in all of them):

- `MergeTransformation` — inputs `{(0,0),(1,0)}`, output `{(0,1)}`.
- `SplitTransformation` — the exact reverse.
- `SpreadTransformation(int n)` — bridges `(i,j)` and `(i,j+n)` into
  `(i+2,j)` plus a staircase at `(i+1,j+1)..(i+1,j+n-1)`
  (`2^i*3^j + 2^i*3^(j+n) = 2^(i+2)*3^j + sum_{k=1}^{n-1} 2^(i+1)*3^(j+k)`).
  `n = 1` has an empty staircase, reducing to `MergeTransformation` applied
  twice into the same cell. Throws for `n < 1`.
- `CornerSplitTransformation` — input `{(0,0)}`, outputs
  `{(-1,0),(0,-1),(-1,-1)}`
  (`2^(i-1)*3^j + 2^i*3^(j-1) + 2^(i-1)*3^(j-1) = 2^i*3^j`). All three
  outputs land at negative offsets, so anchoring at `(0,0)` requires a
  fractional type.
- `RowSpreadTransformation(int n)` — the row-axis counterpart to
  `SpreadTransformation`: bridges `(i,j)` and `(i+n,j)` into `(i,j+1)`
  plus a staircase at `(i+1,j)..(i+n-1,j)`. `n = 1` matches
  `MergeTransformation` exactly. Throws for `n < 1`.
- `TernaryCarryTransformation` — implements `Transformation` directly, not
  through `OffsetTransformation`; see "reduction_zoo" below.

Applying `MergeTransformation` then `SplitTransformation` is always a
round trip back to the original layout, even through a carry. Every
offset-based preset works the same way for negative `i`/`j` on a
fractional type as for non-negative ones.

### AtomicTransformation and atomize()

```cpp
#include "smooth/smooth.hpp"          // AtomicTransformation, AtomApplication
#include "smooth/transformation_zoo.hpp"

smooth::RowSpreadTransformation rowSpread(6);
std::vector<smooth::AtomApplication> atoms = rowSpread.atomize(0, 0);
for (const auto& app : atoms) {
    app.atom->applyAndReportLandings(rep, app.i, app.j);  // rep: a RepresentationBase
}
```

Every `OffsetTransformation` here reduces to exactly two independent
generating families (found by treating each as the integer identity its
inputs/outputs encode, and searching by BFS for which are/aren't reachable
from which others by composition):

- **Merge/Split**, encoding `2^i + 2^(i+1) = 2^i*3` ("1 + 2 = 3").
- **CornerSplit** (and its unbuilt reverse), encoding
  `2^(i-1)*3^j + 2^i*3^(j-1) + 2^(i-1)*3^(j-1) = 2^i*3^j` ("1 + 2 + 3 = 6").

Neither is reachable from the other (a single `Split` fully vacates its
source column, and getting back into it requires a fully-consuming
`Merge`, so `CornerSplit`'s simultaneous two-column, three-bit output
can't arise from `Merge`/`Split` alone). `MergeTransformation`,
`SplitTransformation`, and `CornerSplitTransformation` are tagged as these
families' **atoms** by inheriting from `AtomicTransformation`
(`atomic_transformation.hpp`) instead of `OffsetTransformation` directly —
purely a label, no added behavior.

Every named `OffsetTransformation` — atom or not — has an
`atomize(int i, int j) const` returning `std::vector<AtomApplication>`,
each an atom paired with the `(i, j)` anchor to apply it at. For an atom,
this is trivial (itself, one step). For the composites:

- `SpreadTransformation(n)` — exactly `n` `SplitTransformation`
  applications walking the far input down one column at a time. Pure
  Family A — no `CornerSplitTransformation` needed.
- `RowSpreadTransformation(n)` — the hard case, since it genuinely needs
  `CornerSplitTransformation`. `n = 1` is `MergeTransformation` itself;
  `n = 2` is `CornerSplit(i+2,j)` followed by two merges. For `n >= 3`,
  merging the corner split's leftover pair right away would carry
  straight back and undo the split, so each level corner-splits the
  leftover's own far cell first, pushing the collision one column down.
  Costs `4n - 5` atoms for `n >= 2` (`1` for `n = 1`) — linear, verified
  computationally before being written into C++.

`atomize()`'s correctness criterion is stronger than "preserves value":
applying a transformation directly, and applying its `atomize()`d
sequence, to two copies of the same starting representation, must land on
*exactly* the same set bits, carries and all —
`checkAtomizeMatches()` in the test suite verifies this for every preset.

### Per-representation atom dispatch

```cpp
smooth::RowValuesRepresentation rep(/*allow_fractional=*/false);
rep.setColumnValue(0, 3.0);  // bits 0, 1 -- 3 = 1 + 2
smooth::RepresentationBase& base = rep;  // the dispatch below needs this exact static type

smooth::MergeTransformation().applyAndReportLandings(base, 0, 0);
rep.columnValue(0);  // 0.0
rep.columnValue(1);  // 1.0

smooth::ScalarRepresentation scalar(/*allow_fractional=*/false);
scalar.setColumnValue(0, 3.0);
smooth::RepresentationBase& scalarBase = scalar;
smooth::MergeTransformation().applyAndReportLandings(scalarBase, 0, 0);  // throws std::invalid_argument
```

An atom's `canApply()`/`applyAndReportLandings()` (`AtomicTransformation`)
are specialized per representation, rather than going through the generic
get()/set() bit-grid logic every other `OffsetTransformation` uses:

- Against `RowValuesRepresentation`, applying an atom is ordinary
  arithmetic on the affected column(s)' magnitude (`addToColumnValue()`,
  the same style `TernaryCarryTransformation` uses) instead of clearing
  and carry-setting bit by bit. `MergeTransformation` subtracts `3*2^i`
  from column `j` and adds `2^i` to column `j+1`; `SplitTransformation` is
  the reverse; `CornerSplitTransformation` (touching two columns at once)
  subtracts `2^(i-1)` from column `j` and adds `3*2^(i-1)` to column `j-1`.
- Against `ScalarRepresentation`, atoms don't apply at all — no
  independent per-`(i,j)` structure to rewrite — so both throw
  `std::invalid_argument` unconditionally.
- Against anything else (Sparse, Dynamic), the ordinary get()/set() logic
  is exactly right, unchanged from before this dispatch existed.

`canApply()` isn't specialized beyond the `ScalarRepresentation` check —
`get()` is already just as cheap for RowValues as anywhere else.

This dispatch only fires when an atom is called through an actual
`RepresentationBase&` — the way `TransformationReduction`/
`StaircaseReduction`/`TernaryFormReduction` and
`SmoothNumberBase::applyTransformation()` all already call things. Calling
an atom's methods directly on a *concrete* representation variable, or on
a `SmoothNumberBase`, instead resolves to the inherited generic
`Bits`-templated overload — unaffected, and why every existing atom usage
elsewhere still behaves exactly as before this dispatch was added.

### Applying a transformation via its atoms

```cpp
smooth::RowSpreadTransformation rowSpread(6);

smooth::SmoothInteger n;
n.set(0, 0);
n.set(6, 0);
n.applyTransformation(rowSpread, 0, 0, /*atomize=*/true);
```

`OffsetTransformation::applyAndReportLandings(RepresentationBase& rep, int i, int j, bool viaAtoms)`
(and the matching `apply()`) is the same as the three-argument version,
except that when `viaAtoms` is `true`, it decomposes `*this` via
`atomize()` and applies each atom in sequence instead of running this
transformation's own direct logic. Since every atom's own
`applyAndReportLandings()` is specialized per representation (above),
this is the hook for hyper-optimizing a composite transformation later:
swap in a faster `atomize()` or faster atoms, and every caller opting in
via `viaAtoms=true` gets it for free.

`SmoothNumberBase::applyTransformation()` takes the same `atomize` flag
(default `false`).

## Reduction

```cpp
#include "smooth/reduction.hpp"

// The shared interface every reduction in this library implements:
class Reduction {
public:
    virtual const std::string& name() const = 0;
    virtual void run(RepresentationBase& rep, const std::shared_ptr<Metrics>& metrics = nullptr) const = 0;
};
```

Every reduction lives in `reduction_zoo/` (below) and implements this
interface one of two ways: `TransformationReduction`, the generic engine
(greedily apply a family of `Transformation`s until none can fire), or by
being a subclass of *that* (`MergeReduction`/`BinaryFormReduction`/
`TernaryCarryReduction`, each just fixing its own `Transformation`(s),
name, and bound). `TernaryFormReduction` and `StaircaseReduction` are the
exceptions: each needs its own bespoke search (which transformation to
apply next, and where, depends on current state in a way a fixed list
can't express), so both implement `Reduction` directly.

This is what lets `Plan`'s `Reduce` blueprint node (see "Plan" below) hold
a single `const Reduction*` rather than being hardwired to one
implementation. Every named reduction hardcodes its own `name()` — only
`TransformationReduction` itself takes one explicitly, since it's the
generic mechanism, not a preset.

## reduction_zoo

`include/smooth/reduction_zoo/` holds every concrete `Reduction`:
`TransformationReduction` plus the presets built from it (`MergeReduction`,
`BinaryFormReduction`, `TernaryCarryReduction`) or implementing `Reduction`
directly (`TernaryFormReduction`, `StaircaseReduction`).
`reduction_zoo.hpp` pulls in all six.

### TransformationReduction

```cpp
#include "smooth/reduction_zoo/transformation_reduction.hpp"
#include "smooth/representation_zoo.hpp"
#include "smooth/transformation_zoo.hpp"

smooth::MergeTransformation merge;
smooth::TransformationReduction reduction({&merge}, "merge");

smooth::SparseRepresentation rep(/*allow_fractional=*/true);
rep.setColumnValue(0, 15);  // 15 = 1111 binary -> 4 set bits
reduction.run(rep);
rep.print();  // {(0, 1), (2, 1)}  -- down to 2 bits, still worth 15
```

Implements `Reduction` by greedily applying a family of `Transformation`s
across a `RepresentationBase` until none can fire anywhere: a fixed point.
Its list is a `vector<const Transformation*>`, so different kinds of
`Transformation` can even be mixed into one reduction.

Whether that fixed point is reached depends on the family:
`{MergeTransformation}` always terminates on its own (every application
strictly reduces total set-bit count, which can't go negative);
`{TernaryCarryTransformation}` also terminates, for a different reason
(see its own section). But `{SplitTransformation}` alone has no floor —
nothing about it knows column 0 is special, so it splits forever into
negative columns. The optional trailing constructor parameter
`allowed(int i, int j)` bounds the region a search may explore, fixed for
the reduction's lifetime — see `BinaryFormReduction` below.

This works without rescanning the whole representation after every
application: each transformation's own `affectedAnchors(int i, int j)`
says exactly which anchors a changed cell could newly affect, so `run()`
seeds a worklist from `rep`'s own set bits and only re-examines what
`applyAndReportLandings()` reports as changed. With a `Metrics`, it
increments `transformations_applied` once per successful application.

### MergeReduction

A `TransformationReduction` subclass, configured with just
`MergeTransformation` and the name `"merge"` — greedily combining every
`(i,j)`/`(i+1,j)` pair into `(i,j+1)` until none remain. This is what
`MergingSparsePlan` (see "plan_zoo" below) runs on both operands before
every multiply. Its `MergeTransformation` is a function-local static
(rather than an instance member) since the base class constructor needs
its address before this derived class has finished constructing.

### BinaryFormReduction

```cpp
#include "smooth/reduction_zoo/binary_form_reduction.hpp"
#include "smooth/smooth.hpp"

smooth::SmoothInteger n;
n.set(1, 2);  // 2^1 * 3^2 = 18
auto rep = n.representationAs(smooth::SmoothInteger::Representation::Sparse);

smooth::BinaryFormReduction toBinary;
toBinary.run(*rep);
rep->print();  // {(1, 0), (4, 0)}  -- 18 = 16 + 2, its ordinary binary form
```

Repeatedly applies `SplitTransformation` until every set bit lands in
column 0 — the number's plain binary representation. Since
`SplitTransformation` alone has no natural floor, this is a
`TransformationReduction` bounded via `allowed = [](int, int j){ return j >= 0; }`:
a bit above column 0 still splits all the way down to it, but one already
at column 0 is left alone (producing it would need a disallowed anchor at
column -1).

### TernaryCarryTransformation

```cpp
#include "smooth/representation_zoo/row_values_representation.hpp"
#include "smooth/transformation_zoo/ternary_carry_transformation.hpp"

smooth::RowValuesRepresentation rep(/*allow_fractional=*/false);
rep.setColumnValue(0, 9.0);  // 9 = 1001 binary, at column 0

smooth::TernaryCarryTransformation carry;
carry.canApply(rep, 0, 0);                       // true -- n_0 = 9 has more than one bit set
auto landings = carry.applyAndReportLandings(rep, 0, 0);  // {(0, 0), (0, 1)}
rep.columnValue(0);                              // 6.0  (9 - 3)
rep.columnValue(1);                              // 1.0  (0 + 1)
```

Anchored at column `j` (the row half of the anchor is unused): while
column `j`'s magnitude `n_j` has more than one bit set, subtracts `3` from
it and adds `1` to `n_(j+1)` — value-preserving since `3 * 3^j = 3^(j+1)`.
This is the transformation that forced `Transformation` to become an
interface: its precondition depends on a whole column's magnitude, and
applying it is an ordinary subtraction needing a borrow across bits that
`OffsetTransformation` has no way to express — so it requires a
`RowValuesRepresentation` and throws `std::invalid_argument` otherwise.

One application only decrements column `j`, which may still need further
applications at the same anchor, so `applyAndReportLandings()` reports
column `j` itself as a landing alongside `j+1`; `affectedAnchors(i, j)`
always answers `{(0, j)}` regardless of `i`. This terminates without ever
going negative: the descending sequence `n_j, n_j - 3, n_j - 6, ...` is
confined to one residue class mod 3, whose smallest nonnegative member (0,
1, or 2) always has at most one bit set.

### TernaryCarryReduction

A `TransformationReduction` subclass over just `TernaryCarryTransformation`,
named `"ternary_carry"`. Needs no `allowed` bound — the transformation's own
`canApply()` is already self-limiting.

### TernaryFormReduction

```cpp
#include "smooth/reduction_zoo/ternary_form_reduction.hpp"
#include "smooth/smooth.hpp"

smooth::SmoothInteger n;
n.setValue(13LL);  // 13 = 2^2 + 3^2, not itself a single term
auto rep = n.representationAs(smooth::SmoothInteger::Representation::Sparse);

smooth::TernaryFormReduction toTernary;
toTernary.run(*rep);
rep->print();  // {(2, 0), (0, 2)}  -- 13 = 4*3^0 + 1*3^2
```

Reduces a number to a form where every column with anything in it holds
exactly one bit. So long as some column `j` has more than one set bit,
takes its two *smallest* set rows `i1 < i2` and applies
`RowSpreadTransformation(i2 - i1)` anchored at `(i1, j)` — folding them
into a single bit one column over, plus (when `i2 - i1 > 1`) a staircase
filling the gap, all within column `j` — then repeats.

Always resolving the smallest offending column first is what makes this
terminate: `RowSpreadTransformation`'s outputs never land below its
anchor's column, so a resolved column stays resolved, and within one
column each application strictly decreases its magnitude (by `3 * 2^i1`).
Like `StaircaseReduction` below, it implements `Reduction` directly and
rescans all set bits after every application, rather than fitting
`TransformationReduction`'s mold.

Unlike the two-phase `BinaryFormReduction` + `TernaryCarryReduction`
pipeline this replaces, this works entirely through a bit-level
`OffsetTransformation`, so it runs against *any* `RepresentationBase` with
no special-casing. It also needs no "collapse into column 0 first" phase —
but that means the result can now depend on the *starting* bit layout, not
just the value, unlike the old pipeline (which always erased any head
start by collapsing to column 0 first): 11 as its ordinary binary form
settles differently than 11 built directly as `5*3^0 + 2*3^1`, both being
equally valid single-bit-per-column results.

### StaircaseReduction

```cpp
#include "smooth/reduction_zoo/staircase_reduction.hpp"
#include "smooth/smooth.hpp"

smooth::SmoothInteger n;
n.set(1, 1);  // 6
n.set(4, 5);  // 3888
auto rep = n.representationAs(smooth::SmoothInteger::Representation::Sparse);

smooth::StaircaseReduction staircase;
staircase.run(*rep);
rep->print();  // {(1, 6), (2, 5), (3, 4), (4, 3), (7, 1)}
```

So long as two distinct set bits `(i1,j1)`/`(i2,j2)` exist with `i2 >= i1`
and `j2 >= j1` (i.e. `(i2,j2)` weakly dominates `(i1,j1)`), combines them:
`SpreadTransformation(j2-j1)` for a same-row pair, `RowSpreadTransformation(i2-i1)`
for a same-column pair, or `CornerSplitTransformation` anchored at
`(i2,j2)` for a genuine diagonal pair. The fixed point is an antichain
under the product order — every 3-smooth number has such a "staircase"
form.

Termination isn't free: naively picking *any* dominating pair can run for
tens of thousands of steps. What works reliably is always picking the
*first* dominating pair found while scanning set bits in sorted `(i, j)`
order. Candidate pairs aren't confined to a small local neighborhood, so
(like `TernaryFormReduction`) this rescans from scratch after every
application rather than using `TransformationReduction`'s worklist.
`CornerSplitTransformation` only ever fires here on a bit strictly
dominating another non-negative bit, so its anchor is always at row/column
`>= 1` — starting from an all-non-negative representation, this never
needs a fractional-capable one.

## Metrics

```cpp
#include "smooth/smooth.hpp"

auto metrics = std::make_shared<smooth::Metrics>();
smooth::SmoothInteger n(metrics);  // every concrete type's one constructor
                                    // argument is an optional shared Metrics
```

`smooth::Metrics` is a small named-counter tracker: `increment(name)` bumps
a counter by name, `print()` prints every counter sorted by name. It's
generic — any named event can be tallied. `SmoothNumberBase` increments
`convert_<from>_to_<to>` on every real conversion; each representation
also instruments its own internal work (below), sharing the same object
since `SmoothNumberBase` constructs its representations with the same
`Metrics` it was given.

A number's `Metrics` is optional and *shared*, not copied — two numbers
built with the same object tally onto the same counters, and copy/move
carry the pointer along. **`a + b` keeps a's metrics**, falling back to
b's only if a has none (the signed "different signs" branch has to
capture this choice explicitly up front, since it otherwise builds its
result out of a copy of `b`).

### Instrumentation counters

- **`carries`** — one per ripple-carry step, for Sparse/Dynamic (the two
  raw-bit-grid representations sharing `addSingleBitWithCarry()`).
- **`bit_operations`** — one per `(termA, termB)` pairing during a Sparse/
  Dynamic multiply (an *n*-by-*m* multiplication is *n\*m* of these).
- **`scalar_operations`** — one per plain arithmetic op for RowValues
  (each `accumulate()` call; 2 per column pair in its convolution) and
  Scalar (each `set()`/`addInPlace()`/`multiplyInPlace()` fast path).
- **`bit_iterations`** — one per cell visited while looping, where "cell"
  means whatever that representation actually stores: set coordinates for
  Sparse, every allocated cell for Dynamic, one per column entry for
  RowValues. Scalar has none — nothing to loop over.

Decoding/encoding a value directly (`setColumnValue()`/`setValue()`, and
`forEachSet()`'s bit-decomposition for RowValues/Scalar) is deliberately
not instrumented — it isn't looping over or combining an existing value.

## Signed types

`include/smooth/signed.hpp` defines:

```cpp
template <typename Base>
class Signed : public Base { /* ... */ };

using SmoothSignedInteger = Signed<SmoothInteger>;
using SmoothSignedFloat = Signed<SmoothFloat>;
```

The bit grid can only hold positive terms, so `Signed<Base>` adds the sign
as a separate flag, sign-magnitude style: `isNegative()`/`setNegative(bool)`/
`negate()`; `value()` is `Base::value()` negated if the flag is set;
`setValue()` splits the sign off first, then hands the magnitude to
`Base::setValue()` — so a negative value no longer throws on a signed
type. `operator+`/`operator*` are the signed versions described under
"Addition"/"Multiplication" above.

`set`/`get`/`clear`/`print*` are untouched — always the magnitude only.
`Signed<Base>::value()`/`setValue()` intentionally *hide* (aren't virtual
overrides of) `Base`'s versions — these types are always used by their own
concrete name, never through a `SmoothNumberBase*`.

## Plan

```cpp
#include "smooth/smooth.hpp"

double result = smooth::DefaultPlan()
    .scalar(3)
    .times()
    .left()
      .scalar(4)
      .plus()
      .scalar(2)
    .right()
    .calculate();  // 3 * (4 + 2) = 18
```

`smooth::Plan` is a fluent builder for an arithmetic expression over
3-smooth numbers. Building it produces a **declaration** — a pure,
representation-agnostic record of what was asked for; nothing is computed
yet. Compiling (the first `plan()`/`calculate()`) turns that into a
**blueprint**: the same tree with explicit `Ensure(target)` steps spliced
in wherever a leaf needs a specific representation (or `Reduce` steps, for
a strategy like `MergingSparsePlan`). The blueprint is what's executed
*and printed by `plan()`*, so every conversion shows up as a real,
inspectable step:

```cpp
smooth::SparsePlan().scalar(1).plus().scalar(2).plan();
// add
// ├─ ensure(sparse)
// │  └─ scalar: 1
// └─ ensure(sparse)
//    └─ scalar: 2
// = 3
```

**`Plan` is an interface** with no representation of its own to compute
with. A concrete subclass's entire strategy is one method,
`buildBlueprint()`; `DefaultPlan` is the simplest one (see "plan_zoo"
below for others). Immediately after it runs, `validateBlueprint()`
(private, always run) confirms that stripping every `Ensure` node back out
yields the declaration's exact shape again — so an override can only
*decorate*, never change, what's being computed.

- `scalar(double)` — a leaf; throws `std::invalid_argument` for a negative
  value (every representation is magnitude-only).
- `number(const T&)` — a leaf snapshotting an existing number's value
  immediately. Templated so `T::value()` resolves at `T`'s own concrete
  type, required for a signed `T` whose `value()` hides rather than
  overrides `SmoothNumberBase::value()`.
- `numberVia(SmoothNumberBase&)` — keeps a live reference instead (which
  must outlive `calculate()`/`plan()`); its `Ensure` step calls `n`'s own
  `representationAs(target)` directly, a genuine conversion through `n`'s
  own cache, so `n`'s `Metrics` sees the result.
- `plus()`/`times()` — wraps everything built so far as the new operator's
  left side. Without grouping, a chain is **left-associative**.
- `left()`/`right()` — open/close a nested group, like `(`/`)`. This is
  what makes `3 * (4 + 2)` possible at all.
- `calculate()` — the result, as a `double`.
- `plan(os = std::cout)` — prints the compiled blueprint as a tree (every
  leaf gets an `Ensure` step, even `DefaultPlan`'s own).

`buildBlueprint(const Node&) const` (`protected`, pure virtual) is the one
hook every concrete `Plan` overrides, typically built entirely on
`wrapLeavesWithEnsure(declaration, target)` (`protected`, shared) — walks
the declaration wrapping every leaf in `Ensure{target}`.

Combining two already-converted representations (`Add`/`Multiply`
execution) just clones the left operand and calls its own
`addInPlace()`/`multiplyInPlace()` — one shared implementation, since
those are already virtual over any `RepresentationBase`.

`name()` (public, pure virtual) identifies the strategy and drives the
`convert_to_<name>` counter — independent of the `Ensure` step's own
printed label, which is why `MatrixPlan`'s tree says `ensure(dynamic)`
even though `name()` is `"matrix"`.

Calling something out of order (two values with no operator between them,
an unmatched `left()`/`right()`, etc.) throws `std::invalid_argument`.

Like every concrete type, `Plan` takes an optional shared `Metrics`. A
`Metrics` shared between a `Plan` and the numbers feeding it (via
`number()`/`numberVia()`) tallies both under the same counters:
`convert_to_<name()>`/`add`/`multiply` from compiling, whatever
representation-level work executing produces, and — for `numberVia()` —
the fed-in number's own `convert_<canonical>_to_<target>` counter too
(except `MatrixPlan`, whose target is already every fresh number's
canonical representation).

## plan_zoo

`include/smooth/plan_zoo/` holds `Plan` subclasses, each just routing
`buildBlueprint()` through a different `RepresentationBase`
(`plan_zoo.hpp` pulls in all of them). Three are exactly
`return wrapLeavesWithEnsure(declaration, Representation::X);`:

- `SparsePlan` (`"sparse"`) → `SparseRepresentation`.
- `MatrixPlan` (`"matrix"`) → `DynamicMatrixRepresentation` (called
  "matrix" for historical reasons — `name()` and the printed `Ensure`
  label disagree for this one variant only).
- `RowValuesPlan` (`"row_values"`) → `RowValuesRepresentation`.

The fourth, `MergingSparsePlan` (`"merging_sparse"`), builds on
`SparsePlan`: it calls `SparsePlan::buildBlueprint()` first, then wraps
both operands of every `Multiply` node (however deeply nested) in a
`Reduce` step running `MergeReduction` — coalescing set bits before the
multiply, since a Sparse/Dynamic multiply costs one `bit_operation` per
pair of set bits. `Add` nodes are left untouched:

```cpp
#include "smooth/plan_zoo.hpp"

auto metrics = std::make_shared<smooth::Metrics>();
smooth::MergingSparsePlan p(metrics);
p.scalar(15).times().scalar(15);
p.plan();
// multiply
// ├─ reduce(merge)
// │  └─ ensure(sparse)
// │     └─ scalar: 15
// └─ reduce(merge)
//    └─ ensure(sparse)
//       └─ scalar: 15
// = 225
metrics->print();
// transformations_applied = ...   <- however many merges it took
```

Each `numberVia()` leaf's `Ensure` step genuinely calls
`n.representationAs(target)`, so each of these four genuinely converts a
fed-in number into its own representation:

```cpp
smooth::SparsePlan p(metrics);
p.numberVia(a).plus().numberVia(b).calculate();
metrics->print();
// convert_dynamic_to_sparse = 2   <- one per fed-in number
```

Every representation is magnitude-only, so a negative `scalar()` leaf
throws `std::invalid_argument` — enforced centrally in `Plan::scalar()`.
`DefaultPlan` and all four of these share a common base pointer
(`std::unique_ptr<Plan>`), dispatching virtually and destructing safely.

## Building the demo and tests

```sh
cmake -S . -B build
cmake --build build
./build/smooth_demo
./build/smooth_tests   # or: cd build && ctest --output-on-failure
```

`src/demo.cpp` is a quick tour of the library's main pieces -- basic
set/get/clear/value, `SmoothFloat`'s negative indices, signed types and
`setValue()`, addition/multiplication, the four representations,
`Plan`/`plan_zoo`, a `Transformation`, and a `Reduction` -- not exhaustive
coverage; that's what the test suite is for.

`tests/test_smooth.cpp` is a small, dependency-free assertion-based test
suite (no test framework linked in — see `CMakeLists.txt`) that exercises
every class and behavior described above directly: each representation's
own `get`/`set`/`addInPlace`/`multiplyInPlace`/`clone` strategy; unsigned
and signed addition/multiplication (matching-sign, differing-sign, zero,
and cross-column borrow cases); every counter under "Metrics" pinned down
independently per representation; every `transformation_zoo` preset's
`canApply()`/`applyAndReportLandings()`/value-preservation, including
carry and negative-index cases; `atomize()` for every preset (exact atom
counts, and that applying directly vs. via the atomized sequence lands on
identical bits); the per-representation atom dispatch (RowValues
arithmetic, the `ScalarRepresentation` throw, Sparse left unaffected) and
the `viaAtoms`/`atomize=true` apply path end to end; every `reduction_zoo`
preset (termination, value preservation, exact step counts where
meaningful, and error cases); `Plan`, every `plan_zoo` strategy, and
`validateBlueprint()`'s tamper-detection (via three deliberately broken
`Plan` subclasses). It builds as a second executable, `smooth_tests`,
runnable directly or via `ctest`.

## Profiles: measuring the counters

```sh
cmake -S . -B build   # requires SQLite3 (find_package(SQLite3 REQUIRED))
cmake --build build
./build/smooth_profiles              # writes/reads ./smooth_profiles.db
./build/smooth_profiles some/path.db # or an explicit database path
```

`src/profile_runner.cpp` (the `smooth_profiles` executable) defines a
fixed set of named `Profile`s, each a `name` and a `run(Plan&) -> double`
written purely in terms of `Plan`'s own builder methods — so the same
`run()` works unchanged whether it's bound to a `DefaultPlan`,
`SparsePlan`, `MatrixPlan`, or `RowValuesPlan`, measuring the same
expression identically across every representation.

The profiles look like ordinary everyday arithmetic (sums, price×quantity
products), ranging from 2-number expressions up through `ten_day_totals`
(10 numbers). Two of them use `numberVia()` against real `SmoothInteger`
objects instead of `scalar()`, so each number's own conversion counter
lands under the same `Metrics` row too.

The same file runs every (plan kind, profile) combination against a
SQLite database, skipping anything already present. Each row records the
plan kind, profile name, computed result, and one column per counter a
`Plan`-driven run can produce (`convert_to_<X>`, `add`, `multiply`, the
representation-level counters from "Metrics", and each
`convert_dynamic_to_<X>`).

**The set of columns is fixed on purpose: when a counter is added,
removed, or renamed, delete the database file and let it be recreated from
scratch**, rather than migrating it in place. `smooth_profiles` checks the
database's actual columns against what the current build expects on every
run and refuses to proceed if they don't match, so a stale database is a
hard error, not silently-wrong data.
