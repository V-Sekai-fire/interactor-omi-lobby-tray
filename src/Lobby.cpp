// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "Lobby.h"

#include "Json.h"

#include <algorithm>
#include <cstdlib>

namespace lobbytray
{

namespace
{

std::string text(const Json& object, const char* key)
{
    const Json* value = object.find(key);
    return value != nullptr ? value->str() : std::string();
}

long long number(const Json& object, const char* key)
{
    const Json* value = object.find(key);
    if (value == nullptr || value->kind != Json::Kind::Number)
        return 0;
    return std::strtoll(value->text.c_str(), nullptr, 10);
}

} // namespace

std::string serversUrl(std::string lobby)
{
    while (!lobby.empty() && lobby.back() == '/')
        lobby.pop_back();
    return lobby + "/api/v1/lobby/servers";
}

bool parseServers(const std::string& body, std::vector<Server>& out, std::string* error)
{
    out.clear();
    Json root;
    if (!parseJson(body, root) || root.kind != Json::Kind::Object)
    {
        if (error != nullptr)
            *error = "the lobby did not answer with JSON";
        return false;
    }
    const Json* servers = root.find("servers");
    if (servers == nullptr || servers->kind != Json::Kind::Array)
    {
        if (error != nullptr)
            *error = "the lobby's answer has no server list";
        return false;
    }
    for (const Json& item : servers->items)
    {
        if (item.kind != Json::Kind::Object)
            continue;
        Server s;
        s.name = text(item, "name");
        s.status = text(item, "status");
        s.address = text(item, "address");
        s.platform = text(item, "platform");
        s.motd = text(item, "motd");
        s.clientUrl = text(item, "client_url");
        s.online = number(item, "online");
        s.capacity = number(item, "capacity");
        out.push_back(std::move(s));
    }
    return true;
}

std::vector<Server> joinable(const std::vector<Server>& servers, const std::string& platform)
{
    std::vector<Server> out;
    for (const Server& s : servers)
        if (s.platform == platform && s.status == "up" && !s.address.empty())
            out.push_back(s);
    std::stable_sort(out.begin(), out.end(), [](const Server& a, const Server& b) {
        return a.online != b.online ? a.online > b.online : a.name < b.name;
    });
    return out;
}

std::string label(const Server& server)
{
    const std::string name = server.name.empty() ? server.address : server.name;
    std::string players = std::to_string(server.online);
    if (server.capacity > 0)
        players += "/" + std::to_string(server.capacity);
    return name + " (" + players + ")";
}

std::string joinUrl(const Server& server)
{
    if (server.platform == "overte" && !server.address.empty())
        return "hifi://" + server.address;
    return {};
}

std::string chooseLobby(const std::vector<std::string>& args, const std::string& configText)
{
    for (size_t i = 0; i + 1 < args.size(); ++i)
        if (args[i] == "--lobby" && !args[i + 1].empty())
            return args[i + 1];
    Json config;
    if (!configText.empty() && parseJson(configText, config))
    {
        const Json* lobby = config.find("lobby");
        if (lobby != nullptr && !lobby->str().empty())
            return lobby->str();
    }
    return DefaultLobby;
}

} // namespace lobbytray
