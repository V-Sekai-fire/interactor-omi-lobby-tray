// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// The lobby's server list as service-omi-lobby serves it at GET /api/v1/lobby/servers, and what the
// tray shows and opens for each server. No SDL and no network, so the tests run anywhere.

#pragma once

#include <string>
#include <vector>

namespace lobbytray
{

struct Server
{
    std::string name;
    std::string status; // "up" when the last poll reached it
    std::string address; // host:port
    std::string platform; // "overte" or "basis"
    std::string motd;
    std::string clientUrl; // where to get the platform's client
    long long online = 0;
    long long capacity = 0; // 0 when the server does not say
};

// The lobby the tray reads when no --lobby or lobby-tray.json names another.
constexpr const char* DefaultLobby = "https://omi-lobby.stonecat-ratio.ts.net";

// The list endpoint under a lobby's base address; a trailing slash on the base is dropped.
std::string serversUrl(std::string lobby);

// Parses the endpoint's body; false with error when it is not the expected shape.
bool parseServers(const std::string& text, std::vector<Server>& out, std::string* error = nullptr);

// The servers on one platform that are up, most players first, then by name.
std::vector<Server> joinable(const std::vector<Server>& servers, const std::string& platform);

// The menu text for a server: its name and players, as "Name (1/5000)" or "Name (3)" with no capacity.
std::string label(const Server& server);

// The address that opens the server in its client, empty when the platform has none. Overte's
// installer registers hifi://, so hifi://host:port starts Interface in that domain. Basis has no
// such link today, so the tray copies its address instead.
std::string joinUrl(const Server& server);

// The lobby base from the command line (--lobby URL) or from the text of lobby-tray.json
// ({"lobby": "URL"}); configText is empty when the file is absent. Falls back to DefaultLobby.
std::string chooseLobby(const std::vector<std::string>& args, const std::string& configText);

} // namespace lobbytray
