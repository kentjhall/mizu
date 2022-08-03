// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include <unistd.h>
#include <sys/stat.h>

#include "common/assert.h"
#include "common/fs/path_util.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/bis_factory.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/mode.h"
#include "core/file_sys/partition_filesystem.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs_factory.h"
#include "core/file_sys/savedata_factory.h"
#include "core/file_sys/sdmc_factory.h"
#include "core/file_sys/vfs.h"
#include "core/file_sys/vfs_offset.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/filesystem/fsp_ldr.h"
#include "core/hle/service/filesystem/fsp_pr.h"
#include "core/hle/service/filesystem/fsp_srv.h"
#include "core/loader/loader.h"

namespace Service::FileSystem {

// A default size for normal/journal save data size if application control metadata cannot be found.
// This should be large enough to satisfy even the most extreme requirements (~4.2GB)
constexpr u64 SUFFICIENT_SAVE_DATA_SIZE = 0xF0000000;

static FileSys::VirtualDir GetDirectoryRelativeWrapped(const FileSys::VirtualDir& base,
                                                       std::string_view dir_name_) {
    std::string dir_name(Common::FS::SanitizePath(dir_name_));
    if (dir_name.empty() || dir_name == "." || dir_name == "/" || dir_name == "\\")
        return base;

    return base->GetDirectoryRelative(dir_name);
}

VfsDirectoryServiceWrapper::VfsDirectoryServiceWrapper(FileSys::VirtualDir backing_)
    : backing(std::move(backing_)) {}

VfsDirectoryServiceWrapper::~VfsDirectoryServiceWrapper() = default;

std::string VfsDirectoryServiceWrapper::GetName() const {
    return backing->GetName();
}

ResultCode VfsDirectoryServiceWrapper::CreateFile(const std::string& path_, u64 size) const {
    std::string path(Common::FS::SanitizePath(path_));
    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(path));
    if (dir == nullptr) {
        return FileSys::ERROR_PATH_NOT_FOUND;
    }

    const auto entry_type = GetEntryType(path);
    if (entry_type.Code() == ResultSuccess) {
        return FileSys::ERROR_PATH_ALREADY_EXISTS;
    }

    auto file = dir->CreateFile(Common::FS::GetFilename(path));
    if (file == nullptr) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }
    if (!file->Resize(size)) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }
    return ResultSuccess;
}

ResultCode VfsDirectoryServiceWrapper::DeleteFile(const std::string& path_) const {
    std::string path(Common::FS::SanitizePath(path_));
    if (path.empty()) {
        // TODO(DarkLordZach): Why do games call this and what should it do? Works as is but...
        return ResultSuccess;
    }

    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(path));
    if (dir == nullptr || dir->GetFile(Common::FS::GetFilename(path)) == nullptr) {
        return FileSys::ERROR_PATH_NOT_FOUND;
    }
    if (!dir->DeleteFile(Common::FS::GetFilename(path))) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }

    return ResultSuccess;
}

ResultCode VfsDirectoryServiceWrapper::CreateDirectory(const std::string& path_) const {
    std::string path(Common::FS::SanitizePath(path_));

    // NOTE: This is inaccurate behavior. CreateDirectory is not recursive.
    // CreateDirectory should return PathNotFound if the parent directory does not exist.
    // This is here temporarily in order to have UMM "work" in the meantime.
    // TODO (Morph): Remove this when a hardware test verifies the correct behavior.
    const auto components = Common::FS::SplitPathComponents(path);
    std::string relative_path;
    for (const auto& component : components) {
        // Skip empty path components
        if (component.empty()) {
            continue;
        }
        relative_path = Common::FS::SanitizePath(relative_path + '/' + component);
        auto new_dir = backing->CreateSubdirectory(relative_path);
        if (new_dir == nullptr) {
            // TODO(DarkLordZach): Find a better error code for this
            return ResultUnknown;
        }
    }
    return ResultSuccess;
}

ResultCode VfsDirectoryServiceWrapper::DeleteDirectory(const std::string& path_) const {
    std::string path(Common::FS::SanitizePath(path_));
    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(path));
    if (!dir->DeleteSubdirectory(Common::FS::GetFilename(path))) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }
    return ResultSuccess;
}

