// SPDX-FileCopyrightText: Copyright 2018 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <QDialog>
#include "network/room.h"
#include "network/room_member.h"

namespace Ui {
class ModerationDialog;
}

class QStandardItemModel;

class ModerationDialog : public QDialog {
    Q_OBJECT

public:
    explicit ModerationDialog(Network::RoomNetwork& room_network_, QWidget* parent = nullptr);
    ~ModerationDialog();

signals:
    void StatusMessageReceived(const Network::StatusMessageEntry&);
    void BanListReceived(const Network::Room::BanList&);

private:
    void LoadBanList();
    void PopulateBanList(const Network::Room::BanList& ban_list);
    void SendUnbanRequest(const QString& subject);
    void OnStatusMessageReceived(const Network::StatusMessageEntry& status_message);

    std::unique_ptr<Ui::ModerationDialog> ui;
    QStandardItemModel* model;
    Network::RoomMember::CallbackHandle<Network::StatusMessageEntry> callback_handle_status_message;
    Network::RoomMember::CallbackHandle<Network::Room::BanList> callback_handle_ban_list;

    Network::RoomNetwork& room_network;
};

Q_DECLARE_METATYPE(Network::Room::BanList);
