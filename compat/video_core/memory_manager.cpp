// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include <algorithm>
#include <unistd.h>
#include <sys/mman.h>
#include <asm-generic/mman-common.h>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/div_ceil.h"
#include "core/core.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_base.h"
#include "horizon_servctl.h"

const size_t PAGE_SIZE = ::sysconf(_SC_PAGESIZE);

namespace Tegra {

MemoryManager::MemoryManager() {}

MemoryManager::~MemoryManager() {
    std::shared_lock lock(mtx);
    for (const auto& alloc_range : alloc_ranges) {
        ::munmap(reinterpret_cast<void *>(alloc_range.gpu_addr), alloc_range.size);
    }
}

void MemoryManager::BindRasterizer(VideoCore::RasterizerInterface* rasterizer_) {
    rasterizer = rasterizer_;
}

GPUVAddr MemoryManager::Map(VAddr cpu_addr, GPUVAddr gpu_addr, std::size_t size) {
    std::unique_lock lock(mtx);
    for (auto& alloc_range : alloc_ranges) {
        if (gpu_addr >= alloc_range.gpu_addr &&
            gpu_addr + size <= alloc_range.gpu_addr + alloc_range.size) {
            horizon_servctl_map_memory(cpu_addr, gpu_addr, size);
            UnmapRegion(gpu_addr, size);
            map_ranges.emplace_back(gpu_addr, size, cpu_addr);
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
    rasterizer->GPU().FlushAndInvalidateRegion(ToCacheAddr(gpu_addr), size);
    {
        std::unique_lock lock(mtx);
        if (!UnmapRegion(gpu_addr, size)) {
            UNREACHABLE_MSG("Unmapping non-existent GPU address=0x{:x}", gpu_addr);
        }
    }
}

bool MemoryManager::UnmapRegion(GPUVAddr gpu_addr, std::size_t size) {
    ASSERT(size != 0);

    // unique_lock(mtx) should be held
    for (auto it = map_ranges.begin(); it != map_ranges.end(); ++it) {
        if (gpu_addr >= it->gpu_addr &&
            gpu_addr + size <= it->gpu_addr + it->size) {
            std::vector<MapRange> to_add;
            to_add.reserve(2);
            if (gpu_addr != it->gpu_addr) {
                to_add.emplace_back(
                        it->gpu_addr,
                        gpu_addr - it->gpu_addr,
                        it->cpu_addr);
            }
            if (gpu_addr + size != it->gpu_addr + it->size) {
                to_add.emplace_back(
                        gpu_addr + size,
                        it->gpu_addr + it->size - (gpu_addr + size),
                        it->cpu_addr + (gpu_addr + size - it->gpu_addr));
            }
            map_ranges.erase(it);
            map_ranges.insert(map_ranges.end(), to_add.begin(), to_add.end());
            return true;
        }
    }
    return false;
}

std::optional<GPUVAddr> MemoryManager::AllocateFixed(GPUVAddr gpu_addr, std::size_t size) {
    // mmap'd here to ensure available range, but will be mapped over on Map()
    if (::mmap(reinterpret_cast<void *>(gpu_addr), size, PROT_NONE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0) == MAP_FAILED) {
        return std::nullopt;
    }
    std::unique_lock lock(mtx);
    alloc_ranges.emplace_back(gpu_addr, size);
    return gpu_addr;
}

GPUVAddr MemoryManager::Allocate(std::size_t size, std::size_t align) {
    return *FindAllocateFreeRange(size, align);
}

std::optional<GPUVAddr> MemoryManager::FindAllocateFreeRange(std::size_t size, std::size_t align,
                                                             bool start_32bit_address) {
    if (!align) {
        align = PAGE_SIZE;
    } else {
        align = Common::AlignUp(align, PAGE_SIZE);
    }
#if 0
    if (align != PAGE_SIZE) {
        LOG_WARNING(HW_GPU, "Ignoring requested alignment of 0x{:x}", align);
    }
    align = PAGE_SIZE;
#endif

    std::unique_lock lock(mtx);
    if (align == PAGE_SIZE && !start_32bit_address) {
        // let mmap find the range if aligned normally
        void *addr = ::mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (addr == MAP_FAILED) {
            LOG_CRITICAL(HW_GPU, "mmap (size={}) failed: {}", size, ::strerror(errno));
            return std::nullopt;
        }
        GPUVAddr gpu_addr = reinterpret_cast<GPUVAddr>(addr);
        alloc_ranges.emplace_back(gpu_addr, size);
        return gpu_addr;
    }

    GPUVAddr gpu_addr{start_32bit_address ? address_space_start_low : address_space_start};
    while (gpu_addr < address_space_size) {
        // mmap'd here to find available range, but will be mapped over on Map()
        if (::mmap(reinterpret_cast<void *>(gpu_addr), size, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0) != MAP_FAILED) {
            alloc_ranges.emplace_back(gpu_addr, size);
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
    std::shared_lock lock(mtx);
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
    std::shared_lock lock(mtx);
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
    T value;
    std::memcpy(&value, GetPointer(addr), sizeof(T));
    return value;
}

template <typename T>
void MemoryManager::Write(GPUVAddr addr, T data) {
    std::memcpy(GetPointer(addr), &data, sizeof(T));
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
    std::shared_lock lock(mtx);
    auto it = std::ranges::upper_bound(alloc_ranges, gpu_addr, {}, &AllocRange::gpu_addr);
    --it;
    return it->size - (gpu_addr - it->gpu_addr);
}

void MemoryManager::ReadBlock(GPUVAddr gpu_src_addr, void* dest_buffer, std::size_t size) const {
    const auto submapped_ranges = GetSubmappedRange(gpu_src_addr, size);

    for (const auto& map : submapped_ranges) {
        // Flush must happen on the rasterizer interface, such that memory is always synchronous
        // when it is read (even when in asynchronous GPU mode). Fixes Dead Cells title menu.
        rasterizer->FlushRegion(ToCacheAddr(map.gpu_addr), map.size);

        ASSERT(map.gpu_addr >= gpu_src_addr && map.gpu_addr + map.size <= gpu_src_addr + size &&
               map.gpu_addr - gpu_src_addr + map.size <= size);
        // Not sure msync is necessary for shmem mapping, but can't hurt
        ASSERT_MSG(::msync(reinterpret_cast<void *>(map.gpu_addr & ~(PAGE_SIZE-1)),
                           map.size + (map.gpu_addr & (PAGE_SIZE-1)), MS_SYNC | MS_INVALIDATE) == 0,
                   "msync failed: {}", ::strerror(errno));
        ::memcpy(static_cast<char *>(dest_buffer) + (map.gpu_addr - gpu_src_addr),
                 reinterpret_cast<void *>(map.gpu_addr), map.size);
    }
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
        rasterizer->InvalidateRegion(ToCacheAddr(map.gpu_addr), map.size);

        ASSERT(map.gpu_addr >= gpu_dest_addr && map.gpu_addr + map.size <= gpu_dest_addr + size &&
               map.gpu_addr - gpu_dest_addr + map.size <= size);
        ::memcpy(reinterpret_cast<void *>(map.gpu_addr),
                 static_cast<const char *>(src_buffer) + (map.gpu_addr - gpu_dest_addr), map.size);
        // Not sure msync is necessary for shmem mapping, but can't hurt
        ASSERT_MSG(::msync(reinterpret_cast<void *>(map.gpu_addr & ~(PAGE_SIZE-1)),
                           map.size + (map.gpu_addr & (PAGE_SIZE-1)), MS_SYNC) == 0,
                   "msync failed: {}", ::strerror(errno));
    }
}

void MemoryManager::WriteBlockUnsafe(GPUVAddr gpu_dest_addr, const void* src_buffer,
                                     std::size_t size) {
    ::memcpy(reinterpret_cast<void *>(gpu_dest_addr), src_buffer, size);
}

void MemoryManager::FlushRegion(GPUVAddr gpu_addr, size_t size) const {
    rasterizer->FlushRegion(ToCacheAddr(gpu_addr), size);
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

bool MemoryManager::IsBlockContinuous(GPUVAddr gpu_addr, std::size_t size) const {
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
    std::shared_lock lock(mtx);
    std::vector<MapRange> result{};
    for (const auto& range : map_ranges) {
        if (range.gpu_addr + range.size <= gpu_addr ||
            range.gpu_addr >= gpu_addr + size ||
            !range.cpu_addr) {
            continue;
        }
        auto submap_start = range.gpu_addr < gpu_addr ? gpu_addr : range.gpu_addr;
        auto to_range_end = range.size - (submap_start - range.gpu_addr);
        auto submap_size = submap_start + to_range_end > gpu_addr + size ?
                           gpu_addr + size - submap_start : to_range_end;
        ASSERT(submap_start + submap_size <= gpu_addr + size);
        result.emplace_back(submap_start, submap_size,
                            range.cpu_addr + (submap_start - range.gpu_addr));
    }
    return result;
}

void MemoryManager::SyncCPUWrites()
{
    std::shared_lock lock(mtx);
    for (const auto& mapping : map_ranges) {
        long dirty_len = Common::DivCeil(mapping.size, PAGE_SIZE);
        std::unique_ptr<::loff_t[]> dirty(new ::loff_t[dirty_len]);
        dirty_len = horizon_servctl_memwatch_get_clear(rasterizer->GPU().SessionPid(),
                                                    mapping.cpu_addr, mapping.size, dirty.get(), dirty_len);
        for (::loff_t *off = dirty.get(); off - dirty.get() < dirty_len; ++off) {
            rasterizer->InvalidateRegion(mapping.gpu_addr + *off, PAGE_SIZE);
        }
    }
}

} // namespace Tegra
