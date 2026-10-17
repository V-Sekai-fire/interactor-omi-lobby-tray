// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// OMI Lobby Tray: a tray icon listing the servers on an OMI Lobby (service-omi-lobby), so a player can
// join an Overte domain or a Basis server from the taskbar. The tray itself is SDL3's, as in
// interactor-xr-pilot's OXRSys tray, whose icons it also uses.

#include "Fetch.h"
#include "Lobby.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <atomic>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

using namespace lobbytray;

namespace
{

// The lobby polls its servers every ten minutes, so asking it more often shows nothing new.
constexpr Uint64 RefreshMs = 10 * 60 * 1000;

struct Platform
{
    const char* key;
    const char* title;
    const char* none;
};

constexpr Platform Platforms[] = {
    {"overte", "Overte", "No Overte domains up"},
    {"basis", "Basis", "No Basis servers up"},
};

std::string readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

std::string besideProgram(const char* name)
{
    const char* base = SDL_GetBasePath();
    return std::string(base != nullptr ? base : "") + name;
}

// A fetch runs on its own thread and leaves its result here for the main thread, which owns the tray.
struct Fetched
{
    std::mutex mutex;
    bool running = false;
    bool done = false;
    std::vector<Server> servers;
    std::string error;
};

void startFetch(const std::shared_ptr<Fetched>& fetched, const std::string& url)
{
    {
        std::lock_guard<std::mutex> lock(fetched->mutex);
        if (fetched->running)
            return;
        fetched->running = true;
    }
    std::thread([fetched, url] {
        std::string body;
        std::string error;
        std::vector<Server> servers;
        if (fetchText(url, body, error))
            parseServers(body, servers, &error);
        std::lock_guard<std::mutex> lock(fetched->mutex);
        fetched->servers = std::move(servers);
        fetched->error = std::move(error);
        fetched->running = false;
        fetched->done = true;
    }).detach();
}

class LobbyTray;

struct Target
{
    LobbyTray* app;
    int command; // one of Command, or Join with server set
    Server server;
};

enum Command
{
    Join,
    Refresh,
    OpenPage,
    Quit,
};

class LobbyTray
{
public:
    explicit LobbyTray(std::string lobby)
        : lobby_(std::move(lobby))
        , fetched_(std::make_shared<Fetched>())
    {
        idleIcon_ = SDL_LoadPNG(besideProgram("resources/tray_idle.png").c_str());
        busyIcon_ = SDL_LoadPNG(besideProgram("resources/tray_streaming.png").c_str());
        tray_ = SDL_CreateTray(idleIcon_, "OMI Lobby");
        if (tray_ == nullptr)
            return;
        SDL_TrayMenu* menu = SDL_CreateTrayMenu(tray_);
        status_ = SDL_InsertTrayEntryAt(menu, -1, "Asking the lobby...", SDL_TRAYENTRY_BUTTON | SDL_TRAYENTRY_DISABLED);
        message_ = SDL_InsertTrayEntryAt(menu, -1, "", SDL_TRAYENTRY_BUTTON | SDL_TRAYENTRY_DISABLED);
        SDL_InsertTrayEntryAt(menu, -1, nullptr, 0);
        for (size_t i = 0; i < std::size(Platforms); ++i)
        {
            platformEntries_[i] = SDL_InsertTrayEntryAt(menu, -1, Platforms[i].title, SDL_TRAYENTRY_SUBMENU);
            SDL_CreateTraySubmenu(platformEntries_[i]);
        }
        SDL_InsertTrayEntryAt(menu, -1, nullptr, 0);
        addCommand(menu, "Refresh now", Refresh);
        addCommand(menu, "Open the lobby page", OpenPage);
        SDL_InsertTrayEntryAt(menu, -1, nullptr, 0);
        addCommand(menu, "Quit", Quit);
        refresh();
    }

    ~LobbyTray()
    {
        if (tray_ != nullptr)
            SDL_DestroyTray(tray_);
        SDL_DestroySurface(idleIcon_);
        SDL_DestroySurface(busyIcon_);
    }