ResultCode VfsDirectoryServiceWrapper::DeleteDirectoryRecursively(const std::string& path_) const {
    std::string path(Common::FS::SanitizePath(path_));
    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(path));
    if (!dir->DeleteSubdirectoryRecursive(Common::FS::GetFilename(path))) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }
    return ResultSuccess;
}

ResultCode VfsDirectoryServiceWrapper::CleanDirectoryRecursively(const std::string& path) const {
    const std::string sanitized_path(Common::FS::SanitizePath(path));
    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(sanitized_path));

    if (!dir->CleanSubdirectoryRecursive(Common::FS::GetFilename(sanitized_path))) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }

    return ResultSuccess;
}

ResultCode VfsDirectoryServiceWrapper::RenameFile(const std::string& src_path_,
                                                  const std::string& dest_path_) const {
    std::string src_path(Common::FS::SanitizePath(src_path_));
    std::string dest_path(Common::FS::SanitizePath(dest_path_));
    auto src = backing->GetFileRelative(src_path);
    if (Common::FS::GetParentPath(src_path) == Common::FS::GetParentPath(dest_path)) {
        // Use more-optimized vfs implementation rename.
        if (src == nullptr)
            return FileSys::ERROR_PATH_NOT_FOUND;
        if (!src->Rename(Common::FS::GetFilename(dest_path))) {
            // TODO(DarkLordZach): Find a better error code for this
            return ResultUnknown;
        }
        return ResultSuccess;
    }

    // Move by hand -- TODO(DarkLordZach): Optimize
    auto c_res = CreateFile(dest_path, src->GetSize());
    if (c_res != ResultSuccess)
        return c_res;

    auto dest = backing->GetFileRelative(dest_path);
    ASSERT_MSG(dest != nullptr, "Newly created file with success cannot be found.");

    ASSERT_MSG(dest->WriteBytes(src->ReadAllBytes()) == src->GetSize(),
               "Could not write all of the bytes but everything else has succeded.");

    if (!src->GetContainingDirectory()->DeleteFile(Common::FS::GetFilename(src_path))) {
        // TODO(DarkLordZach): Find a better error code for this
        return ResultUnknown;
    }

    return ResultSuccess;
}

ResultCode VfsDirectoryServiceWrapper::RenameDirectory(const std::string& src_path_,
                                                       const std::string& dest_path_) const {
    std::string src_path(Common::FS::SanitizePath(src_path_));
    std::string dest_path(Common::FS::SanitizePath(dest_path_));
    auto src = GetDirectoryRelativeWrapped(backing, src_path);
    if (Common::FS::GetParentPath(src_path) == Common::FS::GetParentPath(dest_path)) {
        // Use more-optimized vfs implementation rename.
        if (src == nullptr)
            return FileSys::ERROR_PATH_NOT_FOUND;
        if (!src->Rename(Common::FS::GetFilename(dest_path))) {
            // TODO(DarkLordZach): Find a better error code for this
            return ResultUnknown;
        }
        return ResultSuccess;
    }

    // TODO(DarkLordZach): Implement renaming across the tree (move).
    ASSERT_MSG(false,
               "Could not rename directory with path \"{}\" to new path \"{}\" because parent dirs "
               "don't match -- UNIMPLEMENTED",
               src_path, dest_path);

    // TODO(DarkLordZach): Find a better error code for this
    return ResultUnknown;
}

ResultVal<FileSys::VirtualFile> VfsDirectoryServiceWrapper::OpenFile(const std::string& path_,
                                                                     FileSys::Mode mode) const {
    const std::string path(Common::FS::SanitizePath(path_));
    std::string_view npath = path;
    while (!npath.empty() && (npath[0] == '/' || npath[0] == '\\')) {
        npath.remove_prefix(1);
    }

    auto file = backing->GetFileRelative(npath);
    if (file == nullptr) {
        return FileSys::ERROR_PATH_NOT_FOUND;
    }

    if (mode == FileSys::Mode::Append) {
        return MakeResult<FileSys::VirtualFile>(
            std::make_shared<FileSys::OffsetVfsFile>(file, 0, file->GetSize()));
    }

    return MakeResult<FileSys::VirtualFile>(file);
}

ResultVal<FileSys::VirtualDir> VfsDirectoryServiceWrapper::OpenDirectory(const std::string& path_) {
    std::string path(Common::FS::SanitizePath(path_));
    auto dir = GetDirectoryRelativeWrapped(backing, path);
    if (dir == nullptr) {
        // TODO(DarkLordZach): Find a better error code for this
        return FileSys::ERROR_PATH_NOT_FOUND;
    }
    return MakeResult(dir);
}

