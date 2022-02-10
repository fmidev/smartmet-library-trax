#pragma once
#include "Joint.h"
#include <array>
#include <memory>
#include <string>

namespace Trax
{
struct Vertex;

/*
 * JointPool allocates Joint objects so that any returned pointer will
 * remain valid until the pool is destroyed. The compiler should see that
 * a Joint has a trivial destructor and hence should not call them
 * when the pool is destroyed (to be profiled).
 *
 * A non-standard pool is used since at the end we also need to be able
 * to traverse through all the allocated objects. operator[] may not
 * be needed except for the first element, getting the next element
 * we want is faster with 'next' which can utilize knowledge of the
 * structure of the internal buffers.
 *
 * Non-copyable due to internally allocated buffers (m_buffer).
 */

class JointPool
{
 public:
  friend class JointMerger;

  class iterator
  {
    friend class JointPool;

   public:
    iterator(JointPool* p) : pool(p) {}

    Joint* operator*();
    iterator& operator++();
    iterator& operator--();

    bool operator==(const iterator& other) const;
    bool operator!=(const iterator& other) const;

   private:
    JointPool* pool = nullptr;
    std::size_t block_size = 0;  // current block size
    std::size_t block = 0;       // current block
    std::size_t pos = 0;         // current position in the block
  };

  iterator begin();
  iterator end();
  // iterator middle(std::size_t pos);

  ~JointPool();

  JointPool(std::size_t start_size = 16384) : m_start_size(start_size) {}
  JointPool& operator=(JointPool&& other);

  Joint* create(const Vertex& vertex);

  std::size_t size() const { return m_size; }
  // Joint* operator[](std::size_t pos);

  // No copying allowed
  JointPool(const JointPool&) = delete;
  JointPool(JointPool&&) = delete;
  JointPool& operator=(const JointPool&) = delete;

 private:
  Joint* create_slow(const Vertex& vertex);
  void alloc_slow(std::size_t next_size);

  // Packed to 256 since 128 just is not feasible for a large number of vertices even
  // when using uint<N>_t, and 2**27 is plenty enough for any existing huge data.

  std::size_t m_start_size = 0;       // initial size after first alloc
  std::size_t m_size = 0;             // number of elements
  std::size_t m_capacity = 0;         // number of allocated elements
  std::size_t m_blocks = 0;           // number of allocated blocks
  std::size_t m_block = 0;            // current block being allocated from next
  std::size_t m_next = 0;             // block element being allocated next
  std::array<Joint*, 27UL> m_buffer;  // ... for a total of (5+27)*8 = 32*8 = 256 bytes
};

std::string to_string(JointPool& pool);

// It is important to inline below methods for speed

inline JointPool::iterator JointPool::end()
{
  iterator it(this);
  it.block_size = m_start_size << m_block;
  it.block = m_block;
  it.pos = m_next;
  // Must advance to real tail in next block if current block is full
  if (it.pos == it.block_size)
  {
    ++it.block;
    it.pos = 0;
    it.block_size *= 2;
  }
  return it;
}

inline JointPool::iterator& JointPool::iterator::operator++()
{
  ++pos;
  if (pos >= block_size)
  {
    ++block;
    block_size *= 2;
    pos = 0;
  }
  return *this;
}

inline JointPool::iterator& JointPool::iterator::operator--()
{
  if (pos > 0)
    --pos;
  else
  {
    --block;
    block_size /= 2;
    pos = block_size - 1;
  }
  return *this;
}

inline bool JointPool::iterator::operator==(const iterator& other) const
{
  return (pos == other.pos && block == other.block);
}

inline bool JointPool::iterator::operator!=(const iterator& other) const
{
  return (pos != other.pos || block != other.block);
}

inline Joint* JointPool::iterator::operator*()
{
  return &pool->m_buffer[block][pos];
}

}  // namespace Trax
