# smooth

A little library to play with 3-smooth numbers.

A 3-smooth number is one whose only prime factors are 2 and 3, i.e. it can be
written as a sum of terms of the form `2^i * 3^j`. This library represents
such a number as a bit matrix: the `(i, j)` entry is `1` if the term
`2^i * 3^j` is included in the number, `0` otherwise. The number's value is
the sum of `2^i * 3^j` over every set entry.

## `smooth::SmoothNumber`

Header-only class in `include/smooth/smooth_number.hpp`. It owns three
internal representation objects (see below) and delegates to whichever one
is canonical through the `RepresentationBase` interface
(`include/smooth/representation_base.hpp`).

- `SmoothNumber(max_rows, max_cols)` — whole numbers only. Row index `i`
  ranges over `[0, max_rows)` (power of 2), column index `j` ranges over
  `[0, max_cols)` (power of 3). All entries start at `0`.
- `SmoothNumber(max_rows, max_cols, neg_rows, neg_cols)` — also allows
  fractional values: `i` ranges over `[-neg_rows, max_rows)` and `j` over
  `[-neg_cols, max_cols)`, so a negative index contributes a negative power
  (e.g. `i = -1` is a factor of `1/2`, `j = -1` is a factor of `1/3`).
- `set(i, j, value = true)` — set (or clear) the bit at `(i, j)`. `i`/`j` are
  signed so negative indices can be addressed.
- `clear(i, j)` — clear the bit at `(i, j)`.
- `get(i, j)` — read the bit at `(i, j)`.
- `value()` — return the sum of `2^i * 3^j` over all set bits (as a
  `double`; precision degrades for large row/column counts or deeply
  negative indices). Uses a strategy suited to whichever representation is
  currently canonical (see below) rather than always walking the full grid.
- `rows()` / `cols()` — non-negative capacity (`max_rows` / `max_cols`).
- `negRows()` / `negCols()` — negative capacity (`0` for the two-argument
  constructor).

Out-of-bounds `set`/`get`/`clear` calls throw `std::out_of_range`.

### Internal representations

The same number can be held in one of several internal representations,
each a class implementing `RepresentationBase`
(`get`/`set`/`reset`/`value`/`print`):

- **Matrix** (`matrix_representation.hpp`) — the `(i, j)` grid of bits, as
  described above.
- **Sparse** (`sparse_representation.hpp`) — the set of `(i, j)` coordinates
  whose bit is set.
- **RowValues** (`row_values_representation.hpp`) — one number `n_j` per
  column `j`, where `n_j = sum_i (bit(i,j) ? 2^i : 0)`, so the total value
  is `sum_j n_j * 3^j`. `n_j` is an integer if the matrix has no negative
  row capacity, and may be fractional otherwise.

Exactly one representation is **canonical** — the trusted, up-to-date copy.
`set`/`get`/`value()` always operate through the canonical representation. A
`set` call that actually changes a bit invalidates the other
representations (a `set` to the same value it already had does not); an
already-canonical representation is never redundantly reconverted. Which
representation is canonical, and when (if ever) that changes, is decided
internally — there is no public method to force it, by design.

- `canonical()` — which `Representation` (`Matrix`, `Sparse`, or
  `RowValues`) is currently canonical (read-only).
- `printMatrix(os = std::cout)` — converts to Matrix if needed, then prints
  the grid as rows of `0`/`1`. If the matrix has negative row/column
  capacity, a horizontal and/or vertical line marks the boundary between the
  fractional entries (negative index) and the whole-number entries
  (non-negative index); the whole-number part is the block below and to the
  right of the lines.
- `printSparse(os = std::cout)` — converts to Sparse if needed, then prints
  the set of coordinates, e.g. `{(0, 2), (1, 0)}`.
- `printRowValues(os = std::cout)` — converts to RowValues if needed, then
  prints each `n_j`, one per line.

`value()` calls straight through to the canonical representation's own
`value()`, so each avoids doing more work than it needs to:

- Matrix — walks the full `(i, j)` grid (no shortcut to skip zeros).
- Sparse — sums only over the set of coordinates that are actually set,
  skipping the (typically many) zero cells a full grid walk would visit.
- RowValues — a single O(cols) pass computing `sum_j n_j * 3^j` directly,
  since each `n_j` already sums its row's contribution.

This is a Strategy pattern: `SmoothNumber` only ever talks to
representations through the `RepresentationBase` interface (an
`std::array<std::unique_ptr<RepresentationBase>, 3>`), and generic
operations like `ensure()` (rebuild an outdated representation from the
canonical one, cell by cell) are written purely in terms of that interface.
Adding a fourth representation means writing one new class that implements
`RepresentationBase`, adding an enumerator to `Representation`, and adding
one line to register it in the constructor — no existing logic needs to
change.

## Building the demo

```sh
cmake -S . -B build
cmake --build build
./build/smooth_demo
```

`src/demo.cpp` shows constructing a matrix, setting bits, printing it,
reading its value, and viewing the same number through the
Matrix/Sparse/RowValues representations.