ResultVal<FileSys::EntryType> VfsDirectoryServiceWrapper::GetEntryType(
    const std::string& path_) const {
    std::string path(Common::FS::SanitizePath(path_));
    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(path));
    if (dir == nullptr)
        return FileSys::ERROR_PATH_NOT_FOUND;
    auto filename = Common::FS::GetFilename(path);
    // TODO(Subv): Some games use the '/' path, find out what this means.
    if (filename.empty())
        return MakeResult(FileSys::EntryType::Directory);

    if (dir->GetFile(filename) != nullptr)
        return MakeResult(FileSys::EntryType::File);
    if (dir->GetSubdirectory(filename) != nullptr)
        return MakeResult(FileSys::EntryType::Directory);
    return FileSys::ERROR_PATH_NOT_FOUND;
}

ResultVal<FileSys::FileTimeStampRaw> VfsDirectoryServiceWrapper::GetFileTimeStampRaw(
    const std::string& path) const {
    auto dir = GetDirectoryRelativeWrapped(backing, Common::FS::GetParentPath(path));
    if (dir == nullptr) {
        return FileSys::ERROR_PATH_NOT_FOUND;
    }
    if (GetEntryType(path).Failed()) {
        return FileSys::ERROR_PATH_NOT_FOUND;
    }
    return MakeResult(dir->GetFileTimeStamp(Common::FS::GetFilename(path)));
}

FileSystemController::FileSystemController() {}

FileSystemController::~FileSystemController() = default;

void FileSystemController::UnregisterRomFS(::pid_t pid) {
    romfs_factories.erase(pid);
}

ResultCode FileSystemController::RegisterSaveData(
    std::unique_ptr<FileSys::SaveDataFactory>&& factory) {
    ASSERT_MSG(save_data_factory == nullptr, "Tried to register a second save data");
    save_data_factory = std::move(factory);
    LOG_DEBUG(Service_FS, "Registered save data");
    return ResultSuccess;
}

ResultCode FileSystemController::RegisterSDMC(std::unique_ptr<FileSys::SDMCFactory>&& factory) {
    ASSERT_MSG(sdmc_factory == nullptr, "Tried to register a second SDMC");
    sdmc_factory = std::move(factory);
    LOG_DEBUG(Service_FS, "Registered SDMC");
    return ResultSuccess;
}

ResultCode FileSystemController::RegisterBIS(std::unique_ptr<FileSys::BISFactory>&& factory) {
    ASSERT_MSG(bis_factory == nullptr, "Tried to register a second BIS");
    bis_factory = std::move(factory);
    LOG_DEBUG(Service_FS, "Registered BIS");
    return ResultSuccess;
}

void FileSystemController::SetPackedUpdate(::pid_t pid, FileSys::VirtualFile update_raw) {
    LOG_TRACE(Service_FS, "Setting packed update for romfs");

    auto it = romfs_factories.find(pid);
    if (it == romfs_factories.end())
        return;

    it->second.SetPackedUpdate(std::move(update_raw));
}

ResultVal<FileSys::VirtualFile> FileSystemController::OpenRomFSProcess(::pid_t pid) const {
    LOG_TRACE(Service_FS, "Opening RomFS for process (pid={})", pid);

    auto it = romfs_factories.find(pid);
    if (it == romfs_factories.end()) {
        // TODO(bunnei): Find a better error code for this
        return ResultUnknown;
    }

    return it->second.OpenCurrentProcess(Service::GetTitleID());
}

ResultVal<FileSys::VirtualFile> FileSystemController::OpenPatchedRomFS(
    ::pid_t pid, u64 title_id, FileSys::ContentRecordType type) const {
    LOG_TRACE(Service_FS, "Opening patched RomFS for title_id={:016X}", title_id);

    auto it = romfs_factories.find(pid);
    if (it == romfs_factories.end()) {
        // TODO: Find a better error code for this
        return ResultUnknown;
    }

    return it->second.OpenPatchedRomFS(title_id, type);
}

