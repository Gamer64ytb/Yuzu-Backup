// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/ptm/psm.h"

namespace Service::PTM {

class IPsmSession final : public ServiceFramework<IPsmSession> {
public:
    explicit IPsmSession(Core::System& system_)
        : ServiceFramework{system_, "IPsmSession"}, service_context{system_, "IPsmSession"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IPsmSession::BindStateChangeEvent, "BindStateChangeEvent"},
            {1, &IPsmSession::UnbindStateChangeEvent, "UnbindStateChangeEvent"},
            {2, &IPsmSession::SetChargerTypeChangeEventEnabled, "SetChargerTypeChangeEventEnabled"},
            {3, &IPsmSession::SetPowerSupplyChangeEventEnabled, "SetPowerSupplyChangeEventEnabled"},
            {4, &IPsmSession::SetBatteryVoltageStateChangeEventEnabled, "SetBatteryVoltageStateChangeEventEnabled"},
        };
        // clang-format on

        RegisterHandlers(functions);

        state_change_event = service_context.CreateEvent("IPsmSession::state_change_event");
    }

    ~IPsmSession() override {
        service_context.CloseEvent(state_change_event);
    }

    void SignalChargerTypeChanged() {
        if (should_signal && should_signal_charger_type) {
            state_change_event->Signal();
        }
    }

    void SignalPowerSupplyChanged() {
        if (should_signal && should_signal_power_supply) {
            state_change_event->Signal();
        }
    }

    void SignalBatteryVoltageStateChanged() {
        if (should_signal && should_signal_battery_voltage) {
            state_change_event->Signal();
        }
    }

private:
    void BindStateChangeEvent(HLERequestContext& ctx) {
        LOG_DEBUG(Service_PTM, "called");

        should_signal = true;

        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(ResultSuccess);
        rb.PushCopyObjects(state_change_event->GetReadableEvent());
    }

    void UnbindStateChangeEvent(HLERequestContext& ctx) {
        LOG_DEBUG(Service_PTM, "called");

        should_signal = false;

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetChargerTypeChangeEventEnabled(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto state = rp.Pop<bool>();
        LOG_DEBUG(Service_PTM, "called, state={}", state);

        should_signal_charger_type = state;

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetPowerSupplyChangeEventEnabled(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto state = rp.Pop<bool>();
        LOG_DEBUG(Service_PTM, "called, state={}", state);

        should_signal_power_supply = state;

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetBatteryVoltageStateChangeEventEnabled(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto state = rp.Pop<bool>();
        LOG_DEBUG(Service_PTM, "called, state={}", state);

        should_signal_battery_voltage = state;

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    KernelHelpers::ServiceContext service_context;

    bool should_signal_charger_type{};
    bool should_signal_power_supply{};
    bool should_signal_battery_voltage{};
    bool should_signal{};
    Kernel::KEvent* state_change_event;
};

PSM::PSM(Core::System& system_) : ServiceFramework{system_, "psm"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &PSM::GetBatteryChargePercentage, "GetBatteryChargePercentage"},
        {1, &PSM::GetChargerType, "GetChargerType"},
        {2, nullptr, "EnableBatteryCharging"},
        {3, nullptr, "DisableBatteryCharging"},
        {4, nullptr, "IsBatteryChargingEnabled"},
        {5, nullptr, "AcquireControllerPowerSupply"},
        {6, nullptr, "ReleaseControllerPowerSupply"},
        {7, &PSM::OpenSession, "OpenSession"},
        {8, nullptr, "EnableEnoughPowerChargeEmulation"},
        {9, nullptr, "DisableEnoughPowerChargeEmulation"},
        {10, nullptr, "EnableFastBatteryCharging"},
        {11, nullptr, "DisableFastBatteryCharging"},
        {12, nullptr, "GetBatteryVoltageState"},
        {13, nullptr, "GetRawBatteryChargePercentage"},
        {14, nullptr, "IsEnoughPowerSupplied"},
        {15, nullptr, "GetBatteryAgePercentage"},
        {16, nullptr, "GetBatteryChargeInfoEvent"},
        {17, nullptr, "GetBatteryChargeInfoFields"},
        {18, nullptr, "GetBatteryChargeCalibratedEvent"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

PSM::~PSM() = default;

void PSM::GetBatteryChargePercentage(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PTM, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(battery_charge_percentage);
}

void PSM::GetChargerType(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PTM, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(charger_type);
}

void PSM::OpenSession(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PTM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IPsmSession>(system);
}

} // namespace Service::PTM
