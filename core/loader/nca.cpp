// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include <utility>

#include "common/logging/log.h"
#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/romfs_factory.h"
#include "core/hle/service/service.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/deconstructed_rom_directory.h"
#include "core/loader/nca.h"

namespace Loader {

AppLoader_NCA::AppLoader_NCA(FileSys::VirtualFile file_)
    : AppLoader(std::move(file_)), nca(std::make_unique<FileSys::NCA>(file)) {}

AppLoader_NCA::~AppLoader_NCA() = default;

FileType AppLoader_NCA::IdentifyType(const FileSys::VirtualFile& nca_file) {
    const FileSys::NCA nca(nca_file);

    if (nca.GetStatus() == ResultStatus::Success &&
        nca.GetType() == FileSys::NCAContentType::Program) {
        return FileType::NCA;
    }

    return FileType::Error;
}

AppLoader_NCA::LoadResult AppLoader_NCA::Load(::pid_t pid, std::vector<Kernel::CodeSet>& codesets) {
    if (is_loaded) {
        return ResultStatus::ErrorAlreadyLoaded;
    }

    const auto result = nca->GetStatus();
    if (result != ResultStatus::Success) {
        return result;
    }

    if (nca->GetType() != FileSys::NCAContentType::Program) {
        return ResultStatus::ErrorNCANotProgram;
    }

    const auto exefs = nca->GetExeFS();
    if (exefs == nullptr) {
        return ResultStatus::ErrorNoExeFS;
    }

    directory_loader = std::make_unique<AppLoader_DeconstructedRomDirectory>(exefs, true);

    const auto load_result = directory_loader->Load(pid, codesets);
    if (load_result.load_result != ResultStatus::Success) {
        return load_result;
    }

    if (nca->GetRomFS() != nullptr && nca->GetRomFS()->GetSize() > 0) {
        Service::SharedWriter(Service::filesystem_controller)->RegisterRomFS(pid, *this);
    }

    is_loaded = true;
    return load_result;
}

ResultStatus AppLoader_NCA::ReadRomFS(FileSys::VirtualFile& dir) {
    if (nca == nullptr) {
        return ResultStatus::ErrorNotInitialized;
    }

    if (nca->GetRomFS() == nullptr || nca->GetRomFS()->GetSize() == 0) {
        return ResultStatus::ErrorNoRomFS;
    }

    dir = nca->GetRomFS();
    return ResultStatus::Success;
}

u64 AppLoader_NCA::ReadRomFSIVFCOffset() const {
    if (nca == nullptr) {
        return 0;
    }

    return nca->GetBaseIVFCOffset();
}

ResultStatus AppLoader_NCA::ReadProgramId(u64& out_program_id) {
    if (nca == nullptr || nca->GetStatus() != ResultStatus::Success) {
        return ResultStatus::ErrorNotInitialized;
    }

    out_program_id = nca->GetTitleId();
    return ResultStatus::Success;
}

ResultStatus AppLoader_NCA::ReadBanner(std::vector<u8>& buffer) {
    if (nca == nullptr || nca->GetStatus() != ResultStatus::Success) {
        return ResultStatus::ErrorNotInitialized;
    }

    const auto logo = nca->GetLogoPartition();
    if (logo == nullptr) {
        return ResultStatus::ErrorNoIcon;
    }

    buffer = logo->GetFile("StartupMovie.gif")->ReadAllBytes();
    return ResultStatus::Success;
}

ResultStatus AppLoader_NCA::ReadLogo(std::vector<u8>& buffer) {
    if (nca == nullptr || nca->GetStatus() != ResultStatus::Success) {
        return ResultStatus::ErrorNotInitialized;
    }

    const auto logo = nca->GetLogoPartition();
    if (logo == nullptr) {
        return ResultStatus::ErrorNoIcon;
    }

    buffer = logo->GetFile("NintendoLogo.png")->ReadAllBytes();
    return ResultStatus::Success;
}

} // namespace Loader