ResultVal<FileSys::VirtualFile> FileSystemController::OpenPatchedRomFSWithProgramIndex(
    ::pid_t pid, u64 title_id, u8 program_index, FileSys::ContentRecordType type) const {
    LOG_TRACE(Service_FS, "Opening patched RomFS for title_id={:016X}, program_index={}", title_id,
              program_index);

    auto it = romfs_factories.find(pid);
    if (it == romfs_factories.end()) {
        // TODO: Find a better error code for this
        return ResultUnknown;
    }

    return it->second.OpenPatchedRomFSWithProgramIndex(title_id, program_index, type);
}

ResultVal<FileSys::VirtualFile> FileSystemController::OpenRomFS(
    ::pid_t pid, u64 title_id, FileSys::StorageId storage_id, FileSys::ContentRecordType type) const {
    LOG_TRACE(Service_FS, "Opening RomFS for title_id={:016X}, storage_id={:02X}, type={:02X}",
              title_id, storage_id, type);

    auto it = romfs_factories.find(pid);
    if (it == romfs_factories.end()) {
        // TODO(bunnei): Find a better error code for this
        return ResultUnknown;
    }

    return it->second.Open(title_id, storage_id, type);
}

ResultVal<FileSys::VirtualDir> FileSystemController::CreateSaveData(
    FileSys::SaveDataSpaceId space, const FileSys::SaveDataAttribute& save_struct) const {
    LOG_TRACE(Service_FS, "Creating Save Data for space_id={:01X}, save_struct={}", space,
              save_struct.DebugInfo());

    if (save_data_factory == nullptr) {
        return FileSys::ERROR_ENTITY_NOT_FOUND;
    }

    return save_data_factory->Create(space, save_struct);
}

ResultVal<FileSys::VirtualDir> FileSystemController::OpenSaveData(
    FileSys::SaveDataSpaceId space, const FileSys::SaveDataAttribute& attribute) const {
    LOG_TRACE(Service_FS, "Opening Save Data for space_id={:01X}, save_struct={}", space,
              attribute.DebugInfo());

    if (save_data_factory == nullptr) {
        return FileSys::ERROR_ENTITY_NOT_FOUND;
    }

    return save_data_factory->Open(space, attribute);
}

ResultVal<FileSys::VirtualDir> FileSystemController::OpenSaveDataSpace(
    FileSys::SaveDataSpaceId space) const {
    LOG_TRACE(Service_FS, "Opening Save Data Space for space_id={:01X}", space);

    if (save_data_factory == nullptr) {
        return FileSys::ERROR_ENTITY_NOT_FOUND;
    }

    return MakeResult(save_data_factory->GetSaveDataSpaceDirectory(space));
}

ResultVal<FileSys::VirtualDir> FileSystemController::OpenSDMC() const {
    LOG_TRACE(Service_FS, "Opening SDMC");

    if (sdmc_factory == nullptr) {
        return FileSys::ERROR_SD_CARD_NOT_FOUND;
    }

    return sdmc_factory->Open();
}

ResultVal<FileSys::VirtualDir> FileSystemController::OpenBISPartition(
    FileSys::BisPartitionId id) const {
    LOG_TRACE(Service_FS, "Opening BIS Partition with id={:08X}", id);

    if (bis_factory == nullptr) {
        return FileSys::ERROR_ENTITY_NOT_FOUND;
    }

    auto part = bis_factory->OpenPartition(id);
    if (part == nullptr) {
        return FileSys::ERROR_INVALID_ARGUMENT;
    }

    return MakeResult<FileSys::VirtualDir>(std::move(part));
}

ResultVal<FileSys::VirtualFile> FileSystemController::OpenBISPartitionStorage(
    FileSys::BisPartitionId id) const {
    LOG_TRACE(Service_FS, "Opening BIS Partition Storage with id={:08X}", id);

    if (bis_factory == nullptr) {
        return FileSys::ERROR_ENTITY_NOT_FOUND;
    }

    auto part = bis_factory->OpenPartitionStorage(id);
    if (part == nullptr) {
        return FileSys::ERROR_INVALID_ARGUMENT;
    }

    return MakeResult<FileSys::VirtualFile>(std::move(part));
}

