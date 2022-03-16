#pragma once

#include "Polygon.h"
#include "Polyline.h"

namespace Trax
{
class JointPool;

void build_rings(Polylines& shells, Holes& holes, JointPool& joints);
void build_polygons(Polygons& polygons, Polylines& shells, Holes& holes);

}  // namespace Trax
