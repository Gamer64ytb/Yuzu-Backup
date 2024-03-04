// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <QAbstractButton>
#include <QCheckBox>
#include <QPushButton>
#include <QString>
#include <QTimer>

#include "common/fs/fs_util.h"
#include "common/settings_enums.h"
#include "common/settings_input.h"
#include "configuration/shared_widget.h"
#include "core/core.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/xts_archive.h"
#include "core/loader/loader.h"
#include "frontend_common/config.h"
#include "ui_configure_per_game.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_audio.h"
#include "yuzu/configuration/configure_cpu.h"
#include "yuzu/configuration/configure_graphics.h"
#include "yuzu/configuration/configure_graphics_advanced.h"
#include "yuzu/configuration/configure_input_per_game.h"
#include "yuzu/configuration/configure_linux_tab.h"
#include "yuzu/configuration/configure_per_game.h"
#include "yuzu/configuration/configure_per_game_addons.h"
#include "yuzu/configuration/configure_system.h"
#include "yuzu/uisettings.h"
#include "yuzu/util/util.h"
#include "yuzu/vk_device_info.h"

ConfigurePerGame::ConfigurePerGame(QWidget* parent, u64 title_id_, const std::string& file_name,
                                   std::vector<VkDeviceInfo::Record>& vk_device_records,
                                   Core::System& system_)
    : QDialog(parent),
      ui(std::make_unique<Ui::ConfigurePerGame>()), title_id{title_id_}, system{system_},
      builder{std::make_unique<ConfigurationShared::Builder>(this, !system_.IsPoweredOn())},
      tab_group{std::make_shared<std::vector<ConfigurationShared::Tab*>>()} {
    const auto file_path = std::filesystem::path(Common::FS::ToU8String(file_name));
    const auto config_file_name = title_id == 0 ? Common::FS::PathToUTF8String(file_path.filename())
                                                : fmt::format("{:016X}", title_id);
    game_config = std::make_unique<QtConfig>(config_file_name, Config::ConfigType::PerGameConfig);
    addons_tab = std::make_unique<ConfigurePerGameAddons>(system_, this);
    audio_tab = std::make_unique<ConfigureAudio>(system_, tab_group, *builder, this);
    cpu_tab = std::make_unique<ConfigureCpu>(system_, tab_group, *builder, this);
    graphics_advanced_tab =
        std::make_unique<ConfigureGraphicsAdvanced>(system_, tab_group, *builder, this);
    graphics_tab = std::make_unique<ConfigureGraphics>(
        system_, vk_device_records, [&]() { graphics_advanced_tab->ExposeComputeOption(); },
        [](Settings::AspectRatio, Settings::ResolutionSetup) {}, tab_group, *builder, this);
    input_tab = std::make_unique<ConfigureInputPerGame>(system_, game_config.get(), this);
    linux_tab = std::make_unique<ConfigureLinuxTab>(system_, tab_group, *builder, this);
    system_tab = std::make_unique<ConfigureSystem>(system_, tab_group, *builder, this);

    ui->setupUi(this);

    ui->tabWidget->addTab(addons_tab.get(), tr("Add-Ons"));
    ui->tabWidget->addTab(system_tab.get(), tr("System"));
    ui->tabWidget->addTab(cpu_tab.get(), tr("CPU"));
    ui->tabWidget->addTab(graphics_tab.get(), tr("Graphics"));
    ui->tabWidget->addTab(graphics_advanced_tab.get(), tr("Adv. Graphics"));
    ui->tabWidget->addTab(audio_tab.get(), tr("Audio"));
    ui->tabWidget->addTab(input_tab.get(), tr("Input Profiles"));

    // Only show Linux tab on Unix
    linux_tab->setVisible(false);
#ifdef __unix__
    linux_tab->setVisible(true);
    ui->tabWidget->addTab(linux_tab.get(), tr("Linux"));
#endif

    setFocusPolicy(Qt::ClickFocus);
    setWindowTitle(tr("Properties"));

    addons_tab->SetTitleId(title_id);

    scene = new QGraphicsScene;
    ui->icon_view->setScene(scene);

    if (system.IsPoweredOn()) {
        QPushButton* apply_button = ui->buttonBox->addButton(QDialogButtonBox::Apply);
        connect(apply_button, &QAbstractButton::clicked, this,
                &ConfigurePerGame::HandleApplyButtonClicked);
    }

    LoadConfiguration();
}

ConfigurePerGame::~ConfigurePerGame() = default;

void ConfigurePerGame::ApplyConfiguration() {
    for (const auto tab : *tab_group) {
        tab->ApplyConfiguration();
    }
    addons_tab->ApplyConfiguration();
    input_tab->ApplyConfiguration();

    if (Settings::IsDockedMode() && Settings::values.players.GetValue()[0].controller_type ==
                                        Settings::ControllerType::Handheld) {
        Settings::values.use_docked_mode.SetValue(Settings::ConsoleMode::Handheld);
        Settings::values.use_docked_mode.SetGlobal(true);
    }

    system.ApplySettings();
    Settings::LogSettings();

    game_config->SaveAllValues();
}

void ConfigurePerGame::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QDialog::changeEvent(event);
}

void ConfigurePerGame::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigurePerGame::HandleApplyButtonClicked() {
    UISettings::values.configuration_applied = true;
    ApplyConfiguration();
}

void ConfigurePerGame::LoadFromFile(FileSys::VirtualFile file_) {
    file = std::move(file_);
    LoadConfiguration();
}

void ConfigurePerGame::LoadConfiguration() {
    if (file == nullptr) {
        return;
    }

    addons_tab->LoadFromFile(file);

    ui->display_title_id->setText(
        QStringLiteral("%1").arg(title_id, 16, 16, QLatin1Char{'0'}).toUpper());

    const FileSys::PatchManager pm{title_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};
    const auto control = pm.GetControlMetadata();
    const auto loader = Loader::GetLoader(system, file);

    if (control.first != nullptr) {
        ui->display_version->setText(QString::fromStdString(control.first->GetVersionString()));
        ui->display_name->setText(QString::fromStdString(control.first->GetApplicationName()));
        ui->display_developer->setText(QString::fromStdString(control.first->GetDeveloperName()));
    } else {
        std::string title;
        if (loader->ReadTitle(title) == Loader::ResultStatus::Success)
            ui->display_name->setText(QString::fromStdString(title));

        FileSys::NACP nacp;
        if (loader->ReadControlData(nacp) == Loader::ResultStatus::Success)
            ui->display_developer->setText(QString::fromStdString(nacp.GetDeveloperName()));

        ui->display_version->setText(QStringLiteral("1.0.0"));
    }

    if (control.second != nullptr) {
        scene->clear();

        QPixmap map;
        const auto bytes = control.second->ReadAllBytes();
        map.loadFromData(bytes.data(), static_cast<u32>(bytes.size()));

        scene->addPixmap(map.scaled(ui->icon_view->width(), ui->icon_view->height(),
                                    Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    } else {
        std::vector<u8> bytes;
        if (loader->ReadIcon(bytes) == Loader::ResultStatus::Success) {
            scene->clear();

            QPixmap map;
            map.loadFromData(bytes.data(), static_cast<u32>(bytes.size()));

            scene->addPixmap(map.scaled(ui->icon_view->width(), ui->icon_view->height(),
                                        Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
    }

    ui->display_filename->setText(QString::fromStdString(file->GetName()));

    ui->display_format->setText(
        QString::fromStdString(Loader::GetFileTypeString(loader->GetFileType())));

    const auto valueText = ReadableByteSize(file->GetSize());
    ui->display_size->setText(valueText);
}
