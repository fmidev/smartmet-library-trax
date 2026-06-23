#pragma once

#include "Polygon.h"
#include "Polyline.h"
#include <vector>

namespace Trax
{
class JointPool;

void build_rings(Polylines& shells, Holes& holes, JointPool& joints, bool strict);

// Build rings from several band pools at once. The joint graph spans all pools (linked by
// join_boundary); pools are scanned in order so seed-joint visit order matches the
// single-threaded bottom-to-top order, keeping output identical.
void build_rings(Polylines& shells, Holes& holes, const std::vector<JointPool*>& pools, bool strict);

void build_polygons(Polygons& polygons, Polylines& shells, Holes& holes);

}  // namespace Trax
