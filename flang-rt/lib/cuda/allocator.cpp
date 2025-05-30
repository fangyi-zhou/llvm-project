//===-- lib/cuda/allocator.cpp ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "flang/Runtime/CUDA/allocator.h"
#include "flang-rt/runtime/allocator-registry.h"
#include "flang-rt/runtime/derived.h"
#include "flang-rt/runtime/descriptor.h"
#include "flang-rt/runtime/environment.h"
#include "flang-rt/runtime/stat.h"
#include "flang-rt/runtime/terminator.h"
#include "flang-rt/runtime/type-info.h"
#include "flang/Common/ISO_Fortran_binding_wrapper.h"
#include "flang/Runtime/CUDA/common.h"
#include "flang/Support/Fortran.h"

#include "cuda_runtime.h"

namespace Fortran::runtime::cuda {
extern "C" {

void RTDEF(CUFRegisterAllocator)() {
  allocatorRegistry.Register(
      kPinnedAllocatorPos, {&CUFAllocPinned, CUFFreePinned});
  allocatorRegistry.Register(
      kDeviceAllocatorPos, {&CUFAllocDevice, CUFFreeDevice});
  allocatorRegistry.Register(
      kManagedAllocatorPos, {&CUFAllocManaged, CUFFreeManaged});
  allocatorRegistry.Register(
      kUnifiedAllocatorPos, {&CUFAllocUnified, CUFFreeUnified});
}
}

void *CUFAllocPinned(
    std::size_t sizeInBytes, [[maybe_unused]] std::int64_t asyncId) {
  void *p;
  CUDA_REPORT_IF_ERROR(cudaMallocHost((void **)&p, sizeInBytes));
  return p;
}

void CUFFreePinned(void *p) { CUDA_REPORT_IF_ERROR(cudaFreeHost(p)); }

void *CUFAllocDevice(std::size_t sizeInBytes, std::int64_t asyncId) {
  void *p;
  if (Fortran::runtime::executionEnvironment.cudaDeviceIsManaged) {
    CUDA_REPORT_IF_ERROR(
        cudaMallocManaged((void **)&p, sizeInBytes, cudaMemAttachGlobal));
  } else {
    if (asyncId == kNoAsyncId) {
      CUDA_REPORT_IF_ERROR(cudaMalloc(&p, sizeInBytes));
    } else {
      CUDA_REPORT_IF_ERROR(
          cudaMallocAsync(&p, sizeInBytes, (cudaStream_t)asyncId));
    }
  }
  return p;
}

void CUFFreeDevice(void *p) { CUDA_REPORT_IF_ERROR(cudaFree(p)); }

void *CUFAllocManaged(
    std::size_t sizeInBytes, [[maybe_unused]] std::int64_t asyncId) {
  void *p;
  CUDA_REPORT_IF_ERROR(
      cudaMallocManaged((void **)&p, sizeInBytes, cudaMemAttachGlobal));
  return reinterpret_cast<void *>(p);
}

void CUFFreeManaged(void *p) { CUDA_REPORT_IF_ERROR(cudaFree(p)); }

void *CUFAllocUnified(
    std::size_t sizeInBytes, [[maybe_unused]] std::int64_t asyncId) {
  // Call alloc managed for the time being.
  return CUFAllocManaged(sizeInBytes, asyncId);
}

void CUFFreeUnified(void *p) {
  // Call free managed for the time being.
  CUFFreeManaged(p);
}

} // namespace Fortran::runtime::cuda
