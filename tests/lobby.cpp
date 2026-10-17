// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// The tray's list logic against servers.json, an answer captured from the lobby's
// GET /api/v1/lobby/servers. One test per name, so CTest reports each.

#include "Lobby.h"
#include "Overte.h"

#include <filesystem>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

using namespace lobbytray;

namespace
{

int failures = 0;

void check(bool ok, const char* what)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAILED: %s\n", what);
        ++failures;
    }
}

std::string fixture()
{
    std::ifstream file(LOBBY_FIXTURE, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

void parsesTheLobbyAnswer()
{
    std::vector<Server> servers;
    check(parseServers(fixture(), servers), "the captured answer parses");
    check(servers.size() == 17, "all 17 captured servers are read");
    const auto basis = joinable(servers, "basis");
    check(basis.size() == 1, "one Basis server");
    check(!basis.empty() && basis[0].address == "server1.basisvr.org:4296", "Basis address read");
    check(!basis.empty() && basis[0].capacity == 5000, "Basis capacity read as a number");
    check(joinable(servers, "overte").size() == 16, "sixteen Overte domains");
}

void refusesWhatIsNotAList()
{
    std::vector<Server> servers;
    std::string error;
    check(!parseServers("<html>502</html>", servers, &error), "an HTML error page is refused");
    check(!error.empty(), "the refusal says why");
    check(!parseServers("{\"servers\": 3}", servers, &error), "a list that is not an array is refused");
}

void downAndOtherPlatformsAreLeftOut()
{
    std::vector<Server> servers;
    parseServers(R"({"servers":[
        {"name":"B","status":"up","address":"b:1","platform":"overte","online":0},
        {"name":"A","status":"up","address":"a:1","platform":"overte","online":0},
        {"name":"Busy","status":"up","address":"c:1","platform":"overte","online":4},
        {"name":"Gone","status":"down","address":"d:1","platform":"overte","online":0},
        {"name":"Elsewhere","status":"up","address":"e:1","platform":"basis","online":9}]})",
                 servers);
    const auto overte = joinable(servers, "overte");
    check(overte.size() == 3, "a down domain and other platforms are left out");
    check(overte.size() == 3 && overte[0].name == "Busy" && overte[1].name == "A" && overte[2].name == "B",
          "most players first, then by name");
}

void labelsShowPlayers()
{
    Server s;
    s.name = "main basis server";
    s.online = 1;
    s.capacity = 5000;
    check(label(s) == "main basis server (1/5000)", "players out of capacity");
    s.capacity = 0;
    check(label(s) == "main basis server (1)", "players alone when there is no capacity");
    s.name.clear();
    s.address = "h:1";
    check(label(s) == "h:1 (1)", "the address stands in for a missing name");
}

void joinLinks()
{
    Server overte;
    overte.platform = "overte";
    overte.address = "143.110.249.54:40102";
    check(joinUrl(overte) == "hifi://143.110.249.54:40102", "Overte joins through hifi://");
    Server basis;
    basis.platform = "basis";
    basis.address = "server1.basisvr.org:4296";
    check(joinUrl(basis).empty(), "Basis has no link, so the address is copied");
}

void lobbyChoice()
{
    check(serversUrl("https://lobby.example/") == "https://lobby.example/api/v1/lobby/servers", "trailing slash dropped");
    check(chooseLobby({}, "") == DefaultLobby, "no flag and no file: the default lobby");
    check(chooseLobby({}, R"({"lobby":"https://a.example"})") == "https://a.example", "the file names a lobby");
    check(chooseLobby({"--lobby", "https://b.example"}, R"({"lobby":"https://a.example"})") == "https://b.example",
          "the flag wins over the file");
    check(chooseLobby({}, "not json") == DefaultLobby, "an unreadable file falls back to the default");
}

void hifiCommand()
{
    check(hifiOpenCommand(R"(C:\Program Files\Overte\interface.exe)") ==
              R"("C:\Program Files\Overte\interface.exe" --url "%1")",
          "the installer's open command, with the path quoted");
}

void findsInterface()
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "omi-lobby-tray-find";
    fs::remove_all(root);
    const auto touch = [](const fs::path& file) {
        fs::create_directories(file.parent_path());
        std::ofstream(file) << "x";
    };
    touch(root / "a" / "Overte" / "interface.exe");
    touch(root / "b" / "Overte Beta" / "interface.exe");
    touch(root / "a" / "NotOverte" / "interface.exe");
    fs::create_directories(root / "a" / "Overte Empty");
    const auto found = findInterface({root / "a", root / "b", root / "missing"});
    check(found.size() == 2, "both Overte folders with Interface are found, and nothing else");
    check(found.size() == 2 && found[0] == root / "a" / "Overte" / "interface.exe" &&
              found[1] == root / "b" / "Overte Beta" / "interface.exe",
          "found in path order");
    check(findInterface({root / "missing"}).empty(), "a missing folder finds nothing");
    fs::remove_all(root);
}

} // namespace

int main(int argc, char** argv)
{
    const struct
    {
        const char* name;
        void (*run)();
    } tests[] = {
        {"lobby.parses-the-lobby-answer", parsesTheLobbyAnswer},
        {"lobby.refuses-what-is-not-a-list", refusesWhatIsNotAList},
        {"lobby.down-and-other-platforms-left-out", downAndOtherPlatformsAreLeftOut},
        {"lobby.labels-show-players", labelsShowPlayers},
        {"lobby.join-links", joinLinks},
        {"lobby.lobby-choice", lobbyChoice},
        {"overte.hifi-command", hifiCommand},
        {"overte.finds-interface", findsInterface},
    };
    bool ran = false;
    for (const auto& t : tests)
        if (argc < 2 || std::strcmp(argv[1], t.name) == 0)
        {
            t.run();
            ran = true;
        }
    if (!ran)
    {
        std::fprintf(stderr, "no test named %s\n", argv[1]);
        return 2;
    }
    return failures == 0 ? 0 : 1;
}
