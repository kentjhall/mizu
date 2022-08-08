// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <chrono>
#include <mutex>
#include <stop_token>
#include <condition_variable>
#include <sys/mman.h>

#include "core/hle/service/hid/controllers/controller_base.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Service::SM {
class ServiceManager;
}

namespace Service::HID {

constexpr std::size_t SHARED_MEMORY_SIZE{0x40000};

enum class HidController : std::size_t {
    DebugPad,
    Touchscreen,
    Mouse,
    Keyboard,
    XPad,
    HomeButton,
    SleepButton,
    CaptureButton,
    InputDetector,
    UniquePad,
    NPad,
    Gesture,
    ConsoleSixAxisSensor,

    MaxControllers,
};

class IAppletResource final : public ServiceFramework<IAppletResource> {
public:
    explicit IAppletResource();
    ~IAppletResource() override;

    void ActivateController(HidController controller);
    void DeactivateController(HidController controller);

    template <typename T>
    T& GetController(HidController controller) {
        return static_cast<T&>(*controllers[static_cast<size_t>(controller)]);
    }

    template <typename T>
    const T& GetController(HidController controller) const {
        return static_cast<T&>(*controllers[static_cast<size_t>(controller)]);
    }

private:
    template <typename T>
    void MakeController(HidController controller) {
        controllers[static_cast<std::size_t>(controller)] = std::make_unique<T>();
    }

    void GetSharedMemoryHandle(Kernel::HLERequestContext& ctx);
    void UpdateControllers();
    void UpdateMotion();

    ::timer_t pad_update_event;
    ::timer_t motion_update_event;

    std::stop_source stop_source;
    std::condition_variable done_cv;
    std::mutex done_mtx;
    bool pad_is_done = false, motion_is_done = false;

    std::array<std::unique_ptr<ControllerBase>, static_cast<size_t>(HidController::MaxControllers)>
        controllers{};

    static constexpr auto shared_mem_deleter = std::bind(::munmap, std::placeholders::_1, SHARED_MEMORY_SIZE);
    int shared_mem_fd = -1;
    std::unique_ptr<u8, decltype(shared_mem_deleter)> shared_mem;
};

class Hid final : public ServiceFramework<Hid> {
public:
    explicit Hid();
    ~Hid() override;

