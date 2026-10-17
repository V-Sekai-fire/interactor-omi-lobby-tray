// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Setting up hifi:// links for an Overte install. Overte's installer registers hifi:// only in its
// production builds, so the tray can register it per user instead. The registry writes are in main.cpp;
// what they write, and where Interface is looked for, live here so the tests can check them anywhere.

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace lobbytray
{

// The open command Overte's installer writes for hifi:// (cmake/templates/NSIS.template.in), with the
// program path quoted so a path with spaces works: "C:\...\interface.exe" --url "%1".
std::string hifiOpenCommand(const std::string& interfaceExe);

// Overte's interface.exe in each root's Overte* folders (as "Program Files\Overte"), sorted by path.
std::vector<std::filesystem::path> findInterface(const std::vector<std::filesystem::path>& roots);

} // namespace lobbytray
