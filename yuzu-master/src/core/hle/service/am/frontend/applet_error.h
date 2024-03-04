// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/result.h"
#include "core/hle/service/am/frontend/applets.h"

namespace Core {
class System;
}

namespace Service::AM::Frontend {

enum class ErrorAppletMode : u8 {
    ShowError = 0,
    ShowSystemError = 1,
    ShowApplicationError = 2,
    ShowEula = 3,
    ShowErrorPctl = 4,
    ShowErrorRecord = 5,
    ShowUpdateEula = 8,
};

class Error final : public FrontendApplet {
public:
    explicit Error(Core::System& system_, std::shared_ptr<Applet> applet_,
                   LibraryAppletMode applet_mode_, const Core::Frontend::ErrorApplet& frontend_);
    ~Error() override;

    void Initialize() override;

    Result GetStatus() const override;
    void ExecuteInteractive() override;
    void Execute() override;
    Result RequestExit() override;

    void DisplayCompleted();

private:
    union ErrorArguments;

    const Core::Frontend::ErrorApplet& frontend;
    Result error_code = ResultSuccess;
    ErrorAppletMode mode = ErrorAppletMode::ShowError;
    std::unique_ptr<ErrorArguments> args;

    bool complete = false;
};

} // namespace Service::AM::Frontend
