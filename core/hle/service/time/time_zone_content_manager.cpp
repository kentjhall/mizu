// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include <sstream>

#include "common/logging/log.h"
#include "common/settings.h"
#include "common/time_zone.h"
#include "core/core.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/system_archive/system_archive.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/time/time_manager.h"
#include "core/hle/service/time/time_zone_content_manager.h"

namespace Service::Time::TimeZone {

constexpr u64 time_zone_binary_titleid{0x010000000000080E};

static FileSys::VirtualDir GetTimeZoneBinary() {
    const auto* nand{SharedReader(filesystem_controller)->GetSystemNANDContents()};
    const auto nca{nand->GetEntry(time_zone_binary_titleid, FileSys::ContentRecordType::Data)};

    FileSys::VirtualFile romfs;
    if (nca) {
        romfs = nca->GetRomFS();
    }

    if (!romfs) {
        romfs = FileSys::SystemArchive::SynthesizeSystemArchive(time_zone_binary_titleid);
    }

    if (!romfs) {
        LOG_ERROR(Service_Time, "Failed to find or synthesize {:016X!}", time_zone_binary_titleid);
        return {};
    }

    return FileSys::ExtractRomFS(romfs);
}

static std::vector<std::string> BuildLocationNameCache() {
    const FileSys::VirtualDir extracted_romfs{GetTimeZoneBinary()};
    if (!extracted_romfs) {
        LOG_ERROR(Service_Time, "Failed to extract RomFS for {:016X}!", time_zone_binary_titleid);
        return {};
    }

    const FileSys::VirtualFile binary_list{extracted_romfs->GetFile("binaryList.txt")};
    if (!binary_list) {
        LOG_ERROR(Service_Time, "{:016X} has no file binaryList.txt!", time_zone_binary_titleid);
        return {};
    }

    std::vector<char> raw_data(binary_list->GetSize() + 1);
    binary_list->ReadBytes<char>(raw_data.data(), binary_list->GetSize());

    std::stringstream data_stream{raw_data.data()};
    std::string name;
    std::vector<std::string> location_name_cache;
    while (std::getline(data_stream, name)) {
        name.pop_back(); // Remove carriage return
        location_name_cache.emplace_back(std::move(name));
    }
    return location_name_cache;
}

TimeZoneContentManager::TimeZoneContentManager(TimeManager& time_manager)
    : location_name_cache{BuildLocationNameCache()} {
    std::string location_name;
    const auto timezone_setting = Settings::GetTimeZoneString();
    if (timezone_setting == "auto" || timezone_setting == "default") {
        location_name = Common::TimeZone::GetDefaultTimeZone();
    } else {
        location_name = timezone_setting;
    }

    if (FileSys::VirtualFile vfs_file;
        GetTimeZoneInfoFile(location_name, vfs_file) == ResultSuccess) {
        const auto time_point{
            time_manager.GetStandardSteadyClockCore().WriteLocked()->GetCurrentTimePoint()};
        if (time_zone_manager.SetDeviceLocationNameWithTimeZoneRule(
                location_name, vfs_file) != ResultSuccess) {
            UNREACHABLE();
            return;
        }

        time_zone_manager.SetUpdatedTime(time_point);
        time_zone_manager.SetTotalLocationNameCount(
            location_name_cache.size());
        time_zone_manager.SetTimeZoneRuleVersion({});
        time_zone_manager.MarkAsInitialized();
    } else {
        time_zone_manager.MarkAsInitialized();
    }
}

ResultCode TimeZoneContentManager::LoadTimeZoneRule(TimeZoneRule& rules,
                                                    const std::string& location_name) const {
    FileSys::VirtualFile vfs_file;
    if (const ResultCode result{GetTimeZoneInfoFile(location_name, vfs_file)};
        result != ResultSuccess) {
        return result;
    }

    return time_zone_manager.ParseTimeZoneRuleBinary(rules, vfs_file);
}

bool TimeZoneContentManager::IsLocationNameValid(const std::string& location_name) const {
    return std::find(location_name_cache.begin(), location_name_cache.end(), location_name) !=
           location_name_cache.end();
}

ResultCode TimeZoneContentManager::GetTimeZoneInfoFile(const std::string& location_name,
                                                       FileSys::VirtualFile& vfs_file) const {
    if (!IsLocationNameValid(location_name)) {
        return ERROR_TIME_NOT_FOUND;
    }

    const FileSys::VirtualDir extracted_romfs{GetTimeZoneBinary()};
    if (!extracted_romfs) {
        LOG_ERROR(Service_Time, "Failed to extract RomFS for {:016X}!", time_zone_binary_titleid);
        return ERROR_TIME_NOT_FOUND;
    }

    const FileSys::VirtualDir zoneinfo_dir{extracted_romfs->GetSubdirectory("zoneinfo")};
    if (!zoneinfo_dir) {
        LOG_ERROR(Service_Time, "{:016X} has no directory zoneinfo!", time_zone_binary_titleid);
        return ERROR_TIME_NOT_FOUND;
    }

    vfs_file = zoneinfo_dir->GetFileRelative(location_name);
    if (!vfs_file) {
        LOG_ERROR(Service_Time, "{:016X} has no file \"{}\"! Using default timezone.",
                  time_zone_binary_titleid, location_name);
        vfs_file = zoneinfo_dir->GetFile(Common::TimeZone::GetDefaultTimeZone());
    }

    if (!vfs_file) {
        LOG_ERROR(Service_Time, "{:016X} has no file \"{}\"!", time_zone_binary_titleid,
                  location_name);
        return ERROR_TIME_NOT_FOUND;
    }

    return ResultSuccess;
}

} // namespace Service::Time::TimeZone
