// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// One HTTPS GET. WinHTTP on Windows, so the tray needs no extra library there; curl elsewhere, for
// development builds.

#pragma once

#include <string>

namespace lobbytray
{

// The body of a 200 answer; false with error for any failure or other status.
bool fetchText(const std::string& url, std::string& body, std::string& error);

} // namespace lobbytray
