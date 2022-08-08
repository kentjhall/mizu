// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <memory>
#include <vector>
#include <sys/mman.h>
#include "core/hle/service/service.h"

namespace Service {

namespace FileSystem {
class FileSystemController;
} // namespace FileSystem

namespace NS {

enum class FontArchives : u64 {
    Extension = 0x0100000000000810,
    Standard = 0x0100000000000811,
    Korean = 0x0100000000000812,
    ChineseTraditional = 0x0100000000000813,
    ChineseSimple = 0x0100000000000814,
};

constexpr std::array<std::pair<FontArchives, const char*>, 7> SHARED_FONTS{
    std::make_pair(FontArchives::Standard, "nintendo_udsg-r_std_003.bfttf"),
    std::make_pair(FontArchives::ChineseSimple, "nintendo_udsg-r_org_zh-cn_003.bfttf"),
    std::make_pair(FontArchives::ChineseSimple, "nintendo_udsg-r_ext_zh-cn_003.bfttf"),
    std::make_pair(FontArchives::ChineseTraditional, "nintendo_udjxh-db_zh-tw_003.bfttf"),
    std::make_pair(FontArchives::Korean, "nintendo_udsg-r_ko_003.bfttf"),
    std::make_pair(FontArchives::Extension, "nintendo_ext_003.bfttf"),
    std::make_pair(FontArchives::Extension, "nintendo_ext2_003.bfttf"),
};

void DecryptSharedFontToTTF(const std::vector<u32>& input, std::vector<u8>& output);
void EncryptSharedFont(const std::vector<u32>& input, std::vector<u8>& output, std::size_t& offset);

class PL_U final : public ServiceFramework<PL_U> {
public:
    explicit PL_U();
    ~PL_U() override;

private:
    void RequestLoad(Kernel::HLERequestContext& ctx);
    void GetLoadState(Kernel::HLERequestContext& ctx);
    void GetSize(Kernel::HLERequestContext& ctx);
    void GetSharedMemoryAddressOffset(Kernel::HLERequestContext& ctx);
    void GetSharedMemoryNativeHandle(Kernel::HLERequestContext& ctx);
    void GetSharedFontInOrderOfPriority(Kernel::HLERequestContext& ctx);

    struct Impl;
    std::unique_ptr<Impl> impl;

    static constexpr std::size_t shared_mem_size{0x1100000};
    static constexpr auto shared_mem_deleter = std::bind(::munmap, std::placeholders::_1, shared_mem_size);
    int shared_mem_fd;
    std::unique_ptr<u8, decltype(shared_mem_deleter)> shared_mem;
};

} // namespace NS

} // namespace Service
