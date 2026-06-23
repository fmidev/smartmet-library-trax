# Trax contouring performance: what was tried, what worked, what to avoid

This is a record of optimization strategies investigated for the trax contouring pipeline, so
that ideas already measured and rejected are not attempted again in vain. Measurements come from
a convective-precipitation benchmark (2000×2000 grid, 300 Gaussian storms, 11 precip isobands)
unless noted, profiled with `perf`. The benchmark's correctness fingerprint (sum of WKT lengths)
is `20769599` for the 2000×2000 case.

## TL;DR

- **`build_polygons` (hole-to-shell assignment) is <1 % of runtime.** Do **not** try to speed it
  up — no algorithm change there can move the needle. The "~30 % of wall time" claim in the old
  `hole-assignment-redesign` design note is **wrong** for this workload.
- **The hot path is the per-cell marching-squares + joint merge** (`CellBuilder::isoband_linear`,
  `JointBuilder::add`, `JointMerger`) — ~60 % combined. That is where any future speedup must come
  from.
- **Vertices are already pooled and passed by pointer.** There is no copy bloat to remove.
- **Parallelize over row-bands, not contour levels.** Band-parallel scales with grid height and
  reaches ~5× on 24 cores; level-split capped at ~2.3× and could not parallelize single-level
  products.

## Profile (where the time actually goes)

`perf` self/inclusive time, single-threaded, on the benchmark:

| symbol | inclusive | self |
|---|---|---|
| `CellBuilder::isoband_linear` (marching squares) | ~38 % | ~10 % |
| `JointBuilder::add` (emit/dedup a vertex per crossing) | ~25 % | ~12 % |
| `JointMerger` (`merge_row` + `merge_cell` + `find_match`) | — | ~8 % |
| `Polyline::append` | — | ~5 % |
| `polygon_fingerprint` — **benchmark's own WKT check, not contouring** | ~17 % | — |
| **`build_polygons` — entire hole-assignment stage incl. R-tree + pnpoly** | **~0.96 %** | ~0.8 % |

Work scales with `cells × levels-crossed-per-cell`; the grid is already traversed once with
per-cell level-window narrowing (`update_isobands_to_check`). There is no per-level grid re-walk
to remove in the single-threaded path.

## Rejected: `build_polygons` hole-assignment rewrites

The current master assigns free holes to shells using a `boost::geometry` R-tree of shell
bounding boxes (query per hole, pnpoly-verify, pick innermost). Three alternatives were built and
measured:

| approach | result | verdict |
|---|---|---|
| **no-rtree** (linear bbox scan, drop boost.geometry rtree) | **~5 % slower**; ~3 MB smaller lib, fewer deps | only worth it for the dependency/size reduction, never for speed |
| **hole-assignment-redesign** (closure-order pool drain) | no faster than no-rtree; more code | not worth the complexity |
| **rtree-of-holes drain** (index holes, drain inner-shell-first, delete on assign) | correct; pnpoly drops to ~1 per hole (provably optimal); **wall-clock within noise of master** | elegant but pointless — see why below |

All produced byte-identical output (fingerprint `20769599`). The rtree-of-holes drain is the
theoretically best of the four (each hole point-in-polygon tested ~once, no innermost search), yet
it did **not** beat master, because **the whole stage is <1 % of runtime**. Instrumentation: per
level `build_polygons` sees up to ~200 shells / ~200 free holes, ≈1,350 pnpoly tests per rep — a
rounding error against 4 M cells. **Do not optimize hole assignment for performance.** (If the
`boost::geometry` rtree dependency is ever unwanted for its own sake, `no-rtree` is the cleanest
removal, at a ~5 % cost.)

## Rejected: pooling `Vertex` / passing vertices by pointer

Idea: give `Vertex` a memory pool like `JointPool` and store `Vertex*` in `Joint` to avoid copying
32-byte `Vertex` objects through the pipeline.

