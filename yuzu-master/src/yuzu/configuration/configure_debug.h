// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <QScrollArea>

namespace Core {
class System;
}

namespace Ui {
class ConfigureDebug;
}

class ConfigureDebug : public QScrollArea {
    Q_OBJECT

public:
    explicit ConfigureDebug(const Core::System& system_, QWidget* parent = nullptr);
    ~ConfigureDebug() override;

    void ApplyConfiguration();

private:
    void changeEvent(QEvent* event) override;

    void RetranslateUI();
    void SetConfiguration();

    std::unique_ptr<Ui::ConfigureDebug> ui;

    const Core::System& system;

    bool crash_dump_warning_shown{false};
};
