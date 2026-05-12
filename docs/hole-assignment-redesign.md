# Hole-to-shell assignment by closure order

*Status: design sketch, not implemented. Replaces an earlier row-sweep
tagging proposal with a much simpler closure-order pool drain.*

---

## Motivation

`Topology::build_polygons` is up to ~30 % of total contouring wall time on
real meteorological workloads (convective precipitation, scattered radar
echoes). Both the master `boost::rtree` implementation and the no-rtree
linear-scan-with-bbox-sort implementation are doing the same kind of work
— a **global** post-process over (shells × holes) — and only trading
constant factors. The bbox-area sort plus pnpoly verification is correct
but does redundant work the sweep already had the answer to.

---

## The claim

> If an exterior shell closes, every hole it contains must already be
> closed.

This is true because a hole's vertices are geometrically inside the
shell's vertex hull. The hole's last (topmost) row is at most the shell's
last row.

> When an exterior closes, scan the pool of unassigned holes. The first
> one that geometrically fits inside this exterior is its child.

This is true because any *other* exterior that also contains the hole
must enclose this just-closed exterior as well (two exteriors can't both
contain the hole without one nesting inside the other). The enclosing
one is bigger and hasn't finished yet — it will close on a later row.
So the just-closed shell is the innermost. **First fit at close time =
innermost shell — no sort needed.**

Compare to current `find_containing_shell`: today every hole tests against
*every* existing shell, sorted by bbox area, pnpoly-verifying smallest
first. Closure-order replaces this with: every closing shell tests against
the *pending pool* of unassigned holes, first match wins.

---

## What "closes" means in trax

Trax assembles rings in batch via `build_rings`, not as a true incremental
scan-line. We do not have a natural "close event." The proxy:

> A ring's **close key** is `max_row` over its vertices — i.e. the topmost
> grid row the ring touches.

Each `Vertex` already carries `row`. We can stash `max_row` on `Polyline`
alongside the bbox, or compute it once per ring before sorting.

### Why max_row is a valid proxy

A child ring's `max_row` cannot exceed its parent shell's: the child is
geometrically bounded by the parent, so the parent's vertices reach at
least as high. Sorting by `max_row` ascending therefore processes nested
rings inside-out.

### Tiebreaker

Two rings can share the same `max_row` (e.g. hole and parent shell both
touch the grid top edge or the same horizontal). To guarantee a hole is
in the pool when its same-`max_row` parent drains:

```
sort key (ascending):
  1. max_row
  2. orientation: holes (CCW) before shells (CW)
  3. bbox area (innermost first)
```

Within the same `max_row`, all holes are pushed into the pool before any
shell scans it. Within the same `max_row` and same orientation, smaller
bbox first — for shells this is the inside-out invariant; for holes the
order does not affect correctness.

---

## Algorithm

```
void build_polygons(Polygons& out, Polylines& shells, Holes& holes)
{
    // 1. Collect all rings (already CW/CCW-classified by build_rings).
    //    Compute bbox + max_row.
    rings = shells.tagged(CW) ++ holes.tagged(CCW);

    // 2. Sort by (max_row asc, CCW-first, bbox_area asc).
    sort(rings, close_order);

    // 3. Single pass: holes go into pool, shells drain pool.
    Holes pool;
    for (auto& ring : rings)
    {
        if (ring.ccw())
        {
            pool.push_back(std::move(ring));
        }
        else
        {
            Polygon polygon(std::move(ring));
            for (auto it = pool.begin(); it != pool.end(); )
            {
                if (polygon.exterior().bbox().contains(it->bbox())
                    && polygon.contains(*it))
                {
                    polygon.hole(std::move(*it));
                    it = pool.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            out.emplace_back(std::move(polygon));
        }
    }

    // 4. Anything left in the pool is an unparented hole — handled
    //    the same way as today (reverse orientation and emit as a shell;
    //    happens for isolines cut at the grid boundary).
}
```

That is the entire change. No new data structures threaded through
`CellBuilder` / `JointMerger`. No union-find. No region tags on vertices.
Single sort, single pass.

---

## Complexity

Let R = total ring count, H = holes, S = shells, P = max pool size during
the pass.

- Sort: **O(R log R)**.
- Pool scans: each shell scans at most P holes. **O(S · P)**.
- pnpoly cost per check: proportional to hole vertex count, same as
  today's check.

