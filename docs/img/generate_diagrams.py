#!/usr/bin/env python3
"""Generate the SVG diagrams used by docs/trax.md.

Each figure visualises one 2x2 grid cell case from trax's marching-squares
dispatch. SVGs are hand-rolled (no external deps) so they diff cleanly in git.

Run:  python3 docs/img/generate_diagrams.py
"""
from pathlib import Path

OUT = Path(__file__).parent

# Geometry: a square cell inside a square viewport, with margin for corner labels.
W = H = 240
M = 40
CELL = 160
P1 = (M, M + CELL)            # bottom-left
P2 = (M, M)                   # top-left
P3 = (M + CELL, M)            # top-right
P4 = (M + CELL, M + CELL)     # bottom-right
CX, CY = M + CELL / 2, M + CELL / 2

# Corner classification colours (used for the labelled circles).
COL_BELOW   = "#3b82f6"    # blue
COL_INSIDE  = "#10b981"    # green
COL_ABOVE   = "#ef4444"    # red
COL_INVALID = "#9ca3af"    # gray

# Region fills (lighter shades of the corner colours).
FILL_BELOW   = "#dbeafe"
FILL_INSIDE  = "#bbf7d0"
FILL_ABOVE   = "#fee2e2"
FILL_INVALID = "#e5e7eb"

# Stroke colours.
COL_EDGE       = "#111827"     # primary polygon edge
COL_EDGE_GHOST = "#94a3b8"     # ridge / ghost edge — same shape, lighter weight
COL_BORDER     = "#9ca3af"     # cell border

STYLE = """
  .lbl   { font: bold 14px sans-serif; fill: white;   text-anchor: middle; dominant-baseline: middle }
  .clbl  { font: bold 12px sans-serif; fill: #1f2937; text-anchor: middle }
  .ann   { font: 11px sans-serif;      fill: #374151; text-anchor: middle }
  .annl  { font: 11px sans-serif;      fill: #374151 }
"""


def svg_open(w=W, h=H):
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" '
        f'width="{w}" height="{h}">\n'
        f'<style>{STYLE}</style>\n'
        f'<rect width="{w}" height="{h}" fill="white"/>\n'
    )


def svg_close():
    return "</svg>\n"


def cell_border(x=M, y=M, w=CELL, h=CELL):
    return (
        f'<rect x="{x}" y="{y}" width="{w}" height="{h}" '
        f'fill="none" stroke="{COL_BORDER}" stroke-width="1.5"/>\n'
    )


def fill_polygon(points, color):
    pts = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
    return f'<polygon points="{pts}" fill="{color}" stroke="none"/>\n'


def stroke_path(points, color=COL_EDGE, width=2.5, close=False, dashed=False):
    if not points:
        return ""
    d = f"M{points[0][0]:.1f},{points[0][1]:.1f}"
    for x, y in points[1:]:
        d += f" L{x:.1f},{y:.1f}"
    if close:
        d += " Z"
    dash = ' stroke-dasharray="5,3"' if dashed else ""
    return (
        f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{width}" '
        f'stroke-linejoin="round" stroke-linecap="round"{dash}/>\n'
    )


def corner(pt, place, label=None):
    color = {"B": COL_BELOW, "I": COL_INSIDE, "A": COL_ABOVE, "N": COL_INVALID}[place]
    s = (
        f'<circle cx="{pt[0]}" cy="{pt[1]}" r="13" fill="{color}" '
        f'stroke="white" stroke-width="2.5"/>\n'
        f'<text x="{pt[0]}" y="{pt[1]+1}" class="lbl">{place}</text>\n'
    )
    if label:
        # Position label outside the cell, picking direction by quadrant.
        dx = -22 if pt[0] < M + CELL / 2 else 22
        dy = -18 if pt[1] < M + CELL / 2 else 22
        anchor = "end" if dx < 0 else "start"
        s += (
            f'<text x="{pt[0]+dx}" y="{pt[1]+dy}" class="annl" '
            f'text-anchor="{anchor}">{label}</text>\n'
        )
    return s


def write(name, body, w=W, h=H):
    out = OUT / f"{name}.svg"
    out.write_text(svg_open(w, h) + body + svg_close())
    print(f"  {out.name}")


