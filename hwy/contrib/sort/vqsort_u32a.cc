// Copyright 2021 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hwy/contrib/sort/vqsort.h"  // VQSort

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/sort/vqsort_u32a.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep

// After foreach_target
#include "hwy/contrib/sort/vqsort-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

void SortU32Asc(uint32_t* HWY_RESTRICT keys, const size_t num) {
  return VQSortStatic(keys, num, SortAscending());
}

void PartialSortU32Asc(uint32_t* HWY_RESTRICT keys, const size_t num,
                       const size_t k) {
  return VQPartialSortStatic(keys, num, k, SortAscending());
}

void SelectU32Asc(uint32_t* HWY_RESTRICT keys, const size_t num,
                  const size_t k) {
  return VQSelectStatic(keys, num, k, SortAscending());
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_EXPORT(SortU32Asc);
HWY_EXPORT(PartialSortU32Asc);
HWY_EXPORT(SelectU32Asc);
}  // namespace

void VQSort(uint32_t* HWY_RESTRICT keys, const size_t n, SortAscending) {
  HWY_DYNAMIC_DISPATCH(SortU32Asc)(keys, n);
}

void VQPartialSort(uint32_t* HWY_RESTRICT keys, const size_t n, const size_t k,
                   SortAscending) {
  HWY_DYNAMIC_DISPATCH(PartialSortU32Asc)(keys, n, k);
}

void VQSelect(uint32_t* HWY_RESTRICT keys, const size_t n, const size_t k,
              SortAscending) {
  HWY_DYNAMIC_DISPATCH(SelectU32Asc)(keys, n, k);
}

void Sorter::operator()(uint32_t* HWY_RESTRICT keys, size_t n,
                        SortAscending tag) const {
  VQSort(keys, n, tag);
}

}  // namespace hwy
#endif  // HWY_ONCE
