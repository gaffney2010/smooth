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
none of them need any capacity declared up front. All four also take an
optional `std::shared_ptr<Metrics>` as their one constructor argument —
see "Metrics" below.

Every type shares its `get`/`set`/`clear`/`value`/`print*`/`setBounds`/
`add`/`operator+=`/`operator+` behavior — see `SmoothNumberBase` and
"Addition" below — and the two signed types add sign-related methods (and
their own, sign-aware addition) on top (see "Signed types" below). All four
are copyable (a copy deep-clones the underlying representation, sharing no
state with the original) as well as movable.

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
- `setValue(long long)` / `setValue(double)` — replaces whatever this
  number currently holds with a plain integer or floating-point value.
  Implemented by clearing the number, then putting the whole value into row
  `j = 0` (`n[0]` in the `RowValues` view) via the canonical
  representation's `setColumnValue(0, v)` — since `value() = sum_j n_j *
  3^j` and `3^0 = 1`, `n_0` alone reproduces the input exactly. Throws
  `std::invalid_argument` for a negative value on an unsigned type (`Base`
  can only ever hold a positive magnitude — see "Signed types" for how the
  signed types handle negative input), or for a fractional value on a
  non-fractional type.

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
(`get`/`set`/`reset`/`value`/`print`/`forEachSet`/`setColumnValue`/
`addInPlace`/`clone`):

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

`setColumnValue(j, n)` is the same per-representation-strategy idea, but for
*writing* a whole column's contribution at once (used by `setValue()` — see
above) instead of reading. Like every other `RepresentationBase` method,
it's pure virtual — no default lives on the interface, so each
representation is explicit about its own strategy:

- Sparse and Dynamic have no more direct way to encode a number than
  writing its bits one at a time, so both just call the free
  `decomposeColumnValue(rep, j, n)` helper (`representation_base.hpp`),
  which decomposes `n` into bits (integer part via bit-shifting, fractional
  part via repeated doubling) and writes each one through `set()`. Sharing
  that helper — rather than each duplicating the same loop, or the
  interface providing it as a default — is the "share logic where you can"
  part.
- RowValues overrides it directly: because its storage already *is* `n_j`
  per column, it just assigns `n` — O(1), exact, and without the small
  floating-point error that adding/subtracting powers of two one bit at a
  time could otherwise accumulate.

This is a Strategy pattern: `SmoothNumberBase` only ever talks to
representations through the `RepresentationBase` interface (an
`std::array<std::unique_ptr<RepresentationBase>, 3>`), and generic
operations like `ensure()` (rebuild an outdated representation from the
canonical one, via `forEachSet`) are written purely in terms of that
interface. Adding another representation means writing one new class that
implements `RepresentationBase`, adding an enumerator to `Representation`,
and adding one line to register it in the constructor — no existing logic
needs to change.

`clone()` (`std::unique_ptr<RepresentationBase> clone() const`) is what
lets a `SmoothNumberBase` be copied without knowing which concrete
representation types exist: each implementation just returns
`std::make_unique<ThatClass>(*this)`, using its own ordinary copy
constructor.

## Addition

Every type supports adding another number of the same type, or a plain
scalar, in place:

```cpp
void add(const SmoothNumberBase& other);   // and Signed<Base>'s own overload
void add(long long scalar);
void add(double scalar);

SmoothNumberBase& operator+=(const SmoothNumberBase& other);  // thin wrappers
SmoothNumberBase& operator+=(long long scalar);               // over add()
SmoothNumberBase& operator+=(double scalar);
```

plus a value-returning `operator+`, shared by all four types as a single
template (`T operator+(T lhs, const T& rhs)`, and scalar overloads) that
copies its left operand and adds into the copy — this is what the copy
constructor mentioned above exists for.

A scalar is added by converting it the same way `setValue()` does (placing
it in column `j = 0`) and then adding that in — "converting the number"
into a smooth number first, per the request that started this feature.

**Unsigned types** (`add(const SmoothNumberBase&)`, in `SmoothNumberBase`)
dispatch straight to the canonical representation's `addInPlace(other)` —
each representation adds the 1s and handles carries however is natural for
its own storage:

- Sparse and Dynamic have no more direct way to add a number than walking
  `other`'s bits one at a time (captured up front via `forEachSet`, so this
  is safe even when adding a number to itself) and carrying: adding a
  second 1 into a cell that already holds one is the same as moving that
  bit up to the next row (`2 * 2^i * 3^j = 2^(i+1) * 3^j`), so both share
  the `addBitsWithCarry(dst, other)` helper (`representation_base.hpp`).
- RowValues doesn't need explicit carry handling: each contribution just
  adds onto its column's running total, and ordinary floating-point
  addition already produces the correct combined value. When `other` is
  *also* a `RowValuesRepresentation`, its columns already are the totals to
  add, so this adds them directly, column by column — it doesn't even
  decompose `other` into bits first just to reconstruct those same totals.
  Only when `other` is some other representation does it fall back to
  reading `other`'s bits via `forEachSet` and accumulating each one's `2^i`.

**Signed types** need actual signed arithmetic, since the bit grid is
magnitude-only and the sign lives in `Signed<Base>`'s own flag:
`Signed<Base>::operator+=` combines magnitudes via `Base::add()` when both
signs match, and otherwise subtracts the smaller magnitude from the larger
and takes the larger operand's sign (a result of exactly zero is
normalized back to non-negative). The subtraction step uses a second,
protected primitive, `SmoothNumberBase::subtractMagnitudeInPlace()`, not
exposed publicly since plain subtraction has no meaning for the two
unsigned types.

Unlike addition, that subtraction has no natural per-representation
variation, so it isn't dispatched through `RepresentationBase` at all: it
converts both operands to per-column totals via `forEachSet()`, subtracts
column by column, and resolves any column that goes negative by borrowing
from the next column up — one unit of `n_(j+1)` is worth exactly 3 units of
`n_j`, since `3^(j+1) = 3 * 3^j` — before writing the result back through
`setColumnValue()`. (Addition never needs this cross-column borrowing:
overflow in a column only ever carries within that same column, since
`2^i` doubles without ever needing to touch a neighboring column's power of
3.)

## Metrics

```cpp
#include "smooth/smooth.hpp"