# ───────────────────────────────────────────────────────────────────────────
# Edge intersection helpers (schematic — exact coordinates aren't load-bearing).
# ───────────────────────────────────────────────────────────────────────────
def at_left(t):
    """Point on left edge: t=0 at p2 (top), t=1 at p1 (bottom)."""
    return (M, M + CELL * t)


def at_right(t):
    """Point on right edge: t=0 at p3 (top), t=1 at p4 (bottom)."""
    return (M + CELL, M + CELL * t)


def at_top(t):
    """Point on top edge: t=0 at p2 (left), t=1 at p3 (right)."""
    return (M + CELL * t, M)


def at_bottom(t):
    """Point on bottom edge: t=0 at p1 (left), t=1 at p4 (right)."""
    return (M + CELL * t, M + CELL)


# ───────────────────────────────────────────────────────────────────────────
# Setup figures
# ───────────────────────────────────────────────────────────────────────────
def fig_cell_layout():
    body = cell_border()
    for pt, lbl in [(P1, "p1"), (P2, "p2"), (P3, "p3"), (P4, "p4")]:
        body += f'<circle cx="{pt[0]}" cy="{pt[1]}" r="6" fill="#1f2937"/>\n'
    # Labels positioned outside the cell.
    body += f'<text x="{P1[0]-6}" y="{P1[1]+18}" class="annl" text-anchor="end">p1</text>\n'
    body += f'<text x="{P2[0]-6}" y="{P2[1]-8}"  class="annl" text-anchor="end">p2</text>\n'
    body += f'<text x="{P3[0]+6}" y="{P3[1]-8}"  class="annl">p3</text>\n'
    body += f'<text x="{P4[0]+6}" y="{P4[1]+18}" class="annl">p4</text>\n'
    body += f'<text x="{CX}" y="{CY-4}" class="ann">2×2 cell</text>\n'
    body += f'<text x="{CX}" y="{CY+12}" class="ann">corner naming</text>\n'
    return body


def fig_classification():
    # Wider canvas (set in main()) so the corner descriptions fit on both sides.
    # Cell is shifted right so the left-side labels have room.
    cell_x = 100
    body = (f'<rect x="{cell_x}" y="{M}" width="{CELL}" height="{CELL}" '
            f'fill="none" stroke="{COL_BORDER}" stroke-width="1.5"/>\n')
    p1 = (cell_x, M + CELL)
    p2 = (cell_x, M)
    p3 = (cell_x + CELL, M)
    p4 = (cell_x + CELL, M + CELL)
    body += corner(p1, "B")
    body += corner(p2, "I")
    body += corner(p3, "A")
    body += corner(p4, "N")
    body += f'<text x="{p1[0]-22}" y="{p1[1]+5}" class="annl" text-anchor="end">v &lt; lo</text>\n'
    body += f'<text x="{p2[0]-22}" y="{p2[1]+5}" class="annl" text-anchor="end">lo ≤ v &lt; hi</text>\n'
    body += f'<text x="{p3[0]+22}" y="{p3[1]+5}" class="annl">v ≥ hi</text>\n'
    body += f'<text x="{p4[0]+22}" y="{p4[1]+5}" class="annl">v is NaN</text>\n'
    return body


# ───────────────────────────────────────────────────────────────────────────
# Basic shapes
# ───────────────────────────────────────────────────────────────────────────
def fig_full():
    """IIII: the entire cell is Inside the band."""
    body = fill_polygon([P1, P2, P3, P4], FILL_INSIDE)
    body += cell_border()
    body += corner(P1, "I") + corner(P2, "I") + corner(P3, "I") + corner(P4, "I")
    return body


def fig_triangle():
    """BBBI: one Inside corner at p4."""
    cut_b = at_bottom(0.62)   # lo cut on bottom edge (closer to p4)
    cut_r = at_right(0.62)    # lo cut on right edge (closer to p4)
    body = fill_polygon([P2, P3, cut_r, cut_b, P1], FILL_BELOW)
    body += fill_polygon([cut_b, P4, cut_r], FILL_INSIDE)
    body += cell_border()
    body += stroke_path([cut_b, cut_r])
    body += corner(P1, "B") + corner(P2, "B") + corner(P3, "B") + corner(P4, "I")
    return body


