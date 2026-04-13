# smartmet-library-trax

Part of [SmartMet Server](https://github.com/fmidev/smartmet-server). See the [SmartMet Server documentation](https://github.com/fmidev/smartmet-server) for an overview of the ecosystem.

A high-performance C++ library for generating **isobands** (filled contour polygons) and **isolines** (contour lines) from 2D gridded scalar data. Developed by the Finnish Meteorological Institute (FMI) as part of the SmartMet server platform.

Key features:
- Marching-squares contouring with native NaN (missing-value) handling
- Three interpolation modes: linear, midpoint (nearest-neighbour), and logarithmic
- Explicit missing-value isobands and isolines for contouring NaN regions
- Open-ended isobands using `±infinity` limits
- Global grid wrap-around support
- Output as WKT, WKB, OGR geometry, or GEOS geometry

## Documentation

- [Full API and usage guide](docs/trax.md)
