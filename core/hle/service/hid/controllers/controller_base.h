// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <shared_mutex>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/service.h"

namespace Service::HID {
class ControllerBase {
public:
    explicit ControllerBase();
    virtual ~ControllerBase();

    // Called when the controller is initialized
    virtual void OnInit() = 0;

    // When the controller is released
    virtual void OnRelease() = 0;

    // When the controller is requesting an update for the shared memory
    virtual void OnUpdate(u8* data,
                          std::size_t size) = 0;

    // When the controller is requesting a motion update for the shared memory
    virtual void OnMotionUpdate(u8* data,
                                std::size_t size) {}

    // Called when input devices should be loaded
    virtual void OnLoadInputDevices() = 0;

    void ActivateController();

    void DeactivateController();

    bool IsControllerActivated() const;

protected:
    bool is_activated{false};

    struct CommonHeader {
        s64_le timestamp;
        s64_le total_entry_count;
        s64_le last_entry_index;
        s64_le entry_count;
    };
    static_assert(sizeof(CommonHeader) == 0x20, "CommonHeader is an invalid size");

    ;
};

template <class Derived>
class ControllerLockedBase : public ControllerBase {
public:
    explicit ControllerLockedBase() : ControllerBase() {}

    auto ReadLocked() { return Service::SharedReader(mtx, *static_cast<const Derived *>(this)); };
    auto WriteLocked() { return Service::SharedWriter(mtx, *static_cast<Derived *>(this)); };

private:
    std::shared_mutex mtx;
};
} // namespace Service::HID
