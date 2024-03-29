// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include <fmt/format.h>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/fs/path_util.h"
#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/fs_types.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/zstd_compression.h"
#include "common/settings.h"
#include "core/core.h"
#include "video_core/engines/shader_type.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_shader_disk_cache.h"

#define DIR_SEP "/"
#define DIR_SEP_CHR '/'

namespace OpenGL {

using Tegra::Engines::ShaderType;
using VideoCommon::Shader::BindlessSamplerMap;
using VideoCommon::Shader::BoundSamplerMap;
using VideoCommon::Shader::KeyMap;

namespace {

using ShaderCacheVersionHash = std::array<u8, 64>;

struct ConstBufferKey {
    u32 cbuf = 0;
    u32 offset = 0;
    u32 value = 0;
};

struct BoundSamplerKey {
    u32 offset = 0;
    Tegra::Engines::SamplerDescriptor sampler;
};

struct BindlessSamplerKey {
    u32 cbuf = 0;
    u32 offset = 0;
    Tegra::Engines::SamplerDescriptor sampler;
};

constexpr u32 NativeVersion = 20;

ShaderCacheVersionHash GetShaderCacheVersionHash() {
    ShaderCacheVersionHash hash{};
    const std::size_t length = std::min(std::strlen("0"), hash.size());
    std::memcpy(hash.data(), "0", length);
    return hash;
}

} // Anonymous namespace

ShaderDiskCacheEntry::ShaderDiskCacheEntry() = default;

ShaderDiskCacheEntry::~ShaderDiskCacheEntry() = default;

bool ShaderDiskCacheEntry::Load(Common::FS::IOFile& file) {
    if (file.ReadBytes(&type, sizeof(u32)) != sizeof(u32)) {
        return false;
    }
    u32 code_size;
    u32 code_size_b;
    if (file.ReadBytes(&code_size, sizeof(u32)) != sizeof(u32) ||
        file.ReadBytes(&code_size_b, sizeof(u32)) != sizeof(u32)) {
        return false;
    }
    code.resize(code_size);
    code_b.resize(code_size_b);

    if (file.ReadArray(code.data(), code_size) != code_size) {
        return false;
    }
    if (HasProgramA() && file.ReadArray(code_b.data(), code_size_b) != code_size_b) {
        return false;
    }

    u8 is_texture_handler_size_known;
    u32 texture_handler_size_value;
    u32 num_keys;
    u32 num_bound_samplers;
    u32 num_bindless_samplers;
    if (file.ReadArray(&unique_identifier, 1) != 1 || file.ReadArray(&bound_buffer, 1) != 1 ||
        file.ReadArray(&is_texture_handler_size_known, 1) != 1 ||
        file.ReadArray(&texture_handler_size_value, 1) != 1 ||
        file.ReadArray(&graphics_info, 1) != 1 || file.ReadArray(&compute_info, 1) != 1 ||
        file.ReadArray(&num_keys, 1) != 1 || file.ReadArray(&num_bound_samplers, 1) != 1 ||
        file.ReadArray(&num_bindless_samplers, 1) != 1) {
        return false;
    }
    if (is_texture_handler_size_known) {
        texture_handler_size = texture_handler_size_value;
    }

    std::vector<ConstBufferKey> flat_keys(num_keys);
    std::vector<BoundSamplerKey> flat_bound_samplers(num_bound_samplers);
    std::vector<BindlessSamplerKey> flat_bindless_samplers(num_bindless_samplers);
    if (file.ReadArray(flat_keys.data(), flat_keys.size()) != flat_keys.size() ||
        file.ReadArray(flat_bound_samplers.data(), flat_bound_samplers.size()) !=
            flat_bound_samplers.size() ||
        file.ReadArray(flat_bindless_samplers.data(), flat_bindless_samplers.size()) !=
            flat_bindless_samplers.size()) {
        return false;
    }
    for (const auto& key : flat_keys) {
        keys.insert({{key.cbuf, key.offset}, key.value});
    }
    for (const auto& key : flat_bound_samplers) {
        bound_samplers.emplace(key.offset, key.sampler);
    }
    for (const auto& key : flat_bindless_samplers) {
        bindless_samplers.insert({{key.cbuf, key.offset}, key.sampler});
    }

    return true;
}

bool ShaderDiskCacheEntry::Save(Common::FS::IOFile& file) const {
    if (file.WriteObject(static_cast<u32>(type)) != 1 ||
        file.WriteObject(static_cast<u32>(code.size())) != 1 ||
        file.WriteObject(static_cast<u32>(code_b.size())) != 1) {
        return false;
    }
    if (file.WriteArray(code.data(), code.size()) != code.size()) {
        return false;
    }
    if (HasProgramA() && file.WriteArray(code_b.data(), code_b.size()) != code_b.size()) {
        return false;
    }

    if (file.WriteObject(unique_identifier) != 1 || file.WriteObject(bound_buffer) != 1 ||
        file.WriteObject(static_cast<u8>(texture_handler_size.has_value())) != 1 ||
        file.WriteObject(texture_handler_size.value_or(0)) != 1 ||
        file.WriteObject(graphics_info) != 1 || file.WriteObject(compute_info) != 1 ||
        file.WriteObject(static_cast<u32>(keys.size())) != 1 ||
        file.WriteObject(static_cast<u32>(bound_samplers.size())) != 1 ||
        file.WriteObject(static_cast<u32>(bindless_samplers.size())) != 1) {
        return false;
    }

    std::vector<ConstBufferKey> flat_keys;
    flat_keys.reserve(keys.size());
    for (const auto& [address, value] : keys) {
        flat_keys.push_back(ConstBufferKey{address.first, address.second, value});
    }

    std::vector<BoundSamplerKey> flat_bound_samplers;
    flat_bound_samplers.reserve(bound_samplers.size());
    for (const auto& [address, sampler] : bound_samplers) {
        flat_bound_samplers.push_back(BoundSamplerKey{address, sampler});
    }

    std::vector<BindlessSamplerKey> flat_bindless_samplers;
    flat_bindless_samplers.reserve(bindless_samplers.size());
    for (const auto& [address, sampler] : bindless_samplers) {
        flat_bindless_samplers.push_back(
            BindlessSamplerKey{address.first, address.second, sampler});
    }

    return file.WriteArray(flat_keys.data(), flat_keys.size()) == flat_keys.size() &&
           file.WriteArray(flat_bound_samplers.data(), flat_bound_samplers.size()) ==
               flat_bound_samplers.size() &&
           file.WriteArray(flat_bindless_samplers.data(), flat_bindless_samplers.size()) ==
               flat_bindless_samplers.size();
}

ShaderDiskCacheOpenGL::ShaderDiskCacheOpenGL(u64 title_id)
    : title_id{title_id} {}

ShaderDiskCacheOpenGL::~ShaderDiskCacheOpenGL() = default;

std::optional<std::vector<ShaderDiskCacheEntry>> ShaderDiskCacheOpenGL::LoadTransferable() {
    // Skip games without title id
    const bool has_title_id = title_id != 0;
    if (!Settings::values.use_disk_shader_cache || !has_title_id) {
        return {};
    }

    Common::FS::IOFile file(GetTransferablePath(), Common::FS::FileAccessMode::Read);
    if (!file.IsOpen()) {
        LOG_INFO(Render_OpenGL, "No transferable shader cache found");
        is_usable = true;
        return {};
    }

    u32 version{};
    if (file.ReadBytes(&version, sizeof(version)) != sizeof(version)) {
        LOG_ERROR(Render_OpenGL, "Failed to get transferable cache version, skipping it");
        return {};
    }

    if (version < NativeVersion) {
        LOG_INFO(Render_OpenGL, "Transferable shader cache is old, removing");
        file.Close();
        InvalidateTransferable();
        is_usable = true;
        return {};
    }
    if (version > NativeVersion) {
        LOG_WARNING(Render_OpenGL, "Transferable shader cache was generated with a newer version "
                                   "of the emulator, skipping");
        return {};
    }

    // Version is valid, load the shaders
    std::vector<ShaderDiskCacheEntry> entries;
    while (file.Tell() < file.GetSize()) {
        ShaderDiskCacheEntry& entry = entries.emplace_back();
        if (!entry.Load(file)) {
            LOG_ERROR(Render_OpenGL, "Failed to load transferable raw entry, skipping");
            return {};
        }
    }

    is_usable = true;
    return {std::move(entries)};
}

std::vector<ShaderDiskCachePrecompiled> ShaderDiskCacheOpenGL::LoadPrecompiled() {
    if (!is_usable) {
        return {};
    }

    Common::FS::IOFile file(GetPrecompiledPath(), Common::FS::FileAccessMode::Read);
    if (!file.IsOpen()) {
        LOG_INFO(Render_OpenGL, "No precompiled shader cache found");
        return {};
    }

    if (const auto result = LoadPrecompiledFile(file)) {
        return *result;
    }

    LOG_INFO(Render_OpenGL, "Failed to load precompiled cache");
    file.Close();
    InvalidatePrecompiled();
    return {};
}

std::optional<std::vector<ShaderDiskCachePrecompiled>> ShaderDiskCacheOpenGL::LoadPrecompiledFile(
    Common::FS::IOFile& file) {
    // Read compressed file from disk and decompress to virtual precompiled cache file
    std::vector<u8> compressed(file.GetSize());
    file.ReadBytes(compressed.data(), compressed.size());
    const std::vector<u8> decompressed = Common::Compression::DecompressDataZSTD(compressed);
    SaveArrayToPrecompiled(decompressed.data(), decompressed.size());
    precompiled_cache_virtual_file_offset = 0;

    ShaderCacheVersionHash file_hash{};
    if (!LoadArrayFromPrecompiled(file_hash.data(), file_hash.size())) {
        precompiled_cache_virtual_file_offset = 0;
        return {};
    }
    if (GetShaderCacheVersionHash() != file_hash) {
        LOG_INFO(Render_OpenGL, "Precompiled cache is from another version of the emulator");
        precompiled_cache_virtual_file_offset = 0;
        return {};
    }

    std::vector<ShaderDiskCachePrecompiled> entries;
    while (precompiled_cache_virtual_file_offset < precompiled_cache_virtual_file.GetSize()) {
        u32 binary_size;
        auto& entry = entries.emplace_back();
        if (!LoadObjectFromPrecompiled(entry.unique_identifier) ||
            !LoadObjectFromPrecompiled(entry.binary_format) ||
            !LoadObjectFromPrecompiled(binary_size)) {
            return {};
        }

        entry.binary.resize(binary_size);
        if (!LoadArrayFromPrecompiled(entry.binary.data(), entry.binary.size())) {
            return {};
        }
    }
    return entries;
}

void ShaderDiskCacheOpenGL::InvalidateTransferable() {
    if (!Common::FS::RemoveFile(GetTransferablePath())) {
        LOG_ERROR(Render_OpenGL, "Failed to invalidate transferable file={}",
                  GetTransferablePath());
    }
    InvalidatePrecompiled();
}

void ShaderDiskCacheOpenGL::InvalidatePrecompiled() {
    // Clear virtaul precompiled cache file
    precompiled_cache_virtual_file.Resize(0);

    if (!Common::FS::RemoveFile(GetPrecompiledPath())) {
        LOG_ERROR(Render_OpenGL, "Failed to invalidate precompiled file={}", GetPrecompiledPath());
    }
}

void ShaderDiskCacheOpenGL::SaveEntry(const ShaderDiskCacheEntry& entry) {
    if (!is_usable) {
        return;
    }

    const u64 id = entry.unique_identifier;
    if (stored_transferable.find(id) != stored_transferable.end()) {
        // The shader already exists
        return;
    }

    Common::FS::IOFile file = AppendTransferableFile();
    if (!file.IsOpen()) {
        return;
    }
    if (!entry.Save(file)) {
        LOG_ERROR(Render_OpenGL, "Failed to save raw transferable cache entry, removing");
        file.Close();
        InvalidateTransferable();
        return;
    }

    stored_transferable.insert(id);
}

void ShaderDiskCacheOpenGL::SavePrecompiled(u64 unique_identifier, GLuint program) {
    if (!is_usable) {
        return;
    }

    // TODO(Rodrigo): This is a design smell. I shouldn't be having to manually write the header
    // when writing the dump. This should be done the moment I get access to write to the virtual
    // file.
    if (precompiled_cache_virtual_file.GetSize() == 0) {
        SavePrecompiledHeaderToVirtualPrecompiledCache();
    }

    GLint binary_length;
    glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binary_length);

