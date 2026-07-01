#include "app/AppPaths.h"
#include <windows.h>
#include <shlobj.h>
#include <string>

namespace own {

static std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

static std::wstring exeDirW() {
    wchar_t buf[MAX_PATH];
    ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    size_t slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : p.substr(0, slash);
}

static bool dirWritableW(const std::wstring& dir) {
    std::wstring probe = dir + L"\\.own_write_test.tmp";
    HANDLE h = ::CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    ::CloseHandle(h);
    return true;
}

std::string resolveDbPathWin() {
    std::wstring exe = exeDirW();
    wchar_t appdata[MAX_PATH]{};
    ::SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata);
    auto choice = chooseDbPath(wideToUtf8(exe), wideToUtf8(std::wstring(appdata)), dirWritableW(exe));
    if (!choice.portable) {   // ensure the fallback directory exists
        std::wstring d = std::wstring(appdata) + L"\\open_windows_note";
        ::CreateDirectoryW(d.c_str(), nullptr);
    }
    return choice.path;
}

} // namespace own
