// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <vector>
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/hle/api_version.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/spl/csrng.h"
#include "core/hle/service/spl/spl.h"
#include "core/hle/service/spl/spl_module.h"

namespace Service::SPL {

Module::Interface::Interface(Core::System& system_, std::shared_ptr<Module> module_,
                             const char* name)
    : ServiceFramework{system_, name}, module{std::move(module_)},
      rng(Settings::values.rng_seed_enabled ? Settings::values.rng_seed.GetValue()
                                            : static_cast<u32>(std::time(nullptr))) {}

Module::Interface::~Interface() = default;

void Module::Interface::GetConfig(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto config_item = rp.PopEnum<ConfigItem>();

    // This should call svcCallSecureMonitor with the appropriate args.
    // Since we do not have it implemented yet, we will use this for now.
    u64 smc_result{};
    const auto result_code = GetConfigImpl(&smc_result, config_item);

    if (result_code != ResultSuccess) {
        LOG_ERROR(Service_SPL, "called, config_item={}, result_code={}", config_item,
                  result_code.raw);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result_code);
    }

    LOG_DEBUG(Service_SPL, "called, config_item={}, result_code={}, smc_result={}", config_item,
              result_code.raw, smc_result);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result_code);
    rb.Push(smc_result);
}

void Module::Interface::ModularExponentiate(HLERequestContext& ctx) {
    UNIMPLEMENTED_MSG("ModularExponentiate is not implemented!");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSecureMonitorNotImplemented);
}

void Module::Interface::SetConfig(HLERequestContext& ctx) {
    UNIMPLEMENTED_MSG("SetConfig is not implemented!");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSecureMonitorNotImplemented);
}

void Module::Interface::GenerateRandomBytes(HLERequestContext& ctx) {
    LOG_DEBUG(Service_SPL, "called");

    const std::size_t size = ctx.GetWriteBufferSize();

    std::uniform_int_distribution<u16> distribution(0, std::numeric_limits<u8>::max());
    std::vector<u8> data(size);
    std::generate(data.begin(), data.end(), [&] { return static_cast<u8>(distribution(rng)); });

    ctx.WriteBuffer(data);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void Module::Interface::IsDevelopment(HLERequestContext& ctx) {
    UNIMPLEMENTED_MSG("IsDevelopment is not implemented!");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSecureMonitorNotImplemented);
}

void Module::Interface::SetBootReason(HLERequestContext& ctx) {
    UNIMPLEMENTED_MSG("SetBootReason is not implemented!");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSecureMonitorNotImplemented);
}

void Module::Interface::GetBootReason(HLERequestContext& ctx) {
    UNIMPLEMENTED_MSG("GetBootReason is not implemented!");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSecureMonitorNotImplemented);
}

Result Module::Interface::GetConfigImpl(u64* out_config, ConfigItem config_item) const {
    switch (config_item) {
    case ConfigItem::DisableProgramVerification:
    case ConfigItem::DramId:
    case ConfigItem::SecurityEngineInterruptNumber:
    case ConfigItem::FuseVersion:
    case ConfigItem::HardwareType:
    case ConfigItem::HardwareState:
    case ConfigItem::IsRecoveryBoot:
    case ConfigItem::DeviceId:
    case ConfigItem::BootReason:
    case ConfigItem::MemoryMode:
    case ConfigItem::IsDevelopmentFunctionEnabled:
    case ConfigItem::KernelConfiguration:
    case ConfigItem::IsChargerHiZModeEnabled:
    case ConfigItem::QuestState:
    case ConfigItem::RegulatorType:
    case ConfigItem::DeviceUniqueKeyGeneration:
    case ConfigItem::Package2Hash:
        return ResultSecureMonitorNotImplemented;
    case ConfigItem::ExosphereApiVersion:
        // Get information about the current exosphere version.
        *out_config = (u64{HLE::ApiVersion::ATMOSPHERE_RELEASE_VERSION_MAJOR} << 56) |
                      (u64{HLE::ApiVersion::ATMOSPHERE_RELEASE_VERSION_MINOR} << 48) |
                      (u64{HLE::ApiVersion::ATMOSPHERE_RELEASE_VERSION_MICRO} << 40) |
                      (static_cast<u64>(HLE::ApiVersion::GetTargetFirmware()));
        return ResultSuccess;
    case ConfigItem::ExosphereNeedsReboot:
        // We are executing, so we aren't in the process of rebooting.
        *out_config = u64{0};
        return ResultSuccess;
    case ConfigItem::ExosphereNeedsShutdown:
        // We are executing, so we aren't in the process of shutting down.
        *out_config = u64{0};
        return ResultSuccess;
    case ConfigItem::ExosphereGitCommitHash:
        // Get information about the current exosphere git commit hash.
        *out_config = u64{0};
        return ResultSuccess;
    case ConfigItem::ExosphereHasRcmBugPatch:
        // Get information about whether this unit has the RCM bug patched.
        *out_config = u64{0};
        return ResultSuccess;
    case ConfigItem::ExosphereBlankProdInfo:
        // Get whether this unit should simulate a "blanked" PRODINFO.
        *out_config = u64{0};
        return ResultSuccess;
    case ConfigItem::ExosphereAllowCalWrites:
        // Get whether this unit should allow writing to the calibration partition.
        *out_config = u64{0};
        return ResultSuccess;
    case ConfigItem::ExosphereEmummcType:
        // Get what kind of emummc this unit has active.
        *out_config = u64{0};
        return ResultSuccess;
    case ConfigItem::ExospherePayloadAddress:
        // Gets the physical address of the reboot payload buffer, if one exists.
        return ResultSecureMonitorNotInitialized;
    case ConfigItem::ExosphereLogConfiguration:
        // Get the log configuration.
        *out_config = u64{0};
        return ResultSuccess;
    case ConfigItem::ExosphereForceEnableUsb30:
        // Get whether usb 3.0 should be force-enabled.
        *out_config = u64{0};
        return ResultSuccess;
    default:
        return ResultSecureMonitorInvalidArgument;
    }
}

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);
    auto module = std::make_shared<Module>();

    server_manager->RegisterNamedService("csrng", std::make_shared<CSRNG>(system, module));
    server_manager->RegisterNamedService("spl", std::make_shared<SPL>(system, module));
    server_manager->RegisterNamedService("spl:mig", std::make_shared<SPL_MIG>(system, module));
    server_manager->RegisterNamedService("spl:fs", std::make_shared<SPL_FS>(system, module));
    server_manager->RegisterNamedService("spl:ssl", std::make_shared<SPL_SSL>(system, module));
    server_manager->RegisterNamedService("spl:es", std::make_shared<SPL_ES>(system, module));
    server_manager->RegisterNamedService("spl:manu", std::make_shared<SPL_MANU>(system, module));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::SPL
