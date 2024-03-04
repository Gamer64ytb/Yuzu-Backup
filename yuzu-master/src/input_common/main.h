// SPDX-FileCopyrightText: 2017 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Common {
class ParamPackage;
}

namespace Common::Input {
enum class ButtonNames;
}

namespace Settings::NativeAnalog {
enum Values : int;
}

namespace Settings::NativeButton {
enum Values : int;
}

namespace Settings::NativeMotion {
enum Values : int;
}

namespace InputCommon {
class Android;
class Camera;
class Keyboard;
class Mouse;
class TouchScreen;
class VirtualAmiibo;
class VirtualGamepad;
struct MappingData;
} // namespace InputCommon

namespace InputCommon::TasInput {
class Tas;
} // namespace InputCommon::TasInput

namespace InputCommon {
namespace Polling {
/// Type of input desired for mapping purposes
enum class InputType { None, Button, Stick, Motion, Touch };
} // namespace Polling

/**
 * Given a ParamPackage for a Device returned from `GetInputDevices`, attempt to get the default
 * mapping for the device.
 */
using AnalogMapping = std::unordered_map<Settings::NativeAnalog::Values, Common::ParamPackage>;
using ButtonMapping = std::unordered_map<Settings::NativeButton::Values, Common::ParamPackage>;
using MotionMapping = std::unordered_map<Settings::NativeMotion::Values, Common::ParamPackage>;

class InputSubsystem {
public:
    explicit InputSubsystem();
    ~InputSubsystem();

    InputSubsystem(const InputSubsystem&) = delete;
    InputSubsystem& operator=(const InputSubsystem&) = delete;

    InputSubsystem(InputSubsystem&&) = delete;
    InputSubsystem& operator=(InputSubsystem&&) = delete;

    /// Initializes and registers all built-in input device factories.
    void Initialize();

    /// Unregisters all built-in input device factories and shuts them down.
    void Shutdown();

    /// Retrieves the underlying keyboard device.
    [[nodiscard]] Keyboard* GetKeyboard();

    /// Retrieves the underlying keyboard device.
    [[nodiscard]] const Keyboard* GetKeyboard() const;

    /// Retrieves the underlying mouse device.
    [[nodiscard]] Mouse* GetMouse();

    /// Retrieves the underlying mouse device.
    [[nodiscard]] const Mouse* GetMouse() const;

    /// Retrieves the underlying touch screen device.
    [[nodiscard]] TouchScreen* GetTouchScreen();

    /// Retrieves the underlying touch screen device.
    [[nodiscard]] const TouchScreen* GetTouchScreen() const;

    /// Retrieves the underlying tas input device.
    [[nodiscard]] TasInput::Tas* GetTas();

    /// Retrieves the underlying tas input device.
    [[nodiscard]] const TasInput::Tas* GetTas() const;

    /// Retrieves the underlying camera input device.
    [[nodiscard]] Camera* GetCamera();

    /// Retrieves the underlying camera input device.
    [[nodiscard]] const Camera* GetCamera() const;

    /// Retrieves the underlying android input device.
    [[nodiscard]] Android* GetAndroid();

    /// Retrieves the underlying android input device.
    [[nodiscard]] const Android* GetAndroid() const;

    /// Retrieves the underlying virtual amiibo input device.
    [[nodiscard]] VirtualAmiibo* GetVirtualAmiibo();

    /// Retrieves the underlying virtual amiibo input device.
    [[nodiscard]] const VirtualAmiibo* GetVirtualAmiibo() const;

    /// Retrieves the underlying virtual gamepad input device.
    [[nodiscard]] VirtualGamepad* GetVirtualGamepad();

    /// Retrieves the underlying virtual gamepad input device.
    [[nodiscard]] const VirtualGamepad* GetVirtualGamepad() const;

    /**
     * Returns all available input devices that this Factory can create a new device with.
     * Each returned ParamPackage should have a `display` field used for display, a `engine` field
     * for backends to determine if this backend is meant to service the request and any other
     * information needed to identify this in the backend later.
     */
    [[nodiscard]] std::vector<Common::ParamPackage> GetInputDevices() const;

    /// Retrieves the analog mappings for the given device.
    [[nodiscard]] AnalogMapping GetAnalogMappingForDevice(const Common::ParamPackage& device) const;

    /// Retrieves the button mappings for the given device.
    [[nodiscard]] ButtonMapping GetButtonMappingForDevice(const Common::ParamPackage& device) const;

    /// Retrieves the motion mappings for the given device.
    [[nodiscard]] MotionMapping GetMotionMappingForDevice(const Common::ParamPackage& device) const;

    /// Returns an enum containing the name to be displayed from the input engine.
    [[nodiscard]] Common::Input::ButtonNames GetButtonName(
        const Common::ParamPackage& params) const;

    /// Returns true if device is a controller.
    [[nodiscard]] bool IsController(const Common::ParamPackage& params) const;

    /// Returns true if axis of a stick aren't mapped in the correct direction
    [[nodiscard]] bool IsStickInverted(const Common::ParamPackage& device) const;

    /// Reloads the input devices.
    void ReloadInputDevices();

    /// Start polling from all backends for a desired input type.
    void BeginMapping(Polling::InputType type);

    /// Returns an input event with mapping information.
    [[nodiscard]] Common::ParamPackage GetNextInput() const;

    /// Stop polling from all backends.
    void StopMapping() const;

    /// Signals SDL driver for new input events
    void PumpEvents() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

/// Generates a serialized param package for creating a keyboard button device.
std::string GenerateKeyboardParam(int key_code);

/// Generates a serialized param package for creating an analog device taking input from keyboard.
std::string GenerateAnalogParamFromKeys(int key_up, int key_down, int key_left, int key_right,
                                        int key_modifier, float modifier_scale);
} // namespace InputCommon
