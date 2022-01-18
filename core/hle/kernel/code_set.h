// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <vector>
#include <linux/mizu.h>

#include "common/common_types.h"
#include "core/hle/kernel/physical_memory.h"

namespace Kernel {

/**
 * Represents executable data that may be loaded into a kernel process.
 *
 * A code set consists of three basic segments:
 *   - A code (AKA text) segment,
 *   - A read-only data segment (rodata)
 *   - A data segment
 *
 * The code segment is the portion of the object file that contains
 * executable instructions.
 *
 * The read-only data segment in the portion of the object file that
 * contains (as one would expect) read-only data, such as fixed constant
 * values and data structures.
 *
 * The data segment is similar to the read-only data segment -- it contains
 * variables and data structures that have predefined values, however,
 * entities within this segment can be modified.
 */
struct CodeSet final {
    struct mizu_codeset_hdr hdr;

    void SetMemory(std::vector<u8>&& memory_) {
        memory = std::move(memory_);
        hdr.memory_size = memory.size();
    }

    const std::vector<u8>& GetMemory() const {
        return memory;
    }

    auto& CodeSegment() {
        return hdr.segments[0];
    }

    const auto& CodeSegment() const {
        return hdr.segments[0];
    }

    auto& RODataSegment() {
        return hdr.segments[1];
    }

    const auto& RODataSegment() const {
        return hdr.segments[1];
    }

    auto& DataSegment() {
        return hdr.segments[2];
    }

    const auto& DataSegment() const {
        return hdr.segments[2];
    }

private:
    /// The overall data that backs this code set.
    std::vector<u8> memory;
};

} // namespace Kernel
