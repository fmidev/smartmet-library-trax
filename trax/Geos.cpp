#include "Geos.h"
#include <geos/geom/Coordinate.h>
#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/Geometry.h>
#include <geos/geom/LineString.h>
#include <geos/geom/LinearRing.h>
#include <geos/geom/Polygon.h>
#include <smartmet/macgyver/Exception.h>

namespace gg = geos::geom;

namespace Trax
{

namespace
{
std::unique_ptr<gg::LineString> to_geos_linestring(const Polyline& line,
                                                   const gg::GeometryFactory::Ptr& factory)
{
  const auto n = line.size();
  std::vector<gg::Coordinate> points;
  points.reserve(n);

  auto xcoords = line.xcoordinates();
  auto ycoords = line.ycoordinates();

  for (auto i = 0UL; i < n; i++)
    points.emplace_back(xcoords[i], ycoords[i]);

  std::unique_ptr<gg::CoordinateSequence> coords(new gg::CoordinateSequence());
  coords->setPoints(points);

  return factory->createLineString(std::move(coords));
}

std::unique_ptr<gg::LinearRing> to_geos_linearring(const Polyline& ring,
                                                   const gg::GeometryFactory::Ptr& factory)
{
  const auto n = ring.size();

  if (n < 4)
    throw Fmi::Exception(BCP, "Invalid ring found, GEOS rings must have zero or >= 4 points")
        .addParameter("Ring", ring.wkt());

  std::vector<gg::Coordinate> points;
  points.reserve(n);

  auto xcoords = ring.xcoordinates();
  auto ycoords = ring.ycoordinates();

  for (auto i = 0UL; i < n; i++)
    points.emplace_back(xcoords[i], ycoords[i]);

  std::unique_ptr<gg::CoordinateSequence> coords(new gg::CoordinateSequence());
  coords->setPoints(points);

  return factory->createLinearRing(std::move(coords));
}
}  // namespace

std::unique_ptr<gg::Geometry> to_geos_geom(const GeometryCollection& geom,
                                           const gg::GeometryFactory::Ptr& factory)
{
  if (geom.empty())
    return factory->createEmptyGeometry();

  // Built geos linestrings
  std::vector<std::unique_ptr<gg::LineString> > lines;

  for (const auto& line : geom.polylines())
    lines.emplace_back(to_geos_linestring(line, factory));

  // Built geos polygons

  std::vector<std::unique_ptr<gg::Polygon> > polys;

  for (const auto& poly : geom.polygons())
  {
    auto exterior = to_geos_linearring(poly.exterior(), factory);
    const auto& holes = poly.holes();

    std::vector<std::unique_ptr<gg::LinearRing> > holerings;
    holerings.reserve(holes.size());

    for (const auto& hole : holes)
      holerings.emplace_back(to_geos_linearring(hole, factory).release());
    auto&& result = factory->createPolygon(std::move(exterior), std::move(holerings));
    polys.emplace_back(std::move(result));
  }

  // Handle lines

  std::unique_ptr<gg::Geometry> linegeoms;
  const auto& polylines = geom.polylines();
  if (!polylines.empty())
  {
    if (polylines.size() == 1)
    {
      linegeoms = to_geos_linestring(polylines[0], factory);
    }
    else
    {
      std::vector<std::unique_ptr<geos::geom::Geometry> > geoms;
      for (const auto& line : geom.polylines())
        geoms.emplace_back(to_geos_linestring(line, factory));
      linegeoms = factory->createMultiLineString(std::move(geoms));
    }
  }

  // Handle polygons

  std::unique_ptr<gg::Geometry> polygeoms;

  if (!polys.empty())
  {
    if (polys.size() == 1)
      polygeoms = std::move(polys[0]);
    else
    {
      std::vector<std::unique_ptr<geos::geom::Geometry> > geoms;
      std::move(polys.begin(), polys.end(), std::back_inserter(geoms));
      polygeoms = factory->createMultiPolygon(std::move(geoms));
    }
  }

  // Return the simplest possible geometry type

  if (!linegeoms)
    return polygeoms;

  if (!polygeoms)
    return linegeoms;

  std::vector<std::unique_ptr<gg::Geometry> > parts;
  parts.emplace_back(std::move(polygeoms));
  parts.emplace_back(std::move(linegeoms));
  return factory->createGeometryCollection(std::move(parts));
}

}  // namespace Trax
