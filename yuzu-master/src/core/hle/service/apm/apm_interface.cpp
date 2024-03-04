// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/hle/service/apm/apm.h"
#include "core/hle/service/apm/apm_controller.h"
#include "core/hle/service/apm/apm_interface.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::APM {

class ISession final : public ServiceFramework<ISession> {
public:
    explicit ISession(Core::System& system_, Controller& controller_)
        : ServiceFramework{system_, "ISession"}, controller{controller_} {
        static const FunctionInfo functions[] = {
            {0, &ISession::SetPerformanceConfiguration, "SetPerformanceConfiguration"},
            {1, &ISession::GetPerformanceConfiguration, "GetPerformanceConfiguration"},
            {2, &ISession::SetCpuOverclockEnabled, "SetCpuOverclockEnabled"},
        };
        RegisterHandlers(functions);
    }

private:
    void SetPerformanceConfiguration(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        const auto mode = rp.PopEnum<PerformanceMode>();
        const auto config = rp.PopEnum<PerformanceConfiguration>();
        LOG_DEBUG(Service_APM, "called mode={} config={}", mode, config);

        controller.SetPerformanceConfiguration(mode, config);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void GetPerformanceConfiguration(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        const auto mode = rp.PopEnum<PerformanceMode>();
        LOG_DEBUG(Service_APM, "called mode={}", mode);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        rb.PushEnum(controller.GetCurrentPerformanceConfiguration(mode));
    }

    void SetCpuOverclockEnabled(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        const auto cpu_overclock_enabled = rp.Pop<bool>();

        LOG_WARNING(Service_APM, "(STUBBED) called, cpu_overclock_enabled={}",
                    cpu_overclock_enabled);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    Controller& controller;
};

APM::APM(Core::System& system_, std::shared_ptr<Module> apm_, Controller& controller_,
         const char* name)
    : ServiceFramework{system_, name}, apm(std::move(apm_)), controller{controller_} {
    static const FunctionInfo functions[] = {
        {0, &APM::OpenSession, "OpenSession"},
        {1, &APM::GetPerformanceMode, "GetPerformanceMode"},
        {6, &APM::IsCpuOverclockEnabled, "IsCpuOverclockEnabled"},
    };
    RegisterHandlers(functions);
}

APM::~APM() = default;

void APM::OpenSession(HLERequestContext& ctx) {
    LOG_DEBUG(Service_APM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ISession>(system, controller);
}

void APM::GetPerformanceMode(HLERequestContext& ctx) {
    LOG_DEBUG(Service_APM, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.PushEnum(controller.GetCurrentPerformanceMode());
}

void APM::IsCpuOverclockEnabled(HLERequestContext& ctx) {
    LOG_WARNING(Service_APM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(false);
}

APM_Sys::APM_Sys(Core::System& system_, Controller& controller_)
    : ServiceFramework{system_, "apm:sys"}, controller{controller_} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "RequestPerformanceMode"},
        {1, &APM_Sys::GetPerformanceEvent, "GetPerformanceEvent"},
        {2, nullptr, "GetThrottlingState"},
        {3, nullptr, "GetLastThrottlingState"},
        {4, nullptr, "ClearLastThrottlingState"},
        {5, nullptr, "LoadAndApplySettings"},
        {6, &APM_Sys::SetCpuBoostMode, "SetCpuBoostMode"},
        {7, &APM_Sys::GetCurrentPerformanceConfiguration, "GetCurrentPerformanceConfiguration"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

APM_Sys::~APM_Sys() = default;

void APM_Sys::GetPerformanceEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_APM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ISession>(system, controller);
}

void APM_Sys::SetCpuBoostMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto mode = rp.PopEnum<CpuBoostMode>();

    LOG_DEBUG(Service_APM, "called, mode={:08X}", mode);

    controller.SetFromCpuBoostMode(mode);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void APM_Sys::GetCurrentPerformanceConfiguration(HLERequestContext& ctx) {
    LOG_DEBUG(Service_APM, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(
        controller.GetCurrentPerformanceConfiguration(controller.GetCurrentPerformanceMode()));
}

} // namespace Service::APM
