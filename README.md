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
flag — that's the point of having four distinct types instead of one type
with two booleans. All four are default-constructible with no arguments;
none of them need any capacity declared up front.

Every type shares its `get`/`set`/`clear`/`value`/`print*`/`setBounds`
behavior — see `SmoothNumberBase` below — and the two signed types add
sign-related methods on top (see "Signed types" below).

### `SmoothNumberBase`

Header-only class in `include/smooth/smooth_number_base.hpp`. This is the
shared engine behind all four concrete types above; it's not meant to be
constructed directly (its constructor is `protected`). It owns three
internal representation objects (see below) and delegates to whichever one
is canonical through the `RepresentationBase` interface
(`include/smooth/representation_base.hpp`).

- `set(i, j, value = true)` — set (or clear) the bit at `(i, j)`. `i`/`j` are
  signed so negative indices can be addressed (on a fractional type).
- `clear(i, j)` — clear the bit at `(i, j)`.
- `get(i, j)` — read the bit at `(i, j)`.
- `value()` — return the sum of `2^i * 3^j` over all set bits (as a
  `double`; precision degrades for large row/column counts or deeply
  negative indices). Uses a strategy suited to whichever representation is
  currently canonical (see below) rather than always walking a full grid.
  Not virtual — see "Signed types" for why.
- `allowsFractional()` — whether this type permits negative indices
  (`true` for `SmoothFloat`/`SmoothSignedFloat`, `false` for
  `SmoothInteger`/`SmoothSignedInteger`).

A negative index on a non-fractional type throws `std::out_of_range`.

### `setBounds` — an optional, non-binding sanity check

```cpp
void setBounds(std::size_t max_rows, std::size_t max_cols,
               std::size_t neg_rows, std::size_t neg_cols);
```

Every representation is either naturally unbounded (Sparse, RowValues) or
grows to fit whatever gets set into it (Dynamic), so `SmoothNumberBase`
has nothing to preallocate and no real need for a maximum size. `setBounds`
exists anyway as a pure sanity check: once called, any future `set()`/`get()`
outside `[-neg_rows, max_rows) x [-neg_cols, max_cols)` throws
`std::out_of_range`. It doesn't reserve memory, doesn't retroactively
validate bits already set, and isn't required — it's there purely to catch
mistakes if you want that guardrail, nothing more.

### Internal representations

The same number can be held in one of several internal representations,
each a class implementing `RepresentationBase`
(`get`/`set`/`reset`/`value`/`print`/`forEachSet`):

- **Sparse** (`sparse_representation.hpp`) — the set of `(i, j)` coordinates
  whose bit is set. A `std::set` — a literal list of the coordinates that
  are actually on.
- **RowValues** (`row_values_representation.hpp`) — one number `n_j` per
  column `j` that has anything set, where `n_j = sum_i (bit(i,j) ? 2^i : 0)`,
  so the total value is `sum_j n_j * 3^j`. Stored as a `std::map<int,
  double>` keyed by `j` (an entry is dropped once it returns to zero), so it
  only ever holds entries for columns with something in them. `n_j` is an
  integer if the number doesn't allow fractional terms, and may be
  fractional otherwise.
- **Dynamic** (`dynamic_matrix_representation.hpp`) — a bit grid that starts
  at 0x0 and grows only as needed: whenever `set()` turns on a bit outside
  the currently allocated range, the array doubles in whichever of the four
  directions (more positive rows, more negative rows, more positive
  columns, more negative columns) ran out — repeatedly, if one `set()` call
  jumps far past the current capacity — and the old contents are copied
  into the new, larger array. `reset()` drops it back to 0x0, so converting
  into Dynamic from another representation regrows it from scratch, one
  doubling at a time, as each set bit is replayed into it.

Exactly one representation is **canonical** — the trusted, up-to-date copy
(`Dynamic` by default). `set`/`get`/`value()` always operate through the
canonical representation. A `set` call that actually changes a bit
invalidates the other representations (a `set` to the same value it already
had does not); an already-canonical representation is never redundantly
reconverted. Which representation is canonical, and when (if ever) that
changes, is decided internally — there is no public method to force it, by
design.