Why it does not help: most cell vertices are **cancelled** when adjacent cells merge
(`merge_cell` reuses the previous cell's shared-edge joints); the survivors each become exactly one
pooled `Joint`, which **embeds the `Vertex` by value**. So vertices are *already* pool-backed and
thereafter referenced only by `Joint*` (`prev/next/alt`); the merge comparison `j->vertex == v`
reads the embedded vertex by reference. There is exactly **one** necessary copy — stack scratch
buffer → pooled `Joint` — which any scheme must pay. A separate vertex pool would not remove that
copy and would add a cache-missing indirection to the hottest comparison in the pipeline.

The one *cache* (not copy) angle left: `Joint` is 64 B and carries `x,y` (16 B) that the merge
phase never reads (it only needs `column,row,type`). A hot/cold split could tighten cache during
the merge walk — **untried**, expected to be a small single-digit % at best; benchmark before
committing.

## Rejected: level-split multithreading (the old model, now removed)

Split the sorted contour levels across threads; each thread runs the full grid loop for its subset
of levels. Measured scaling, 24-core box, 11 levels:

| threads | 2 | 4 | 6 | 8 | 11 | 16 | 24 |
|---|---|---|---|---|---|---|---|
| speedup | 1.02× | 1.60× | 2.02× | 1.87× | 2.27× | 2.31× | 2.30× |

Two structural failures: (1) parallelism is **capped at the level count** — beyond 11 threads
nothing is gained, and a single-level product gets **1×**; (2) every thread **re-traverses all
cells** (and re-reads the whole grid), so the fixed per-cell cost and memory bandwidth are
multiplied by the thread count. The asymptote `1 + Tc/Tr ≈ 2.3×` implies ~40–45 % of the work is
replicated traversal that no scheduling can remove.

## Rejected: round-robin level assignment (tweak to level-split)

Distributing levels round-robin instead of in contiguous chunks fixed the *load imbalance*
(2 threads 1.02×→1.42×, 4 threads 1.60×→2.04×) because the heavy low-threshold bands no longer all
landed on one thread. But it **did not raise the ~2.3× ceiling** — that ceiling is the `#levels`
cap plus the N× traversal, which scheduling cannot fix. Superseded by band-parallel.

## Adopted: band-parallel contouring

Split the grid's cell-rows into contiguous **bands**, contour each independently (one traversal
total), stitch adjacent bands along their one shared boundary row (`join_boundary`, a cross-pool
`merge_row`), then assemble rings across band pools. Concurrency is bounded by a server-injected
shared `Fmi::ThreadPool` (see `CLAUDE.md` → Multi-threading).

Measured scaling, 24-core box, same benchmark (pool sized to thread count):

| threads | 2 | 4 | 8 | 16 | 24 |
|---|---|---|---|---|---|
| speedup | 1.60× | 2.53× | 3.30× | 4.63× | 5.12× |

Parallelizes single-level products, byte-identical output (verified under ThreadSanitizer and
AddressSanitizer, and across 1–100 bands).

### Phase profile (what limits scaling)

Per-phase wall time on the benchmark (pool sized to thread count):

| threads | contour (parallel) | join | **assemble (serial)** | total |
|--------:|----:|----:|----:|----:|
| 4  | 117.4 ms | 0.09 | 44.7 ms | 162 ms |
| 8  | 69.4 ms | 0.24 | 54.9 ms | 125 ms |
| 16 | 35.8 ms | 0.46 | 55.8 ms | 92 ms |
| 24 | 29.2 ms | 0.78 | **64.8 ms** | 95 ms |

The **contour phase scales near-linearly**; the **join is negligible** (<1 ms even at 24 bands).
The bottleneck is the **serial per-level assembly** (`build_rings` + `build_polygons`, run once per
level after the join): it does not parallelize and even *grows* with band count (each level's
`build_rings` scans all band pools), overtaking the contour phase at ≥16 threads. This is why
total saturates ~92 ms and speedup tops out near 5× rather than continuing to climb.

**The clear next lever is to parallelize the per-level assembly across the worker pool** — the
levels are independent, so each level's `build_rings`+`build_polygons` can be its own task. With
~11 levels that should largely remove the serial tail (rough estimate: ~36 ms total at 24 threads,
~13× instead of ~5×). Deferred from the initial rewrite to keep it reviewable. (The benchmark also
spends ~17 % in its own serial WKT fingerprinting, which a real consumer using WKB/OGR avoids.)

## Where to look next (untried, plausible)

These target the actual hot path and have **not** been tried:

- **Shrink `Vertex`** (32 B: `2×int32 + enum + 2×double + bool`) — copied through `add`/`push_back`
  and chased during merge; smaller vertices = less memory traffic in the dominant phase.
- **Index `JointMerger`'s neighbour search** — `find_match` is a linear forward/backward scan of
  the previous row; an indexed lookup could cut the ~8 % merge cost.
- **Parallelize per-level assembly** over the shared pool (currently serial after the band join).
