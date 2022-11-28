#pragma once

#include "CellBuilder.h"
#include "GeometryCollection.h"
#include "JointMerger.h"
#include "Vertex.h"
#include <list>
#include <vector>

namespace Trax
{
struct Cell;
class Range;

class Builder
{
 public:
  Builder(std::size_t width, std::size_t height);
  Builder(Builder&& other) noexcept;

  ~Builder() = default;
  Builder() = delete;
  Builder(const Builder& other) = delete;
  Builder& operator=(const Builder& other) = delete;
  Builder& operator=(Builder&& other) = delete;

  // Get the final result
  GeometryCollection result();

  // Add rings from a single cell
  // void add(CellJoints& edges, std::size_t row);

  // Finalize (partial) results
  void finish_row();
  void finish_isolines(bool strict);
  void finish_isobands(bool strict);

  JointMerger& merger() { return m_merger; }

 private:
  GeometryCollection m_geom;  // final result

  JointMerger m_merger;  // Joints for all grid rows
};

}  // namespace Trax
