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

#ifndef BF_BLOOM_FILTER_LIBBF_HPP
#define BF_BLOOM_FILTER_LIBBF_HPP

#include "cpu/o3/wrap.hh"

namespace bf {

/// The abstract Bloom filter interface.
class bloom_filter
{
  bloom_filter(bloom_filter const&) = delete;
  bloom_filter& operator=(bloom_filter const&) = delete;

public:
  bloom_filter() = default;
  virtual ~bloom_filter() = default;

  /// Adds an element to the Bloom filter.
  /// @tparam T The type of the element to insert.
  /// @param x An instance of type `T`.
  template <typename T>
  void add(T const& x)
  {
    add(wrap(x));
  }

  /// Adds an element to the Bloom filter.
  /// @param o A wrapped object.
  virtual void add(object const& o) = 0;

  /// Retrieves the count of an element.
  /// @tparam T The type of the element to query.
  /// @param x An instance of type `T`.
  /// @return A frequency estimate for *x*.
  template <typename T>
  size_t lookup(T const& x) const
  {
    return lookup(wrap(x));
  }

  /// Retrieves the count of an element.
  /// @param o A wrapped object.
  /// @return A frequency estimate for *o*.
  virtual size_t lookup(object const& o) const = 0;

  /// Removes all items from the Bloom filter.
  virtual void clear() = 0;
};

} // namespace bf

#endif
