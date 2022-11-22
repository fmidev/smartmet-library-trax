#pragma once

namespace Trax
{
class JointMerger;
struct Cell;
class Range;

namespace CellBuilder
{
void isoband_linear(JointMerger& joints, const Cell& c, const Range& range);
void isoline_linear(JointMerger& joints, const Cell& c, float limit);
void isoband_logarithmic(JointMerger& joints, const Cell& c, const Range& range);
void isoline_logarithmic(JointMerger& joints, const Cell& c, float limit);
void isoband_midpoint(JointMerger& joints, const Cell& c, const Range& range);

}  // namespace CellBuilder
}  // namespace Trax
