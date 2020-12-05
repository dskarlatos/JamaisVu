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

#ifndef BF_H3_HPP
#define BF_H3_HPP

#include <limits>
#include <random>

namespace bf {

/// An implementation of the H3 hash function family.
template <typename T, int N>
class h3
{
  static size_t const bits_per_byte =
    std::numeric_limits<unsigned char>::digits;

public:
  constexpr static size_t byte_range =
    std::numeric_limits<unsigned char>::max() + 1;

  h3(T seed = 0)
  {
    T bits[N * bits_per_byte];
    std::minstd_rand0 prng(seed);
    for (size_t bit = 0; bit < N * bits_per_byte; ++bit)
    {
      bits[bit] = 0;
      for (size_t i = 0; i < sizeof(T)/2; i++)
        bits[bit] = (bits[bit] << 16) | (prng() & 0xFFFF);
    }

    for (size_t byte = 0; byte < N; ++byte)
      for (size_t val = 0; val < byte_range; ++val)
      {
        bytes_[byte][val] = 0;
        for (size_t bit = 0; bit < bits_per_byte; ++bit)
          if (val & (1 << bit))
            bytes_[byte][val] ^= bits[byte * bits_per_byte + bit];
      }
  }

  T operator()(void const* data, size_t size, size_t offset = 0) const
  {
    auto *p = static_cast<unsigned char const*>(data);
    T result = 0;
    // Duff's Device.
    auto n = (size + 7) / 8;
// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
    switch (size % 8)
    {
      case 0:	do { result ^= bytes_[offset++][*p++];
      case 7:      result ^= bytes_[offset++][*p++];
      case 6:      result ^= bytes_[offset++][*p++];
      case 5:      result ^= bytes_[offset++][*p++];
      case 4:      result ^= bytes_[offset++][*p++];
      case 3:      result ^= bytes_[offset++][*p++];
      case 2:      result ^= bytes_[offset++][*p++];
      case 1:      result ^= bytes_[offset++][*p++];
              } while ( --n > 0 );
    }
#pragma GCC diagnostic pop
    return result;
  }

private:
  T bytes_[N][byte_range];
};

} // namespace bf

#endif
