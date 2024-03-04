// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "input_common/input_engine.h"

namespace InputCommon {

void InputEngine::PreSetController(const PadIdentifier& identifier) {
    std::scoped_lock lock{mutex};
    controller_list.try_emplace(identifier);
}

void InputEngine::PreSetButton(const PadIdentifier& identifier, int button) {
    std::scoped_lock lock{mutex};
    ControllerData& controller = controller_list.at(identifier);
    controller.buttons.try_emplace(button, false);
}

void InputEngine::PreSetHatButton(const PadIdentifier& identifier, int button) {
    std::scoped_lock lock{mutex};
    ControllerData& controller = controller_list.at(identifier);
    controller.hat_buttons.try_emplace(button, u8{0});
}

void InputEngine::PreSetAxis(const PadIdentifier& identifier, int axis) {
    std::scoped_lock lock{mutex};
    ControllerData& controller = controller_list.at(identifier);
    controller.axes.try_emplace(axis, 0.0f);
}

void InputEngine::PreSetMotion(const PadIdentifier& identifier, int motion) {
    std::scoped_lock lock{mutex};
    ControllerData& controller = controller_list.at(identifier);
    controller.motions.try_emplace(motion);
}

void InputEngine::SetButton(const PadIdentifier& identifier, int button, bool value) {
    {
        std::scoped_lock lock{mutex};
        ControllerData& controller = controller_list.at(identifier);
        if (!configuring) {
            controller.buttons.insert_or_assign(button, value);
        }
    }
    TriggerOnButtonChange(identifier, button, value);
}

void InputEngine::SetHatButton(const PadIdentifier& identifier, int button, u8 value) {
    {
        std::scoped_lock lock{mutex};
        ControllerData& controller = controller_list.at(identifier);
        if (!configuring) {
            controller.hat_buttons.insert_or_assign(button, value);
        }
    }
    TriggerOnHatButtonChange(identifier, button, value);
}

void InputEngine::SetAxis(const PadIdentifier& identifier, int axis, f32 value) {
    {
        std::scoped_lock lock{mutex};
        ControllerData& controller = controller_list.at(identifier);
        if (!configuring) {
            controller.axes.insert_or_assign(axis, value);
        }
    }
    TriggerOnAxisChange(identifier, axis, value);
}

void InputEngine::SetBattery(const PadIdentifier& identifier, Common::Input::BatteryLevel value) {
    {
        std::scoped_lock lock{mutex};
        ControllerData& controller = controller_list.at(identifier);
        if (!configuring) {
            controller.battery = value;
        }
    }
    TriggerOnBatteryChange(identifier, value);
}

void InputEngine::SetColor(const PadIdentifier& identifier, Common::Input::BodyColorStatus value) {
    {
        std::scoped_lock lock{mutex};
        ControllerData& controller = controller_list.at(identifier);
        if (!configuring) {
            controller.color = value;
        }
    }
    TriggerOnColorChange(identifier, value);
}

void InputEngine::SetMotion(const PadIdentifier& identifier, int motion, const BasicMotion& value) {
    {
        std::scoped_lock lock{mutex};
        ControllerData& controller = controller_list.at(identifier);
        if (!configuring) {
            controller.motions.insert_or_assign(motion, value);
        }
    }
    TriggerOnMotionChange(identifier, motion, value);
}

void InputEngine::SetCamera(const PadIdentifier& identifier,
                            const Common::Input::CameraStatus& value) {
    {
        std::scoped_lock lock{mutex};
        ControllerData& controller = controller_list.at(identifier);
        if (!configuring) {
            controller.camera = value;
        }
    }
    TriggerOnCameraChange(identifier, value);
}

void InputEngine::SetNfc(const PadIdentifier& identifier, const Common::Input::NfcStatus& value) {
    {
        std::scoped_lock lock{mutex};
        ControllerData& controller = controller_list.at(identifier);
        if (!configuring) {
            controller.nfc = value;
        }
    }
    TriggerOnNfcChange(identifier, value);
}

bool InputEngine::GetButton(const PadIdentifier& identifier, int button) const {
    std::scoped_lock lock{mutex};
    const auto controller_iter = controller_list.find(identifier);
    if (controller_iter == controller_list.cend()) {
        LOG_ERROR(Input, "Invalid identifier guid={}, pad={}, port={}", identifier.guid.RawString(),
                  identifier.pad, identifier.port);
        return false;
    }
    const ControllerData& controller = controller_iter->second;
    const auto button_iter = controller.buttons.find(button);
    if (button_iter == controller.buttons.cend()) {
        LOG_ERROR(Input, "Invalid button {}", button);
        return false;
    }
    return button_iter->second;
}

bool InputEngine::GetHatButton(const PadIdentifier& identifier, int button, u8 direction) const {
    std::scoped_lock lock{mutex};
    const auto controller_iter = controller_list.find(identifier);
    if (controller_iter == controller_list.cend()) {
        LOG_ERROR(Input, "Invalid identifier guid={}, pad={}, port={}", identifier.guid.RawString(),
                  identifier.pad, identifier.port);
        return false;
    }
    const ControllerData& controller = controller_iter->second;
    const auto hat_iter = controller.hat_buttons.find(button);
    if (hat_iter == controller.hat_buttons.cend()) {
        LOG_ERROR(Input, "Invalid hat button {}", button);
        return false;
    }
    return (hat_iter->second & direction) != 0;
}

f32 InputEngine::GetAxis(const PadIdentifier& identifier, int axis) const {
    std::scoped_lock lock{mutex};
    const auto controller_iter = controller_list.find(identifier);
    if (controller_iter == controller_list.cend()) {
        LOG_ERROR(Input, "Invalid identifier guid={}, pad={}, port={}", identifier.guid.RawString(),
                  identifier.pad, identifier.port);
        return 0.0f;
    }
    const ControllerData& controller = controller_iter->second;
    const auto axis_iter = controller.axes.find(axis);
    if (axis_iter == controller.axes.cend()) {
        LOG_ERROR(Input, "Invalid axis {}", axis);
        return 0.0f;
    }
    return axis_iter->second;
}

Common::Input::BatteryLevel InputEngine::GetBattery(const PadIdentifier& identifier) const {
    std::scoped_lock lock{mutex};
    const auto controller_iter = controller_list.find(identifier);
    if (controller_iter == controller_list.cend()) {
        LOG_ERROR(Input, "Invalid identifier guid={}, pad={}, port={}", identifier.guid.RawString(),
                  identifier.pad, identifier.port);
        return Common::Input::BatteryLevel::Charging;
    }
    const ControllerData& controller = controller_iter->second;
    return controller.battery;
}

Common::Input::BodyColorStatus InputEngine::GetColor(const PadIdentifier& identifier) const {
    std::scoped_lock lock{mutex};
    const auto controller_iter = controller_list.find(identifier);
    if (controller_iter == controller_list.cend()) {
        LOG_ERROR(Input, "Invalid identifier guid={}, pad={}, port={}", identifier.guid.RawString(),
                  identifier.pad, identifier.port);
        return {};
    }
    const ControllerData& controller = controller_iter->second;
    return controller.color;
}

BasicMotion InputEngine::GetMotion(const PadIdentifier& identifier, int motion) const {
    std::scoped_lock lock{mutex};
    const auto controller_iter = controller_list.find(identifier);
    if (controller_iter == controller_list.cend()) {
        LOG_ERROR(Input, "Invalid identifier guid={}, pad={}, port={}", identifier.guid.RawString(),
                  identifier.pad, identifier.port);
        return {};
    }
    const ControllerData& controller = controller_iter->second;
    return controller.motions.at(motion);
}

Common::Input::CameraStatus InputEngine::GetCamera(const PadIdentifier& identifier) const {
    std::scoped_lock lock{mutex};
    const auto controller_iter = controller_list.find(identifier);
    if (controller_iter == controller_list.cend()) {
        LOG_ERROR(Input, "Invalid identifier guid={}, pad={}, port={}", identifier.guid.RawString(),
                  identifier.pad, identifier.port);
        return {};
    }
    const ControllerData& controller = controller_iter->second;
    return controller.camera;
}

Common::Input::NfcStatus InputEngine::GetNfc(const PadIdentifier& identifier) const {
    std::scoped_lock lock{mutex};
    const auto controller_iter = controller_list.find(identifier);
    if (controller_iter == controller_list.cend()) {
        LOG_ERROR(Input, "Invalid identifier guid={}, pad={}, port={}", identifier.guid.RawString(),
                  identifier.pad, identifier.port);
        return {};
    }
    const ControllerData& controller = controller_iter->second;
    return controller.nfc;
}

void InputEngine::ResetButtonState() {
    for (const auto& controller : controller_list) {
        for (const auto& button : controller.second.buttons) {
            SetButton(controller.first, button.first, false);
        }
        for (const auto& button : controller.second.hat_buttons) {
            SetHatButton(controller.first, button.first, 0);
        }
    }
}

void InputEngine::ResetAnalogState() {
    for (const auto& controller : controller_list) {
        for (const auto& axis : controller.second.axes) {
            SetAxis(controller.first, axis.first, 0.0);
        }
    }
}

void InputEngine::TriggerOnButtonChange(const PadIdentifier& identifier, int button, bool value) {
    std::scoped_lock lock{mutex_callback};
    for (const auto& poller_pair : callback_list) {
        const InputIdentifier& poller = poller_pair.second;
        if (!IsInputIdentifierEqual(poller, identifier, EngineInputType::Button, button)) {
            continue;
        }
        if (poller.callback.on_change) {
            poller.callback.on_change();
        }
    }
    if (!configuring || !mapping_callback.on_data) {
        return;
    }

    PreSetButton(identifier, button);
    if (value == GetButton(identifier, button)) {
        return;
    }
    mapping_callback.on_data(MappingData{
        .engine = GetEngineName(),
        .pad = identifier,
        .type = EngineInputType::Button,
        .index = button,
        .button_value = value,
    });
}

void InputEngine::TriggerOnHatButtonChange(const PadIdentifier& identifier, int button, u8 value) {
    std::scoped_lock lock{mutex_callback};
    for (const auto& poller_pair : callback_list) {
        const InputIdentifier& poller = poller_pair.second;
        if (!IsInputIdentifierEqual(poller, identifier, EngineInputType::HatButton, button)) {
            continue;
        }
        if (poller.callback.on_change) {
            poller.callback.on_change();
        }
    }
    if (!configuring || !mapping_callback.on_data) {
        return;
    }
    for (std::size_t index = 1; index < 0xff; index <<= 1) {
        bool button_value = (value & index) != 0;
        if (button_value == GetHatButton(identifier, button, static_cast<u8>(index))) {
            continue;
        }
        mapping_callback.on_data(MappingData{
            .engine = GetEngineName(),
            .pad = identifier,
            .type = EngineInputType::HatButton,
            .index = button,
            .hat_name = GetHatButtonName(static_cast<u8>(index)),
        });
    }
}

void InputEngine::TriggerOnAxisChange(const PadIdentifier& identifier, int axis, f32 value) {
    std::scoped_lock lock{mutex_callback};
    for (const auto& poller_pair : callback_list) {
        const InputIdentifier& poller = poller_pair.second;
        if (!IsInputIdentifierEqual(poller, identifier, EngineInputType::Analog, axis)) {
            continue;
        }
        if (poller.callback.on_change) {
            poller.callback.on_change();
        }
    }
    if (!configuring || !mapping_callback.on_data) {
        return;
    }
    if (std::abs(value - GetAxis(identifier, axis)) < 0.5f) {
        return;
    }
    mapping_callback.on_data(MappingData{
        .engine = GetEngineName(),
        .pad = identifier,
        .type = EngineInputType::Analog,
        .index = axis,
        .axis_value = value,
    });
}

void InputEngine::TriggerOnBatteryChange(const PadIdentifier& identifier,
                                         [[maybe_unused]] Common::Input::BatteryLevel value) {
    std::scoped_lock lock{mutex_callback};
    for (const auto& poller_pair : callback_list) {
        const InputIdentifier& poller = poller_pair.second;
        if (!IsInputIdentifierEqual(poller, identifier, EngineInputType::Battery, 0)) {
            continue;
        }
        if (poller.callback.on_change) {
            poller.callback.on_change();
        }
    }
}

void InputEngine::TriggerOnColorChange(const PadIdentifier& identifier,
                                       [[maybe_unused]] Common::Input::BodyColorStatus value) {
    std::scoped_lock lock{mutex_callback};
    for (const auto& poller_pair : callback_list) {
        const InputIdentifier& poller = poller_pair.second;
        if (!IsInputIdentifierEqual(poller, identifier, EngineInputType::Color, 0)) {
            continue;
        }
        if (poller.callback.on_change) {
            poller.callback.on_change();
        }
    }
}

void InputEngine::TriggerOnMotionChange(const PadIdentifier& identifier, int motion,
                                        const BasicMotion& value) {
    std::scoped_lock lock{mutex_callback};
    for (const auto& poller_pair : callback_list) {
        const InputIdentifier& poller = poller_pair.second;
        if (!IsInputIdentifierEqual(poller, identifier, EngineInputType::Motion, motion)) {
            continue;
        }
        if (poller.callback.on_change) {
            poller.callback.on_change();
        }
    }
    if (!configuring || !mapping_callback.on_data) {
        return;
    }
    const auto old_value = GetMotion(identifier, motion);
    bool is_active = false;
    if (std::abs(value.accel_x - old_value.accel_x) > 1.5f ||
        std::abs(value.accel_y - old_value.accel_y) > 1.5f ||
        std::abs(value.accel_z - old_value.accel_z) > 1.5f) {
        is_active = true;
    }
    if (std::abs(value.gyro_x - old_value.gyro_x) > 0.6f ||
        std::abs(value.gyro_y - old_value.gyro_y) > 0.6f ||
        std::abs(value.gyro_z - old_value.gyro_z) > 0.6f) {
        is_active = true;
    }
    if (!is_active) {
        return;
    }
    mapping_callback.on_data(MappingData{
        .engine = GetEngineName(),
        .pad = identifier,
        .type = EngineInputType::Motion,
        .index = motion,
        .motion_value = value,
    });
}

void InputEngine::TriggerOnCameraChange(const PadIdentifier& identifier,
                                        [[maybe_unused]] const Common::Input::CameraStatus& value) {
    std::scoped_lock lock{mutex_callback};
    for (const auto& poller_pair : callback_list) {
        const InputIdentifier& poller = poller_pair.second;
        if (!IsInputIdentifierEqual(poller, identifier, EngineInputType::Camera, 0)) {
            continue;
        }
        if (poller.callback.on_change) {
            poller.callback.on_change();
        }
    }
}

void InputEngine::TriggerOnNfcChange(const PadIdentifier& identifier,
                                     [[maybe_unused]] const Common::Input::NfcStatus& value) {
    std::scoped_lock lock{mutex_callback};
    for (const auto& poller_pair : callback_list) {
        const InputIdentifier& poller = poller_pair.second;
        if (!IsInputIdentifierEqual(poller, identifier, EngineInputType::Nfc, 0)) {
            continue;
        }
        if (poller.callback.on_change) {
            poller.callback.on_change();
        }
    }
}

bool InputEngine::IsInputIdentifierEqual(const InputIdentifier& input_identifier,
                                         const PadIdentifier& identifier, EngineInputType type,
                                         int index) const {
    if (input_identifier.type != type) {
        return false;
    }
    if (input_identifier.index != index) {
        return false;
    }
    if (input_identifier.identifier != identifier) {
        return false;
    }
    return true;
}

void InputEngine::BeginConfiguration() {
    configuring = true;
}

void InputEngine::EndConfiguration() {
    configuring = false;
}

const std::string& InputEngine::GetEngineName() const {
    return input_engine;
}

int InputEngine::SetCallback(InputIdentifier input_identifier) {
    std::scoped_lock lock{mutex_callback};
    callback_list.insert_or_assign(last_callback_key, std::move(input_identifier));
    return last_callback_key++;
}

void InputEngine::SetMappingCallback(MappingCallback callback) {
    std::scoped_lock lock{mutex_callback};
    mapping_callback = std::move(callback);
}

void InputEngine::DeleteCallback(int key) {
    std::scoped_lock lock{mutex_callback};
    const auto& iterator = callback_list.find(key);
    if (iterator == callback_list.end()) {
        LOG_ERROR(Input, "Tried to delete non-existent callback {}", key);
        return;
    }
    callback_list.erase(iterator);
}

} // namespace InputCommon
