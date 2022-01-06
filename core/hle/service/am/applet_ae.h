// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "core/hle/service/service.h"

namespace Service {
namespace FileSystem {
class FileSystemController;
}

namespace NVFlinger {
class NVFlinger;
}

namespace AM {

class AppletMessageQueue;

class AppletAE final : public ServiceFramework<AppletAE> {
public:
    explicit AppletAE(std::shared_ptr<Shared<AppletMessageQueue>> msg_queue_);
    ~AppletAE() override;

    const std::shared_ptr<Shared<AppletMessageQueue>>& GetMessageQueue() const;

private:
    void OpenSystemAppletProxy(Kernel::HLERequestContext& ctx);
    void OpenLibraryAppletProxy(Kernel::HLERequestContext& ctx);
    void OpenLibraryAppletProxyOld(Kernel::HLERequestContext& ctx);

    std::shared_ptr<Shared<AppletMessageQueue>> msg_queue;
};

} // namespace AM
} // namespace Service
