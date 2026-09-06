# smooth

A little library to play with 3-smooth numbers.

A 3-smooth number is one whose only prime factors are 2 and 3, i.e. it can be
written as a sum of terms of the form `2^i * 3^j`. This library represents
such a number as a bit matrix: the `(i, j)` entry is `1` if the term
`2^i * 3^j` is included in the number, `0` otherwise. The number's value is
the sum of `2^i * 3^j` over every set entry.

## `smooth::SmoothNumber`

Header-only class in `include/smooth/smooth_number.hpp`.

- `SmoothNumber(max_rows, max_cols)` — construct a `max_rows x max_cols`
  matrix, all entries initialized to `0`. Row index `i` corresponds to the
  power of 2, column index `j` to the power of 3.
- `set(i, j, value = true)` — set (or clear) the bit at `(i, j)`.
- `clear(i, j)` — clear the bit at `(i, j)`.
- `get(i, j)` — read the bit at `(i, j)`.
- `print(os = std::cout)` — print the matrix as rows of `0`/`1`.
- `value()` — return the sum of `2^i * 3^j` over all set bits (as a
  `double`; precision degrades for large row/column counts).
- `rows()` / `cols()` — matrix dimensions.

Out-of-bounds `set`/`get`/`clear` calls throw `std::out_of_range`.

## Building the demo

```sh
cmake -S . -B build
cmake --build build
./build/smooth_demo
```

`src/demo.cpp` shows constructing a matrix, setting bits, printing it, and
reading its value.
