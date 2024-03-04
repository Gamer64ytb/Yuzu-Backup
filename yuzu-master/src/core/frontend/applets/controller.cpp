// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/settings_enums.h"
#include "core/frontend/applets/controller.h"
#include "hid_core/frontend/emulated_controller.h"
#include "hid_core/hid_core.h"
#include "hid_core/hid_types.h"

namespace Core::Frontend {

ControllerApplet::~ControllerApplet() = default;

DefaultControllerApplet::DefaultControllerApplet(HID::HIDCore& hid_core_) : hid_core{hid_core_} {}

DefaultControllerApplet::~DefaultControllerApplet() = default;

void DefaultControllerApplet::Close() const {}

void DefaultControllerApplet::ReconfigureControllers(ReconfigureCallback callback,
                                                     const ControllerParameters& parameters) const {
    LOG_INFO(Service_HID, "called, deducing the best configuration based on the given parameters!");

    const std::size_t min_supported_players =
        parameters.enable_single_mode ? 1 : parameters.min_players;

    // Disconnect Handheld first.
    auto* handheld = hid_core.GetEmulatedController(Core::HID::NpadIdType::Handheld);
    handheld->Disconnect();

    // Deduce the best configuration based on the input parameters.
    for (std::size_t index = 0; index < hid_core.available_controllers - 2; ++index) {
        auto* controller = hid_core.GetEmulatedControllerByIndex(index);

        // First, disconnect all controllers regardless of the value of keep_controllers_connected.
        // This makes it easy to connect the desired controllers.
        controller->Disconnect();

        // Only connect the minimum number of required players.
        if (index >= min_supported_players) {
            continue;
        }

        // Connect controllers based on the following priority list from highest to lowest priority:
        // Pro Controller -> Dual Joycons -> Left Joycon/Right Joycon -> Handheld
        if (parameters.allow_pro_controller) {
            controller->SetNpadStyleIndex(Core::HID::NpadStyleIndex::Fullkey);
            controller->Connect(true);
        } else if (parameters.allow_dual_joycons) {
            controller->SetNpadStyleIndex(Core::HID::NpadStyleIndex::JoyconDual);
            controller->Connect(true);
        } else if (parameters.allow_left_joycon && parameters.allow_right_joycon) {
            // Assign left joycons to even player indices and right joycons to odd player indices.
            // We do this since Captain Toad Treasure Tracker expects a left joycon for Player 1 and
            // a right Joycon for Player 2 in 2 Player Assist mode.
            if (index % 2 == 0) {
                controller->SetNpadStyleIndex(Core::HID::NpadStyleIndex::JoyconLeft);
                controller->Connect(true);
            } else {
                controller->SetNpadStyleIndex(Core::HID::NpadStyleIndex::JoyconRight);
                controller->Connect(true);
            }
        } else if (index == 0 && parameters.enable_single_mode && parameters.allow_handheld &&
                   !Settings::IsDockedMode()) {
            // We should *never* reach here under any normal circumstances.
            controller->SetNpadStyleIndex(Core::HID::NpadStyleIndex::Handheld);
            controller->Connect(true);
        } else {
            ASSERT_MSG(false, "Unable to add a new controller based on the given parameters!");
        }
    }

    callback(true);
}

} // namespace Core::Frontend
