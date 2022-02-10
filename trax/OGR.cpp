#include "OGR.h"

namespace Trax
{
namespace
{
OGRLineString* to_ogr_linestring(const Polyline& line)
{
  const auto n = line.size();

  auto* ogrline = new OGRLineString;

  auto xcoords = line.xcoordinates();
  auto ycoords = line.ycoordinates();

  ogrline->setPoints(n, &xcoords[0], &ycoords[0]);

  return ogrline;
}

OGRLinearRing* to_ogr_linearring(const Polyline& ring)
{
  const auto n = ring.size();

  auto* ogrring = new OGRLinearRing;

  auto xcoords = ring.xcoordinates();
  auto ycoords = ring.ycoordinates();

  ogrring->setPoints(n, &xcoords[0], &ycoords[0]);

  return ogrring;
}
}  // namespace

std::unique_ptr<OGRGeometry> to_ogr_geom(const GeometryCollection& geom)
{
  if (geom.empty())
    return std::unique_ptr<OGRGeometry>(new OGRGeometryCollection);

  // Built OGR linestrings
  auto* lines = new std::vector<OGRLineString*>;

  for (const auto& line : geom.polylines())
    lines->push_back(to_ogr_linestring(line));

  // Built OGR polygons

  auto* polys = new std::vector<OGRPolygon*>;

  for (const auto& poly : geom.polygons())
  {
    auto* ogrpoly = new OGRPolygon;

    auto* exterior = to_ogr_linearring(poly.exterior());
    ogrpoly->addRingDirectly(exterior);

    const auto& holes = poly.holes();
    for (const auto& hole : holes)
    {
      auto* interior = to_ogr_linearring(hole);
      ogrpoly->addRingDirectly(interior);
    }
    polys->push_back(ogrpoly);
  }

  // Handle lines

  OGRGeometry* linegeoms = nullptr;

  if (!lines->empty())
  {
    if (lines->size() == 1)
      linegeoms = (*lines)[0];
    else
    {
      auto* multiline = new OGRMultiLineString;
      for (auto* line : *lines)
        multiline->addGeometryDirectly(line);
      linegeoms = multiline;
    }
  }
  delete lines;

  // Handle polygons

  OGRGeometry* polygeoms = nullptr;

  if (!polys->empty())
  {
    if (polys->size() == 1)
      polygeoms = (*polys)[0];
    else
    {
      auto* multipoly = new OGRMultiPolygon;
      for (auto* poly : *polys)
        multipoly->addGeometryDirectly(poly);
      polygeoms = multipoly;
    }
  }

  delete polys;

  // Return the simplest possible geometry type

  if (linegeoms == nullptr)
    return std::unique_ptr<OGRGeometry>(polygeoms);

  if (polygeoms == nullptr)
    return std::unique_ptr<OGRGeometry>(linegeoms);

  auto* ogrgeom = new OGRGeometryCollection;
  ogrgeom->addGeometryDirectly(polygeoms);
  ogrgeom->addGeometryDirectly(linegeoms);
  return std::unique_ptr<OGRGeometry>(ogrgeom);
}

}  // namespace Trax