    bool ok() const { return tray_ != nullptr; }
    bool quitting() const { return quit_; }

    // Shows a finished fetch, and asks again once the lobby has polled again.
    void poll()
    {
        if (SDL_GetTicks() - lastFetchMs_ >= RefreshMs)
            refresh();
        std::vector<Server> servers;
        std::string error;
        {
            std::lock_guard<std::mutex> lock(fetched_->mutex);
            if (!fetched_->done)
                return;
            fetched_->done = false;
            servers = std::move(fetched_->servers);
            error = std::move(fetched_->error);
        }
        show(servers, error);
    }

private:
    void addCommand(SDL_TrayMenu* menu, const char* text, Command command)
    {
        SDL_TrayEntry* entry = SDL_InsertTrayEntryAt(menu, -1, text, SDL_TRAYENTRY_BUTTON);
        targets_.push_back(std::make_unique<Target>(Target{this, command, {}}));
        SDL_SetTrayEntryCallback(entry, &LobbyTray::onEntry, targets_.back().get());
    }

    void refresh()
    {
        lastFetchMs_ = SDL_GetTicks();
        startFetch(fetched_, serversUrl(lobby_));
    }

    void show(const std::vector<Server>& servers, const std::string& error)
    {
        if (!error.empty())
        {
            // Keep the last good list: a missed poll should not empty the menu.
            SDL_SetTrayEntryLabel(status_, ("Lobby unreachable: " + error).c_str());
            return;
        }
        joinTargets_.clear();
        long long players = 0;
        size_t listed = 0;
        for (size_t i = 0; i < std::size(Platforms); ++i)
        {
            SDL_TrayMenu* submenu = SDL_GetTraySubmenu(platformEntries_[i]);
            int count = 0;
            const SDL_TrayEntry** old = SDL_GetTrayEntries(submenu, &count);
            std::vector<SDL_TrayEntry*> stale;
            for (int k = 0; k < count; ++k)
                stale.push_back(const_cast<SDL_TrayEntry*>(old[k]));
            for (SDL_TrayEntry* entry : stale)
                SDL_RemoveTrayEntry(entry);

            const std::vector<Server> up = joinable(servers, Platforms[i].key);
            const std::string title = std::string(Platforms[i].title) + " (" + std::to_string(up.size()) + ")";
            SDL_SetTrayEntryLabel(platformEntries_[i], title.c_str());
            if (up.empty())
                SDL_InsertTrayEntryAt(submenu, -1, Platforms[i].none, SDL_TRAYENTRY_BUTTON | SDL_TRAYENTRY_DISABLED);
            for (const Server& s : up)
            {
                SDL_TrayEntry* entry = SDL_InsertTrayEntryAt(submenu, -1, label(s).c_str(), SDL_TRAYENTRY_BUTTON);
                joinTargets_.push_back(std::make_unique<Target>(Target{this, Join, s}));
                SDL_SetTrayEntryCallback(entry, &LobbyTray::onEntry, joinTargets_.back().get());
                players += s.online;
            }
            listed += up.size();
        }
        const std::string summary = std::to_string(listed) + (listed == 1 ? " server up, " : " servers up, ") +
                                    std::to_string(players) + (players == 1 ? " player online" : " players online");
        SDL_SetTrayEntryLabel(status_, summary.c_str());
        SDL_SetTrayTooltip(tray_, ("OMI Lobby: " + summary).c_str());
        SDL_SetTrayIcon(tray_, players > 0 && busyIcon_ != nullptr ? busyIcon_ : idleIcon_);
    }

    void join(const Server& s)
    {
        const std::string url = joinUrl(s);
        if (!url.empty() && SDL_OpenURL(url.c_str()))
        {
            SDL_SetTrayEntryLabel(message_, ("Opened " + url).c_str());
            return;
        }
        SDL_SetClipboardText(s.address.c_str());
        const std::string client = s.platform == "overte" ? "Overte" : s.platform == "basis" ? "Basis" : s.platform;
        const std::string why = url.empty() ? "" : client + " did not open: ";
        SDL_SetTrayEntryLabel(message_, (why + "copied " + s.address + "; paste it into " + client).c_str());
    }

