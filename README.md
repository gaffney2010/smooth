# smooth

A little library to play with 3-smooth numbers.

A 3-smooth number is one whose only prime factors are 2 and 3, i.e. it can be
written as a sum of terms of the form `2^i * 3^j`. This library represents
such a number as a bit matrix: the `(i, j)` entry is `1` if the term
`2^i * 3^j` is included in the number, `0` otherwise. The number's value is
the sum of `2^i * 3^j` over every set entry.

## `smooth::SmoothNumber`

Header-only class in `include/smooth/smooth_number.hpp`.

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
- `print(os = std::cout)` — print the matrix as rows of `0`/`1`. If the
  matrix has negative row/column capacity, a horizontal and/or vertical line
  is drawn to mark the boundary between the fractional entries (negative
  index) and the whole-number entries (non-negative index); the whole-number
  part is the block below and to the right of the lines.
- `value()` — return the sum of `2^i * 3^j` over all set bits (as a
  `double`; precision degrades for large row/column counts or deeply
  negative indices).
- `rows()` / `cols()` — non-negative capacity (`max_rows` / `max_cols`).
- `negRows()` / `negCols()` — negative capacity (`0` for the two-argument
  constructor).

Out-of-bounds `set`/`get`/`clear` calls throw `std::out_of_range`.

## Building the demo

```sh
cmake -S . -B build
cmake --build build
./build/smooth_demo
```

`src/demo.cpp` shows constructing a matrix, setting bits, printing it, and
reading its value.