    GLenum binary_format;
    std::vector<u8> binary(binary_length);
    glGetProgramBinary(program, binary_length, nullptr, &binary_format, binary.data());

    if (!SaveObjectToPrecompiled(unique_identifier) || !SaveObjectToPrecompiled(binary_format) ||
        !SaveObjectToPrecompiled(static_cast<u32>(binary.size())) ||
        !SaveArrayToPrecompiled(binary.data(), binary.size())) {
        LOG_ERROR(Render_OpenGL, "Failed to save binary program file in shader={:016X}, removing",
                  unique_identifier);
        InvalidatePrecompiled();
    }
}

Common::FS::IOFile ShaderDiskCacheOpenGL::AppendTransferableFile() const {
    if (!EnsureDirectories()) {
        return {};
    }

    const auto transferable_path{GetTransferablePath()};
    const bool existed = Common::FS::Exists(transferable_path);

    Common::FS::IOFile file(transferable_path, Common::FS::FileAccessMode::Append);
    if (!file.IsOpen()) {
        LOG_ERROR(Render_OpenGL, "Failed to open transferable cache in path={}", transferable_path);
        return {};
    }
    if (!existed || file.GetSize() == 0) {
        // If the file didn't exist, write its version
        if (file.WriteObject(NativeVersion) != 1) {
            LOG_ERROR(Render_OpenGL, "Failed to write transferable cache version in path={}",
                      transferable_path);
            return {};
        }
    }
    return file;
}

