// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include "common/common_types.h"

#include "core/frontend/applets/applet.h"

namespace Core::Frontend {

class ParentalControlsApplet : public Applet {
public:
    virtual ~ParentalControlsApplet();

    // Prompts the user to enter a PIN and calls the callback with whether or not it matches the
    // correct PIN. If the bool is passed, and the PIN was recently entered correctly, the frontend
    // should not prompt and simply return true.
    virtual void VerifyPIN(std::function<void(bool)> finished,
                           bool suspend_future_verification_temporarily) = 0;

    // Prompts the user to enter a PIN and calls the callback for correctness. Frontends can
    // optionally alert the user that this is to change parental controls settings.
    virtual void VerifyPINForSettings(std::function<void(bool)> finished) = 0;

    // Prompts the user to create a new PIN for pctl and stores it with the service.
    virtual void RegisterPIN(std::function<void()> finished) = 0;

    // Prompts the user to verify the current PIN and then store a new one into pctl.
    virtual void ChangePIN(std::function<void()> finished) = 0;
};

class DefaultParentalControlsApplet final : public ParentalControlsApplet {
public:
    ~DefaultParentalControlsApplet() override;

    void Close() const override;
    void VerifyPIN(std::function<void(bool)> finished,
                   bool suspend_future_verification_temporarily) override;
    void VerifyPINForSettings(std::function<void(bool)> finished) override;
    void RegisterPIN(std::function<void()> finished) override;
    void ChangePIN(std::function<void()> finished) override;
};

class PhotoViewerApplet : public Applet {
public:
    virtual ~PhotoViewerApplet();

    virtual void ShowPhotosForApplication(u64 title_id, std::function<void()> finished) const = 0;
    virtual void ShowAllPhotos(std::function<void()> finished) const = 0;
};

class DefaultPhotoViewerApplet final : public PhotoViewerApplet {
public:
    ~DefaultPhotoViewerApplet() override;

    void Close() const override;
    void ShowPhotosForApplication(u64 title_id, std::function<void()> finished) const override;
    void ShowAllPhotos(std::function<void()> finished) const override;
};

} // namespace Core::Frontend