- `canonical()` — which `Representation` (`Sparse`, `RowValues`, or
  `Dynamic`) is currently canonical (read-only).
- `printSparse(os = std::cout)` — converts to Sparse if needed, then prints
  the set of coordinates, e.g. `{(0, 2), (1, 0)}`.
- `printRowValues(os = std::cout)` — converts to RowValues if needed, then
  prints each `n_j`, one per line.
- `printDynamic(os = std::cout)` — converts to Dynamic if needed, then
  prints its currently allocated capacity followed by the grid within it. A
  horizontal and/or vertical line marks the boundary between the fractional
  entries (negative index) and the whole-number entries (non-negative
  index); the whole-number part is the block below and to the right of the
  lines.

`value()` calls straight through to the canonical representation's own
`value()`, so each avoids doing more work than it needs to:

- Sparse — sums only over the set of coordinates that are actually set,
  skipping the zero cells a full grid walk would visit.
- RowValues — a single pass over the (typically few) columns with anything
  set, computing `sum_j n_j * 3^j` directly, with no per-bit decoding.
- Dynamic — walks only its own currently allocated capacity, which (after a
  conversion) is sized just large enough to cover the set bits.

Converting one representation from another goes through `forEachSet(fn)`,
which asks the *source* representation to invoke `fn(i, j)` once per set
bit — each representation enumerates its own bits however is natural for
its storage (Sparse walks its set, RowValues decodes each `n_j` back into
individual bits, Dynamic walks its own allocated capacity). This is what
makes conversion possible without any global bounds: nobody needs to know
"the largest index that might be set" up front.

This is a Strategy pattern: `SmoothNumberBase` only ever talks to
representations through the `RepresentationBase` interface (an
`std::array<std::unique_ptr<RepresentationBase>, 3>`), and generic
operations like `ensure()` (rebuild an outdated representation from the
canonical one, via `forEachSet`) are written purely in terms of that
interface. Adding another representation means writing one new class that
implements `RepresentationBase`, adding an enumerator to `Representation`,
and adding one line to register it in the constructor — no existing logic
needs to change.

## Signed types

`include/smooth/signed.hpp` defines:

```cpp
template <typename Base>
class Signed : public Base { /* ... */ };

using SmoothSignedInteger = Signed<SmoothInteger>;
using SmoothSignedFloat = Signed<SmoothFloat>;
```

The `(i, j)` bit grid can only ever hold positive terms (`2^i * 3^j > 0`
always), so a sign can't live in the grid itself — `Signed<Base>` adds it
as a separate flag, sign-magnitude style, on top of whichever base type you
give it:

- `isNegative()` — whether the sign flag is set.
- `setNegative(bool)` — set it directly.
- `negate()` — flip it.
- `value()` — `Base::value()`, negated if the sign flag is set.

`set`/`get`/`clear`/`print*` are untouched — they still only ever see the
magnitude. This is the "share logic where you can" part of the design: the
sign behavior is written exactly once, as a template over `Base`, and reused
for both `SmoothSignedInteger` and `SmoothSignedFloat` rather than being
duplicated in two separate classes. `Signed<Base>::value()` intentionally
*hides* rather than overrides `Base::value()` (it isn't virtual) — these
types are always used by their own concrete name, never through a
`SmoothNumberBase*`, so static hiding is enough, and it avoids paying for
virtual dispatch on `value()` for the two types that don't need a sign at
all.

## Building the demo

```sh
cmake -S . -B build
cmake --build build
./build/smooth_demo
```

`src/demo.cpp` shows constructing a `SmoothInteger`, setting bits, printing
it, reading its value, using `setBounds()` as an optional guardrail, a
`SmoothFloat` with fractional terms, `SmoothSignedInteger` /
`SmoothSignedFloat` negation, viewing a number through all three
representations, and — using `DynamicMatrixRepresentation` directly, since
none of the four types expose it as a live growable object — the doubling
growth happening step by step as bits are set farther and farther out.