u64 FileSystemController::GetFreeSpaceSize(FileSys::StorageId id) const {
    switch (id) {
    case FileSys::StorageId::None:
    case FileSys::StorageId::GameCard:
        return 0;
    case FileSys::StorageId::SdCard:
        if (sdmc_factory == nullptr)
            return 0;
        return sdmc_factory->GetSDMCFreeSpace();
    case FileSys::StorageId::Host:
        if (bis_factory == nullptr)
            return 0;
        return bis_factory->GetSystemNANDFreeSpace() + bis_factory->GetUserNANDFreeSpace();
    case FileSys::StorageId::NandSystem:
        if (bis_factory == nullptr)
            return 0;
        return bis_factory->GetSystemNANDFreeSpace();
    case FileSys::StorageId::NandUser:
        if (bis_factory == nullptr)
            return 0;
        return bis_factory->GetUserNANDFreeSpace();
    }

    return 0;
}

u64 FileSystemController::GetTotalSpaceSize(FileSys::StorageId id) const {
    switch (id) {
    case FileSys::StorageId::None:
    case FileSys::StorageId::GameCard:
        return 0;
    case FileSys::StorageId::SdCard:
        if (sdmc_factory == nullptr)
            return 0;
        return sdmc_factory->GetSDMCTotalSpace();
    case FileSys::StorageId::Host:
        if (bis_factory == nullptr)
            return 0;
        return bis_factory->GetFullNANDTotalSpace();
    case FileSys::StorageId::NandSystem:
        if (bis_factory == nullptr)
            return 0;
        return bis_factory->GetSystemNANDTotalSpace();
    case FileSys::StorageId::NandUser:
        if (bis_factory == nullptr)
            return 0;
        return bis_factory->GetUserNANDTotalSpace();
    }
    return 0;
}

FileSys::SaveDataSize FileSystemController::ReadSaveDataSize(FileSys::SaveDataType type,
                                                             u64 title_id, u128 user_id) const {
#if 0
    if (save_data_factory == nullptr) {
        return {0, 0};
    }

    const auto value = save_data_factory->ReadSaveDataSize(type, title_id, user_id);

    if (value.normal == 0 && value.journal == 0) {
        FileSys::SaveDataSize new_size{SUFFICIENT_SAVE_DATA_SIZE, SUFFICIENT_SAVE_DATA_SIZE};

        FileSys::NACP nacp;
        int res = mizu_servctl(MIZU_SCTL_LOADER_READ_CTL_DATA, (unsigned long)&nacp);

        if (res == -1) {
            const FileSys::PatchManager pm{Service::GetTitleID()};
            const auto metadata = pm.GetControlMetadata();
            const auto& nacp_unique = metadata.first;

            if (nacp_unique != nullptr) {
                new_size = {nacp_unique->GetDefaultNormalSaveSize(),
                            nacp_unique->GetDefaultJournalSaveSize()};
            }
        } else {
            new_size = {nacp.GetDefaultNormalSaveSize(), nacp.GetDefaultJournalSaveSize()};
        }

        WriteSaveDataSize(type, title_id, user_id, new_size);
        return new_size;
    }

    return value;
#endif
    LOG_CRITICAL(Service_FS, "mizu TODO");
    return {0, 0};
}

void FileSystemController::WriteSaveDataSize(FileSys::SaveDataType type, u64 title_id, u128 user_id,
                                             FileSys::SaveDataSize new_value) const {
    if (save_data_factory != nullptr)
        save_data_factory->WriteSaveDataSize(type, title_id, user_id, new_value);
}

void FileSystemController::SetGameCard(FileSys::VirtualFile file) {
    gamecard = std::make_unique<FileSys::XCI>(file);
    const auto dir = gamecard->ConcatenatedPseudoDirectory();
    gamecard_registered = std::make_unique<FileSys::RegisteredCache>(dir);
    gamecard_placeholder = std::make_unique<FileSys::PlaceholderCache>(dir);
}

FileSys::XCI* FileSystemController::GetGameCard() const {
    return gamecard.get();
}

FileSys::RegisteredCache* FileSystemController::GetGameCardContents() const {
    return gamecard_registered.get();
}

FileSys::PlaceholderCache* FileSystemController::GetGameCardPlaceholder() const {
    return gamecard_placeholder.get();
}

FileSys::RegisteredCache* FileSystemController::GetSystemNANDContents() const {
    LOG_TRACE(Service_FS, "Opening System NAND Contents");

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetSystemNANDContents();
}