auto metrics = std::make_shared<smooth::Metrics>();
smooth::SmoothInteger n(metrics);  // every concrete type's one constructor
                                    // argument is an optional shared Metrics
```

`smooth::Metrics` (`include/smooth/metrics.hpp`) is a small named-counter
tracker:

- `increment(const std::string& name)` — bump a counter by name (starting
  from 0 the first time it's named).
- `print(os = std::cout)` — print every counter's current value, one per
  line, sorted by name.

It's deliberately generic — any named event could be tallied on it — but
for now the only thing that increments a counter is `SmoothNumberBase`
counting representation conversions: every time `ensure()` actually
converts (not when the target is already valid), it increments
`convert_<from>_to_<to>`, e.g. `convert_dynamic_to_row_values`.

A number's `Metrics` is optional (`nullptr` by default) and, when given, is
*shared*, not copied: `hasMetrics()`, `metricsPtr()`, and `setMetricsPtr()`
expose the underlying `std::shared_ptr<Metrics>`, so two numbers
constructed with the same `Metrics` object tally onto the same counters.
Copying or moving a number carries its `Metrics` pointer along (still
shared with the original), consistent with everything else about
`SmoothNumberBase`'s copy semantics.

Addition follows two rules for which `Metrics` a result ends up with:

- **`a += b` always keeps a's metrics.** `add()`/`operator+=` never touch
  `metrics_` themselves, so this falls out for free for the two unsigned
  types — but `Signed<Base>::operator+=`'s "different signs, `|a| < |b|`"
  branch internally replaces `*this` wholesale with a copy of `b` (to get
  at `b`'s larger magnitude before subtracting), which would otherwise
  silently adopt `b`'s metrics instead. It works around that by capturing
  `this->metricsPtr()` up front and restoring it with `setMetricsPtr()`
  after, regardless of which internal branch ran.
- **`a + b` keeps a's metrics, unless a has none, in which case it falls
  back to b's (if b has any).** The shared `operator+` template (see
  "Addition") builds its result by copying `a` and adding `b` into that
  copy — which, per the rule above, already keeps a's metrics — and then,
  only if that copy still has no metrics, adopts `b`'s.

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
- `setValue(long long)` / `setValue(double)` — splits the sign off of the
  input (`setNegative(v < 0)`), then hands the non-negative magnitude to
  `Base::setValue()`, so a negative value no longer throws on a signed
  type — it's encoded via the sign flag instead.
- `add`/`operator+=` (`const Signed<Base>&`, `long long`, `double`) —
  proper signed addition; see "Addition" above.

`set`/`get`/`clear`/`print*` are untouched — they still only ever see the
magnitude. This is the "share logic where you can" part of the design: the
sign behavior, and the "split the sign off before encoding" behavior for
`setValue`, are each written exactly once, as a template over `Base`, and
reused for both `SmoothSignedInteger` and `SmoothSignedFloat` rather than
being duplicated in two separate classes. `Signed<Base>::value()` and
`Signed<Base>::setValue()` intentionally *hide* rather than override
`Base`'s versions (neither is virtual) — these types are always used by
their own concrete name, never through a `SmoothNumberBase*`, so static
hiding is enough, and it avoids paying for virtual dispatch for a feature
only the signed types need.

## Building the demo and tests

```sh
cmake -S . -B build
cmake --build build
./build/smooth_demo
./build/smooth_tests   # or: cd build && ctest --output-on-failure
```

`src/demo.cpp` shows constructing a `SmoothInteger`, setting bits, printing
it, reading its value, using `setBounds()` as an optional guardrail, a
`SmoothFloat` with fractional terms, `SmoothSignedInteger` /
`SmoothSignedFloat` negation, converting plain numbers via `setValue()`
(including a negative value on a signed type), number + number and
number + scalar addition (`add()`, `operator+=`, and the value-returning
`operator+`), signed addition with a sign flip, viewing a number through
all three representations, using `DynamicMatrixRepresentation`,
`SparseRepresentation`, and `RowValuesRepresentation` directly (since a
`SmoothNumberBase`'s canonical representation always starts out, and for
now stays, Dynamic) to run each representation's own `addInPlace()`
strategy, including a `Sparse` carry, directly, and attaching a `Metrics`
to a number to show its conversion counters, plus the `a += b` / `a + b`
metrics-inheritance rules.

`tests/test_smooth.cpp` is a small, dependency-free assertion-based test
suite (no test framework linked in — see `CMakeLists.txt`) covering all of
the above: bounds/fractional restrictions, `setValue()`, signed
sign-handling, agreement across all three representations, each
`RepresentationBase` implementation exercised directly (including
`clone()` independence and `Sparse`'s carry), unsigned and signed addition
(same-sign, both differing-sign directions, the zero tie, the
cross-column/mixed-radix borrow case, and scalar addition), `Metrics`
counters (including that redundant conversions aren't double-counted, and
the `a += b`/`a + b` metrics-inheritance rules — notably that the signed
swap branch still keeps a's metrics), and copy/move
semantics. It builds as a second executable, `smooth_tests`, runnable
directly or via `ctest`.
