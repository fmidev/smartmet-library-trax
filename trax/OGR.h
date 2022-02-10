#pragma once

#include "GeometryCollection.h"
#include <memory>
#include <ogr_geometry.h>

namespace Trax
{
std::unique_ptr<OGRGeometry> to_ogr_geom(const GeometryCollection& geom);

}  // namespace Trax
