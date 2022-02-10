#pragma once

#include "Polygon.h"
#include "Polyline.h"

namespace Trax
{
class JointPool;

void build_rings(Polygons& polygons, Holes& holes, JointPool& joints);
void assign_holes(Polygons& polygons, Holes& holes);

}  // namespace Trax