void ShaderDiskCacheOpenGL::SavePrecompiledHeaderToVirtualPrecompiledCache() {
    const auto hash{GetShaderCacheVersionHash()};
    if (!SaveArrayToPrecompiled(hash.data(), hash.size())) {
        LOG_ERROR(
            Render_OpenGL,
            "Failed to write precompiled cache version hash to virtual precompiled cache file");
    }
}

void ShaderDiskCacheOpenGL::SaveVirtualPrecompiledFile() {
    precompiled_cache_virtual_file_offset = 0;
    const std::vector<u8> uncompressed = precompiled_cache_virtual_file.ReadAllBytes();
    const std::vector<u8> compressed =
        Common::Compression::CompressDataZSTDDefault(uncompressed.data(), uncompressed.size());

    const auto precompiled_path{GetPrecompiledPath()};
    Common::FS::IOFile file(precompiled_path, Common::FS::FileAccessMode::Write);

    if (!file.IsOpen()) {
        LOG_ERROR(Render_OpenGL, "Failed to open precompiled cache in path={}", precompiled_path);
        return;
    }
    if (file.WriteBytes(compressed.data(), compressed.size()) != compressed.size()) {
        LOG_ERROR(Render_OpenGL, "Failed to write precompiled cache version in path={}",
                  precompiled_path);
    }
}

