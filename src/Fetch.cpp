// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "Fetch.h"

#include <iterator>

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#else
#include <cstdio>
#endif

namespace lobbytray
{

#if defined(_WIN32)

namespace
{

std::wstring wide(const std::string& s)
{
    if (s.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring out(size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), out.data(), n);
    return out;
}

struct Handle
{
    HINTERNET h = nullptr;
    ~Handle()
    {
        if (h != nullptr)
            WinHttpCloseHandle(h);
    }
};

} // namespace

bool fetchText(const std::string& url, std::string& body, std::string& error)
{
    body.clear();
    const std::wstring wurl = wide(url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = DWORD(std::size(host));
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = DWORD(std::size(path));
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts))
    {
        error = "not a web address: " + url;
        return false;
    }
    Handle session{WinHttpOpen(L"omi-lobby-tray", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0)};
    if (session.h == nullptr)
    {
        error = "WinHTTP did not start";
        return false;
    }
    WinHttpSetTimeouts(session.h, 10000, 10000, 15000, 15000);
    Handle connection{WinHttpConnect(session.h, host, parts.nPort, 0)};
    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    Handle request{connection.h != nullptr ? WinHttpOpenRequest(connection.h, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                                                WINHTTP_DEFAULT_ACCEPT_TYPES, flags)
                                           : nullptr};
    if (request.h == nullptr || !WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.h, nullptr))
    {
        error = "could not reach the lobby (WinHTTP error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    DWORD status = 0;
    DWORD size = sizeof(status);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &size, WINHTTP_NO_HEADER_INDEX);
    for (;;)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.h, &available) || available == 0)
            break;
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request.h, chunk.data(), available, &read) || read == 0)
            break;
        body.append(chunk.data(), read);
    }
    if (status != 200)
    {
        error = "the lobby answered HTTP " + std::to_string(status);
        return false;
    }
    return true;
}

#else

bool fetchText(const std::string& url, std::string& body, std::string& error)
{
    body.clear();
    for (char c : url)
        if (c == '\'' || c == '\\')
        {
            error = "refusing an address with quotes: " + url;
            return false;
        }
    FILE* pipe = popen(("curl -fsS --max-time 20 '" + url + "'").c_str(), "r");
    if (pipe == nullptr)
    {
        error = "could not run curl";
        return false;
    }
    char buffer[4096];
    size_t n = 0;
    while ((n = fread(buffer, 1, sizeof(buffer), pipe)) > 0)
        body.append(buffer, n);
    if (pclose(pipe) != 0)
    {
        error = "could not reach the lobby (curl failed)";
        return false;
    }
    return true;
}

#endif

} // namespace lobbytray
