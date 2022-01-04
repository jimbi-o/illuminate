#ifndef ILLUMINATE_D3D12_MEMORY_ALLOCATOR_H
#define ILLUMINATE_D3D12_MEMORY_ALLOCATOR_H
#include "illuminate/memory/memory_allocation.h"
namespace illuminate {
extern LinearAllocator gSystemMemoryAllocator;
LinearAllocator GetTemporalMemoryAllocator();
}
#endif
