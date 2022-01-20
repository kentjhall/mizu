// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include <cstring>
#include <vector>

#include "common/common_funcs.h"
#include "common/hex_util.h"
#include "common/logging/log.h"
#include "common/lz4_compression.h"
#include "common/settings.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/file_sys/patch_manager.h"
#include "core/hle/kernel/code_set.h"
#include "core/loader/nso.h"
#include "core/memory.h"

namespace Loader {
namespace {
struct MODHeader {
    u32_le magic;
    u32_le dynamic_offset;
    u32_le bss_start_offset;
    u32_le bss_end_offset;
    u32_le eh_frame_hdr_start_offset;
    u32_le eh_frame_hdr_end_offset;
    u32_le module_offset; // Offset to runtime-generated module object. typically equal to .bss base
};
static_assert(sizeof(MODHeader) == 0x1c, "MODHeader has incorrect size.");

std::vector<u8> DecompressSegment(const std::vector<u8>& compressed_data,
                                  const NSOSegmentHeader& header) {
    std::vector<u8> uncompressed_data =
        Common::Compression::DecompressDataLZ4(compressed_data, header.size);

    ASSERT_MSG(uncompressed_data.size() == header.size, "{} != {}", header.size,
               uncompressed_data.size());

    return uncompressed_data;
}
} // Anonymous namespace

bool NSOHeader::IsSegmentCompressed(size_t segment_num) const {
    ASSERT_MSG(segment_num < 3, "Invalid segment {}", segment_num);
    return ((flags >> segment_num) & 1) != 0;
}

AppLoader_NSO::AppLoader_NSO(FileSys::VirtualFile file_) : AppLoader(std::move(file_)) {}

FileType AppLoader_NSO::IdentifyType(const FileSys::VirtualFile& in_file) {
    u32 magic = 0;
    if (in_file->ReadObject(&magic) != sizeof(magic)) {
        return FileType::Error;
    }

    if (Common::MakeMagic('N', 'S', 'O', '0') != magic) {
        return FileType::Error;
    }

    return FileType::NSO;
}

bool AppLoader_NSO::LoadModule(std::vector<Kernel::CodeSet>& codesets,
                               const FileSys::VfsFile& nso_file,
                               bool should_pass_arguments,
                               std::optional<FileSys::PatchManager> pm) {
    if (nso_file.GetSize() < sizeof(NSOHeader)) {
        return 0;
    }

    NSOHeader nso_header{};
    if (sizeof(NSOHeader) != nso_file.ReadObject(&nso_header)) {
        return 0;
    }

    if (nso_header.magic != Common::MakeMagic('N', 'S', 'O', '0')) {
        return 0;
    }

    // Build program image
    Kernel::CodeSet codeset;
    std::vector<u8> program_image;
    for (std::size_t i = 0; i < nso_header.segments.size(); ++i) {
        std::vector<u8> data = nso_file.ReadBytes(nso_header.segments_compressed_size[i],
                                                  nso_header.segments[i].offset);
        if (nso_header.IsSegmentCompressed(i)) {
            data = DecompressSegment(data, nso_header.segments[i]);
        }
        program_image.resize(nso_header.segments[i].location + static_cast<u32>(data.size()));
        std::memcpy(program_image.data() + nso_header.segments[i].location, data.data(),
                    data.size());
        codeset.hdr.segments[i].addr = nso_header.segments[i].location;
        codeset.hdr.segments[i].offset = nso_header.segments[i].location;
        codeset.hdr.segments[i].size = nso_header.segments[i].size;
    }

    if (should_pass_arguments && !Settings::values.program_args.GetValue().empty()) {
        const auto arg_data{Settings::values.program_args.GetValue()};

        codeset.DataSegment().size += NSO_ARGUMENT_DATA_ALLOCATION_SIZE;
        NSOArgumentHeader args_header{
            NSO_ARGUMENT_DATA_ALLOCATION_SIZE, static_cast<u32_le>(arg_data.size()), {}};
        const auto end_offset = program_image.size();
        program_image.resize(static_cast<u32>(program_image.size()) +
                             NSO_ARGUMENT_DATA_ALLOCATION_SIZE);
        std::memcpy(program_image.data() + end_offset, &args_header, sizeof(NSOArgumentHeader));
        std::memcpy(program_image.data() + end_offset + sizeof(NSOArgumentHeader), arg_data.data(),
                    arg_data.size());
    }

    codeset.DataSegment().size += nso_header.segments[2].bss_size;
    const u32 image_size{
        PageAlignSize(static_cast<u32>(program_image.size()) + nso_header.segments[2].bss_size)};
    program_image.resize(image_size);

    for (std::size_t i = 0; i < nso_header.segments.size(); ++i) {
        codeset.hdr.segments[i].size = PageAlignSize(codeset.hdr.segments[i].size);
    }

    // Apply patches if necessary
    if (pm && (pm->HasNSOPatch(nso_header.build_id) || Settings::values.dump_nso)) {
        std::vector<u8> pi_header;
        pi_header.insert(pi_header.begin(), reinterpret_cast<u8*>(&nso_header),
                         reinterpret_cast<u8*>(&nso_header) + sizeof(NSOHeader));
        pi_header.insert(pi_header.begin() + sizeof(NSOHeader), program_image.data(),
                         program_image.data() + program_image.size());

        pi_header = pm->PatchNSO(pi_header, nso_file.GetName());

        std::copy(pi_header.begin() + sizeof(NSOHeader), pi_header.end(), program_image.data());
    }

#if 0 // mizu TEMP maybe (if cheats get implemented)
    // Apply cheats if they exist and the program has a valid title ID
    if (pm) {
        system.SetCurrentProcessBuildID(nso_header.build_id);
        const auto cheats = pm->CreateCheatList(nso_header.build_id);
        if (!cheats.empty()) {
            system.RegisterCheatList(cheats, nso_header.build_id, load_base, image_size);
        }
    }
#endif

    // Load codeset for current process
    codeset.SetMemory(std::move(program_image));
    codesets.push_back(std::move(codeset));

    return image_size;
}

AppLoader_NSO::LoadResult AppLoader_NSO::Load(::pid_t, std::vector<Kernel::CodeSet>& codesets) {
    if (is_loaded) {
        return ResultStatus::ErrorAlreadyLoaded;
    }

    // Load module
    if (!LoadModule(codesets, *file, true)) {
        return ResultStatus::ErrorLoadingNSO;
    }

    LOG_DEBUG(Loader, "loaded module {}", file->GetName());

    is_loaded = true;
    return ResultStatus::Success;
}

} // namespace Loader
