#pragma once

#include "Vertex.h"

namespace Trax
{
struct Joint
{
  explicit Joint(const Vertex& v) : vertex(v) {}
  Joint() = default;

  // Note the placement of Vertex for optimal alignment
  Joint* prev = nullptr;  // previous vertex on the path
  Joint* next = nullptr;  // next vertex on the path
  Joint* alt = nullptr;   // next alternate vertex if any, circular linked list
  Vertex vertex;          // this vertex, may have duplicates
  bool used = false;      // has path through 'next' been used already
};

}  // namespace Trax
