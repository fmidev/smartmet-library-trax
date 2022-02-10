#pragma once

#include "GeometryCollection.h"
#include <geos/geom/GeometryFactory.h>
#include <memory>

namespace Trax
{
std::unique_ptr<geos::geom::Geometry> to_geos_geom(const GeometryCollection& geom,
                                                   const geos::geom::GeometryFactory::Ptr& factory);
}  // namespace Trax
