# trax developer guide

This guide is for developers who change `smartmet-library-trax`. trax turns gridded values
into isolines and isobands with marching squares; the contour engine uses it for every
contoured WMS layer, and the WFS plugin gets its contours through the engine too.

The usage and algorithm reference is [trax.md](trax.md): the API, missing-value handling,
the cell cases, and the performance design. [contouring-performance.md](contouring-performance.md)
records what has been measured and which optimisations were tried and rejected; read it
before starting on performance work. [CLAUDE.md](../CLAUDE.md) has the pipeline and
threading summary.

## Contents

1. [Building and testing](#1-building-and-testing)
2. [Where things are](#2-where-things-are)
3. [Changing the cell logic](#3-changing-the-cell-logic)
4. [Threads](#4-threads)
5. [Reproducibility](#5-reproducibility)
6. [Compatibility](#6-compatibility)
7. [Known pitfalls](#7-known-pitfalls)

---

## 1. Building and testing

```bash
make                                   # -O3, -march=x86-64 (see §5)
make test                              # Boost.Test programs in test/
make && make -C test CellTest && ./test/CellTest
make -C test ASAN=yes test             # or TSAN=yes
```

`test/TestGrid.h` is a small `Grid` implementation with a regular coordinate grid, which is
also the easiest way to try a case by hand. `ContourTest` compares full contouring results
as WKT; `CellTest` covers the individual cells.

## 2. Where things are

| File | Role |
|------|------|
| `Contour.cpp`, `Impl.cpp` | Public API; level sorting, the row loop, band-parallel dispatch, result assembly. |
| `CellBuilder.cpp` | The marching-squares cases for rectangles and triangles, for isobands and isolines, in linear, logarithmic and midpoint modes. Most of the code, and most of the risk. |
| `JointMerger.cpp`, `JointPool.cpp` | Connecting cell vertices across cell edges and rows; the arena of joints. |
| `Topology.cpp` | Building rings from joints and polygons (shells and holes) from rings. |
| `Polygon.cpp`, `Polyline.cpp`, `GeometryCollection.cpp` | Result geometry and WKT/WKB output. |
| `OGR.cpp`, `Geos.cpp` | Conversion to OGR and GEOS geometries. |
| `Smoother.cpp`, `SavitzkyGolay.cpp` | Optional smoothing of the results. |

## 3. Changing the cell logic

* Every cell case must produce edge intersections that are **bit-identical** to what the
  neighbouring cell produces for the same edge, or the joints do not match and rings break.
  Intersections are always computed with the edge endpoints in lexicographic order for this
  reason; keep using the existing helpers instead of computing intersections inline.
* Saddles, corners exactly on a limit, ridges (equal-valued edges), "melts" and NaN
  corners are special cases, each described in [trax.md](trax.md#how-the-algorithm-works).
  Add a test for the exact configuration when you change one of them.
* Isobands are half-open (`lo <= z < hi`); `closed_range(true)` includes the upper limit of
  the last finite band. Missing-value bands and lines (`Range(NaN, NaN)`, value `NaN`) are
  always sorted to index 0 and handled apart from the finite levels.
* Run the whole test suite with `strict(true)` and `validate(true)` in mind: `strict` throws
  on topology errors instead of dropping the fragment, `validate` checks every result with
  GEOS. Both are for testing only; `validate` is very slow.

## 4. Threads

Parallelism needs two settings (see [trax.md](trax.md#parallel-row-band-processing)):

* `Contour::set_worker_threads(n)` once per process, which creates the shared pool
  (threads `trax-1` … `trax-n`). In the server, the WMS plugin does this at start-up.
* `threads(k)` per request, which splits the grid into k row-bands.

Without the pool, `threads(k)` falls back to the single-threaded path. The result is the same
either way. A `Contour` object is used by one thread; the `Grid` is only read and may be
shared.

## 5. Reproducibility

The output must not depend on the machine or the thread count: the contour engine caches
results and the WMS tests compare images. The Makefile compiles with `-march=x86-64` instead
of RHEL 10's default `x86-64-v3` because FMA instructions change the rounding of the
intersection calculations, and band-parallel results are joined in a fixed order. Keep both
properties when optimising: compare the WKT of large test cases before and after, not only
the timings.

## 6. Compatibility

The installed headers are used by the contour engine (and through it by the plugins). The
`Grid` interface is virtual and implemented by the engine; changing it, `Contour`, or the
result types requires rebuilding the engine. Bump the spec version when the installed library
changes.

## 7. Known pitfalls

* **Infinite grid values are missing values.** `+inf` and `-inf` are treated like NaN when
  contouring and smoothing; infinite isoband limits are not affected.
* **Midpoint isolines are not supported**; `isolines()` with `InterpolationType::Midpoint`
  returns nothing.
* **Both threading settings are needed** for parallel contouring (§4).
* **Do not build with `-march=x86-64-v3`** or add FMA-producing flags (§5).
* **`validate(true)` is for debugging only**; it runs GEOS validation on every geometry.
