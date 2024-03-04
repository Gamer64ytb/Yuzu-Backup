// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/input.h"
#include "common/param_package.h"
#include "common/point.h"
#include "common/quaternion.h"
#include "common/vector_math.h"
#include "hid_core/frontend/motion_input.h"
#include "hid_core/hid_types.h"

namespace Core::HID {
static constexpr std::size_t MaxTouchDevices = 32;
static constexpr std::size_t MaxActiveTouchInputs = 16;

struct ConsoleMotionInfo {
    Common::Input::MotionStatus raw_status{};
    MotionInput emulated{};
};

using ConsoleMotionDevices = std::array<std::unique_ptr<Common::Input::InputDevice>, 2>;
using TouchDevices = std::array<std::unique_ptr<Common::Input::InputDevice>, MaxTouchDevices>;

using ConsoleMotionParams = std::array<Common::ParamPackage, 2>;
using TouchParams = std::array<Common::ParamPackage, MaxTouchDevices>;

using ConsoleMotionValues = ConsoleMotionInfo;
using TouchValues = std::array<Common::Input::TouchStatus, MaxTouchDevices>;

// Contains all motion related data that is used on the services
struct ConsoleMotion {
    Common::Vec3f accel{};
    Common::Vec3f gyro{};
    Common::Vec3f rotation{};
    std::array<Common::Vec3f, 3> orientation{};
    Common::Quaternion<f32> quaternion{};
    Common::Vec3f gyro_bias{};
    f32 verticalization_error{};
    bool is_at_rest{};
};

using TouchFingerState = std::array<TouchFinger, MaxActiveTouchInputs>;

struct ConsoleStatus {
    // Data from input_common
    ConsoleMotionValues motion_values{};
    TouchValues touch_values{};

    // Data for HID services
    ConsoleMotion motion_state{};
    TouchFingerState touch_state{};
};

enum class ConsoleTriggerType {
    Motion,
    Touch,
    All,
};

struct ConsoleUpdateCallback {
    std::function<void(ConsoleTriggerType)> on_change;
};

class EmulatedConsole {
public:
    /**
     * Contains all input data within the emulated switch console tablet such as touch and motion
     */
    explicit EmulatedConsole();
    ~EmulatedConsole();

    YUZU_NON_COPYABLE(EmulatedConsole);
    YUZU_NON_MOVEABLE(EmulatedConsole);

    /// Removes all callbacks created from input devices
    void UnloadInput();

    /**
     * Sets the emulated console into configuring mode
     * This prevents the modification of the HID state of the emulated console by input commands
     */
    void EnableConfiguration();

    /// Returns the emulated console into normal mode, allowing the modification of the HID state
    void DisableConfiguration();

    /// Returns true if the emulated console is in configuring mode
    bool IsConfiguring() const;

    /// Reload all input devices
    void ReloadInput();

    /// Overrides current mapped devices with the stored configuration and reloads all input devices
    void ReloadFromSettings();

    /// Saves the current mapped configuration
    void SaveCurrentConfig();

    /// Reverts any mapped changes made that weren't saved
    void RestoreConfig();

    // Returns the current mapped motion device
    Common::ParamPackage GetMotionParam() const;

    /**
     * Updates the current mapped motion device
     * @param param ParamPackage with controller data to be mapped
     */
    void SetMotionParam(Common::ParamPackage param);

    /// Returns the latest status of motion input from the console with parameters
    ConsoleMotionValues GetMotionValues() const;

    /// Returns the latest status of touch input from the console with parameters
    TouchValues GetTouchValues() const;

    /// Returns the latest status of motion input from the console
    ConsoleMotion GetMotion() const;

    /// Returns the latest status of touch input from the console
    TouchFingerState GetTouch() const;

    /**
     * Adds a callback to the list of events
     * @param update_callback A ConsoleUpdateCallback that will be triggered
     * @return an unique key corresponding to the callback index in the list
     */
    int SetCallback(ConsoleUpdateCallback update_callback);

    /**
     * Removes a callback from the list stopping any future events to this object
     * @param key Key corresponding to the callback index in the list
     */
    void DeleteCallback(int key);

private:
    /// Creates and stores the touch params
    void SetTouchParams();

    /**
     * Updates the motion status of the console
     * @param callback A CallbackStatus containing gyro and accelerometer data
     */
    void SetMotion(const Common::Input::CallbackStatus& callback);

    /**
     * Updates the touch status of the console
     * @param callback A CallbackStatus containing the touch position
     * @param index Finger ID to be updated
     */
    void SetTouch(const Common::Input::CallbackStatus& callback, std::size_t index);

    std::optional<std::size_t> GetIndexFromFingerId(std::size_t finger_id) const;

    std::optional<std::size_t> GetNextFreeIndex() const;

    /**
     * Triggers a callback that something has changed on the console status
     * @param type Input type of the event to trigger
     */
    void TriggerOnChange(ConsoleTriggerType type);

    bool is_configuring{false};
    f32 motion_sensitivity{0.01f};

    ConsoleMotionParams motion_params;
    TouchParams touch_params;

    ConsoleMotionDevices motion_devices;
    TouchDevices touch_devices;

    mutable std::mutex mutex;
    mutable std::mutex callback_mutex;
    std::unordered_map<int, ConsoleUpdateCallback> callback_list;
    int last_callback_key = 0;

    // Stores the current status of all console input
    ConsoleStatus console;
};

} // namespace Core::HID
