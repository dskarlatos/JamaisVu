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

#include "cpu/o3/counter_vector.hh"

#include <cassert>

namespace bf {

counter_vector::counter_vector(size_t cells, size_t width)
    : bits_(cells * width), width_(width) {
  assert(cells > 0);
  assert(width > 0);
}

counter_vector& counter_vector::operator|=(counter_vector const& other) {
  assert(size() == other.size());
  assert(width() == other.width());
  for (size_t cell = 0; cell < size(); ++cell) {
    bool carry = false;
    size_t lsb = cell * width_;
    for (size_t i = 0; i < width_; ++i) {
      bool b1 = bits_[lsb + i];
      bool b2 = other.bits_[lsb + i];
      bits_[lsb + i] = b1 ^ b2 ^ carry;
      carry = (b1 && b2) || (carry && (b1 != b2));
    }
    if (carry)
      for (size_t i = 0; i < width_; ++i)
        bits_.set(lsb + i);
  }
  return *this;
}

counter_vector operator|(counter_vector const& x, counter_vector const& y) {
  counter_vector cv(x);
  return cv |= y;
}

bool counter_vector::increment(size_t cell, size_t value) {
  assert(cell < size());
  assert(value != 0);
  size_t lsb = cell * width_;
  bool carry = false;
  for (size_t i = 0; i < width_; ++i) {
    bool b1 = bits_[lsb + i];
    bool b2 = value & (1 << i);
    bits_[lsb + i] = b1 ^ b2 ^ carry;
    carry = (b1 && b2) || (carry && (b1 != b2));
  }
  if (carry)
    for (size_t i = 0; i < width_; ++i)
      bits_[lsb + i] = true;
  return !carry;
}

bool counter_vector::decrement(size_t cell, size_t value) {
  assert(cell < size());
  assert(value != 0);
  value = ~value + 1; // A - B := A + ~B + 1
  bool carry = false;
  size_t lsb = cell * width_;
  for (size_t i = 0; i < width_; ++i) {
    bool b1 = bits_[lsb + i];
    bool b2 = value & (1 << i);
    bits_[lsb + i] = b1 ^ b2 ^ carry;
    carry = (b1 && b2) || (carry && (b1 != b2));
  }
  return carry;
}

size_t counter_vector::count(size_t cell) const {
  assert(cell < size());
  size_t cnt = 0, order = 1;
  size_t lsb = cell * width_;
  for (auto i = lsb; i < lsb + width_; ++i, order <<= 1)
    if (bits_[i])
      cnt |= order;
  return cnt;
}

void counter_vector::set(size_t cell, size_t value) {
  assert(cell < size());
  assert(value <= max());
  bitvector bits(width_, value);
  auto lsb = cell * width_;
  for (size_t i = 0; i < width_; ++i)
    bits_[lsb + i] = bits[i];
}

void counter_vector::clear() {
  bits_.reset();
}

size_t counter_vector::size() const {
  return bits_.size() / width_;
}

size_t counter_vector::max() const {
  using limits = std::numeric_limits<size_t>;
  return limits::max() >> (limits::digits - width());
}

size_t counter_vector::width() const {
  return width_;
}

} // namespace bf
