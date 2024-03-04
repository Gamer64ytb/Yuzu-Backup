// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstring>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend/applets/controller.h"
#include "core/hle/result.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/frontend/applet_controller.h"
#include "core/hle/service/am/service/storage.h"
#include "hid_core/frontend/emulated_controller.h"
#include "hid_core/hid_core.h"
#include "hid_core/hid_types.h"
#include "hid_core/resources/npad/npad.h"

namespace Service::AM::Frontend {

[[maybe_unused]] constexpr Result ResultControllerSupportCanceled{ErrorModule::HID, 3101};
[[maybe_unused]] constexpr Result ResultControllerSupportNotSupportedNpadStyle{ErrorModule::HID,
                                                                               3102};

static Core::Frontend::ControllerParameters ConvertToFrontendParameters(
    ControllerSupportArgPrivate private_arg, ControllerSupportArgHeader header, bool enable_text,
    std::vector<IdentificationColor> identification_colors, std::vector<ExplainText> text) {
    Core::HID::NpadStyleTag npad_style_set;
    npad_style_set.raw = private_arg.style_set;

    return {
        .min_players = std::max(s8{1}, header.player_count_min),
        .max_players = header.player_count_max,
        .keep_controllers_connected = header.enable_take_over_connection,
        .enable_single_mode = header.enable_single_mode,
        .enable_border_color = header.enable_identification_color,
        .border_colors = std::move(identification_colors),
        .enable_explain_text = enable_text,
        .explain_text = std::move(text),
        .allow_pro_controller = npad_style_set.fullkey == 1,
        .allow_handheld = npad_style_set.handheld == 1,
        .allow_dual_joycons = npad_style_set.joycon_dual == 1,
        .allow_left_joycon = npad_style_set.joycon_left == 1,
        .allow_right_joycon = npad_style_set.joycon_right == 1,
    };
}

Controller::Controller(Core::System& system_, std::shared_ptr<Applet> applet_,
                       LibraryAppletMode applet_mode_,
                       const Core::Frontend::ControllerApplet& frontend_)
    : FrontendApplet{system_, applet_, applet_mode_}, frontend{frontend_} {}

Controller::~Controller() = default;

void Controller::Initialize() {
    FrontendApplet::Initialize();

    LOG_INFO(Service_HID, "Initializing Controller Applet.");

    LOG_DEBUG(Service_HID,
              "Initializing Applet with common_args: arg_version={}, lib_version={}, "
              "play_startup_sound={}, size={}, system_tick={}, theme_color={}",
              common_args.arguments_version, common_args.library_version,
              common_args.play_startup_sound, common_args.size, common_args.system_tick,
              common_args.theme_color);

    controller_applet_version = ControllerAppletVersion{common_args.library_version};

    const std::shared_ptr<IStorage> private_arg_storage = PopInData();
    ASSERT(private_arg_storage != nullptr);

    const auto& private_arg = private_arg_storage->GetData();
    ASSERT(private_arg.size() == sizeof(ControllerSupportArgPrivate));

    std::memcpy(&controller_private_arg, private_arg.data(), private_arg.size());
    ASSERT_MSG(controller_private_arg.arg_private_size == sizeof(ControllerSupportArgPrivate),
               "Unknown ControllerSupportArgPrivate revision={} with size={}",
               controller_applet_version, controller_private_arg.arg_private_size);

    // Some games such as Cave Story+ set invalid values for the ControllerSupportMode.
    // Defer to arg_size to set the ControllerSupportMode.
    if (controller_private_arg.mode >= ControllerSupportMode::MaxControllerSupportMode) {
        switch (controller_private_arg.arg_size) {
        case sizeof(ControllerSupportArgOld):
        case sizeof(ControllerSupportArgNew):
            controller_private_arg.mode = ControllerSupportMode::ShowControllerSupport;
            break;
        case sizeof(ControllerUpdateFirmwareArg):
            controller_private_arg.mode = ControllerSupportMode::ShowControllerFirmwareUpdate;
            break;
        case sizeof(ControllerKeyRemappingArg):
            controller_private_arg.mode =
                ControllerSupportMode::ShowControllerKeyRemappingForSystem;
            break;
        default:
            UNIMPLEMENTED_MSG("Unknown ControllerPrivateArg mode={} with arg_size={}",
                              controller_private_arg.mode, controller_private_arg.arg_size);
            controller_private_arg.mode = ControllerSupportMode::ShowControllerSupport;
            break;
        }
    }

    // Some games such as Cave Story+ set invalid values for the ControllerSupportCaller.
    // This is always 0 (Application) except with ShowControllerFirmwareUpdateForSystem.
    if (controller_private_arg.caller >= ControllerSupportCaller::MaxControllerSupportCaller) {
        if (controller_private_arg.flag_1 &&
            (controller_private_arg.mode == ControllerSupportMode::ShowControllerFirmwareUpdate ||
             controller_private_arg.mode ==
                 ControllerSupportMode::ShowControllerKeyRemappingForSystem)) {
            controller_private_arg.caller = ControllerSupportCaller::System;
        } else {
            controller_private_arg.caller = ControllerSupportCaller::Application;
        }
    }

    switch (controller_private_arg.mode) {
    case ControllerSupportMode::ShowControllerSupport:
    case ControllerSupportMode::ShowControllerStrapGuide: {
        const std::shared_ptr<IStorage> user_arg_storage = PopInData();
        ASSERT(user_arg_storage != nullptr);

        const auto& user_arg = user_arg_storage->GetData();
        switch (controller_applet_version) {
        case ControllerAppletVersion::Version3:
        case ControllerAppletVersion::Version4:
        case ControllerAppletVersion::Version5:
            ASSERT(user_arg.size() == sizeof(ControllerSupportArgOld));
            std::memcpy(&controller_user_arg_old, user_arg.data(), user_arg.size());
            break;
        case ControllerAppletVersion::Version7:
        case ControllerAppletVersion::Version8:
            ASSERT(user_arg.size() == sizeof(ControllerSupportArgNew));
            std::memcpy(&controller_user_arg_new, user_arg.data(), user_arg.size());
            break;
        default:
            UNIMPLEMENTED_MSG("Unknown ControllerSupportArg revision={} with size={}",
                              controller_applet_version, controller_private_arg.arg_size);
            ASSERT(user_arg.size() >= sizeof(ControllerSupportArgNew));
            std::memcpy(&controller_user_arg_new, user_arg.data(), sizeof(ControllerSupportArgNew));
            break;
        }
        break;
    }
    case ControllerSupportMode::ShowControllerFirmwareUpdate: {
        const std::shared_ptr<IStorage> update_arg_storage = PopInData();
        ASSERT(update_arg_storage != nullptr);

        const auto& update_arg = update_arg_storage->GetData();
        ASSERT(update_arg.size() == sizeof(ControllerUpdateFirmwareArg));

        std::memcpy(&controller_update_arg, update_arg.data(), update_arg.size());
        break;
    }
    case ControllerSupportMode::ShowControllerKeyRemappingForSystem: {
        const std::shared_ptr<IStorage> remapping_arg_storage = PopInData();
        ASSERT(remapping_arg_storage != nullptr);

        const auto& remapping_arg = remapping_arg_storage->GetData();
        ASSERT(remapping_arg.size() == sizeof(ControllerKeyRemappingArg));

        std::memcpy(&controller_key_remapping_arg, remapping_arg.data(), remapping_arg.size());
        break;
    }
    default: {
        UNIMPLEMENTED_MSG("Unimplemented ControllerSupportMode={}", controller_private_arg.mode);
        break;
    }
    }
}

Result Controller::GetStatus() const {
    return status;
}

void Controller::ExecuteInteractive() {
    ASSERT_MSG(false, "Attempted to call interactive execution on non-interactive applet.");
}

void Controller::Execute() {
    switch (controller_private_arg.mode) {
    case ControllerSupportMode::ShowControllerSupport: {
        const auto parameters = [this] {
            switch (controller_applet_version) {
            case ControllerAppletVersion::Version3:
            case ControllerAppletVersion::Version4:
            case ControllerAppletVersion::Version5:
                return ConvertToFrontendParameters(
                    controller_private_arg, controller_user_arg_old.header,
                    controller_user_arg_old.enable_explain_text,
                    std::vector<IdentificationColor>(
                        controller_user_arg_old.identification_colors.begin(),
                        controller_user_arg_old.identification_colors.end()),
                    std::vector<ExplainText>(controller_user_arg_old.explain_text.begin(),
                                             controller_user_arg_old.explain_text.end()));
            case ControllerAppletVersion::Version7:
            case ControllerAppletVersion::Version8:
            default:
                return ConvertToFrontendParameters(
                    controller_private_arg, controller_user_arg_new.header,
                    controller_user_arg_new.enable_explain_text,
                    std::vector<IdentificationColor>(
                        controller_user_arg_new.identification_colors.begin(),
                        controller_user_arg_new.identification_colors.end()),
                    std::vector<ExplainText>(controller_user_arg_new.explain_text.begin(),
                                             controller_user_arg_new.explain_text.end()));
            }
        }();

        is_single_mode = parameters.enable_single_mode;

        LOG_DEBUG(Service_HID,
                  "Controller Parameters: min_players={}, max_players={}, "
                  "keep_controllers_connected={}, enable_single_mode={}, enable_border_color={}, "
                  "enable_explain_text={}, allow_pro_controller={}, allow_handheld={}, "
                  "allow_dual_joycons={}, allow_left_joycon={}, allow_right_joycon={}",
                  parameters.min_players, parameters.max_players,
                  parameters.keep_controllers_connected, parameters.enable_single_mode,
                  parameters.enable_border_color, parameters.enable_explain_text,
                  parameters.allow_pro_controller, parameters.allow_handheld,
                  parameters.allow_dual_joycons, parameters.allow_left_joycon,
                  parameters.allow_right_joycon);

        frontend.ReconfigureControllers(
            [this](bool is_success) { ConfigurationComplete(is_success); }, parameters);
        break;
    }
    case ControllerSupportMode::ShowControllerStrapGuide:
    case ControllerSupportMode::ShowControllerFirmwareUpdate:
    case ControllerSupportMode::ShowControllerKeyRemappingForSystem:
        UNIMPLEMENTED_MSG("ControllerSupportMode={} is not implemented",
                          controller_private_arg.mode);
        ConfigurationComplete(true);
        break;
    default: {
        ConfigurationComplete(true);
        break;
    }
    }
}

void Controller::ConfigurationComplete(bool is_success) {
    ControllerSupportResultInfo result_info{};

    // If enable_single_mode is enabled, player_count is 1 regardless of any other parameters.
    // Otherwise, only count connected players from P1-P8.
    result_info.player_count = is_single_mode ? 1 : system.HIDCore().GetPlayerCount();

    result_info.selected_id = static_cast<u32>(system.HIDCore().GetFirstNpadId());

    result_info.result =
        is_success ? ControllerSupportResult::Success : ControllerSupportResult::Cancel;

    LOG_DEBUG(Service_HID, "Result Info: player_count={}, selected_id={}, result={}",
              result_info.player_count, result_info.selected_id, result_info.result);

    complete = true;
    out_data = std::vector<u8>(sizeof(ControllerSupportResultInfo));
    std::memcpy(out_data.data(), &result_info, out_data.size());

    PushOutData(std::make_shared<IStorage>(system, std::move(out_data)));
    Exit();
}

Result Controller::RequestExit() {
    frontend.Close();
    R_SUCCEED();
}

} // namespace Service::AM::Frontend
