// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "common/uuid.h"
#include "core/hle/service/glue/glue_manager.h"
#include "core/hle/service/service.h"

namespace Service::Account {

class ProfileManager;

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(std::shared_ptr<Module> module_,
                           std::shared_ptr<Shared<ProfileManager>> profile_manager_,
                           const char* name);
        ~Interface() override;

        void GetUserCount(Kernel::HLERequestContext& ctx);
        void GetUserExistence(Kernel::HLERequestContext& ctx);
        void ListAllUsers(Kernel::HLERequestContext& ctx);
        void ListOpenUsers(Kernel::HLERequestContext& ctx);
        void GetLastOpenedUser(Kernel::HLERequestContext& ctx);
        void GetProfile(Kernel::HLERequestContext& ctx);
        void InitializeApplicationInfo(Kernel::HLERequestContext& ctx);
        void InitializeApplicationInfoRestricted(Kernel::HLERequestContext& ctx);
        void GetBaasAccountManagerForApplication(Kernel::HLERequestContext& ctx);
        void IsUserRegistrationRequestPermitted(Kernel::HLERequestContext& ctx);
        void TrySelectUserWithoutInteraction(Kernel::HLERequestContext& ctx);
        void IsUserAccountSwitchLocked(Kernel::HLERequestContext& ctx);
        void GetProfileEditor(Kernel::HLERequestContext& ctx);
        void ListQualifiedUsers(Kernel::HLERequestContext& ctx);
        void LoadOpenContext(Kernel::HLERequestContext& ctx);
        void ListOpenContextStoredUsers(Kernel::HLERequestContext& ctx);
        void StoreSaveDataThumbnailApplication(Kernel::HLERequestContext& ctx);
        void StoreSaveDataThumbnailSystem(Kernel::HLERequestContext& ctx);

    private:
        ResultCode InitializeApplicationInfoBase();
        void StoreSaveDataThumbnail(Kernel::HLERequestContext& ctx, const Common::UUID& uuid,
                                    const u64 tid);

        enum class ApplicationType : u32_le {
            GameCard = 0,
            Digital = 1,
            Unknown = 3,
        };

        struct ApplicationInfo {
            Service::Glue::ApplicationLaunchProperty launch_property;
            ApplicationType application_type;

            constexpr explicit operator bool() const {
                return launch_property.title_id != 0x0;
            }
        };

        ApplicationInfo application_info{};

    protected:
        std::shared_ptr<Module> module;
        std::shared_ptr<Shared<ProfileManager>> profile_manager;
    };
};

/// Registers all ACC services with the service manager.
void InstallInterfaces();

} // namespace Service::Account