    std::shared_ptr<IAppletResource> GetAppletResource();

private:
    void CreateAppletResource(Kernel::HLERequestContext& ctx);
    void ActivateDebugPad(Kernel::HLERequestContext& ctx);
    void ActivateTouchScreen(Kernel::HLERequestContext& ctx);
    void ActivateMouse(Kernel::HLERequestContext& ctx);
    void ActivateKeyboard(Kernel::HLERequestContext& ctx);
    void SendKeyboardLockKeyEvent(Kernel::HLERequestContext& ctx);
    void ActivateXpad(Kernel::HLERequestContext& ctx);
    void GetXpadIDs(Kernel::HLERequestContext& ctx);
    void ActivateSixAxisSensor(Kernel::HLERequestContext& ctx);
    void DeactivateSixAxisSensor(Kernel::HLERequestContext& ctx);
    void StartSixAxisSensor(Kernel::HLERequestContext& ctx);
    void StopSixAxisSensor(Kernel::HLERequestContext& ctx);
    void EnableSixAxisSensorFusion(Kernel::HLERequestContext& ctx);
    void SetSixAxisSensorFusionParameters(Kernel::HLERequestContext& ctx);
    void GetSixAxisSensorFusionParameters(Kernel::HLERequestContext& ctx);
    void ResetSixAxisSensorFusionParameters(Kernel::HLERequestContext& ctx);
    void SetGyroscopeZeroDriftMode(Kernel::HLERequestContext& ctx);
    void GetGyroscopeZeroDriftMode(Kernel::HLERequestContext& ctx);
    void ResetGyroscopeZeroDriftMode(Kernel::HLERequestContext& ctx);
    void IsSixAxisSensorAtRest(Kernel::HLERequestContext& ctx);
    void IsFirmwareUpdateAvailableForSixAxisSensor(Kernel::HLERequestContext& ctx);
    void ActivateGesture(Kernel::HLERequestContext& ctx);
    void SetSupportedNpadStyleSet(Kernel::HLERequestContext& ctx);
    void GetSupportedNpadStyleSet(Kernel::HLERequestContext& ctx);
    void SetSupportedNpadIdType(Kernel::HLERequestContext& ctx);
    void ActivateNpad(Kernel::HLERequestContext& ctx);
    void DeactivateNpad(Kernel::HLERequestContext& ctx);
    void AcquireNpadStyleSetUpdateEventHandle(Kernel::HLERequestContext& ctx);
    void DisconnectNpad(Kernel::HLERequestContext& ctx);
    void GetPlayerLedPattern(Kernel::HLERequestContext& ctx);
    void ActivateNpadWithRevision(Kernel::HLERequestContext& ctx);
    void SetNpadJoyHoldType(Kernel::HLERequestContext& ctx);
    void GetNpadJoyHoldType(Kernel::HLERequestContext& ctx);
    void SetNpadJoyAssignmentModeSingleByDefault(Kernel::HLERequestContext& ctx);
    void SetNpadJoyAssignmentModeSingle(Kernel::HLERequestContext& ctx);
    void SetNpadJoyAssignmentModeDual(Kernel::HLERequestContext& ctx);
    void MergeSingleJoyAsDualJoy(Kernel::HLERequestContext& ctx);
    void StartLrAssignmentMode(Kernel::HLERequestContext& ctx);
    void StopLrAssignmentMode(Kernel::HLERequestContext& ctx);
    void SetNpadHandheldActivationMode(Kernel::HLERequestContext& ctx);
    void GetNpadHandheldActivationMode(Kernel::HLERequestContext& ctx);
    void SwapNpadAssignment(Kernel::HLERequestContext& ctx);
    void IsUnintendedHomeButtonInputProtectionEnabled(Kernel::HLERequestContext& ctx);
    void EnableUnintendedHomeButtonInputProtection(Kernel::HLERequestContext& ctx);
    void SetNpadAnalogStickUseCenterClamp(Kernel::HLERequestContext& ctx);
    void GetVibrationDeviceInfo(Kernel::HLERequestContext& ctx);
    void SendVibrationValue(Kernel::HLERequestContext& ctx);
    void GetActualVibrationValue(Kernel::HLERequestContext& ctx);
    void CreateActiveVibrationDeviceList(Kernel::HLERequestContext& ctx);
    void PermitVibration(Kernel::HLERequestContext& ctx);
    void IsVibrationPermitted(Kernel::HLERequestContext& ctx);
    void SendVibrationValues(Kernel::HLERequestContext& ctx);
    void SendVibrationGcErmCommand(Kernel::HLERequestContext& ctx);
    void GetActualVibrationGcErmCommand(Kernel::HLERequestContext& ctx);
    void BeginPermitVibrationSession(Kernel::HLERequestContext& ctx);
    void EndPermitVibrationSession(Kernel::HLERequestContext& ctx);
    void IsVibrationDeviceMounted(Kernel::HLERequestContext& ctx);
    void ActivateConsoleSixAxisSensor(Kernel::HLERequestContext& ctx);
    void StartConsoleSixAxisSensor(Kernel::HLERequestContext& ctx);
    void StopConsoleSixAxisSensor(Kernel::HLERequestContext& ctx);
    void ActivateSevenSixAxisSensor(Kernel::HLERequestContext& ctx);
    void StartSevenSixAxisSensor(Kernel::HLERequestContext& ctx);
    void StopSevenSixAxisSensor(Kernel::HLERequestContext& ctx);
    void InitializeSevenSixAxisSensor(Kernel::HLERequestContext& ctx);
    void FinalizeSevenSixAxisSensor(Kernel::HLERequestContext& ctx);
    void ResetSevenSixAxisSensorTimestamp(Kernel::HLERequestContext& ctx);
    void SetIsPalmaAllConnectable(Kernel::HLERequestContext& ctx);
    void SetPalmaBoostMode(Kernel::HLERequestContext& ctx);
    void SetNpadCommunicationMode(Kernel::HLERequestContext& ctx);
    void GetNpadCommunicationMode(Kernel::HLERequestContext& ctx);
    void SetTouchScreenConfiguration(Kernel::HLERequestContext& ctx);

    enum class VibrationDeviceType : u32 {
        Unknown = 0,
        LinearResonantActuator = 1,
        GcErm = 2,
    };

    enum class VibrationDevicePosition : u32 {
        None = 0,
        Left = 1,
        Right = 2,
    };

    enum class VibrationGcErmCommand : u64 {
        Stop = 0,
        Start = 1,
        StopHard = 2,
    };

    struct VibrationDeviceInfo {
        VibrationDeviceType type{};
        VibrationDevicePosition position{};
    };
    static_assert(sizeof(VibrationDeviceInfo) == 0x8, "VibrationDeviceInfo has incorrect size.");

    Shared<std::shared_ptr<IAppletResource>> applet_resource;
};

/// Reload input devices. Used when input configuration changed
void ReloadInputDevices();

/// Registers all HID services with specified service manager.
void InstallInterfaces();

} // namespace Service::HID
