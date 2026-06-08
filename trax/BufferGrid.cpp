#include "BufferGrid.h"
#include <macgyver/Exception.h>

namespace Trax
{
BufferGrid::BufferGrid(std::shared_ptr<const Grid> source, std::vector<float> values)
    : m_source(std::move(source)), m_values(std::move(values))
{
  if (!m_source)
    throw Fmi::Exception(BCP, "BufferGrid requires a non-null source grid");

  m_width = m_source->width();
  m_height = m_source->height();

  if (m_values.size() != m_width * m_height)
    throw Fmi::Exception(BCP, "BufferGrid value buffer does not match the source grid dimensions");
}

}  // namespace Trax
