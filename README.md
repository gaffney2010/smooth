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
`operator+`/`operator*` behavior — see `SmoothNumberBase` and
"Addition"/"Multiplication" below — and the two signed types add
sign-related methods (and their own, sign-aware `operator+`/`operator*`) on
top (see "Signed types" below). All four are copyable (a
copy deep-clones the underlying representation, sharing no state with the
original) as well as movable.

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
`addInPlace`/`multiplyInPlace`/`clone`):

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
- **Scalar** (`scalar_representation.hpp`) — stores the number as a single
  plain value (an integer or a float) rather than a `(i, j)` bit grid:
  every term it can hold lives in column `j = 0`, so its value is just that
  one number (`3^0 = 1`). It's still a `RepresentationBase`, purely so a
  number stored this way can still convert to and from the others through
  the ordinary machinery — it isn't a standalone type of its own. Because
  it can only hold column-0 terms, converting a number with a genuine
  multi-column value (e.g. one with a bit at `(0, 2)`, i.e. `3^2`) into
  Scalar — or `set()`/`setColumnValue()` on any column but 0 — throws
  `std::invalid_argument`.

Exactly one representation is **canonical** — the trusted, up-to-date copy
(`Dynamic` by default). `set`/`get`/`value()` always operate through the
canonical representation. A `set` call that actually changes a bit
invalidates the other representations (a `set` to the same value it already
had does not); an already-canonical representation is never redundantly
reconverted. Which representation is canonical, and when (if ever) that
changes, is decided internally — there is no public method to force it, by
design (every number currently starts out, and stays, canonical on
`Dynamic`).

- `canonical()` — which `Representation` (`Sparse`, `RowValues`, `Dynamic`,
  or `Scalar`) is currently canonical (read-only).
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
- `printScalar(os = std::cout)` — converts to Scalar if needed, then prints
  the number as a plain integer or float. Converting throws
  `std::invalid_argument` if the number's value isn't representable as a
  plain number (see Scalar, above).

`value()` calls straight through to the canonical representation's own
`value()`, so each avoids doing more work than it needs to:

- Sparse — sums only over the set of coordinates that are actually set,
  skipping the zero cells a full grid walk would visit.
- RowValues — a single pass over the (typically few) columns with anything
  set, computing `sum_j n_j * 3^j` directly, with no per-bit decoding.
- Dynamic — walks only its own currently allocated capacity, which (after a
  conversion) is sized just large enough to cover the set bits.
- Scalar — the stored value already *is* the total (`3^0 = 1`), so this is
  just returning it, no computation at all.

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

Addition is purely value-returning — there is no `add()` method and no
`operator+=`, only `operator+`, which never mutates either operand.
Scalars can't be added either; use `setValue()` to build a plain number
first, or the Scalar representation (below), and add that. Each concrete
type defines its own `operator+`:

```cpp
friend SmoothInteger operator+(SmoothInteger a, const SmoothInteger& b);
friend SmoothFloat operator+(SmoothFloat a, const SmoothFloat& b);
friend Signed<Base> operator+(Signed<Base> a, const Signed<Base>& b);  // both signed types
```

Each is a **hidden friend**: a `friend` function defined inline inside the
class body, found only via argument-dependent lookup on its own operand
types. That's needed because the in-place building block it calls,
`SmoothNumberBase::addMatchingInPlace()` (and, for signed types,
`subtractMagnitudeInPlace()`), is `protected` — not part of the public
API, since it mutates in place — and a plain free function couldn't reach
a protected member, but a friend defined inside a derived class can,
through an object of that derived type, per ordinary protected-access
rules. `operator+` copies its left operand (`a`, taken by value — this is
what the copy constructor mentioned above exists for), mutates the copy in
place using that protected primitive, and returns it.

**Representations must match.** `addMatchingInPlace()` throws
`std::invalid_argument` unless `a.canonical() == b.canonical()` — there is
no implicit reconciliation between two different representations the way
`ensure()`'s `forEachSet`-based conversion provides elsewhere. In practice
every freshly constructed number starts out (and, for now, stays)
canonical on `Dynamic`, so two independently constructed numbers always
match; the check exists for when that stops being universally true (e.g.
if a future heuristic — or a future public API — ever picks a different
representation for some numbers).

