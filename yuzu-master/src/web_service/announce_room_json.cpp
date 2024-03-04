// SPDX-FileCopyrightText: Copyright 2017 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <future>
#include <nlohmann/json.hpp>
#include "common/detached_tasks.h"
#include "common/logging/log.h"
#include "web_service/announce_room_json.h"
#include "web_service/web_backend.h"

namespace AnnounceMultiplayerRoom {

static void to_json(nlohmann::json& json, const Member& member) {
    if (!member.username.empty()) {
        json["username"] = member.username;
    }
    json["nickname"] = member.nickname;
    if (!member.avatar_url.empty()) {
        json["avatarUrl"] = member.avatar_url;
    }
    json["gameName"] = member.game.name;
    json["gameId"] = member.game.id;
}

static void from_json(const nlohmann::json& json, Member& member) {
    member.nickname = json.at("nickname").get<std::string>();
    member.game.name = json.at("gameName").get<std::string>();
    member.game.id = json.at("gameId").get<u64>();
    try {
        member.username = json.at("username").get<std::string>();
        member.avatar_url = json.at("avatarUrl").get<std::string>();
    } catch (const nlohmann::detail::out_of_range&) {
        member.username = member.avatar_url = "";
        LOG_DEBUG(Network, "Member \'{}\' isn't authenticated", member.nickname);
    }
}

static void to_json(nlohmann::json& json, const Room& room) {
    json["port"] = room.information.port;
    json["name"] = room.information.name;
    if (!room.information.description.empty()) {
        json["description"] = room.information.description;
    }
    json["preferredGameName"] = room.information.preferred_game.name;
    json["preferredGameId"] = room.information.preferred_game.id;
    json["maxPlayers"] = room.information.member_slots;
    json["netVersion"] = room.net_version;
    json["hasPassword"] = room.has_password;
    if (room.members.size() > 0) {
        nlohmann::json member_json = room.members;
        json["players"] = member_json;
    }
}

static void from_json(const nlohmann::json& json, Room& room) {
    room.verify_uid = json.at("externalGuid").get<std::string>();
    room.ip = json.at("address").get<std::string>();
    room.information.name = json.at("name").get<std::string>();
    try {
        room.information.description = json.at("description").get<std::string>();
    } catch (const nlohmann::detail::out_of_range&) {
        room.information.description = "";
        LOG_DEBUG(Network, "Room \'{}\' doesn't contain a description", room.information.name);
    }
    room.information.host_username = json.at("owner").get<std::string>();
    room.information.port = json.at("port").get<u16>();
    room.information.preferred_game.name = json.at("preferredGameName").get<std::string>();
    room.information.preferred_game.id = json.at("preferredGameId").get<u64>();
    room.information.member_slots = json.at("maxPlayers").get<u32>();
    room.net_version = json.at("netVersion").get<u32>();
    room.has_password = json.at("hasPassword").get<bool>();
    try {
        room.members = json.at("players").get<std::vector<Member>>();
    } catch (const nlohmann::detail::out_of_range& e) {
        LOG_DEBUG(Network, "Out of range {}", e.what());
    }
}

} // namespace AnnounceMultiplayerRoom

namespace WebService {

void RoomJson::SetRoomInformation(const std::string& name, const std::string& description,
                                  const u16 port, const u32 max_player, const u32 net_version,
                                  const bool has_password,
                                  const AnnounceMultiplayerRoom::GameInfo& preferred_game) {
    room.information.name = name;
    room.information.description = description;
    room.information.port = port;
    room.information.member_slots = max_player;
    room.net_version = net_version;
    room.has_password = has_password;
    room.information.preferred_game = preferred_game;
}
void RoomJson::AddPlayer(const AnnounceMultiplayerRoom::Member& member) {
    room.members.push_back(member);
}

WebService::WebResult RoomJson::Update() {
    if (room_id.empty()) {
        LOG_ERROR(WebService, "Room must be registered to be updated");
        return WebService::WebResult{WebService::WebResult::Code::LibError,
                                     "Room is not registered", ""};
    }
    nlohmann::json json{{"players", room.members}};
    return client.PostJson(fmt::format("/lobby/{}", room_id), json.dump(), false);
}

WebService::WebResult RoomJson::Register() {
    nlohmann::json json = room;
    auto result = client.PostJson("/lobby", json.dump(), false);
    if (result.result_code != WebService::WebResult::Code::Success) {
        return result;
    }
    auto reply_json = nlohmann::json::parse(result.returned_data);
    room = reply_json.get<AnnounceMultiplayerRoom::Room>();
    room_id = reply_json.at("id").get<std::string>();
    return WebService::WebResult{WebService::WebResult::Code::Success, "", room.verify_uid};
}

void RoomJson::ClearPlayers() {
    room.members.clear();
}

AnnounceMultiplayerRoom::RoomList RoomJson::GetRoomList() {
    auto reply = client.GetJson("/lobby", true).returned_data;
    if (reply.empty()) {
        return {};
    }
    return nlohmann::json::parse(reply).at("rooms").get<AnnounceMultiplayerRoom::RoomList>();
}

void RoomJson::Delete() {
    if (room_id.empty()) {
        LOG_ERROR(WebService, "Room must be registered to be deleted");
        return;
    }
    Common::DetachedTasks::AddTask([host_{this->host}, username_{this->username},
                                    token_{this->token}, room_id_{this->room_id}]() {
        // create a new client here because the this->client might be destroyed.
        Client{host_, username_, token_}.DeleteJson(fmt::format("/lobby/{}", room_id_), "", false);
    });
}

} // namespace WebService
