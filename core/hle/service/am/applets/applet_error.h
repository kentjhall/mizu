// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include "core/hle/result.h"
#include "core/hle/service/am/applets/applets.h"

namespace Core {
class System;
}

namespace Service::AM::Applets {

enum class ErrorAppletMode : u8 {
    ShowError = 0,
    ShowSystemError = 1,
    ShowApplicationError = 2,
    ShowEula = 3,
    ShowErrorPctl = 4,
    ShowErrorRecord = 5,
    ShowUpdateEula = 8,
};

class Error final : public Applet {
public:
    explicit Error(LibraryAppletMode applet_mode_,
                   const Core::Frontend::ErrorApplet& frontend_);
    ~Error() override;

    void Initialize() override;

    bool TransactionComplete() const override;
    ResultCode GetStatus() const override;
    void ExecuteInteractive() override;
    void Execute() override;

    void DisplayCompleted();

private:
    union ErrorArguments;

    const Core::Frontend::ErrorApplet& frontend;
    ResultCode error_code = ResultSuccess;
    ErrorAppletMode mode = ErrorAppletMode::ShowError;
    std::unique_ptr<ErrorArguments> args;

    bool complete = false;
};

} // namespace Service::AM::Applets