**Unsigned types'** `addMatchingInPlace()` (in `SmoothNumberBase`)
dispatches straight to the canonical representation's `addInPlace(other)`
— each representation adds the 1s and handles carries however is natural
for its own storage:

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
- Scalar adds the two stored values directly when `other` is also a
  `ScalarRepresentation`; otherwise it decomposes `other`'s bits via
  `forEachSet`, throwing if any of them fall outside column 0.

**Signed types** need actual signed arithmetic, since the bit grid is
magnitude-only and the sign lives in `Signed<Base>`'s own flag:
`Signed<Base>::operator+` combines magnitudes via `Base::addMatchingInPlace()`
when both signs match, and otherwise subtracts the smaller magnitude from
the larger and takes the larger operand's sign (a result of exactly zero
is normalized back to non-negative). The subtraction step uses a second,
protected primitive, `SmoothNumberBase::subtractMagnitudeInPlace()` (which
enforces the same matching-representation rule), not exposed publicly
since plain subtraction has no meaning for the two unsigned types.

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

## Multiplication

`operator*` gets exactly the same treatment as `operator+`: purely
value-returning (no `multiply()`, no `operator*=`), no scalars, and the
same hidden-friend/protected-`...MatchingInPlace()`/matching-representation
machinery, all for the same reasons described under "Addition" — so this
section only covers what's different: the math, and each representation's
strategy for it.

`(2^i1 * 3^j1) * (2^i2 * 3^j2) = 2^(i1+i2) * 3^(j1+j2)`: multiplying two
smooth numbers means pairing up *every* term of one with *every* term of
the other and adding exponents. `SmoothNumberBase::multiplyMatchingInPlace()`
dispatches to the canonical representation's `multiplyInPlace(other)`:

- Sparse and Dynamic have no more direct way to multiply than forming
  every pairwise sum of exponents and carrying each one in — both share
  the `multiplyBitsWithCarry(dst, a, b)` helper (`representation_base.hpp`),
  which captures both operands' terms up front (so `x.multiplyInPlace(x)`,
  squaring `x`, is safe) before resetting `dst` and carrying each pairwise
  term in via `addSingleBitWithCarry()` — the same one-term carry step
  `addBitsWithCarry()` also uses, extracted out so addition and
  multiplication share it. Notably, this is the *same* strategy for both
  Sparse and Dynamic: being a raw bit grid rather than a `std::set` doesn't
  change anything about it, so neither needs to convert to the other (or
  to anything else) just to multiply.
- RowValues instead convolves column totals: this is the same operation as
  multiplying two polynomials in the variable 3, or long multiplication in
  base 3 (except a "digit" `n_j` can be any magnitude, not just `0..2`) —
  the product's column `j1 + j2` gets `n_j1 * n_j2` added in, for every
  pair of columns `(j1, j2)`. When `other` is also a `RowValuesRepresentation`
  its columns are used directly; otherwise its column totals are first
  computed via `forEachSet`.
- Scalar multiplies its one stored value directly by `other`'s when
  `other` is also a `ScalarRepresentation`; otherwise `other`'s column `j`
  contributes a term at column `0 + j = j`, so this throws unless every
  such `j` is `0` — except when this Scalar's own value is exactly `0`,
  since `0 * anything` is `0` regardless of `other`'s shape, so that case
  never throws.

Signed multiplication is simpler than signed addition: there's no
subtraction to worry about; the result's sign is just whether exactly one
operand was negative (the usual sign-XOR rule), with a zero product
normalized back to non-negative the same way a zero sum is.

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

It's deliberately generic — any named event can be tallied on it.
`SmoothNumberBase` uses it to count representation conversions: every time
`ensure()` actually converts (not when the target is already valid), it
increments `convert_<from>_to_<to>`, e.g. `convert_dynamic_to_row_values`.
Each representation also instruments its own internal work directly (see
below), and since `SmoothNumberBase` constructs its four representations
with the same `Metrics` it was given, those counters land on the same
object automatically — no separate wiring needed.