bool ShaderDiskCacheOpenGL::EnsureDirectories() const {
    const auto CreateDir = [](const std::string& dir) {
        if (!Common::FS::CreateDir(dir)) {
            LOG_ERROR(Render_OpenGL, "Failed to create directory={}", dir);
            return false;
        }
        return true;
    };

    return CreateDir(Common::FS::GetMizuPath(Common::FS::MizuPath::ShaderDir)) &&
           CreateDir(GetBaseDir()) && CreateDir(GetTransferableDir()) &&
           CreateDir(GetPrecompiledDir());
}

std::string ShaderDiskCacheOpenGL::GetTransferablePath() const {
    return Common::FS::SanitizePath(GetTransferableDir() + DIR_SEP_CHR + GetTitleID() + ".bin");
}

std::string ShaderDiskCacheOpenGL::GetPrecompiledPath() const {
    return Common::FS::SanitizePath(GetPrecompiledDir() + DIR_SEP_CHR + GetTitleID() + ".bin");
}

std::string ShaderDiskCacheOpenGL::GetTransferableDir() const {
    return GetBaseDir() + DIR_SEP "transferable";
}

std::string ShaderDiskCacheOpenGL::GetPrecompiledDir() const {
    return GetBaseDir() + DIR_SEP "precompiled";
}

std::string ShaderDiskCacheOpenGL::GetBaseDir() const {
    return Common::FS::GetMizuPath(Common::FS::MizuPath::ShaderDir) / "opengl";
}

std::string ShaderDiskCacheOpenGL::GetTitleID() const {
    return fmt::format("{:016X}", title_id);
}

} // namespace OpenGL
