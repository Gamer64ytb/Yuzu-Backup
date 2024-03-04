// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <QKeySequence>
#include <QString>
#include <QWidget>
#include "hid_core/hid_types.h"

class QDialog;
class QSettings;
class QShortcut;
class ControllerShortcut;

namespace Core::HID {
enum class ControllerTriggerType;
class EmulatedController;
} // namespace Core::HID

struct ControllerButtonSequence {
    Core::HID::CaptureButtonState capture{};
    Core::HID::HomeButtonState home{};
    Core::HID::NpadButtonState npad{};
};

class ControllerShortcut : public QObject {
    Q_OBJECT

public:
    explicit ControllerShortcut(Core::HID::EmulatedController* controller);
    ~ControllerShortcut();

    void SetKey(const ControllerButtonSequence& buttons);
    void SetKey(const std::string& buttons_shortcut);

    ControllerButtonSequence ButtonSequence() const;

    void SetEnabled(bool enable);
    bool IsEnabled() const;

Q_SIGNALS:
    void Activated();

private:
    void ControllerUpdateEvent(Core::HID::ControllerTriggerType type);

    bool is_enabled{};
    bool active{};
    int callback_key{};
    ControllerButtonSequence button_sequence{};
    std::string name{};
    Core::HID::EmulatedController* emulated_controller = nullptr;
};

class HotkeyRegistry final {
public:
    friend class ConfigureHotkeys;

    explicit HotkeyRegistry();
    ~HotkeyRegistry();

    /**
     * Loads hotkeys from the settings file.
     *
     * @note Yet unregistered hotkeys which are present in the settings will automatically be
     *       registered.
     */
    void LoadHotkeys();

    /**
     * Saves all registered hotkeys to the settings file.
     *
     * @note Each hotkey group will be stored a settings group; For each hotkey inside that group, a
     *       settings group will be created to store the key sequence and the hotkey context.
     */
    void SaveHotkeys();

    /**
     * Returns a QShortcut object whose activated() signal can be connected to other QObjects'
     * slots.
     *
     * @param group  General group this hotkey belongs to (e.g. "Main Window", "Debugger").
     * @param action Name of the action (e.g. "Start Emulation", "Load Image").
     * @param widget Parent widget of the returned QShortcut.
     * @warning If multiple QWidgets' call this function for the same action, the returned QShortcut
     *          will be the same. Thus, you shouldn't rely on the caller really being the
     *          QShortcut's parent.
     */
    QShortcut* GetHotkey(const std::string& group, const std::string& action, QWidget* widget);
    ControllerShortcut* GetControllerHotkey(const std::string& group, const std::string& action,
                                            Core::HID::EmulatedController* controller);

    /**
     * Returns a QKeySequence object whose signal can be connected to QAction::setShortcut.
     *
     * @param group  General group this hotkey belongs to (e.g. "Main Window", "Debugger").
     * @param action Name of the action (e.g. "Start Emulation", "Load Image").
     */
    QKeySequence GetKeySequence(const std::string& group, const std::string& action);

    /**
     * Returns a Qt::ShortcutContext object who can be connected to other
     * QAction::setShortcutContext.
     *
     * @param group  General group this shortcut context belongs to (e.g. "Main Window",
     * "Debugger").
     * @param action Name of the action (e.g. "Start Emulation", "Load Image").
     */
    Qt::ShortcutContext GetShortcutContext(const std::string& group, const std::string& action);

private:
    struct Hotkey {
        QKeySequence keyseq;
        std::string controller_keyseq;
        QShortcut* shortcut = nullptr;
        ControllerShortcut* controller_shortcut = nullptr;
        Qt::ShortcutContext context = Qt::WindowShortcut;
        bool repeat;
    };

    using HotkeyMap = std::map<std::string, Hotkey>;
    using HotkeyGroupMap = std::map<std::string, HotkeyMap>;

    HotkeyGroupMap hotkey_groups;
};