A number's `Metrics` is optional (`nullptr` by default) and, when given, is
*shared*, not copied: `hasMetrics()`, `metricsPtr()`, and `setMetricsPtr()`
expose the underlying `std::shared_ptr<Metrics>`, so two numbers
constructed with the same `Metrics` object tally onto the same counters.
Copying or moving a number carries its `Metrics` pointer along (still
shared with the original), consistent with everything else about
`SmoothNumberBase`'s copy semantics.

**`a + b` keeps a's metrics, unless a has none, in which case it falls back
to b's (if b has any).** For the two unsigned types this falls out
directly: `operator+` copies `a` (carrying `a`'s metrics along) and adds
`b` into that copy without `addMatchingInPlace()` ever touching
`metrics_`, so the copy still has whatever `a` had; only if that's
`nullptr` does it adopt `b`'s. `Signed<Base>::operator+` has to be more
deliberate about it: its "different signs, `|a| < |b|`" branch builds the
result out of a copy of `b` (to get at `b`'s larger magnitude before
subtracting), which would otherwise silently carry `b`'s metrics through
regardless of what `a` had. So it captures the correct choice (`a`'s,
falling back to `b`'s) once up front, before any branch runs, and stamps
it onto the final result at the end, regardless of which branch ran.

### Instrumentation counters: `carries`, `bit_operations`, `scalar_operations`, `bit_iterations`

Beyond conversions, each `RepresentationBase` implementation instruments
its own add/multiply/loop work directly, when constructed with a
`Metrics` (each one now takes an optional `std::shared_ptr<Metrics>` as a
trailing constructor argument, exactly like `SmoothNumberBase` and `Plan`):

- **`carries`** — one per ripple-carry step. Adding a bit into a cell that's
  already occupied moves that bit up to the next row instead
  (`addSingleBitWithCarry` in `representation_base.hpp`); each such step,
  for both `SparseRepresentation` and `DynamicMatrixRepresentation`
  (the two raw-bit-grid representations that share this helper), counts one
  carry.
- **`bit_operations`** — one per `(termA, termB)` pairing during a
  multiply, in `multiplyBitsWithCarry` (also shared by Sparse and
  DynamicMatrix): an *n*-bit by *m*-bit multiplication pairs every term of
  one with every term of the other, so it's *n\*m* bit operations.
- **`scalar_operations`** — one per plain integer/float multiply or add,
  for the two representations that do arithmetic on raw numbers rather
  than bits: `RowValuesRepresentation` (each call to its shared
  `accumulate()` helper — the core of `set()` and `addInPlace()` — is one
  operation; its convolution-based `multiplyInPlace()` counts 2 per
  column pair, one multiply and one add) and `ScalarRepresentation`
  (`set()`'s `value_ += delta`, and `addInPlace()`/`multiplyInPlace()`'s
  scalar-to-scalar fast paths, are each one operation).
- **`bit_iterations`** — one per cell visited while looping over a
  representation's contents, where "cell" means something different per
  representation, matching what it actually stores: for
  `SparseRepresentation`, one per coordinate in its `std::set` (i.e. only
  the 1s — it has no notion of the 0s in between); for
  `DynamicMatrixRepresentation`, one per cell of its currently allocated
  capacity, 1s and 0s alike (in `value()`, `print()`, `forEachSet()`, and
  `growToFit()`'s copy loop); for `RowValuesRepresentation`, one per
  column entry (i.e. one per stored "row" total `n_j`, regardless of that
  row's magnitude) in `value()`, `print()`, `forEachSet()`, and both loop
  levels of `multiplyInPlace()`'s convolution. `ScalarRepresentation`
  holds a single value with nothing to loop over, so it has no
  `bit_iterations` at all.

`setColumnValue()`/`setValue()` (encoding a fresh number directly into a
representation) and the bit-decomposition loops inside `forEachSet()` for
RowValues/Scalar (recovering individual bits from a stored total) are
deliberately *not* instrumented — they're decoding/encoding a value, not
looping over or arithmetically combining an existing one.

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
- `operator+(Signed<Base>, const Signed<Base>&)` — proper, value-returning
  signed addition; see "Addition" above.
- `operator*(Signed<Base>, const Signed<Base>&)` — value-returning signed
  multiplication (the sign-XOR rule); see "Multiplication" above.

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

## Plan

```cpp
#include "smooth/smooth.hpp"

double result = smooth::Plan()
    .scalar(3)
    .times()
    .left()
      .scalar(4)
      .plus()
      .scalar(2)
    .right()
    .calculate();  // 3 * (4 + 2) = 18
```

`smooth::Plan` (`include/smooth/plan.hpp`) is a fluent builder for a scalar
arithmetic expression over 3-smooth numbers. Building never computes
anything — it just records an expression tree; compiling that tree into a
concrete sequence of steps (and, for now, actually running the arithmetic)
happens lazily, the first time `plan()` or `calculate()` is called.

- `scalar(double)` — a leaf holding a plain value.
- `number(const T&)` — a leaf holding an existing `SmoothNumberBase`-derived
  object's value. Templated specifically so `T::value()` resolves at `T`'s
  own concrete type — required for a signed `T`, whose `value()`
  intentionally hides (isn't a virtual override of)
  `SmoothNumberBase::value()`; calling it through a `SmoothNumberBase&`
  would silently drop the sign.
- `plus()` / `times()` — wraps whatever's been built so far at the current
  nesting level into a new operator node, as its left side, and expects the
  next thing built to become its right side. Without any `left()`/`right()`
  grouping, this makes a plain chain **left-associative**, like a simple
  calculator: `scalar(3).times().scalar(4).plus().scalar(2)` computes
  `(3 * 4) + 2 = 14`, since each operator wraps the *entire* accumulated
  result so far, not just the value immediately before it.
- `left()` / `right()` — open and close a nested group, the way `(` and `)`
  do. `left()` always starts a fresh, independent sub-expression; `right()`
  always finishes the most recently opened one and plugs its completed
  value into whichever slot is open one level up. Which slot that is — a
  pending operator's still-empty right side, or the top-level result — is
  just whatever's actually open there; `left()`/`right()` name the bracket
  pair, not a side of the parent operator (which is why, in the example
  above, `left()` is what opens the group that ends up as `times()`'s
  right-hand operand). This is what makes `3 * (4 + 2)` possible at all —
  without it, the plain left-associative chain would give `(3 * 4) + 2`
  instead.
- `calculate()` — the computed result, as a `double`.
- `plan(os = std::cout)` — prints the compiled steps as a tree, e.g. (for
  the example above):
  ```
  multiply
  ├─ convert to scalar: 3
  └─ add
     ├─ convert to scalar: 4
     └─ convert to scalar: 2
  = 18
  ```

`Plan` itself computes by converting every leaf to a plain scalar and
evaluating the whole thing with ordinary `double` arithmetic — it doesn't
try to pick a smarter representation (Sparse, RowValues, Dynamic, Scalar)
for the actual computation the way the rest of this library does. That's
what three `protected virtual` methods are for, each with a sensible
default in `Plan` itself:

- `name() const` — public; identifies which strategy is in use ("scalar"
  in `Plan` itself). Drives the `"convert to <name>"` leaf label in
  `plan()`'s tree and the `convert_to_<name>` metrics counter, so
  overriding it alone already shows up in both places.
- `convertLeaf(double raw) const` — converts a leaf's raw value into
  whatever this variant actually computes with, returned back out as a
  `double` (the type `plan()`/`calculate()` deal in regardless of
  subclass). The default is a no-op — a plain `double` already *is* how
  `Plan` computes.
- `combine(Op op, double left, double right) const` — combines two
  already-converted values with `Op::Add`/`Op::Multiply`, again returning
  a `double`. The default is ordinary `double` arithmetic.

A subclass overriding `convertLeaf()`/`combine()` to round-trip through a
real `RepresentationBase` — using its own `setColumnValue()` to encode a
leaf and its own `addInPlace()`/`multiplyInPlace()` to combine two — is
what "converts everything to `<representation>` and computes that way"
means in practice; see "plan_zoo" below for three such subclasses.

Calling something out of order (two values with no operator between them,
an operator with nothing built yet, an unmatched `left()`/`right()`,
`calculate()`/`plan()` on an incomplete expression) throws
`std::invalid_argument`.

Like every concrete `SmoothNumberBase`-derived type, `Plan`'s constructor
takes an optional shared `std::shared_ptr<Metrics>`
(`Plan(metrics)`/`Plan()`, `hasMetrics()`, `metricsPtr()` — see "Metrics"
above). Compiling increments one counter per step —
`convert_to_<name()>`, `add`, or `multiply` — mirroring how
`SmoothNumberBase` counts each representation conversion, so a `Metrics`
shared between a `Plan` and the numbers that feed it (via `number()`)
tallies both under the same counters.

## plan_zoo

`include/smooth/plan_zoo/` holds `Plan` subclasses, each overriding
`name()`/`convertLeaf()`/`combine()` to compute via a specific
`RepresentationBase` instead of `Plan`'s default plain `double`
arithmetic — nothing else about `Plan` changes; building, `plan()`,
`calculate()`, `Metrics`, and error-handling are all inherited as-is.
`include/smooth/plan_zoo.hpp` is a convenience header pulling in all of
them, mirroring `smooth.hpp`. For now there are three, with more meant to
follow as this library explores which representation is actually fastest
for what:

- `SparsePlan` (`name()` → `"sparse"`) — `SparseRepresentation`.
- `MatrixPlan` (`name()` → `"matrix"`) — `DynamicMatrixRepresentation`,
  called "matrix" here since that's what this library calls its
  grid-shaped representation now that the old fixed-size
  `MatrixRepresentation` has been superseded by it.
- `RowValuesPlan` (`name()` → `"row_values"`) — `RowValuesRepresentation`.

```cpp
#include "smooth/plan_zoo.hpp"

smooth::SparsePlan p;
p.scalar(3).times().left().scalar(4).plus().scalar(2).right();
p.plan();
// multiply
// ├─ convert to sparse: 3
// └─ add
//    ├─ convert to sparse: 4
//    └─ convert to sparse: 2
// = 18
```

Each representation is magnitude-only (`RepresentationBase` can never hold
a negative value — the same reason `SmoothNumberBase::setValue()` rejects
a negative value for the two unsigned types), so a negative leaf on any of
these three throws `std::invalid_argument`; `Plan` itself has no such
restriction, since it never touches a representation at all. All three —
and `Plan` itself — share a common base pointer: `std::unique_ptr<Plan>`
holding any of them dispatches `name()`/`calculate()`/etc. virtually and
destructs safely, since `Plan` has a virtual destructor.

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
(including a negative value on a signed type), value-returning `operator+`
and `operator*` for both a plain and a signed case, viewing a number
through Dynamic/Sparse/RowValues plus `printScalar()` throwing on a number
with a multi-column term, using `DynamicMatrixRepresentation`,
`SparseRepresentation`, `RowValuesRepresentation`, and
`ScalarRepresentation` directly (since a `SmoothNumberBase`'s canonical
representation always starts out, and for now stays, Dynamic) to run each
representation's own `addInPlace()`/`multiplyInPlace()` strategy, including
a `Sparse` carry (for both addition and a carry-colliding multiplication),
a `RowValues` convolution, and Scalar's own throw for a non-column-0 term,
attaching a `Metrics` to a number to show its conversion counters and the
`a + b` metrics-inheritance rule, comparing the `carries`/`bit_operations`/
`scalar_operations`/`bit_iterations` counters a single add-then-multiply
produces on Sparse, DynamicMatrix, and RowValues directly, and building a
`Plan` (the confirmed
`3 * (4 + 2)` example, an unbracketed left-associative chain, and
`number()` correctly capturing a signed operand's sign) and printing it,
including a `Metrics` shared between a `Plan` and a `SmoothInteger` it
reads via `number()`, tallying both under the same counters; and, from
`plan_zoo`, `SparsePlan`/`MatrixPlan`/`RowValuesPlan` all computing the
same expression (each `plan()`-printed under its own `name()`), plus one
used polymorphically through a `Plan*`.

`tests/test_smooth.cpp` is a small, dependency-free assertion-based test
suite (no test framework linked in — see `CMakeLists.txt`) covering all of
the above: bounds/fractional restrictions, `setValue()`, signed
sign-handling, agreement across Dynamic/Sparse/RowValues plus
`printScalar()`'s success/throw cases, each `RepresentationBase`
implementation exercised directly (including `clone()` independence,
`Sparse`'s addition and multiplication carries, `RowValues`'s convolution,
and `ScalarRepresentation`'s column-0 restriction, including its `0 *
anything` exemption), value-returning unsigned and signed addition
(same-sign, both differing-sign directions, the zero tie, and the
cross-column/mixed-radix borrow case — and that neither operand is ever
mutated), value-returning unsigned and signed multiplication (including
the sign-XOR rule and a zero product's sign normalization), that both
addition and multiplication require matching representations, `Metrics`
counters (including that redundant conversions aren't double-counted, and
the `a + b` metrics-inheritance rule — notably that the signed swap branch
still keeps a's metrics), the `carries`/`bit_operations`/
`scalar_operations`/`bit_iterations` counters pinned down independently
against each representation directly (a single and a multi-step carry
chain, an *n*-by-*m* bit-operation count, each representation's own
notion of a "cell" for `bit_iterations`, and both `scalar_operations`
sources for RowValues and Scalar), copy/move semantics, and `Plan` (the confirmed
example, unbracketed left-associative chaining, `number()`'s sign
correctness, nested `left()`/`right()` groups two levels deep, `plan()`'s
tree rendering, every usage-error case, and its own `Metrics` support —
one counter per compiled step, memoized compilation, and propagation to a
`SmoothNumber` sharing the same `Metrics`), and `plan_zoo` (checked
generically against all three subclasses: `name()`, that each computes
`3 * (4 + 2) = 18` via its own representation, that `plan()` labels every
leaf with that representation's name, that a negative leaf throws, its
`Metrics` counter name, that the base `Plan`'s `name()` is unaffected, and
polymorphic dispatch/destruction through a `Plan*`). It builds as a second
executable, `smooth_tests`, runnable directly or via `ctest`.

## Profiles: measuring the counters

```sh
cmake -S . -B build   # requires SQLite3 (find_package(SQLite3 REQUIRED))
cmake --build build
./build/smooth_profiles              # writes/reads ./smooth_profiles.db
./build/smooth_profiles some/path.db # or an explicit database path
```

`include/smooth/profiles.hpp` defines a fixed set of named `Profile`s —
each just a `name` and a `build(Plan&)` function written directly in terms
of `Plan`'s own builder methods (`scalar()`/`plus()`/`times()`/`left()`/
`right()`). Because every `Plan` subclass shares that exact interface,
the same `build()` works unchanged against a `Plan&`, `SparsePlan&`,
`MatrixPlan&`, or `RowValuesPlan&` — it's the same expression, measured
identically across every representation.

The profiles are meant to look like ordinary, everyday arithmetic — sums
and (price × quantity)-style products, not edge cases (no zeros,
negatives, or single-leaf expressions) — ranging from `two_number_sum`
and `two_number_product` (2 numbers) up through `weighted_basket` and
`nested_score_totals` (6-8 numbers, mixing both operators and, for
`nested_score_totals`, two levels of nested `left()`/`right()` groups) to
`ten_day_totals` (10 numbers, the upper end of "typical" this project is
using to see how the counters scale).

`src/profile_runner.cpp` (the `smooth_profiles` executable) runs every
(plan kind, profile) combination — `scalar`/`sparse`/`matrix`/
`row_values` × every `Profile` — against a SQLite database, skipping any
combination already present so re-running only does work for newly added
plan kinds or profiles. Each row records the plan kind, the profile name,
the computed result, and one column per counter `Plan`-driven work can
currently produce: `convert_to_scalar`/`convert_to_sparse`/
`convert_to_matrix`/`convert_to_row_values`, `add`, `multiply`, `carries`,
`bit_operations`, `scalar_operations`, and `bit_iterations` (the
per-representation `convert_<X>_to_<Y>` counters — see "Metrics" — can't
appear here, since `Plan`/`plan_zoo` build a fresh representation per leaf
directly rather than routing through `SmoothNumberBase::ensure()`).

The set of columns is fixed on purpose: **when a counter is added, removed,
or renamed, delete the database file and let it be recreated from
scratch**, rather than migrating it in place. `smooth_profiles` checks the
database's actual columns against what the current build expects on every
run and refuses to proceed (with that same instruction) if they don't
match, so a stale database is a hard error, not silently-wrong data.
