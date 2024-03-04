// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Set {

class IFirmwareDebugSettingsServer final : public ServiceFramework<IFirmwareDebugSettingsServer> {
public:
    explicit IFirmwareDebugSettingsServer(Core::System& system_);
    ~IFirmwareDebugSettingsServer() override;
};

} // namespace Service::Set
