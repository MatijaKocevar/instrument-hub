#include "audio/RtGuard.h"

#include <cstdlib>
#include <new>

#if IHUB_RT_GUARD && IHUB_RT_GUARD_WRAP_MALLOC && defined(__linux__) && \
    (defined(__GNUC__) || defined(__clang__))
#define IHUB_RT_HAS_MALLOC_WRAP 1
#include <malloc.h>
#else
#define IHUB_RT_HAS_MALLOC_WRAP 0
#endif

#if IHUB_RT_GUARD

namespace {
void* rawAllocate(std::size_t size) noexcept {
  AudioEngine::RtDetail::skipMallocHook = true;
  void* pointer = std::malloc(size == 0 ? 1 : size);
  AudioEngine::RtDetail::skipMallocHook = false;
  return pointer;
}

void* rawAlignedAllocate(std::size_t size, std::size_t alignment) noexcept {
  const std::size_t rounded =
      size == 0 ? alignment : ((size + alignment - 1) / alignment) * alignment;
  return std::aligned_alloc(alignment, rounded);
}
}  // namespace

void* operator new(std::size_t size) {
  AudioEngine::RtGuard::noteAllocation("operator new");

  if (void* pointer = rawAllocate(size)) return pointer;

  throw std::bad_alloc();
}

void* operator new[](std::size_t size) { return ::operator new(size); }

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
  AudioEngine::RtGuard::noteAllocation("operator new (nothrow)");
  return rawAllocate(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
  return ::operator new(size, std::nothrow);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
  AudioEngine::RtGuard::noteAllocation("operator new (aligned)");

  if (void* pointer =
          rawAlignedAllocate(size, static_cast<std::size_t>(alignment)))
    return pointer;

  throw std::bad_alloc();
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
  return ::operator new(size, alignment);
}

void* operator new(std::size_t size, std::align_val_t alignment,
                   const std::nothrow_t&) noexcept {
  AudioEngine::RtGuard::noteAllocation("operator new (aligned, nothrow)");
  return rawAlignedAllocate(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
  return ::operator new(size, alignment, std::nothrow);
}

void operator delete(void* pointer) noexcept {
  AudioEngine::RtGuard::noteAllocation("operator delete");
  std::free(pointer);
}

void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }

void operator delete(void* pointer, std::size_t) noexcept {
  ::operator delete(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
  ::operator delete(pointer);
}

void operator delete(void* pointer, std::align_val_t) noexcept {
  ::operator delete(pointer);
}

void operator delete[](void* pointer, std::align_val_t) noexcept {
  ::operator delete(pointer);
}

void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept {
  ::operator delete(pointer);
}

void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept {
  ::operator delete(pointer);
}

#endif

#if IHUB_RT_HAS_MALLOC_WRAP

extern "C" {
void* __real_malloc(std::size_t);
void* __wrap_malloc(std::size_t size) {
  if (!AudioEngine::RtDetail::skipMallocHook)
    AudioEngine::RtGuard::noteAllocation("malloc");

  return __real_malloc(size);
}

void* __real_calloc(std::size_t count, std::size_t size);
void* __wrap_calloc(std::size_t count, std::size_t size) {
  if (!AudioEngine::RtDetail::skipMallocHook)
    AudioEngine::RtGuard::noteAllocation("calloc");

  return __real_calloc(count, size);
}

void* __real_realloc(void* pointer, std::size_t size);
void* __wrap_realloc(void* pointer, std::size_t size) {
  if (!AudioEngine::RtDetail::skipMallocHook)
    AudioEngine::RtGuard::noteAllocation("realloc");

  return __real_realloc(pointer, size);
}
}

#endif