def fig_side_rectangle():
    """BBII: two adjacent Inside corners on the right side."""
    cut_t = at_top(0.55)     # lo cut on top edge
    cut_b = at_bottom(0.55)  # lo cut on bottom edge
    body = fill_polygon([P2, cut_t, cut_b, P1], FILL_BELOW)
    body += fill_polygon([cut_t, P3, P4, cut_b], FILL_INSIDE)
    body += cell_border()
    body += stroke_path([cut_t, cut_b])
    body += corner(P1, "B") + corner(P2, "B") + corner(P3, "I") + corner(P4, "I")
    return body


def fig_side_stripe():
    """BBBA: one Above corner at p4 — both lo and hi cross bottom and right."""
    blo = at_bottom(0.55)     # lo cut on bottom (closer to p1)
    bhi = at_bottom(0.78)     # hi cut on bottom (closer to p4)
    rlo = at_right(0.55)      # lo cut on right
    rhi = at_right(0.78)      # hi cut on right
    body = fill_polygon([P2, P3, rlo, blo, P1], FILL_BELOW)
    body += fill_polygon([blo, bhi, rhi, rlo], FILL_INSIDE)
    body += fill_polygon([bhi, P4, rhi], FILL_ABOVE)
    body += cell_border()
    body += stroke_path([blo, rlo])
    body += stroke_path([bhi, rhi])
    body += corner(P1, "B") + corner(P2, "B") + corner(P3, "B") + corner(P4, "A")
    return body


def fig_pentagon():
    """BIAA: one Below, one Inside, two Above. Five-vertex Inside polygon."""
    llo = at_left(0.55)       # lo cut on left (between p1=B and p2=I)
    thi = at_top(0.4)         # hi cut on top (between p2=I and p3=A)
    blo = at_bottom(0.4)      # lo cut on bottom (between p1=B and p4=A)
    bhi = at_bottom(0.6)      # hi cut on bottom
    body = fill_polygon([P1, llo, blo], FILL_BELOW)
    body += fill_polygon([llo, P2, thi, bhi, blo], FILL_INSIDE)
    body += fill_polygon([thi, P3, P4, bhi], FILL_ABOVE)
    body += cell_border()
    body += stroke_path([llo, blo])      # lo curve
    body += stroke_path([thi, bhi])      # hi curve
    body += corner(P1, "B") + corner(P2, "I") + corner(P3, "A") + corner(P4, "A")
    return body


def fig_hexagon():
    """IIBA: two adjacent Inside, one Below opposite, one Above opposite. 6 vertices."""
    tlo = at_top(0.7)         # lo cut on top (between p2=I and p3=B)
    rlo = at_right(0.4)       # lo cut on right (between p3=B and p4=A)
    rhi = at_right(0.7)       # hi cut on right
    bhi = at_bottom(0.7)      # hi cut on bottom (between p1=I and p4=A)
    body = fill_polygon([tlo, P3, rlo], FILL_BELOW)
    body += fill_polygon([P1, P2, tlo, rlo, rhi, bhi], FILL_INSIDE)
    body += fill_polygon([rhi, P4, bhi], FILL_ABOVE)
    body += cell_border()
    body += stroke_path([tlo, rlo])
    body += stroke_path([rhi, bhi])
    body += corner(P1, "I") + corner(P2, "I") + corner(P3, "B") + corner(P4, "A")
    return body


# ───────────────────────────────────────────────────────────────────────────
# Saddles
# ───────────────────────────────────────────────────────────────────────────
def fig_saddle_connected():
    """BIBI saddle, centre Inside → connected hexagon.

    With centre Inside, the gradient near the B corners is steep and the lo
    cuts on each edge land close to those B corners. Result: small Below
    triangles at p1 and p3, large Inside hexagon connecting p2 and p4.
    """
    llo = at_left(0.75)      # lo on left, close to p1=B (75% from p2 → near p1)
    blo = at_bottom(0.25)    # lo on bottom, close to p1=B
    tlo = at_top(0.75)       # lo on top, close to p3=B
    rlo = at_right(0.25)     # lo on right, close to p3=B
    body = fill_polygon([P1, llo, blo], FILL_BELOW)        # small B triangle near p1
    body += fill_polygon([P3, rlo, tlo], FILL_BELOW)       # small B triangle near p3
    body += fill_polygon([llo, P2, tlo, rlo, P4, blo], FILL_INSIDE)  # large hexagon
    body += cell_border()
    body += stroke_path([llo, blo])
    body += stroke_path([tlo, rlo])
    body += corner(P1, "B") + corner(P2, "I") + corner(P3, "B") + corner(P4, "I")
    body += f'<text x="{CX}" y="{CY+4}" class="ann">centre = I</text>\n'
    return body


