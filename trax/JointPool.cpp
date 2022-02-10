#include "JointPool.h"
#include "Joint.h"
#include "Vertex.h"

#if 1
#include <fmt/format.h>
#include <iostream>
#endif

namespace Trax
{
namespace
{
// MyMutex pool_mutex;

/*
 * If operator[] pos is < start_size, finding indexed joint from m_buffer is immediate.
 * Otherwise substract start_size, double start_size, and try again:
 *
 *   400 > 256 so...
 *   400 - 256 = 144 < 512 --> second buffer at position 144
 *
 *  4000 > 256 so...
 *  4000 - 256  = 3744 >= 512
 *  3744 - 512  = 3232 >= 1024
 *  3232 - 1024 = 2208 >= 2048
 *  2208 - 2048 = 160 < 4096 --> fifth buffer at position 160
 */

}  // namespace

JointPool::iterator JointPool::begin()
{
  iterator it(this);
  it.block_size = m_start_size;
  return it;
}

// JointPool::iterator JointPool::middle(std::size_t pos)
// {
//   iterator it(this);
//   it.block_size = m_start_size;
//   it.block = 0;
//   it.pos = pos;
//
//   while (it.pos >= it.block_size)
//   {
//     ++it.block;
//     it.pos -= it.block_size;
//     it.block_size *= 2;
//   }
//   return it;
// }

JointPool::~JointPool()
{
  for (auto i = 0UL; i < m_blocks; i++)
    // delete[] m_buffer[i];
    free(m_buffer[i]);
}

JointPool& JointPool::operator=(JointPool&& other)
{
  if (this != &other)
  {
    m_start_size = other.m_start_size;
    m_size = other.m_size;
    m_capacity = other.m_capacity;
    m_blocks = other.m_blocks;
    m_block = other.m_block;
    m_next = other.m_next;
    m_buffer = std::move(other.m_buffer);
  }
  return *this;
}

// Return Nth element or nullptr
// Joint* JointPool::operator[](std::size_t pos)
// {
//   auto it = middle(pos);
//
//   if (it.block < m_blocks)
//     return &m_buffer[it.block][it.pos];
//
//   return nullptr;  // or throw if memory hasn't run out already...
// }

Joint* JointPool::create(const Vertex& vertex)
{
  // WriteLock lock(pool_mutex);
  if (m_size < m_capacity)
  {
    auto* joint = new (&m_buffer[m_block][m_next]) Joint(vertex);
    ++m_next;
    ++m_size;
    return joint;
  }
  return create_slow(vertex);
}

void JointPool::alloc_slow(std::size_t next_size)
{
  // m_buffer[m_block] = new Joint[next_size];
  auto nbytes = next_size * sizeof(Joint);
#if 0
  std::cout << "Allocating " << next_size << "*" << sizeof(Joint) << " = " << nbytes
            << " bytes\tvertex =" << sizeof(Vertex) << "\n";
#endif
  void* buffer = malloc(nbytes);
  m_buffer[m_block] = reinterpret_cast<Joint*>(buffer);
}

Joint* JointPool::create_slow(const Vertex& vertex)
{
  // First allocation requires a special check here
  if (m_size > 0)
    ++m_block;
  auto next_size = m_start_size << m_block;

  // Allocate the new block
  alloc_slow(next_size);
  ++m_blocks;

  // Allocate from it
  auto* joint = new (&m_buffer[m_block][0]) Joint(vertex);

  // And update indexes
  ++m_size;
  m_capacity += next_size;
  m_next = 1;

  return joint;
}

// For debugging only
std::size_t resolve(JointPool& pool, Joint* joint)
{
  auto i = 0UL;
  for (auto it = pool.begin(), end = pool.end(); it != end; ++it, ++i)
    if (*it == joint)
      return i;
  return 0xffffffff;
}

std::string to_string(JointPool& pool)
{
  std::string out;
  if (pool.size() == 0)
    return "EMPTY";

  auto i = 0UL;
  for (auto it = pool.begin(), end = pool.end(); it != end; ++it)
  {
    auto* j = *it;
    if (!j->used)
    {
      out += fmt::format("   {}\t{}\t{},{},{} : {},{},{}",
                         i,
                         reinterpret_cast<void*>(j),
                         j->vertex.x,
                         j->vertex.y,
                         j->used ? "T" : "F",
                         j->vertex.column,
                         j->vertex.row,
                         to_string(j->vertex.type));
      auto* prev = j->prev;
      auto* next = j->next;
      out += fmt::format("\tprev={},{} @ {}\tnext={},{} @ {}",
                         prev->vertex.x,
                         prev->vertex.y,
                         resolve(pool, prev),
                         next->vertex.x,
                         next->vertex.y,
                         resolve(pool, next));

      if (j->alt)
        out += fmt::format("\tALT={}", resolve(pool, j->alt));
      out += '\n';
    }
    ++i;
  }
  return out;
}

}  // namespace Trax
