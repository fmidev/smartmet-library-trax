#include "Geos.h"
#include <geos/geom/CoordinateArraySequence.h>
#include <geos/geom/CoordinateSequenceFactory.h>
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
gg::LineString* to_geos_linestring(const Polyline& line, const gg::GeometryFactory::Ptr& factory)
{
  const auto n = line.size();
  std::vector<gg::Coordinate> points;
  points.reserve(n);

  auto xcoords = line.xcoordinates();
  auto ycoords = line.ycoordinates();

  for (auto i = 0UL; i < n; i++)
    points.emplace_back(gg::Coordinate(xcoords[i], ycoords[i]));

  gg::CoordinateSequence* coords = new gg::CoordinateArraySequence();
  coords->setPoints(points);

  return factory->createLineString(coords);
}

gg::LinearRing* to_geos_linearring(const Polyline& ring, const gg::GeometryFactory::Ptr& factory)
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
    points.emplace_back(gg::Coordinate(xcoords[i], ycoords[i]));

  gg::CoordinateSequence* coords = new gg::CoordinateArraySequence();
  coords->setPoints(points);

  return factory->createLinearRing(coords);
}
}  // namespace

std::unique_ptr<gg::Geometry> to_geos_geom(const GeometryCollection& geom,
                                           const gg::GeometryFactory::Ptr& factory)
{
  if (geom.empty())
    return factory->createEmptyGeometry();

  // Built geos linestrings
  auto* lines = new std::vector<gg::LineString*>;

  for (const auto& line : geom.polylines())
    lines->push_back(to_geos_linestring(line, factory));

  // Built geos polygons

  auto* polys = new std::vector<gg::Polygon*>;

  for (const auto& poly : geom.polygons())
  {
    auto* exterior = to_geos_linearring(poly.exterior(), factory);

    const auto& holes = poly.holes();
    if (holes.empty())
      polys->push_back(factory->createPolygon(exterior, nullptr));
    else
    {
      auto* holerings = new std::vector<gg::LinearRing*>;
      for (const auto& hole : holes)
        holerings->push_back(to_geos_linearring(hole, factory));
      auto&& result = factory->createPolygon(exterior, holerings);
      polys->push_back(result);
    }
  }

  // Handle lines

  gg::Geometry* linegeoms = nullptr;

  if (!lines->empty())
  {
    if (lines->size() == 1)
      linegeoms = (*lines)[0];
    else
    {
      auto* geoms = new std::vector<geos::geom::Geometry*>;
      std::copy(lines->begin(), lines->end(), std::back_inserter(*geoms));
      linegeoms = factory->createMultiLineString(geoms);
    }
  }
  delete lines;

  // Handle polygons

  gg::Geometry* polygeoms = nullptr;

  if (!polys->empty())
  {
    if (polys->size() == 1)
      polygeoms = (*polys)[0];
    else
    {
      auto* geoms = new std::vector<geos::geom::Geometry*>;
      std::copy(polys->begin(), polys->end(), std::back_inserter(*geoms));
      polygeoms = factory->createMultiPolygon(geoms);
    }
  }

  delete polys;

  // Return the simplest possible geometry type

  if (linegeoms == nullptr)
    return std::unique_ptr<gg::Geometry>(polygeoms);

  if (polygeoms == nullptr)
    return std::unique_ptr<gg::Geometry>(linegeoms);

  auto* parts = new std::vector<gg::Geometry*>{polygeoms, linegeoms};
  return std::unique_ptr<gg::Geometry>(factory->createGeometryCollection(parts));
}

}  // namespace Trax
