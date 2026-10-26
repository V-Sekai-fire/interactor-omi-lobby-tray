# interactor-omi-lobby-tray

**Join an Overte or Basis server from the taskbar.**

Who is online, and where do you go? A Windows tray icon that lists the servers on an OMI Lobby, so you can join an Overte domain or a Basis server from the taskbar. It is the desktop companion to [service-omi-lobby](https://github.com/V-Sekai-fire/service-omi-lobby).

## What it does

The tray reads the lobby's list from `GET /api/v1/lobby/servers` at start, every 10 minutes after that (the lobby polls its servers at the same rate), and when you pick **Refresh now**. The menu shows:

- How many servers are up and how many players are online. The icon changes when anyone is online.
- **Overte**: each domain that is up, as name and players (`Hytrion (0/12)`). Picking one opens `hifi://<address>`, so Interface starts in that domain. Until `hifi://` links are set up, it copies the address instead and says so.
- **Basis**: each Basis server that is up. Basis has no link that opens a server, so picking one copies its address (`server1.basisvr.org:4296`) to the clipboard to paste into Basis.
- **Set up Overte links...**: registers `hifi://` for your account (`HKCU\Software\Classes\hifi`, no administrator prompt) with the entries Overte's installer writes, which it does only in production builds. It uses the Overte install it finds under Program Files or your Programs folder, and asks you to pick `interface.exe` when it finds none. Once links work, the same entry lets you pick another Interface.
- **Open the lobby page** and **Quit**.

When a refresh fails, the menu keeps the last list and the status line says why. A second start leaves the first icon in charge.

## Try it

Unzip `omi-lobby-tray-windows-x64.zip` anywhere and run `omi-lobby-tray.exe`.

## Which lobby

In order: `--lobby <URL>` on the command line, then `{"lobby": "<URL>"}` in a `lobby-tray.json` beside the program, then the staging lobby at `https://omi-lobby.stonecat-ratio.ts.net`. The URL is the lobby's base address; the tray adds `/api/v1/lobby/servers`.

The lobby's API must be reachable from your machine. The staging lobby serves it publicly. The Fly deploy of the lobby keeps its API private, so the tray cannot read that one until it gets a public list route.

`omi-lobby-tray --list` prints the servers the menu would show, one per line (`platform`, label, join link or address), and exits.

## Build and run

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

CMake fetches SDL3 (release-3.4.8, the tray library XR Pilot uses) and links it statically, so the program is one file plus `resources/`. `-DOMI_LOBBY_TRAY_APP=OFF` builds only the list logic and its tests, with no SDL. On Windows the tray fetches with WinHTTP; other systems, used for development only, run `curl`.

CI builds on Windows, runs the tests and keeps `omi-lobby-tray-windows-x64.zip` as an artifact of every run; a `v*` tag attaches it to the release.

## Where it comes from

The tray is extracted from [interactor-xr-pilot](https://github.com/V-Sekai-fire/interactor-xr-pilot)'s OXRSys tray: the same SDL3 tray, the same icons, and its JSON reader (`src/Json.*`). The OpenXR runtime parts stay in XR Pilot.

## Licence

Apache-2.0 OR MIT (`LICENSE-APACHE`, `LICENSE-MIT`). The icons in `resources/` come from XR Pilot's tray, which is MPL-2.0 like the OXRSys tray it was ported from.
