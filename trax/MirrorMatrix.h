// ======================================================================
/*!
 * \brief Matrix adapter with mirror boundary conditions
 *
 * The data is mirrored at the borders so that the trend in the
 * data is preserved.
 *
 * For example, in 1D case we interpret like this:
 *
 *   f(-2) = f(0) - (f(2)-f(0)) = 2*f(0)-f(2)
 *
 * Similarly at the other end we have
 *
 *   f(w) = f(w-1) - (f(w-2) - f(w-1)) = 2*f(w-1) - f(w-2)
 *
 * To generalize:
 *
 *   f(j) = 2*f(w-1) - f( (w-1)-(j-(w-1))) = 2*f(w-1) - f(2*w-j-2)
 *
 * In the 2D case we simply apply the formulas first for i, then
 * for j.
 *
 * Warning: The mirroring does not extend beyond one grid width!
 */
// ======================================================================

#pragma once

#include <cassert>

namespace Trax
{
template <typename T>
class MirrorMatrix
{
 public:
  MirrorMatrix(const T& theMatrix) : M(theMatrix), W(theMatrix.width()), H(theMatrix.height()) {}
  std::size_t width() const { return W; }
  std::size_t height() const { return H; }
  double operator()(long i, long j) const
  {
    // Simple reflection does not work out of these bounds
    assert(i > -W);
    assert(i < 2 * W - 1);
    assert(j > -H);
    assert(j < 2 * H - 1);

    if (i < 0)
    {
      if (j < 0)
        return 2 * (2 * M(0, 0) - M(0, -j)) - (2 * M(-i, 0) - M(-i, -j));
      else if (j >= H)
        return 2 * (2 * M(0, H - 1) - M(0, 2 * H - j - 2)) -
               (2 * M(-i, H - 1) - M(-i, 2 * H - j - 2));
      else
        return 2 * M(0, j) - M(-i, j);
    }
    else if (i >= W)
    {
      if (j < 0)
        return 2 * (2 * M(W - 1, 0) - M(W - 1, -j)) -
               (2 * M(2 * W - i - 2, 0) - M(2 * W - i - 2, -j));
      else if (j >= H)
        return 2 * (2 * M(W - 1, H - 1) - M(W - 1, 2 * H - j - 2)) -
               (2 * M(2 * W - i - 2, H - 1) - M(2 * W - i - 2, 2 * H - j - 2));
      else
        return 2 * M(W - 1, j) - M(2 * W - i - 2, j);
    }
    else
    {
      if (j < 0)
        return 2 * M(i, 0) - M(i, -j);
      else if (j >= H)
        return 2 * M(i, H - 1) - M(i, 2 * H - j - 2);
      else
        return M(i, j);
    }
  }

 private:
  MirrorMatrix();
  const T& M;
  long W;
  long H;
};

}  // namespace Trax