FileSys::RegisteredCache* FileSystemController::GetUserNANDContents() const {
    LOG_TRACE(Service_FS, "Opening User NAND Contents");

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetUserNANDContents();
}

FileSys::RegisteredCache* FileSystemController::GetSDMCContents() const {
    LOG_TRACE(Service_FS, "Opening SDMC Contents");

    if (sdmc_factory == nullptr)
        return nullptr;

    return sdmc_factory->GetSDMCContents();
}

FileSys::PlaceholderCache* FileSystemController::GetSystemNANDPlaceholder() const {
    LOG_TRACE(Service_FS, "Opening System NAND Placeholder");

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetSystemNANDPlaceholder();
}

FileSys::PlaceholderCache* FileSystemController::GetUserNANDPlaceholder() const {
    LOG_TRACE(Service_FS, "Opening User NAND Placeholder");

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetUserNANDPlaceholder();
}

FileSys::PlaceholderCache* FileSystemController::GetSDMCPlaceholder() const {
    LOG_TRACE(Service_FS, "Opening SDMC Placeholder");

    if (sdmc_factory == nullptr)
        return nullptr;

    return sdmc_factory->GetSDMCPlaceholder();
}

FileSys::RegisteredCache* FileSystemController::GetRegisteredCacheForStorage(
    FileSys::StorageId id) const {
    switch (id) {
    case FileSys::StorageId::None:
    case FileSys::StorageId::Host:
        UNIMPLEMENTED();
        return nullptr;
    case FileSys::StorageId::GameCard:
        return GetGameCardContents();
    case FileSys::StorageId::NandSystem:
        return GetSystemNANDContents();
    case FileSys::StorageId::NandUser:
        return GetUserNANDContents();
    case FileSys::StorageId::SdCard:
        return GetSDMCContents();
    }

    return nullptr;
}

FileSys::PlaceholderCache* FileSystemController::GetPlaceholderCacheForStorage(
    FileSys::StorageId id) const {
    switch (id) {
    case FileSys::StorageId::None:
    case FileSys::StorageId::Host:
        UNIMPLEMENTED();
        return nullptr;
    case FileSys::StorageId::GameCard:
        return GetGameCardPlaceholder();
    case FileSys::StorageId::NandSystem:
        return GetSystemNANDPlaceholder();
    case FileSys::StorageId::NandUser:
        return GetUserNANDPlaceholder();
    case FileSys::StorageId::SdCard:
        return GetSDMCPlaceholder();
    }

    return nullptr;
}

FileSys::VirtualDir FileSystemController::GetSystemNANDContentDirectory() const {
    LOG_TRACE(Service_FS, "Opening system NAND content directory");

    if (bis_factory == nullptr)
        return FileSys::VirtualDir();

    return bis_factory->GetSystemNANDContentDirectory();
}

FileSys::VirtualDir FileSystemController::GetUserNANDContentDirectory() const {
    LOG_TRACE(Service_FS, "Opening user NAND content directory");

    if (bis_factory == nullptr)
        return FileSys::VirtualDir();

    return bis_factory->GetUserNANDContentDirectory();
}

FileSys::VirtualDir FileSystemController::GetSDMCContentDirectory() const {
    LOG_TRACE(Service_FS, "Opening SDMC content directory");

    if (sdmc_factory == nullptr)
        return FileSys::VirtualDir();

    return sdmc_factory->GetSDMCContentDirectory();
}

FileSys::VirtualDir FileSystemController::GetNANDImageDirectory() const {
    LOG_TRACE(Service_FS, "Opening NAND image directory");

    if (bis_factory == nullptr)
        return FileSys::VirtualDir();

    return bis_factory->GetImageDirectory();
}

FileSys::VirtualDir FileSystemController::GetSDMCImageDirectory() const {
    LOG_TRACE(Service_FS, "Opening SDMC image directory");

    if (sdmc_factory == nullptr)
        return FileSys::VirtualDir();

    return sdmc_factory->GetImageDirectory();
}

FileSys::VirtualDir FileSystemController::GetContentDirectory(ContentStorageId id) const {
    switch (id) {
    case ContentStorageId::System:
        return GetSystemNANDContentDirectory();
    case ContentStorageId::User:
        return GetUserNANDContentDirectory();
    case ContentStorageId::SdCard:
        return GetSDMCContentDirectory();
    }

    return nullptr;
}

