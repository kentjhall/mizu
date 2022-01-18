// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <unistd.h>
#include <sys/mman.h>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/memory.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_base.h"
#include "mizu_servctl.h"

const size_t PAGE_SIZE = ::sysconf(_SC_PAGESIZE);

namespace Tegra {

MemoryManager::MemoryManager() {}

MemoryManager::~MemoryManager() {
    for (const auto& map_range : map_ranges) {
        ::munmap(reinterpret_cast<void *>(map_range.gpu_addr), map_range.size);
    }
}

void MemoryManager::BindRasterizer(VideoCore::RasterizerInterface* rasterizer_) {
    rasterizer = rasterizer_;
}

GPUVAddr MemoryManager::Map(VAddr cpu_addr, GPUVAddr gpu_addr, std::size_t size) {
    for (auto& range : map_ranges) {
        if (range.gpu_addr == gpu_addr && range.size == size) {
            mizu_servctl_map_memory(cpu_addr, gpu_addr, size);
            range.cpu_addr = cpu_addr;
            return gpu_addr;
        }
    }
    LOG_CRITICAL(HW_GPU, "Attempt to map GPU memory outside allocated range (gpu_addr=0x{:x})", gpu_addr);
    return 0;
}

GPUVAddr MemoryManager::MapAllocate(VAddr cpu_addr, std::size_t size, std::size_t align) {
    return Map(cpu_addr, *FindAllocateFreeRange(size, align), size);
}

GPUVAddr MemoryManager::MapAllocate32(VAddr cpu_addr, std::size_t size) {
    const std::optional<GPUVAddr> gpu_addr = FindAllocateFreeRange(size, 1, true);
    ASSERT(gpu_addr);
    return Map(cpu_addr, *gpu_addr, size);
}

void MemoryManager::Unmap(GPUVAddr gpu_addr, std::size_t size) {
    if (size == 0) {
        return;
    }
    auto it = map_ranges.begin();
    for (; it != map_ranges.end(); ++it) {
        if (it->gpu_addr == gpu_addr && it->size == size) {
            map_ranges.erase(it);
            break;
        }
    }
    if (it == map_ranges.end()) {
        LOG_WARNING(HW_GPU, "Unmapping non-existent GPU address=0x{:x}", gpu_addr);
    }

    const auto submapped_ranges = GetSubmappedRange(gpu_addr, size);

    for (const auto& map : submapped_ranges) {
        // Flush and invalidate through the GPU interface, to be asynchronous if possible.
        rasterizer->UnmapMemory(map.cpu_addr, map.size);
    }

    ::munmap(reinterpret_cast<void *>(gpu_addr), size);
}

std::optional<GPUVAddr> MemoryManager::AllocateFixed(GPUVAddr gpu_addr, std::size_t size) {
    // mmap'd here to ensure available range, but will be mapped over on Map()
    if (::mmap(reinterpret_cast<void *>(gpu_addr), size, PROT_NONE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE,
               -1, 0) == MAP_FAILED) {
        return std::nullopt;
    }
    map_ranges.emplace_back(gpu_addr, size, 0);
    return gpu_addr;
}

GPUVAddr MemoryManager::Allocate(std::size_t size, std::size_t align) {
    return *FindAllocateFreeRange(size, align);
}

std::optional<GPUVAddr> MemoryManager::FindAllocateFreeRange(std::size_t size, std::size_t align,
                                                             bool start_32bit_address) {
#if 0
    if (!align) {
        align = PAGE_SIZE;
    } else {
        align = Common::AlignUp(align, PAGE_SIZE);
    }
#endif
    if (align != PAGE_SIZE) {
        LOG_WARNING(HW_GPU, "Ignoring requested alignment of 0x{:x}", align);
    }
    align = PAGE_SIZE;

    if (align == PAGE_SIZE && !start_32bit_address) {
        // let mmap find the range if aligned normally
        void *addr = ::mmap(NULL, size, PROT_NONE,
                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (addr == MAP_FAILED) {
            LOG_CRITICAL(HW_GPU, "mmap (size={}) failed: {}", size, ::strerror(errno));
            return std::nullopt;
        }
        GPUVAddr gpu_addr = reinterpret_cast<GPUVAddr>(addr);
        map_ranges.emplace_back(gpu_addr, size, 0);
        return gpu_addr;
    }

    GPUVAddr gpu_addr{start_32bit_address ? address_space_start_low : address_space_start};
    while (gpu_addr < address_space_size) {
        // mmap'd here to find available range, but will be mapped over on Map()
        if (::mmap(reinterpret_cast<void *>(gpu_addr), size, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE,
                   -1, 0) != MAP_FAILED) {
            map_ranges.emplace_back(gpu_addr, size, 0);
            return gpu_addr;
        } else {
            if (errno != EEXIST) {
                LOG_CRITICAL(HW_GPU, "mmap failed with unexpected error: {}",
                                     ::strerror(errno));
            }
            gpu_addr += PAGE_SIZE;

            const auto remainder{gpu_addr % align};
            if (remainder) {
                gpu_addr = (gpu_addr - remainder) + align;
            }
        }
    }

    LOG_CRITICAL(HW_GPU, "no mapping found (size={}, align={}, start_32bit_address={})",
                         size, align, start_32bit_address);
    return std::nullopt;
}

std::optional<VAddr> MemoryManager::GpuToCpuAddress(GPUVAddr gpu_addr) const {
    if (gpu_addr == 0) {
        return std::nullopt;
    }
    for (const auto& range : map_ranges) {
        if (gpu_addr >= range.gpu_addr &&
            gpu_addr < range.gpu_addr + range.size &&
            range.cpu_addr) {
            return range.cpu_addr + (gpu_addr - range.gpu_addr);
        }
    }
    return std::nullopt;
}

std::optional<VAddr> MemoryManager::GpuToCpuAddress(GPUVAddr addr, std::size_t size) const {
    for (const auto& range : map_ranges) {
        if (range.gpu_addr <= addr &&
            range.gpu_addr + range.size >= addr + size &&
            range.cpu_addr) {
            return range.cpu_addr;
        }
    }
    return std::nullopt;
}

template <typename T>
T MemoryManager::Read(GPUVAddr addr) const {
    // NOTE: Avoid adding any extra logic to this fast-path block
    T value;
    std::memcpy(&value, GetPointer(addr), sizeof(T));
    return value;
}

template <typename T>
void MemoryManager::Write(GPUVAddr addr, T data) {
    // NOTE: Avoid adding any extra logic to this fast-path block
    std::memcpy(GetPointer(addr), &data, sizeof(T));
    return;
}

template u8 MemoryManager::Read<u8>(GPUVAddr addr) const;
template u16 MemoryManager::Read<u16>(GPUVAddr addr) const;
template u32 MemoryManager::Read<u32>(GPUVAddr addr) const;
template u64 MemoryManager::Read<u64>(GPUVAddr addr) const;
template void MemoryManager::Write<u8>(GPUVAddr addr, u8 data);
template void MemoryManager::Write<u16>(GPUVAddr addr, u16 data);
template void MemoryManager::Write<u32>(GPUVAddr addr, u32 data);
template void MemoryManager::Write<u64>(GPUVAddr addr, u64 data);

u8* MemoryManager::GetPointer(GPUVAddr gpu_addr) {
    return reinterpret_cast<u8 *>(gpu_addr);
}

const u8* MemoryManager::GetPointer(GPUVAddr gpu_addr) const {
    return reinterpret_cast<const u8 *>(gpu_addr);
}

size_t MemoryManager::BytesToMapEnd(GPUVAddr gpu_addr) const noexcept {
    auto it = std::ranges::upper_bound(map_ranges, gpu_addr, {}, &MapRange::gpu_addr);
    --it;
    return it->size - (gpu_addr - it->gpu_addr);
}

void MemoryManager::ReadBlock(GPUVAddr gpu_src_addr, void* dest_buffer, std::size_t size) const {
    const auto submapped_ranges = GetSubmappedRange(gpu_src_addr, size);

    for (const auto& map : submapped_ranges) {
        // Flush must happen on the rasterizer interface, such that memory is always synchronous
        // when it is read (even when in asynchronous GPU mode). Fixes Dead Cells title menu.
        rasterizer->FlushRegion(map.cpu_addr, map.size);
    }
    ::memcpy(dest_buffer, reinterpret_cast<void *>(gpu_src_addr), size);
    ASSERT_MSG(::msync(reinterpret_cast<void *>(gpu_src_addr & ~(PAGE_SIZE-1)),
                       size + (gpu_src_addr & (PAGE_SIZE-1)), MS_SYNC) == 0,
               "msync failed: {}", ::strerror(errno));
}

void MemoryManager::ReadBlockUnsafe(GPUVAddr gpu_src_addr, void* dest_buffer,
                                    const std::size_t size) const {
    ::memcpy(dest_buffer, reinterpret_cast<void *>(gpu_src_addr), size);
}

void MemoryManager::WriteBlock(GPUVAddr gpu_dest_addr, const void* src_buffer, std::size_t size) {
    const auto submapped_ranges = GetSubmappedRange(gpu_dest_addr, size);

    for (const auto& map : submapped_ranges) {
        // Invalidate must happen on the rasterizer interface, such that memory is always
        // synchronous when it is written (even when in asynchronous GPU mode).
        rasterizer->InvalidateRegion(map.cpu_addr, map.size);
    }
    ::memcpy(reinterpret_cast<void *>(gpu_dest_addr), src_buffer, size);
    ASSERT_MSG(::msync(reinterpret_cast<void *>(gpu_dest_addr & ~(PAGE_SIZE-1)),
                       size + (gpu_dest_addr & (PAGE_SIZE-1)), MS_SYNC) == 0,
               "msync failed: {}", ::strerror(errno));
}

void MemoryManager::WriteBlockUnsafe(GPUVAddr gpu_dest_addr, const void* src_buffer,
                                     std::size_t size) {
    ::memcpy(reinterpret_cast<void *>(gpu_dest_addr), src_buffer, size);
}

void MemoryManager::FlushRegion(GPUVAddr gpu_addr, size_t size) const {
    for (const auto& range : map_ranges) {
        if (range.gpu_addr + range.size <= gpu_addr ||
            range.gpu_addr >= gpu_addr + size ||
            !range.cpu_addr) {
            continue;
        }
        auto to_flush = range.gpu_addr + range.size > gpu_addr + size ?
                        gpu_addr + size - range.gpu_addr : range.size;
        ASSERT(range.gpu_addr + to_flush < gpu_addr + size);
        rasterizer->FlushRegion(range.cpu_addr, to_flush);
    }
    ASSERT_MSG(::msync(reinterpret_cast<void *>(gpu_addr & ~(PAGE_SIZE-1)),
                       size + (gpu_addr & (PAGE_SIZE-1)), MS_SYNC) == 0,
               "msync failed: {}", ::strerror(errno));
}

void MemoryManager::CopyBlock(GPUVAddr gpu_dest_addr, GPUVAddr gpu_src_addr, std::size_t size) {
    std::vector<u8> tmp_buffer(size);
    ReadBlock(gpu_src_addr, tmp_buffer.data(), size);

    // The output block must be flushed in case it has data modified from the GPU.
    // Fixes NPC geometry in Zombie Panic in Wonderland DX
    FlushRegion(gpu_dest_addr, size);
    WriteBlock(gpu_dest_addr, tmp_buffer.data(), size);
}

bool MemoryManager::IsContinousRange(GPUVAddr gpu_addr, std::size_t size) const {
    return !!GpuToCpuAddress(gpu_addr, size);
}

bool MemoryManager::IsFullyMappedRange(GPUVAddr gpu_addr, std::size_t size) const {
    errno = 0;
    ::madvise(reinterpret_cast<void *>(gpu_addr & ~(PAGE_SIZE-1)),
              size + (gpu_addr & (PAGE_SIZE-1)), MADV_NORMAL);
    if (errno && errno != ENOMEM) {
        LOG_CRITICAL(HW_GPU, "madvise failed with unexpected error: {}", ::strerror(errno));
    }
    return errno == 0;
}

std::vector<MemoryManager::MapRange> MemoryManager::GetSubmappedRange(
    GPUVAddr gpu_addr, std::size_t size) const {
    std::vector<MapRange> result{};
    for (const auto& range : map_ranges) {
        if (range.gpu_addr + range.size <= gpu_addr ||
            range.gpu_addr >= gpu_addr + size ||
            !range.cpu_addr) {
            continue;
        }
        auto submap_size = range.gpu_addr + range.size > gpu_addr + size ?
                           gpu_addr + size - range.gpu_addr : range.size;
        ASSERT(range.gpu_addr + submap_size <= gpu_addr + size);
        result.emplace_back(range.gpu_addr, submap_size, range.cpu_addr);
    }
    return std::move(result);
}

} // namespace Tegra
