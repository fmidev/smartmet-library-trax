# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Full developer documentation: `docs/developer-guide.md`, `docs/trax.md` and `docs/contouring-performance.md`.

**smartmet-library-trax** is a high-performance C++ library for generating isolines (contour lines) and isobands (filled contour polygons) from 2D gridded scalar data using marching squares. Part of the SmartMet Server ecosystem by the Finnish Meteorological Institute (FMI). All types live in the `Trax` namespace.

## Build Commands

```bash
make                  # Build libsmartmet-trax.so (uses -O3)
make test             # Build and run all Boost.Test unit tests
make format           # Run clang-format on all source and test files
make clean            # Remove build artifacts
make rpm              # Build RPM package
make install          # Install library and headers (PREFIX=/usr)
```

Build depends on the shared SmartMet build configuration:
```bash
# Must be installed first (from smartbuildcfg):
$(PREFIX)/share/smartmet/devel/makefile.inc
```

### Running a Single Test

Tests link against `../libsmartmet-trax.so`, so build the library first:
```bash
make && cd test && make CellTest && ./CellTest
```

### Sanitizers

```bash
make -C test ASAN=yes test    # AddressSanitizer + UBSan
make -C test TSAN=yes test    # ThreadSanitizer
```

## Dependencies

- **smartmet-library-macgyver** (FMI utility library, provides `Fmi::Exception`)
- **GDAL**, **GEOS** (with unstable C++ API: `-DUSE_UNSTABLE_GEOS_CPP_API`), **fmt** (12.x)
- **Boost.Test** (test only, header-only via `included/unit_test.hpp`)

## Architecture

### Public API

The main entry point is `Contour` (pimpl pattern via `Impl`):

1. Create a `Contour` object and configure it (interpolation type, threading, etc.)
2. Provide a `Grid` subclass (virtual interface for grid data access) and either `IsobandLimits` (lo/hi range pairs) or `IsolineValues` (threshold values)
3. Call `isobands()` or `isolines()` to get `GeometryCollections` (one `GeometryCollection` per requested contour level)
4. Convert results via `wkt()`, `wkb()`, `to_ogr_geom()` (`OGR.h`), or `to_geos_geom()` (`Geos.h`)

To implement the `Grid` interface, subclass `Trax::Grid` and implement `x()`, `y()`, `operator()`, `set()`, `valid()`, `width()`, `height()`; `shift()`, `bbox()`, `shell()` and `xperiod()` are optional overrides. See `test/TestGrid.h` for a concrete example.

Optional pre-contouring smoothing: `Trax::smooth(grid, SmoothOptions)` (`Smoother.h`, methods Box, Median, Morphology and the legacy SavitzkyGolay; Pyramid is declared but throws).

### Contouring Pipeline

The pipeline flows: **Grid** -> **CellBuilder** -> **JointMerger** -> **Topology** -> **GeometryCollection**, coordinated per-level by `Builder`.

1. **Grid cell processing** (`CellBuilder`): For each 2x2 cell, classify using marching-squares case logic and produce edge intersection vertices. Three interpolation modes: `Linear`, `Midpoint`, `Logarithmic`. Midpoint isolines are not supported (intentional no-op in `Impl::isoline`).

2. **Joint merging** (`JointMerger` / `JointPool`): Vertices from adjacent cells are connected into linked chains via `Joint` nodes (prev/next/alt pointers). `JointPool` is a custom arena allocator with geometrically-growing blocks (starting at 16K joints, up to 27 doublings) that avoids per-vertex heap allocation while keeping all pointers stable. `JointMerger` merges joints row-by-row then cell-by-cell, connecting matching vertices across shared cell edges.

3. **Topology building** (`Topology.h`): `build_rings()` assembles connected joints into closed rings (`Polyline`), then `build_polygons()` classifies them as shells or holes and groups into `Polygon` objects based on containment.

4. **Output**: `GeometryCollection` holds the resulting polygons (isobands) or polylines (isolines) and provides WKT/WKB serialization.

### Multi-threading (band-parallel)

Contouring is parallelized over **horizontal row-bands**, not contour levels. The grid's
cell-rows are split into contiguous bands; each band is contoured independently into its own
sub-`Impl` (one `JointMerger`/`JointPool` per level) over a row sub-range via `Impl::contour_rows`
— so the grid is traversed exactly once. Adjacent bands are then stitched along their single
shared boundary row by `join_boundary` (a cross-pool application of `merge_row`'s matching), after
which each level's rings are assembled across all band pools by the multi-pool `build_rings` +
`build_polygons`. This scales with grid height and parallelizes single-level products (the old
level-split scheme could not, and was capped at the level count). Output is byte-identical to
single-threaded — bands are joined and scanned in fixed order.

Concurrency is bounded by a **process-wide worker pool**, injected once by the host via the static
`Contour::set_worker_threads(n)` (n = pool size; n == 0 disables the pool). Reuses `Fmi::ThreadPool`.
Per request, `Contour::threads(k)` requests `k` row-bands; band 0 runs on the calling thread and the
rest are dispatched to the shared pool (`run_bands`), so total contour threads never exceed the pool
size no matter how many requests run concurrently. The server should size the pool from a fraction
of `hardware_concurrency` and pass a small per-request `k` (≈3–8: contouring is only one phase of a
request, so more bands give diminishing end-to-end benefit).

Each shared-pool worker is named `trax-1` … `trax-N` (the pool is constructed with the name `trax`;
`Fmi::ThreadPool` names its workers `<name>-<n>`) so the band-parallel threads are identifiable in
`top`/`htop`/`gdb`. The host's calling thread runs band 0 inline and is never renamed.

**Both settings are required for parallelism** — they are independent knobs, and contouring runs
single-threaded unless *both* are in effect:

| | `threads()` not set (k=1) | `threads(k>1)` set |
|---|---|---|
| **no pool** (`set_worker_threads` not called / n=0) | single-threaded | single-threaded (short-circuits to the single-threaded engine — no band overhead) |
| **pool of N** | single-threaded (pool idle) | **parallel** on the pool |

This makes adoption incremental and safe: a call site may set `threads(k)` before the pool is wired
up (it just runs single-threaded), and enabling the pool does not change any request that has not
opted in via `threads(k)`. The default (neither set) is exactly the classic single-threaded path.

See `docs/contouring-performance.md` for optimization strategies that were tried and rejected.

### Key Design Details

- Grid coordinates use `double`; grid values use `float`. The `Vertex` type carries both world coordinates and grid indices (`column`, `row`) for fast merge lookups.
- `SmallVector<Vertex, 8>` is a stack-allocated fixed-capacity vector used for per-cell vertex output (max 8 vertices per cell).
- `Cell` holds four `GridPoint` corners (below-left, above-left, above-right, below-right) with grid indices for position tracking.
- NaN values in the grid represent missing data. The library can generate explicit missing-value isobands/isolines for NaN regions (enabled by adding a NaN range/value to limits, always sorted to index 0).
- `Grid::shift()` and `Grid::shell()` support global wrap-around grids and midpoint interpolation boundary control.
- `Impl` maintains `m_min_index`/`m_max_index` to narrow which contour levels need checking per cell, exploiting spatial coherence of adjacent cells.

## Platform Notes

- **x86-64 only**: The Makefile forces `-march=x86-64` (not v3) to avoid rounding-related contouring errors with AVX2 at `-O2` and above.
- CI runs on CircleCI with RHEL 8 and RHEL 10 Docker images.
