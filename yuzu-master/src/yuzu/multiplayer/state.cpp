// SPDX-FileCopyrightText: Copyright 2018 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QStandardItemModel>
#include "common/announce_multiplayer_room.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "yuzu/game_list.h"
#include "yuzu/multiplayer/client_room.h"
#include "yuzu/multiplayer/direct_connect.h"
#include "yuzu/multiplayer/host_room.h"
#include "yuzu/multiplayer/lobby.h"
#include "yuzu/multiplayer/message.h"
#include "yuzu/multiplayer/state.h"
#include "yuzu/uisettings.h"
#include "yuzu/util/clickable_label.h"

MultiplayerState::MultiplayerState(QWidget* parent, QStandardItemModel* game_list_model_,
                                   QAction* leave_room_, QAction* show_room_, Core::System& system_)
    : QWidget(parent), game_list_model(game_list_model_), leave_room(leave_room_),
      show_room(show_room_), system{system_}, room_network{system.GetRoomNetwork()} {
    if (auto member = room_network.GetRoomMember().lock()) {
        // register the network structs to use in slots and signals
        state_callback_handle = member->BindOnStateChanged(
            [this](const Network::RoomMember::State& state) { emit NetworkStateChanged(state); });
        connect(this, &MultiplayerState::NetworkStateChanged, this,
                &MultiplayerState::OnNetworkStateChanged);
        error_callback_handle = member->BindOnError(
            [this](const Network::RoomMember::Error& error) { emit NetworkError(error); });
        connect(this, &MultiplayerState::NetworkError, this, &MultiplayerState::OnNetworkError);
    }

    qRegisterMetaType<Network::RoomMember::State>();
    qRegisterMetaType<Network::RoomMember::Error>();
    qRegisterMetaType<WebService::WebResult>();
    announce_multiplayer_session = std::make_shared<Core::AnnounceMultiplayerSession>(room_network);
    announce_multiplayer_session->BindErrorCallback(
        [this](const WebService::WebResult& result) { emit AnnounceFailed(result); });
    connect(this, &MultiplayerState::AnnounceFailed, this, &MultiplayerState::OnAnnounceFailed);

    status_text = new ClickableLabel(this);
    status_icon = new ClickableLabel(this);

    connect(status_text, &ClickableLabel::clicked, this, &MultiplayerState::OnOpenNetworkRoom);
    connect(status_icon, &ClickableLabel::clicked, this, &MultiplayerState::OnOpenNetworkRoom);

    connect(static_cast<QApplication*>(QApplication::instance()), &QApplication::focusChanged, this,
            [this](QWidget* /*old*/, QWidget* now) {
                if (client_room && client_room->isAncestorOf(now)) {
                    HideNotification();
                }
            });

    retranslateUi();
}

MultiplayerState::~MultiplayerState() = default;

void MultiplayerState::Close() {
    if (state_callback_handle) {
        if (auto member = room_network.GetRoomMember().lock()) {
            member->Unbind(state_callback_handle);
        }
    }

    if (error_callback_handle) {
        if (auto member = room_network.GetRoomMember().lock()) {
            member->Unbind(error_callback_handle);
        }
    }
    if (host_room) {
        host_room->close();
    }
    if (direct_connect) {
        direct_connect->close();
    }
    if (client_room) {
        client_room->close();
    }
    if (lobby) {
        lobby->close();
    }
}

void MultiplayerState::retranslateUi() {
    status_text->setToolTip(tr("Current connection status"));

    UpdateNotificationStatus();

    if (lobby) {
        lobby->RetranslateUi();
    }
    if (host_room) {
        host_room->RetranslateUi();
    }
    if (client_room) {
        client_room->RetranslateUi();
    }
    if (direct_connect) {
        direct_connect->RetranslateUi();
    }
}

void MultiplayerState::SetNotificationStatus(NotificationStatus status) {
    notification_status = status;
    UpdateNotificationStatus();
}

