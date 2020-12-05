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

#include "cpu/o3/hash.hh"

#include <stdexcept>

#include <cassert>

namespace bf {

default_hash_function::default_hash_function(size_t seed) : h3_(seed) {
}

size_t default_hash_function::operator()(object const& o) const {
  // FIXME: fall back to a generic universal hash function (e.g., HMAC/MD5) for
  // too large objects.
  if (o.size() > max_obj_size)
    throw std::runtime_error("object too large");
  return o.size() == 0 ? 0 : h3_(o.data(), o.size());
}

default_hasher::default_hasher(std::vector<hash_function> fns)
    : fns_(std::move(fns)) {
}

std::vector<digest> default_hasher::operator()(object const& o) const {
  std::vector<digest> d(fns_.size());
  for (size_t i = 0; i < fns_.size(); ++i)
    d[i] = fns_[i](o);
  return d;
}

double_hasher::double_hasher(size_t k, hash_function h1, hash_function h2)
    : k_(k), h1_(std::move(h1)), h2_(std::move(h2)) {
}

std::vector<digest> double_hasher::operator()(object const& o) const {
  auto d1 = h1_(o);
  auto d2 = h2_(o);
  std::vector<digest> d(k_);
  for (size_t i = 0; i < d.size(); ++i)
    d[i] = d1 + i * d2;
  return d;
}

hasher make_hasher(size_t k, size_t seed, bool double_hashing) {
  assert(k > 0);
  std::minstd_rand0 prng(seed);
  if (double_hashing) {
    auto h1 = default_hash_function(prng());
    auto h2 = default_hash_function(prng());
    return double_hasher(k, std::move(h1), std::move(h2));
  } else {
    std::vector<hash_function> fns(k);
    for (size_t i = 0; i < k; ++i)
      fns[i] = default_hash_function(prng());
    return default_hasher(std::move(fns));
  }
}

} // namespace bf
