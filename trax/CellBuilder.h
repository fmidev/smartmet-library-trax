#pragma once

namespace Trax
{
class JointMerger;
struct Cell;
class Range;

namespace CellBuilder
{
// subdivide = 0 disables interior densification (straight segments between edge
// intersections, original behaviour). subdivide in [1..4] inserts that many extra
// samples on the true bilinear level curve between consecutive edge intersections.
void isoband_linear(JointMerger& joints, const Cell& c, const Range& range, int subdivide = 0);
void isoline_linear(JointMerger& joints, const Cell& c, float limit, int subdivide = 0);
void isoband_logarithmic(JointMerger& joints, const Cell& c, const Range& range, int subdivide = 0);
void isoline_logarithmic(JointMerger& joints, const Cell& c, float limit, int subdivide = 0);
void isoband_midpoint(JointMerger& joints, const Cell& c, const Range& range, double shell);

}  // namespace CellBuilder
}  // namespace Trax
