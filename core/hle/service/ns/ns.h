// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service {

namespace FileSystem {
class FileSystemController;
} // namespace FileSystem

namespace NS {

class IAccountProxyInterface final : public ServiceFramework<IAccountProxyInterface> {
public:
    explicit IAccountProxyInterface();
    ~IAccountProxyInterface() override;
};

class IApplicationManagerInterface final : public ServiceFramework<IApplicationManagerInterface> {
public:
    explicit IApplicationManagerInterface();
    ~IApplicationManagerInterface() override;

    ResultVal<u8> GetApplicationDesiredLanguage(u32 supported_languages);
    ResultVal<u64> ConvertApplicationLanguageToLanguageCode(u8 application_language);

private:
    void GetApplicationControlData(Kernel::HLERequestContext& ctx);
    void GetApplicationDesiredLanguage(Kernel::HLERequestContext& ctx);
    void ConvertApplicationLanguageToLanguageCode(Kernel::HLERequestContext& ctx);
};

class IApplicationVersionInterface final : public ServiceFramework<IApplicationVersionInterface> {
public:
    explicit IApplicationVersionInterface();
    ~IApplicationVersionInterface() override;
};

class IContentManagementInterface final : public ServiceFramework<IContentManagementInterface> {
public:
    explicit IContentManagementInterface();
    ~IContentManagementInterface() override;
};

class IDocumentInterface final : public ServiceFramework<IDocumentInterface> {
public:
    explicit IDocumentInterface();
    ~IDocumentInterface() override;
};

class IDownloadTaskInterface final : public ServiceFramework<IDownloadTaskInterface> {
public:
    explicit IDownloadTaskInterface();
    ~IDownloadTaskInterface() override;
};

class IECommerceInterface final : public ServiceFramework<IECommerceInterface> {
public:
    explicit IECommerceInterface();
    ~IECommerceInterface() override;
};

class IFactoryResetInterface final : public ServiceFramework<IFactoryResetInterface> {
public:
    explicit IFactoryResetInterface();
    ~IFactoryResetInterface() override;
};

class NS final : public ServiceFramework<NS> {
public:
    explicit NS(const char* name);
    ~NS() override;

    std::shared_ptr<IApplicationManagerInterface> GetApplicationManagerInterface() const;

private:
    template <typename T, typename... Args>
    void PushInterface(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_NS, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<T>();
    }

    void PushIApplicationManagerInterface(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_NS, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IApplicationManagerInterface>();
    }

    template <typename T, typename... Args>
    std::shared_ptr<T> GetInterface(Args&&... args) const {
        static_assert(std::is_base_of_v<Kernel::SessionRequestHandler, T>,
                      "Not a base of ServiceFrameworkBase");

        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

/// Registers all NS services with the service manager.
void InstallInterfaces();

} // namespace NS
} // namespace Service