def fig_saddle_split():
    """BIBI saddle, centre Below → two disjoint Inside triangles.

    With centre Below, the gradient near the I corners is steep and the lo
    cuts land close to those I corners. Result: small Inside triangles at
    p2 and p4, large Below background.
    """
    llo = at_left(0.25)      # lo on left, close to p2=I
    tlo = at_top(0.25)       # lo on top, close to p2=I
    rlo = at_right(0.75)     # lo on right, close to p4=I
    blo = at_bottom(0.75)    # lo on bottom, close to p4=I
    body = fill_polygon([P1, P2, P3, P4], FILL_BELOW)
    body += fill_polygon([P2, tlo, llo], FILL_INSIDE)      # small I triangle at p2
    body += fill_polygon([P4, blo, rlo], FILL_INSIDE)      # small I triangle at p4
    body += cell_border()
    body += stroke_path([llo, tlo])
    body += stroke_path([rlo, blo])
    body += corner(P1, "B") + corner(P2, "I") + corner(P3, "B") + corner(P4, "I")
    body += f'<text x="{CX}" y="{CY+4}" class="ann">centre = B</text>\n'
    return body


# ───────────────────────────────────────────────────────────────────────────
# Special cases
# ───────────────────────────────────────────────────────────────────────────
def fig_boundary_collision():
    """A corner exactly equal to hi → the polygon vertex is snapped to the corner.

    Compare two situations: the normal corner-triangle case (lo cut at the
    edge interior) on the left, and a degenerate case where the corner value
    equals the limit on the right. In the latter case trax's intersect()
    short-circuits to the corner coordinates exactly.
    """
    blo = at_bottom(0.55)
    rlo = at_right(0.55)
    # Below dominates the cell; Inside band shrinks to a triangle at p4 with
    # the apex sitting EXACTLY on the corner because p4.z = hi.
    body = fill_polygon([P1, P2, P3, rlo, blo], FILL_BELOW)
    body += fill_polygon([blo, P4, rlo], FILL_INSIDE)
    body += cell_border()
    body += stroke_path([blo, P4])     # snapped — vertex lands on the corner
    body += stroke_path([rlo, P4])
    # Mark the snap point with a small filled bullseye on top of the corner.
    body += (f'<circle cx="{P4[0]}" cy="{P4[1]}" r="6" fill="none" '
             f'stroke="{COL_EDGE}" stroke-width="2"/>\n')
    body += corner(P1, "B") + corner(P2, "B") + corner(P3, "B") + corner(P4, "A")
    body += f'<text x="{P4[0]-4}" y="{P4[1]-20}" class="annl" text-anchor="end">value = hi</text>\n'
    body += f'<text x="{P4[0]-4}" y="{P4[1]-6}"  class="annl" text-anchor="end">vertex snapped here</text>\n'
    return body


def fig_ridge_edge():
    """Edge with both endpoints exactly = X — must not produce a stray isoline."""
    # Cell has all four corners equal to X (entirely Inside, since lo ≤ X < hi
    # if we treat X as the lo limit). We highlight the top edge as a "ridge".
    body = fill_polygon([P1, P2, P3, P4], FILL_INSIDE)
    body += cell_border()
    # Highlight one edge (top) as a ridge (both endpoints equal limit value).
    body += stroke_path([P2, P3], color=COL_EDGE_GHOST, width=4, dashed=True)
    body += corner(P1, "I") + corner(P2, "I") + corner(P3, "I") + corner(P4, "I")
    body += f'<text x="{CX}" y="{P2[1]-12}" class="ann">both endpoints = X — no isoline drawn</text>\n'
    body += f'<text x="{CX}" y="{CY+4}" class="ann">cell is uniformly Inside</text>\n'
    return body


