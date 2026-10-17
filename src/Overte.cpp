// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "Overte.h"

#include <algorithm>
#include <system_error>

namespace lobbytray
{

std::string hifiOpenCommand(const std::string& interfaceExe)
{
    return "\"" + interfaceExe + "\" --url \"%1\"";
}

std::vector<std::filesystem::path> findInterface(const std::vector<std::filesystem::path>& roots)
{
    namespace fs = std::filesystem;
    std::vector<fs::path> found;
    for (const fs::path& root : roots)
    {
        std::error_code ec;
        for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
        {
            const std::string name = it->path().filename().string();
            if (name.rfind("Overte", 0) != 0 || !it->is_directory(ec))
                continue;
            const fs::path exe = it->path() / "interface.exe";
            if (fs::is_regular_file(exe, ec))
                found.push_back(exe);
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

} // namespace lobbytray
