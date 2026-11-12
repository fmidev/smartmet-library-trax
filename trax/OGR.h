#pragma once

#include <memory>
#include <ogr_geometry.h>

namespace Trax
{
class GeometryCollection;

std::unique_ptr<OGRGeometry> to_ogr_geom(const GeometryCollection& geom);

}  // namespace Trax
