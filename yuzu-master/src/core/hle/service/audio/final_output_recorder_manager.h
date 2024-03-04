// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Audio {

class IFinalOutputRecorderManager final : public ServiceFramework<IFinalOutputRecorderManager> {
public:
    explicit IFinalOutputRecorderManager(Core::System& system_);
    ~IFinalOutputRecorderManager() override;
};

} // namespace Service::Audio
