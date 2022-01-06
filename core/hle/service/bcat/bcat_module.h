// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service {

namespace FileSystem {
class FileSystemController;
} // namespace FileSystem

namespace BCAT {

class Backend;

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(std::shared_ptr<Module> module_, const char* name);
        ~Interface() override;

        void CreateBcatService(Kernel::HLERequestContext& ctx);
        void CreateDeliveryCacheStorageService(Kernel::HLERequestContext& ctx);
        void CreateDeliveryCacheStorageServiceWithApplicationId(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> module;
        std::unique_ptr<Backend> backend;
    };
};

/// Registers all BCAT services with the specified service manager.
void InstallInterfaces();

} // namespace BCAT

} // namespace Service