FileSys::VirtualDir FileSystemController::GetImageDirectory(ImageDirectoryId id) const {
    switch (id) {
    case ImageDirectoryId::NAND:
        return GetNANDImageDirectory();
    case ImageDirectoryId::SdCard:
        return GetSDMCImageDirectory();
    }

    return nullptr;
}

FileSys::VirtualDir FileSystemController::GetModificationLoadRoot(u64 title_id) const {
    LOG_TRACE(Service_FS, "Opening mod load root for tid={:016X}", title_id);

    if (bis_factory == nullptr)
        return FileSys::VirtualDir();

    return bis_factory->GetModificationLoadRoot(title_id);
}

FileSys::VirtualDir FileSystemController::GetSDMCModificationLoadRoot(u64 title_id) const {
    LOG_TRACE(Service_FS, "Opening SDMC mod load root for tid={:016X}", title_id);

    if (sdmc_factory == nullptr) {
        return nullptr;
    }

    return sdmc_factory->GetSDMCModificationLoadRoot(title_id);
}

FileSys::VirtualDir FileSystemController::GetModificationDumpRoot(u64 title_id) const {
    LOG_TRACE(Service_FS, "Opening mod dump root for tid={:016X}", title_id);

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetModificationDumpRoot(title_id);
}

FileSys::VirtualDir FileSystemController::GetBCATDirectory(u64 title_id) const {
    LOG_TRACE(Service_FS, "Opening BCAT root for tid={:016X}", title_id);

    if (bis_factory == nullptr)
        return nullptr;

    return bis_factory->GetBCATDirectory(title_id);
}

void FileSystemController::SetAutoSaveDataCreation(bool enable) {
    save_data_factory->SetAutoCreate(enable);
}

void FileSystemController::CreateFactories(bool overwrite) {
    if (overwrite) {
        bis_factory = nullptr;
        save_data_factory = nullptr;
        sdmc_factory = nullptr;
    }

    using MizuPath = Common::FS::MizuPath;
    const auto sdmc_dir_path = Common::FS::GetMizuPath(MizuPath::SDMCDir);
    const auto sdmc_load_dir_path = sdmc_dir_path / "atmosphere/contents";
    const auto rw_mode = FileSys::Mode::ReadWrite;

    auto nand_directory = SharedWriter(filesystem)->OpenDirectory(
            Common::FS::GetMizuPathString(MizuPath::NANDDir), rw_mode);
    auto sd_directory = SharedWriter(filesystem)->
        OpenDirectory(Common::FS::PathToUTF8String(sdmc_dir_path), rw_mode);
    auto load_directory = SharedWriter(filesystem)->
        OpenDirectory(Common::FS::GetMizuPathString(MizuPath::LoadDir), FileSys::Mode::Read);
    auto sd_load_directory = SharedWriter(filesystem)->
        OpenDirectory(Common::FS::PathToUTF8String(sdmc_load_dir_path), FileSys::Mode::Read);
    auto dump_directory = SharedWriter(filesystem)->
        OpenDirectory(Common::FS::GetMizuPathString(MizuPath::DumpDir), rw_mode);

    if (bis_factory == nullptr) {
        bis_factory = std::make_unique<FileSys::BISFactory>(
            nand_directory, std::move(load_directory), std::move(dump_directory));
        SharedWriter(content_provider)->SetSlot(FileSys::ContentProviderUnionSlot::SysNAND,
                                                           bis_factory->GetSystemNANDContents());
        SharedWriter(content_provider)->SetSlot(FileSys::ContentProviderUnionSlot::UserNAND,
                                                           bis_factory->GetUserNANDContents());
    }

    if (save_data_factory == nullptr) {
        save_data_factory =
            std::make_unique<FileSys::SaveDataFactory>(std::move(nand_directory));
    }

    if (sdmc_factory == nullptr) {
        sdmc_factory = std::make_unique<FileSys::SDMCFactory>(std::move(sd_directory),
                                                              std::move(sd_load_directory));
        SharedWriter(content_provider)->SetSlot(FileSys::ContentProviderUnionSlot::SDMC,
                                                           sdmc_factory->GetSDMCContents());
    }
}

void InstallInterfaces() {
    MakeService<FSP_LDR>();
    MakeService<FSP_PR>();
    MakeService<FSP_SRV>();
}

} // namespace Service::FileSystem