    static void onEntry(void* userdata, SDL_TrayEntry*)
    {
        Target* target = static_cast<Target*>(userdata);
        LobbyTray* app = target->app;
        switch (target->command)
        {
        case Join:
            app->join(target->server);
            break;
        case Refresh:
            app->refresh();
            break;
        case OpenPage:
            SDL_OpenURL(app->lobby_.c_str());
            break;
        case Quit:
            app->quit_ = true;
            break;
        }
    }

    std::string lobby_;
    std::shared_ptr<Fetched> fetched_;
    SDL_Tray* tray_ = nullptr;
    SDL_TrayEntry* status_ = nullptr;
    SDL_TrayEntry* message_ = nullptr;
    SDL_TrayEntry* platformEntries_[std::size(Platforms)] = {};
    SDL_Surface* idleIcon_ = nullptr;
    SDL_Surface* busyIcon_ = nullptr;
    std::vector<std::unique_ptr<Target>> targets_;
    std::vector<std::unique_ptr<Target>> joinTargets_;
    Uint64 lastFetchMs_ = 0;
    bool quit_ = false;
};

// --list: print what the menu would show, for scripts and for checking a lobby from a shell.
int list(const std::string& lobby)
{
#if defined(_WIN32)
    // A GUI program has no console of its own. Output redirected to a pipe or file already works;
    // otherwise borrow the shell's console.
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    const bool redirected = out != nullptr && out != INVALID_HANDLE_VALUE && GetFileType(out) != FILE_TYPE_UNKNOWN;
    if (!redirected && AttachConsole(ATTACH_PARENT_PROCESS))
    {
        std::freopen("CONOUT$", "w", stdout);
        std::freopen("CONOUT$", "w", stderr);
    }
#endif
    std::string body;
    std::string error;
    std::vector<Server> servers;
    if (!fetchText(serversUrl(lobby), body, error) || !parseServers(body, servers, &error))
    {
        std::fprintf(stderr, "omi-lobby-tray: %s\n", error.c_str());
        return 1;
    }
    for (const Platform& p : Platforms)
        for (const Server& s : joinable(servers, p.key))
        {
            const std::string url = joinUrl(s);
            std::printf("%s\t%s\t%s\n", p.key, label(s).c_str(), url.empty() ? s.address.c_str() : url.c_str());
        }
    std::fflush(stdout);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);
    bool listOnly = false;
    for (const std::string& a : args)
    {
        if (a == "--help" || a == "-h")
        {
            std::printf("omi-lobby-tray [--lobby URL] [--list]\n"
                        "Lists an OMI Lobby's servers in a tray icon. --lobby picks the lobby (else lobby-tray.json\n"
                        "beside the program, else %s); --list prints the servers and exits.\n",
                        DefaultLobby);
            return 0;
        }
        listOnly = listOnly || a == "--list";
    }
    if (!SDL_Init(listOnly ? 0 : SDL_INIT_VIDEO))
    {
        std::fprintf(stderr, "omi-lobby-tray: %s\n", SDL_GetError());
        return 1;
    }
    const std::string lobby = chooseLobby(args, readFile(besideProgram("lobby-tray.json")));
    if (listOnly)
    {
        const int rc = list(lobby);
        SDL_Quit();
        return rc;
    }
#if defined(_WIN32)
    // One icon per desktop: a second start leaves the first in charge.
    CreateMutexW(nullptr, TRUE, L"Local\\V-Sekai-fire.omi-lobby-tray");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        SDL_Quit();
        return 0;
    }
#endif
    int rc = 0;
    {
        LobbyTray tray(lobby);
        if (!tray.ok())
        {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OMI Lobby", SDL_GetError(), nullptr);
            rc = 1;
        }
        while (rc == 0 && !tray.quitting())
        {
            SDL_Event e;
            if (SDL_WaitEventTimeout(&e, 500) && e.type == SDL_EVENT_QUIT)
                break;
            tray.poll();
        }
    }
    SDL_Quit();
    return rc;
}
