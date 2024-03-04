// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "common/scratch_buffer.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/hle/service/service.h"

namespace Service::Nvidia {

class NVDRV final : public ServiceFramework<NVDRV> {
public:
    explicit NVDRV(Core::System& system_, std::shared_ptr<Module> nvdrv_, const char* name);
    ~NVDRV() override;

    std::shared_ptr<Module> GetModule() const {
        return nvdrv;
    }

private:
    void Open(HLERequestContext& ctx);
    void Ioctl1(HLERequestContext& ctx);
    void Ioctl2(HLERequestContext& ctx);
    void Ioctl3(HLERequestContext& ctx);
    void Close(HLERequestContext& ctx);
    void Initialize(HLERequestContext& ctx);
    void QueryEvent(HLERequestContext& ctx);
    void SetAruid(HLERequestContext& ctx);
    void SetGraphicsFirmwareMemoryMarginEnabled(HLERequestContext& ctx);
    void GetStatus(HLERequestContext& ctx);
    void DumpGraphicsMemoryInfo(HLERequestContext& ctx);

    void ServiceError(HLERequestContext& ctx, NvResult result);

    std::shared_ptr<Module> nvdrv;

    u64 pid{};
    bool is_initialized{};
    NvCore::SessionId session_id{};
    Common::ScratchBuffer<u8> output_buffer;
    Common::ScratchBuffer<u8> inline_output_buffer;
};

} // namespace Service::Nvidia