**Practical P is small.** In the convective benchmark each Gaussian
peak's hole closes on roughly the same row range as the surrounding
shell — pool size at any moment is roughly the number of "in flight"
features at that row, not the global hole count. Worst case (all H holes
close before any S shell) degenerates to O(H · S), but that pattern does
not arise in either smooth or convective real data.

**Compared to today (no-rtree):** today is O(H · S) average with bbox
fast-rejects, plus a sort of candidates per hole. The new path eliminates
the candidate sort entirely and replaces "every hole tests every shell"
with "every shell tests the much smaller pool of pending holes."

---

## What this design does *not* require

- No changes to `CellBuilder`.
- No changes to `JointMerger`.
- No changes to `build_rings`.
- No new fields on `Vertex` or `Joint`.
- No new union-find data structure.
- No interaction with saddle disambiguation, ghost flags, or ridge
  suppression — those all live upstream and decide ring shape; this
  algorithm operates on already-shaped rings.

Holes that touch their parent shell are still attached during
`extract_left_turning_sequence` exactly as today. They never enter
`build_polygons`. The closure-order pool drain only sees the "free"
holes — the same set today's `find_containing_shell` processes.

The change is contained to one function.

---

## Edge cases

1. **`max_row` on Polyline.** Add a single integer field updated as
   vertices are appended, or compute once before sorting by iterating
   the ring's points. The Vertex has `row`; bbox stores world coords,
   so we cannot derive `max_row` from bbox alone.

2. **Holes touching grid boundary (isoline mode).** Today's fallback
   converts them to reversed-orientation shells. Same fallback applies
   — pool leftover at end is processed identically.

3. **Multi-threading.** Each level still runs in its own `Impl`. Sort
   + pool are local. No cross-thread state.

4. **`Contour::subdivide`.** Operates upstream of `build_polygons`;
   produces more rings of finer resolution but does not change the
   close-order invariant.

5. **NaN-band.** Treated as any other isoband — rings come out of
   `build_rings` with consistent CW/CCW orientation, and the algorithm
   reads orientation only.

6. **Ties beyond (max_row, orientation, bbox_area).** Two disjoint
   holes with identical bbox at the same `max_row` — neither contains
   the other, neither is in the other's pnpoly result, so order does
   not matter. Two disjoint shells with identical bbox at the same
   `max_row` — same: neither contains the other, neither sees the
   other's children.

---

## What I checked and what is genuinely unresolved

Checked:
- Closure-order proof: holds for "any other exterior containing the hole
  is strictly larger and still open." Counter-examples I tried (siblings,
  nested triples, same-`max_row` parent-child, U-shape false bbox
  containment) all assign correctly.
- Tiebreaker: holes-before-shells at same `max_row` suffices because a
  child's bbox area ≤ parent's bbox area when nested.

Unresolved (would want to verify before coding):
- Best place to compute `max_row` per ring. Probably during
  `extract_right_turning_sequence` / `extract_left_turning_sequence`
  where vertices are appended, to avoid a second pass over each ring.
- Pool data structure. `std::vector<Polyline>` with erase-from-middle
  is O(N) per erase. Probably fine because P is small. If profiling
  shows otherwise, switch to swap-and-pop (order in the pool does not
  matter — first geometric fit wins regardless of pool ordering).
- Empty-pool short-circuit: skip the pool loop entirely when the pool
  is empty. Trivial guard, common case once features stop overlapping.

---

## Why this beats the earlier row-sweep tagging proposal

The earlier proposal wanted to thread `region_id` tags through
`CellBuilder` / `JointMerger`, maintain a per-row interval frontier, and
resolve hole-to-shell via union-find lookup at ring close. It was correct
in principle but invasive — it touched the saddle-disambiguation /
ghost-edge / ridge-suppression machinery that has already been a source
of subtle bugs.

The closure-order design uses the **same insight** ("the sweep already
knew which exterior the hole belonged to") but reads the answer out of a
much smaller artefact: the closure order of fully-formed rings. No
machinery in the cell-level pipeline changes. The simplification is real
— fewer moving parts, no new failure modes, easy to revert if benchmarks
disagree.

---

## Next step

Add `max_row` to `Polyline` (set as vertices are appended), rewrite
`Topology::build_polygons` per the algorithm above, and re-run the
convective benchmark. Confirm:

1. Fingerprint (WKT length sum) matches today.
2. ContourTest suite passes — especially the `test/data/topology.txt`
   cases the no-rtree commit added (saddles, donuts, holes-touching-
   holes, U/S shapes).
3. `build_polygons` is no longer ~30 % of profiler time on a real
   FMI workload.
