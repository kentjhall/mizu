// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include <algorithm>
#include <utility>
#include "core/file_sys/vfs_layered.h"

namespace FileSys {

LayeredVfsDirectory::LayeredVfsDirectory(std::vector<VirtualDir> dirs_, std::string name_)
    : dirs(std::move(dirs_)), name(std::move(name_)) {}

LayeredVfsDirectory::~LayeredVfsDirectory() = default;

VirtualDir LayeredVfsDirectory::MakeLayeredDirectory(std::vector<VirtualDir> dirs,
                                                     std::string name) {
    if (dirs.empty())
        return nullptr;
    if (dirs.size() == 1)
        return dirs[0];

    return VirtualDir(new LayeredVfsDirectory(std::move(dirs), std::move(name)));
}

VirtualFile LayeredVfsDirectory::GetFileRelative(std::string_view path) const {
    for (const auto& layer : dirs) {
        const auto file = layer->GetFileRelative(path);
        if (file != nullptr)
            return file;
    }

    return nullptr;
}

VirtualDir LayeredVfsDirectory::GetDirectoryRelative(std::string_view path) const {
    std::vector<VirtualDir> out;
    for (const auto& layer : dirs) {
        auto dir = layer->GetDirectoryRelative(path);
        if (dir != nullptr) {
            out.push_back(std::move(dir));
        }
    }

    return MakeLayeredDirectory(std::move(out));
}

VirtualFile LayeredVfsDirectory::GetFile(std::string_view file_name) const {
    return GetFileRelative(file_name);
}

VirtualDir LayeredVfsDirectory::GetSubdirectory(std::string_view subdir_name) const {
    return GetDirectoryRelative(subdir_name);
}

std::string LayeredVfsDirectory::GetFullPath() const {
    return dirs[0]->GetFullPath();
}

std::vector<VirtualFile> LayeredVfsDirectory::GetFiles() const {
    std::vector<VirtualFile> out;
    for (const auto& layer : dirs) {
        for (const auto& file : layer->GetFiles()) {
            if (std::find_if(out.begin(), out.end(), [&file](const VirtualFile& comp) {
                    return comp->GetName() == file->GetName();
                }) == out.end()) {
                out.push_back(file);
            }
        }
    }

    return out;
}

std::vector<VirtualDir> LayeredVfsDirectory::GetSubdirectories() const {
    std::vector<std::string> names;
    for (const auto& layer : dirs) {
        for (const auto& sd : layer->GetSubdirectories()) {
            if (std::find(names.begin(), names.end(), sd->GetName()) == names.end())
                names.push_back(sd->GetName());
        }
    }

    std::vector<VirtualDir> out;
    out.reserve(names.size());
    for (const auto& subdir : names)
        out.push_back(GetSubdirectory(subdir));

    return out;
}

bool LayeredVfsDirectory::IsWritable() const {
    return false;
}

bool LayeredVfsDirectory::IsReadable() const {
    return true;
}

std::string LayeredVfsDirectory::GetName() const {
    return name.empty() ? dirs[0]->GetName() : name;
}

VirtualDir LayeredVfsDirectory::GetParentDirectory() const {
    return dirs[0]->GetParentDirectory();
}

VirtualDir LayeredVfsDirectory::CreateSubdirectory(std::string_view subdir_name) {
    return nullptr;
}

VirtualFile LayeredVfsDirectory::CreateFile(std::string_view file_name) {
    return nullptr;
}

bool LayeredVfsDirectory::DeleteSubdirectory(std::string_view subdir_name) {
    return false;
}

bool LayeredVfsDirectory::DeleteFile(std::string_view file_name) {
    return false;
}

bool LayeredVfsDirectory::Rename(std::string_view new_name) {
    name = new_name;
    return true;
}

} // namespace FileSys
