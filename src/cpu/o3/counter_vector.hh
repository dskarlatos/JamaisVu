/* Copyright (c) 2016, Matthias Vallentin
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    3. Neither the name of the copyright holder nor the names of its
       contributors may be used to endorse or promote products derived from
       this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

GitHub: https://github.com/mavam/libbf */

#ifndef BF_COUNTER_VECTOR_HPP
#define BF_COUNTER_VECTOR_HPP

#include <cstdint>
#include <vector>
#include <string>
#include "cpu/o3/bitvector.hh"

namespace bf {

/// The *fixed width* storage policy implements a bit vector where each
/// cell represents a counter having a fixed number of bits.
class counter_vector
{
  /// Generates a string representation of a counter vector.
  /// The arguments have the same meaning as in bf::bitvector.
  friend std::string to_string(counter_vector const& v, bool all = false,
                               size_t cut_off = 0)
  {
    return to_string(v.bits_, false, all, cut_off);
  }

  friend counter_vector operator|(counter_vector const& x,
                                  counter_vector const& y);

public:
  /// Construct a counter vector of size @f$O(mw)@f$ where *m is the number of
  /// cells and *w the number of bits per cell.
  ///
  /// @param cells The number of cells.
  ///
  /// @param width The number of bits per cell.
  ///
  /// @pre `cells > 0 && width > 0`
  counter_vector(size_t cells, size_t width);

  /// Merges this counter vector with another counter vector.
  /// @param other The other counter vector.
  /// @return A reference to `*this`.
  /// @pre `size() == other.size() && width() == other.width()`
  counter_vector& operator|=(counter_vector const& other);

  /// Increments a cell counter by a given value. If the value is larger
  /// than or equal to max(), all bits are set to 1.
  ///
  /// @param cell The cell index.
  ///
  /// @param value The value that is added to the current cell value.
  ///
  /// @return `true` if the increment succeeded, `false` if all bits in
  /// the cell were already 1.
  ///
  /// @pre `cell < size()`
  bool increment(size_t cell, size_t value = 1);

  /// Decrements a cell counter.
  ///
  /// @param cell The cell index.
  ///
  /// @return `true` if decrementing succeeded, `false` if all bits in the
  /// cell were already 0.
  ///
  /// @pre `cell < size()`
  bool decrement(size_t cell, size_t value = 1);

  /// Retrieves the counter of a cell.
  ///
  /// @param cell The cell index.
  ///
  /// @return The counter associated with *cell*.
  ///
  /// @pre `cell < size()`
  size_t count(size_t cell) const;

  /// Sets a cell to a given value.
  /// @param cell The cell whose value changes.
  /// @param value The new value of the cell.
  /// @pre `cell < size()`
  void set(size_t cell, size_t value);

  /// Sets all counter values to 0.
  void clear();

  /// Retrieves the number of cells.
  /// @return The number of cells in the counter vector.
  size_t size() const;

  /// Retrieves the maximum possible counter value.
  /// @return The maximum counter value constrained by the cell width.
  size_t max() const;

  /// Retrieves the counter width.
  /// @return The number of bits per cell.
  size_t width() const;

private:
  bitvector bits_;
  size_t width_;
};


} // namespace bf

#endif
