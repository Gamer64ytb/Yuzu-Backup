// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>

#include "common/common_types.h"

#include "core/frontend/applets/applet.h"
#include "core/hle/service/am/frontend/applet_software_keyboard_types.h"

namespace Core::Frontend {

struct KeyboardInitializeParameters {
    std::u16string ok_text;
    std::u16string header_text;
    std::u16string sub_text;
    std::u16string guide_text;
    std::u16string initial_text;
    char16_t left_optional_symbol_key;
    char16_t right_optional_symbol_key;
    u32 max_text_length;
    u32 min_text_length;
    s32 initial_cursor_position;
    Service::AM::Frontend::SwkbdType type;
    Service::AM::Frontend::SwkbdPasswordMode password_mode;
    Service::AM::Frontend::SwkbdTextDrawType text_draw_type;
    Service::AM::Frontend::SwkbdKeyDisableFlags key_disable_flags;
    bool use_blur_background;
    bool enable_backspace_button;
    bool enable_return_button;
    bool disable_cancel_button;
};

struct InlineAppearParameters {
    u32 max_text_length;
    u32 min_text_length;
    f32 key_top_scale_x;
    f32 key_top_scale_y;
    f32 key_top_translate_x;
    f32 key_top_translate_y;
    Service::AM::Frontend::SwkbdType type;
    Service::AM::Frontend::SwkbdKeyDisableFlags key_disable_flags;
    bool key_top_as_floating;
    bool enable_backspace_button;
    bool enable_return_button;
    bool disable_cancel_button;
};

struct InlineTextParameters {
    std::u16string input_text;
    s32 cursor_position;
};

class SoftwareKeyboardApplet : public Applet {
public:
    using SubmitInlineCallback =
        std::function<void(Service::AM::Frontend::SwkbdReplyType, std::u16string, s32)>;
    using SubmitNormalCallback =
        std::function<void(Service::AM::Frontend::SwkbdResult, std::u16string, bool)>;

    virtual ~SoftwareKeyboardApplet();

    virtual void InitializeKeyboard(bool is_inline,
                                    KeyboardInitializeParameters initialize_parameters,
                                    SubmitNormalCallback submit_normal_callback_,
                                    SubmitInlineCallback submit_inline_callback_) = 0;

    virtual void ShowNormalKeyboard() const = 0;

    virtual void ShowTextCheckDialog(Service::AM::Frontend::SwkbdTextCheckResult text_check_result,
                                     std::u16string text_check_message) const = 0;

    virtual void ShowInlineKeyboard(InlineAppearParameters appear_parameters) const = 0;

    virtual void HideInlineKeyboard() const = 0;

    virtual void InlineTextChanged(InlineTextParameters text_parameters) const = 0;

    virtual void ExitKeyboard() const = 0;
};

class DefaultSoftwareKeyboardApplet final : public SoftwareKeyboardApplet {
public:
    ~DefaultSoftwareKeyboardApplet() override;

    void Close() const override;

    void InitializeKeyboard(bool is_inline, KeyboardInitializeParameters initialize_parameters,
                            SubmitNormalCallback submit_normal_callback_,
                            SubmitInlineCallback submit_inline_callback_) override;

    void ShowNormalKeyboard() const override;

    void ShowTextCheckDialog(Service::AM::Frontend::SwkbdTextCheckResult text_check_result,
                             std::u16string text_check_message) const override;

    void ShowInlineKeyboard(InlineAppearParameters appear_parameters) const override;

    void HideInlineKeyboard() const override;

    void InlineTextChanged(InlineTextParameters text_parameters) const override;

    void ExitKeyboard() const override;

private:
    void SubmitNormalText(std::u16string text) const;
    void SubmitInlineText(std::u16string_view text) const;

    KeyboardInitializeParameters parameters{};

    mutable SubmitNormalCallback submit_normal_callback;
    mutable SubmitInlineCallback submit_inline_callback;
};

} // namespace Core::Frontend