def fig_melt():
    """AIAI saddle, both Above corners exactly = hi → forced connected topology."""
    # p1=A (=hi), p2=I, p3=A (=hi), p4=I
    # Without the melt fix the cell would split into two triangles; with the
    # fix it stays connected as one hexagon (the on-limit corners pinch but
    # are not separated).
    llo = at_left(0.6)
    tlo = at_top(0.4)
    rlo = at_right(0.6)
    blo = at_bottom(0.4)
    body = fill_polygon([P1, blo, llo], FILL_ABOVE)
    body += fill_polygon([P3, tlo, rlo], FILL_ABOVE)
    body += fill_polygon([llo, P2, tlo, rlo, P4, blo], FILL_INSIDE)
    body += cell_border()
    body += stroke_path([llo, blo])
    body += stroke_path([tlo, rlo])
    # Mark the on-limit Above corners with dashed rings.
    for pt in (P1, P3):
        body += (f'<circle cx="{pt[0]}" cy="{pt[1]}" r="20" fill="none" '
                 f'stroke="{COL_ABOVE}" stroke-width="2" stroke-dasharray="3,3"/>\n')
    body += corner(P1, "A") + corner(P2, "I") + corner(P3, "A") + corner(P4, "I")
    body += f'<text x="{CX}" y="{CY+4}" class="ann">A corners = hi exactly</text>\n'
    return body


def fig_double_saddle_connected():
    """BABA, centre Inside — single 8-vertex ring threading through the cell."""
    # Edge-cut positions chosen so the polygon is convex and readable.
    llo = at_left(0.7)        # lo on left (closer to p1=B)
    lhi = at_left(0.3)        # hi on left (closer to p2=A)
    thi = at_top(0.3)         # hi on top  (closer to p2=A)
    tlo = at_top(0.7)         # lo on top  (closer to p3=B)
    rlo = at_right(0.3)       # lo on right(closer to p3=B)
    rhi = at_right(0.7)       # hi on right(closer to p4=A)
    bhi = at_bottom(0.7)      # hi on bottom(closer to p4=A)
    blo = at_bottom(0.3)      # lo on bottom(closer to p1=B)
    # Above regions: two diagonal corners (p2 and p4).
    body = fill_polygon([P2, thi, lhi], FILL_ABOVE)
    body += fill_polygon([P4, bhi, rhi], FILL_ABOVE)
    # Below regions: the other two diagonal corners (p1 and p3).
    body += fill_polygon([P1, llo, blo], FILL_BELOW)
    body += fill_polygon([P3, tlo, rlo], FILL_BELOW)
    # Inside ring threads between them.
    body += fill_polygon([llo, lhi, thi, tlo, rlo, rhi, bhi, blo], FILL_INSIDE)
    body += cell_border()
    body += stroke_path([llo, blo])      # lo curves
    body += stroke_path([tlo, rlo])
    body += stroke_path([lhi, thi])      # hi curves
    body += stroke_path([rhi, bhi])
    body += corner(P1, "B") + corner(P2, "A") + corner(P3, "B") + corner(P4, "A")
    body += f'<text x="{CX}" y="{CY+4}" class="ann">centre = I</text>\n'
    return body


def fig_double_saddle_below():
    """BABA, centre Below — two thin Inside stripes near the two A corners."""
    llo = at_left(0.7)
    lhi = at_left(0.3)
    thi = at_top(0.3)
    tlo = at_top(0.7)
    rlo = at_right(0.3)
    rhi = at_right(0.7)
    bhi = at_bottom(0.7)
    blo = at_bottom(0.3)
    # Whole cell is Below background.
    body = fill_polygon([P1, P2, P3, P4], FILL_BELOW)
    # Two Above triangles at p2 and p4.
    body += fill_polygon([P2, thi, lhi], FILL_ABOVE)
    body += fill_polygon([P4, bhi, rhi], FILL_ABOVE)
    # Two Inside stripes wrap each Above corner.
    body += fill_polygon([llo, lhi, thi, tlo], FILL_INSIDE)
    body += fill_polygon([rlo, rhi, bhi, blo], FILL_INSIDE)
    body += cell_border()
    body += stroke_path([llo, tlo])
    body += stroke_path([lhi, thi])
    body += stroke_path([rlo, blo])
    body += stroke_path([rhi, bhi])
    body += corner(P1, "B") + corner(P2, "A") + corner(P3, "B") + corner(P4, "A")
    body += f'<text x="{CX}" y="{CY+4}" class="ann">centre = B</text>\n'
    return body


