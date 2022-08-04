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

class AppletOE final : public ServiceFramework<AppletOE> {
public:
    explicit AppletOE(std::shared_ptr<Shared<AppletMessageQueueMap>> msg_queue_map_);
    ~AppletOE() override;

    void SetupSession(::pid_t req_pid) override;
    void CleanupSession(::pid_t req_pid) override;

    const std::shared_ptr<Shared<AppletMessageQueue>>& GetMessageQueue(::pid_t req_pid) const;

private:
    void OpenApplicationProxy(Kernel::HLERequestContext& ctx);

    std::shared_ptr<Shared<AppletMessageQueueMap>> msg_queue_map;
};

} // namespace AM
} // namespace Service
