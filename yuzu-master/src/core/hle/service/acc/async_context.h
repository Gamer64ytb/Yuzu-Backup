// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Account {

class IAsyncContext : public ServiceFramework<IAsyncContext> {
public:
    explicit IAsyncContext(Core::System& system_);
    ~IAsyncContext() override;

    void GetSystemEvent(HLERequestContext& ctx);
    void Cancel(HLERequestContext& ctx);
    void HasDone(HLERequestContext& ctx);
    void GetResult(HLERequestContext& ctx);

protected:
    virtual bool IsComplete() const = 0;
    virtual void Cancel() = 0;
    virtual Result GetResult() const = 0;

    void MarkComplete();

    KernelHelpers::ServiceContext service_context;

    std::atomic<bool> is_complete{false};
    Kernel::KEvent* completion_event;
};

} // namespace Service::Account