void MultiplayerState::UpdateNotificationStatus() {
    switch (notification_status) {
    case NotificationStatus::Uninitialized:
        status_icon->setPixmap(QIcon::fromTheme(QStringLiteral("disconnected")).pixmap(16));
        status_text->setText(tr("Not Connected. Click here to find a room!"));
        leave_room->setEnabled(false);
        show_room->setEnabled(false);
        break;
    case NotificationStatus::Disconnected:
        status_icon->setPixmap(QIcon::fromTheme(QStringLiteral("disconnected")).pixmap(16));
        status_text->setText(tr("Not Connected"));
        leave_room->setEnabled(false);
        show_room->setEnabled(false);
        break;
    case NotificationStatus::Connected:
        status_icon->setPixmap(QIcon::fromTheme(QStringLiteral("connected")).pixmap(16));
        status_text->setText(tr("Connected"));
        leave_room->setEnabled(true);
        show_room->setEnabled(true);
        break;
    case NotificationStatus::Notification:
        status_icon->setPixmap(
            QIcon::fromTheme(QStringLiteral("connected_notification")).pixmap(16));
        status_text->setText(tr("New Messages Received"));
        leave_room->setEnabled(true);
        show_room->setEnabled(true);
        break;
    }

    // Clean up status bar if game is running
    if (system.IsPoweredOn()) {
        status_text->clear();
    }
}

void MultiplayerState::OnNetworkStateChanged(const Network::RoomMember::State& state) {
    LOG_DEBUG(Frontend, "Network State: {}", Network::GetStateStr(state));
    if (state == Network::RoomMember::State::Joined ||
        state == Network::RoomMember::State::Moderator) {

        OnOpenNetworkRoom();
        SetNotificationStatus(NotificationStatus::Connected);
    } else {
        SetNotificationStatus(NotificationStatus::Disconnected);
    }

    current_state = state;
}

void MultiplayerState::OnNetworkError(const Network::RoomMember::Error& error) {
    LOG_DEBUG(Frontend, "Network Error: {}", Network::GetErrorStr(error));
    switch (error) {
    case Network::RoomMember::Error::LostConnection:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::LOST_CONNECTION);
        break;
    case Network::RoomMember::Error::HostKicked:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::HOST_KICKED);
        break;
    case Network::RoomMember::Error::CouldNotConnect:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::UNABLE_TO_CONNECT);
        break;
    case Network::RoomMember::Error::NameCollision:
        NetworkMessage::ErrorManager::ShowError(
            NetworkMessage::ErrorManager::USERNAME_NOT_VALID_SERVER);
        break;
    case Network::RoomMember::Error::IpCollision:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::IP_COLLISION);
        break;
    case Network::RoomMember::Error::RoomIsFull:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::ROOM_IS_FULL);
        break;
    case Network::RoomMember::Error::WrongPassword:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::WRONG_PASSWORD);
        break;
    case Network::RoomMember::Error::WrongVersion:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::WRONG_VERSION);
        break;
    case Network::RoomMember::Error::HostBanned:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::HOST_BANNED);
        break;
    case Network::RoomMember::Error::UnknownError:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::UNABLE_TO_CONNECT);
        break;
    case Network::RoomMember::Error::PermissionDenied:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::PERMISSION_DENIED);
        break;
    case Network::RoomMember::Error::NoSuchUser:
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::NO_SUCH_USER);
        break;
    }
}

void MultiplayerState::OnAnnounceFailed(const WebService::WebResult& result) {
    announce_multiplayer_session->Stop();
    QMessageBox::warning(this, tr("Error"),
                         tr("Failed to update the room information. Please check your Internet "
                            "connection and try hosting the room again.\nDebug Message: ") +
                             QString::fromStdString(result.result_string),
                         QMessageBox::Ok);
}

void MultiplayerState::OnSaveConfig() {
    emit SaveConfig();
}