def fig_double_saddle_above():
    """BABA, centre Above — two thin Inside stripes near the two B corners."""
    llo = at_left(0.7)
    lhi = at_left(0.3)
    thi = at_top(0.3)
    tlo = at_top(0.7)
    rlo = at_right(0.3)
    rhi = at_right(0.7)
    bhi = at_bottom(0.7)
    blo = at_bottom(0.3)
    # Whole cell is Above background.
    body = fill_polygon([P1, P2, P3, P4], FILL_ABOVE)
    # Two Below triangles at p1 and p3.
    body += fill_polygon([P1, llo, blo], FILL_BELOW)
    body += fill_polygon([P3, tlo, rlo], FILL_BELOW)
    # Two Inside stripes wrap each Below corner.
    body += fill_polygon([llo, lhi, bhi, blo], FILL_INSIDE)
    body += fill_polygon([thi, tlo, rlo, rhi], FILL_INSIDE)
    body += cell_border()
    body += stroke_path([llo, blo])
    body += stroke_path([lhi, bhi])
    body += stroke_path([thi, rhi])
    body += stroke_path([tlo, rlo])
    body += corner(P1, "B") + corner(P2, "A") + corner(P3, "B") + corner(P4, "A")
    body += f'<text x="{CX}" y="{CY+4}" class="ann">centre = A</text>\n'
    return body


def fig_nan_triangle():
    """Cell with one NaN corner — contoured as a triangle with a phantom diagonal.

    Configuration: p2 = NaN, p1 = B, p3 = B, p4 = I. The phantom diagonal runs
    from p1 to p3, splitting the cell into a NaN half (containing p2) and a
    valid triangle (containing p4 = I) where contouring proceeds.
    """
    cut_r = at_right(0.62)        # lo cut on right edge between p3=B and p4=I
    cut_b = at_bottom(0.62)       # lo cut on bottom edge between p1=B and p4=I
    # NaN half (above the phantom diagonal) — gray.
    body = fill_polygon([P1, P2, P3], FILL_INVALID)
    # Valid triangle (below the diagonal): Below background + Inside corner triangle.
    body += fill_polygon([P1, P3, cut_r, cut_b], FILL_BELOW)
    body += fill_polygon([cut_b, P4, cut_r], FILL_INSIDE)
    body += cell_border()
    # Phantom diagonal drawn as dashed gray.
    body += stroke_path([P1, P3], color=COL_EDGE_GHOST, width=2, dashed=True)
    # The lo level curve inside the valid triangle.
    body += stroke_path([cut_b, cut_r])
    body += corner(P1, "B") + corner(P2, "N") + corner(P3, "B") + corner(P4, "I")
    # Label the phantom diagonal — placed perpendicular to the line, slightly
    # toward the NaN side so it sits inside the gray (invalid) half.
    label_x = M + CELL * 0.42
    label_y = M + CELL * 0.42
    body += (f'<text transform="translate({label_x},{label_y}) rotate(-45)" '
             f'class="ann">phantom diagonal</text>\n')
    return body


# ───────────────────────────────────────────────────────────────────────────
FIGURES = [
    ("cell-layout",              fig_cell_layout),
    ("shape-full",               fig_full),
    ("shape-triangle",           fig_triangle),
    ("shape-side-rectangle",     fig_side_rectangle),
    ("shape-side-stripe",        fig_side_stripe),
    ("shape-pentagon",           fig_pentagon),
    ("shape-hexagon",            fig_hexagon),
    ("saddle-connected",         fig_saddle_connected),
    ("saddle-split",             fig_saddle_split),
    ("boundary-collision",       fig_boundary_collision),
    ("ridge-edge",               fig_ridge_edge),
    ("melt",                     fig_melt),
    ("double-saddle-connected",  fig_double_saddle_connected),
    ("double-saddle-below",      fig_double_saddle_below),
    ("double-saddle-above",      fig_double_saddle_above),
    ("nan-triangle",             fig_nan_triangle),
]


def main():
    print(f"Writing SVGs to {OUT}/")
    # Wider canvas for the classification legend so the side labels fit.
    write("classification", fig_classification(), w=360, h=240)
    for name, fn in FIGURES:
        write(name, fn())
    print(f"Done — {len(FIGURES) + 1} figures.")


if __name__ == "__main__":
    main()
