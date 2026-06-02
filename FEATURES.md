# smartmet-library-trax — Feature List

A structured inventory of capabilities provided by the trax library.
Use as a checklist when drafting release notes. When new functionality
is added, append the new entry under the matching section (and bump
the *Last updated* line at the bottom).

`smartmet-library-trax` is a high-performance C++ marching-squares
contouring library used across the SmartMet ecosystem. It generates
**isolines** (contour lines) and **isobands** (filled contour
polygons) from 2-D gridded scalar data. All public types live in the
`Trax::` namespace. The library produces `libsmartmet-trax.so` and is
linked by `smartmet-engine-contour`, several WMS/EDR layers, and grid
tools.

---

## 1. Core contouring API

- **`Trax::Contour`** — top-level engine (pimpl over `Impl`).
- **`Contour::isobands(grid, limits)`** — generate filled bands
  between value ranges.
- **`Contour::isolines(grid, values)`** — generate contour lines at
  threshold values.
- **`Trax::IsobandLimits`** — list of `(lo, hi)` range pairs,
  including open-ended bounds via `±infinity`.
- **`Trax::IsolineValues`** — list of threshold values.
- **`Trax::GeometryCollection`** — result holder (one per requested
  level).

## 2. Grid abstraction

- **`Trax::Grid`** — abstract grid interface: `x()`, `y()`,
  `operator()`, `set()`, `valid()`, `width()`, `height()`.
- **Caller-provided subclass** — bring your own grid layout; the
  library never copies data.
- **Sample implementation** — `test/TestGrid.h`.
- **`Trax::GridPoint`** — single grid sample with cell indices.
- **`Trax::Grid::shift()` / `Grid::shell()`** — global wrap-around
  control and midpoint-interpolation boundary control.

## 3. Marching-squares core

- **Marching-squares case logic** in `CellBuilder`.
- **Per-cell `Cell`** — four `GridPoint` corners (BL / AL / AR / BR)
  with grid indices.
- **`SmallVector<Vertex, 8>`** — stack-allocated capacity-8 vector
  for cell output (max 8 vertices per cell).
- **`Trax::Joint`** — linked chain node (prev/next/alt pointers).
- **`Trax::JointPool`** — custom arena allocator with geometrically-
  growing blocks (16K initial, up to 27 doublings); avoids per-vertex
  heap allocation while keeping pointers stable.
- **`Trax::JointMerger`** — connects matching vertices across shared
  cell edges, row-by-row then cell-by-cell.
- **`Trax::Topology`** — `build_rings()` assembles joints into
  closed `Polyline`s, `build_polygons()` classifies shells vs holes
  and groups them into `Trax::Polygon`s based on containment.
- **`Trax::Place`**, **`Trax::Vertex`**, **`Trax::VertexType`** —
  topology primitives.

## 4. Interpolation modes

- **`Trax::InterpolationType::Linear`** — standard linear
  interpolation along cell edges.
- **`Trax::InterpolationType::Midpoint`** — nearest-neighbour /
  midpoint interpolation; supports isobands. **Isolines in midpoint
  mode are an intentional no-op.**
- **`Trax::InterpolationType::Logarithmic`** — logarithmic
  interpolation along edges.

## 5. Cell subdivision

- **`Contour::subdivide(n)`** — replace the straight marching-squares
  diamond between edge intersections with `n-1` samples along the
  **true bilinear level curve**.
- **`n` up to 10** — higher values produce visibly smoother
  isolines/isobands at increased cost.
- **Per-cell bilinear sampling** — the level curve is solved
  analytically inside each 2×2 cell.

## 6. NaN / missing-value handling

- **NaN as missing** — NaN values in the grid are treated as data
  gaps.
- **Explicit missing-value isobands** — supply a NaN range in
  `IsobandLimits`; trax produces an isoband covering the NaN region
  (always sorted to index 0).
- **Explicit missing-value isolines** — supply a NaN in
  `IsolineValues`.
- **Per-cell NaN gating** — `valid()` from the user grid lets the
  caller short-circuit invalid cells.

## 7. Open-ended isobands

- **`±infinity` in `IsobandLimits`** — supports unbounded ranges,
  e.g. `(-∞, 0]` or `[100, +∞)`.
- **Edge handling** — wrap-around grids and pole proximity are
  handled by `Grid::shift()` / `shell()`.

## 8. Multi-threading

- **`Contour::threads(n)`** — `0` = auto, `1` = single-threaded
  (default), `n > 1` = `n` worker threads.
- **Per-thread `Impl`** — each thread has its own `Builder`,
  `JointPool`, and `Topology`.
- **Level chunking** — sorted isoband / isoline levels are split
  into chunks; threads run independently via `std::async` and
  results are mapped back to original positions.

## 9. Output formats

- **`GeometryCollection::wkt()`** — Well-Known Text.
- **`GeometryCollection::wkb()`** — Well-Known Binary.
- **`Trax::to_ogr_geom()`** (`OGR.h`) — convert to `OGRGeometry`.
- **`Trax::to_geos_geom()`** (`Geos.h`) — convert to GEOS geometry.
- **`Trax::Polygon` / `Polyline`** — direct access to geometry
  primitives if you don't need WKT/WKB/OGR/GEOS.
- **`Trax::BBox`** — bounding box of a result.
- **`Trax::Endian`** — explicit byte-order handling for WKB.

## 10. Performance optimisations

- **Spatial coherence pruning** — `Impl` tracks `m_min_index` /
  `m_max_index` per cell to narrow which contour levels need
  checking; adjacent cells with similar values reuse the bracket.
- **Arena allocation** — `JointPool` keeps the working set in
  contiguous blocks; no per-vertex `new`/`delete`.
- **Stack-allocated cell scratch** — `SmallVector<Vertex, 8>`
  avoids heap traffic in the inner loop.
- **`-O3` build** — production library compiled at the highest
  optimisation level.
- **x86-64 only** — Makefile forces `-march=x86-64` (not v3) to
  avoid rounding-related contouring errors that AVX2 introduces at
  `-O2` or higher.

## 11. Documentation

- **`docs/trax.md`** — full API and usage guide.

## 12. Testing

- **Boost.Test** unit tests (header-only via `included/unit_test.hpp`).
- **Per-test build**:
  `make && cd test && make CellTest && ./CellTest`.
- **Sanitiser variants**:
  - `make -C test ASAN=yes test` — address + UB sanitiser.
  - `make -C test TSAN=yes test` — thread sanitiser.
- **Reference test grid** — `test/TestGrid.h` is a worked example
  of the `Grid` interface.

## 13. Build & integration

- **Output**: `libsmartmet-trax.so`.
- **Build**: `make` (uses `-O3`).
- **Format**: `make format` runs clang-format.
- **Install**: `make install` (default `PREFIX=/usr`).
- **RPM**: `make rpm`.
- **Linked SmartMet libraries**: `smartmet-library-macgyver` (for
  `Fmi::Exception`).
- **External libraries**: GDAL, GEOS (with the unstable C++ API,
  `-DUSE_UNSTABLE_GEOS_CPP_API`), fmt 12.x.
- **CI**: CircleCI on RHEL 8 / RHEL 10 with the
  `fmidev/smartmet-cibase-{8,10}` Docker images.
- **Public headers** installed under `/usr/include/smartmet/trax/`.

---

*Last updated: 2026-06-01.*