void MultiplayerState::UpdateThemedIcons() {
    if (show_notification) {
        status_icon->setPixmap(
            QIcon::fromTheme(QStringLiteral("connected_notification")).pixmap(16));
    } else if (current_state == Network::RoomMember::State::Joined ||
               current_state == Network::RoomMember::State::Moderator) {

        status_icon->setPixmap(QIcon::fromTheme(QStringLiteral("connected")).pixmap(16));
    } else {
        status_icon->setPixmap(QIcon::fromTheme(QStringLiteral("disconnected")).pixmap(16));
    }
    if (client_room)
        client_room->UpdateIconDisplay();
}

static void BringWidgetToFront(QWidget* widget) {
    widget->show();
    widget->activateWindow();
    widget->raise();
}

void MultiplayerState::OnViewLobby() {
    if (lobby == nullptr) {
        lobby = new Lobby(this, game_list_model, announce_multiplayer_session, system);
        connect(lobby, &Lobby::SaveConfig, this, &MultiplayerState::OnSaveConfig);
    }
    lobby->RefreshLobby();
    BringWidgetToFront(lobby);
}

void MultiplayerState::OnCreateRoom() {
    if (host_room == nullptr) {
        host_room = new HostRoomWindow(this, game_list_model, announce_multiplayer_session, system);
        connect(host_room, &HostRoomWindow::SaveConfig, this, &MultiplayerState::OnSaveConfig);
    }
    BringWidgetToFront(host_room);
}

bool MultiplayerState::OnCloseRoom() {
    if (!NetworkMessage::WarnCloseRoom())
        return false;
    if (auto room = room_network.GetRoom().lock()) {
        // if you are in a room, leave it
        if (auto member = room_network.GetRoomMember().lock()) {
            member->Leave();
            LOG_DEBUG(Frontend, "Left the room (as a client)");
        }

        // if you are hosting a room, also stop hosting
        if (room->GetState() != Network::Room::State::Open) {
            return true;
        }
        // Save ban list
        UISettings::values.multiplayer_ban_list = room->GetBanList();

        room->Destroy();
        announce_multiplayer_session->Stop();
        LOG_DEBUG(Frontend, "Closed the room (as a server)");
    }
    return true;
}

void MultiplayerState::ShowNotification() {
    if (client_room && client_room->isAncestorOf(QApplication::focusWidget()))
        return; // Do not show notification if the chat window currently has focus
    show_notification = true;
    QApplication::alert(nullptr);
    QApplication::beep();
    SetNotificationStatus(NotificationStatus::Notification);
}

void MultiplayerState::HideNotification() {
    show_notification = false;
    SetNotificationStatus(NotificationStatus::Connected);
}

void MultiplayerState::OnOpenNetworkRoom() {
    if (auto member = room_network.GetRoomMember().lock()) {
        if (member->IsConnected()) {
            if (client_room == nullptr) {
                client_room = new ClientRoomWindow(this, room_network);
                connect(client_room, &ClientRoomWindow::ShowNotification, this,
                        &MultiplayerState::ShowNotification);
            }
            BringWidgetToFront(client_room);
            return;
        }
    }
    // If the user is not a member of a room, show the lobby instead.
    // This is currently only used on the clickable label in the status bar
    OnViewLobby();
}

void MultiplayerState::OnDirectConnectToRoom() {
    if (direct_connect == nullptr) {
        direct_connect = new DirectConnectWindow(system, this);
        connect(direct_connect, &DirectConnectWindow::SaveConfig, this,
                &MultiplayerState::OnSaveConfig);
    }
    BringWidgetToFront(direct_connect);
}

bool MultiplayerState::IsHostingPublicRoom() const {
    return announce_multiplayer_session->IsRunning();
}

void MultiplayerState::UpdateCredentials() {
    announce_multiplayer_session->UpdateCredentials();
}

void MultiplayerState::UpdateGameList(QStandardItemModel* game_list) {
    game_list_model = game_list;
    if (lobby) {
        lobby->UpdateGameList(game_list);
    }
    if (host_room) {
        host_room->UpdateGameList(game_list);
    }
}
