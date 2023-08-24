#include "Geos.h"
#include <geos/version.h>

#define GEOS_VERSION_ID (100*GEOS_VERSION_MAJOR + GEOS_VERSION_MINOR)

#include <geos/geom/Coordinate.h>
#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/Geometry.h>
#include <geos/geom/LineString.h>
#include <geos/geom/LinearRing.h>
#include <geos/geom/Polygon.h>
#include <smartmet/macgyver/Exception.h>

#if GEOS_VERSION_ID < 312
#include <geos/geom/CoordinateArraySequence.h>
#endif

namespace gg = geos::geom;

namespace Trax
{

#if GEOS_VERSION_ID < 312

//   ↓↓↓↓↓↓↓↓↓↓↓↓  GEOS-3.11  ↓↓↓↓↓↓↓↓↓↓↓↓↓

namespace
{
std::unique_ptr<gg::LineString> to_geos_linestring(const Polyline& line, const gg::GeometryFactory::Ptr& factory)
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

  return std::unique_ptr<gg::LineString>(factory->createLineString(coords));
}

std::unique_ptr<gg::LinearRing> to_geos_linearring(const Polyline& ring, const gg::GeometryFactory::Ptr& factory)
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

  return std::unique_ptr<gg::LinearRing>(factory->createLinearRing(coords));
}
}  // namespace

std::unique_ptr<gg::Geometry> to_geos_geom(const GeometryCollection& geom,
                                           const gg::GeometryFactory::Ptr& factory)
{
  if (geom.empty())
    return factory->createEmptyGeometry();

  // Built geos linestrings
  auto* lines = new std::vector<gg::LineString *>;

  for (const auto& line : geom.polylines())
      lines->push_back(to_geos_linestring(line, factory).release());

  // Built geos polygons

  auto* polys = new std::vector<gg::Polygon*>;

  for (const auto& poly : geom.polygons())
  {
      auto* exterior = to_geos_linearring(poly.exterior(), factory).release();

    const auto& holes = poly.holes();
    if (holes.empty())
      polys->push_back(factory->createPolygon(exterior, nullptr));
    else
    {
      auto* holerings = new std::vector<gg::LinearRing*>;
      for (const auto& hole : holes)
          holerings->push_back(to_geos_linearring(hole, factory).release());
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

//   ↑↑↑↑↑↑↑↑↑↑↑↑  GEOS-3.11  ↑↑↑↑↑↑↑↑↑↑↑↑↑

#else

//   ↓↓↓↓↓↓↓↓↓↓↓↓  GEOS-3.12+ ↓↓↓↓↓↓↓↓↓↓↓↓↓

namespace
{
std::unique_ptr<gg::LineString> to_geos_linestring(const Polyline& line, const gg::GeometryFactory::Ptr& factory)
{
  const auto n = line.size();
  std::vector<gg::Coordinate> points;
  points.reserve(n);

  auto xcoords = line.xcoordinates();
  auto ycoords = line.ycoordinates();

  for (auto i = 0UL; i < n; i++)
    points.emplace_back(gg::Coordinate(xcoords[i], ycoords[i]));

  std::unique_ptr<gg::CoordinateSequence> coords(new gg::CoordinateSequence());
  coords->setPoints(points);

  return factory->createLineString(std::move(coords));
}

std::unique_ptr<gg::LinearRing> to_geos_linearring(const Polyline& ring, const gg::GeometryFactory::Ptr& factory)
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
    std::vector<std::unique_ptr<gg::LinearRing>> holerings;
    const auto& holes = poly.holes();

    for (const auto& hole : holes)
        holerings.emplace_back(to_geos_linearring(hole, factory).release());
    auto&& result = factory->createPolygon(std::move(exterior), std::move(holerings));
    polys.emplace_back(std::move(result));
  }

  // Handle lines

  std::unique_ptr<gg::Geometry> linegeoms;
  auto polylines = geom.polylines();
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

  if (linegeoms.get() == nullptr)
    return polygeoms;

  if (polygeoms.get() == nullptr)
    return linegeoms;

  std::vector<std::unique_ptr<gg::Geometry> > parts;
  parts.emplace_back(std::move(polygeoms));
  parts.emplace_back(std::move(linegeoms));
  return factory->createGeometryCollection(std::move(parts));
}

#endif //  ↑↑↑↑↑↑↑↑↑↑↑↑  GEOS-3.12+  ↑↑↑↑↑↑↑↑↑↑↑↑↑

}  // namespace Trax
