# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

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

To implement the `Grid` interface, subclass `Trax::Grid` and implement `x()`, `y()`, `operator()`, `set()`, `valid()`, `width()`, `height()`. See `test/TestGrid.h` for a concrete example.

### Contouring Pipeline

The pipeline flows: **Grid** -> **CellBuilder** -> **JointMerger** -> **Topology** -> **GeometryCollection**, coordinated per-level by `Builder`.

1. **Grid cell processing** (`CellBuilder`): For each 2x2 cell, classify using marching-squares case logic and produce edge intersection vertices. Three interpolation modes: `Linear`, `Midpoint`, `Logarithmic`. Midpoint isolines are not supported (intentional no-op in `Impl::isoline`).

2. **Joint merging** (`JointMerger` / `JointPool`): Vertices from adjacent cells are connected into linked chains via `Joint` nodes (prev/next/alt pointers). `JointPool` is a custom arena allocator with geometrically-growing blocks (starting at 16K joints, up to 27 doublings) that avoids per-vertex heap allocation while keeping all pointers stable. `JointMerger` merges joints row-by-row then cell-by-cell, connecting matching vertices across shared cell edges.

3. **Topology building** (`Topology.h`): `build_rings()` assembles connected joints into closed rings (`Polyline`), then `build_polygons()` classifies them as shells or holes and groups into `Polygon` objects based on containment.

4. **Output**: `GeometryCollection` holds the resulting polygons (isobands) or polylines (isolines) and provides WKT/WKB serialization.

### Multi-threading

`Contour::threads(n)` enables parallel contouring (0 = auto, 1 = single-threaded default, N>1 = N threads). The sorted isoband/isoline levels are split into chunks across threads, each running an independent `Impl` with its own `Builder` objects. Results are mapped back to original positions after completion. Uses `std::async` with `std::launch::async`.

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
