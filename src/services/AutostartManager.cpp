#include "services/AutostartManager.h"
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <string>

static std::wstring startupLnkPath() {
    PWSTR dir = nullptr;
    std::wstring path;
    if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &dir)) && dir) {
        path = dir; path += L"\\open_windows_note.lnk";
    }
    if (dir) ::CoTaskMemFree(dir);
    return path;
}
static std::wstring exePath() {
    wchar_t buf[MAX_PATH]; DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::wstring(buf, n);
}
namespace own_svc {
bool autostartIsEnabled() {
    std::wstring lnk = startupLnkPath();
    if (lnk.empty()) return false;
    DWORD a = ::GetFileAttributesW(lnk.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
bool autostartSetEnabled(bool on) {
    std::wstring lnk = startupLnkPath();
    if (lnk.empty()) return false;
    if (!on) {
        ::DeleteFileW(lnk.c_str());
        return !autostartIsEnabled();
    }
    HRESULT hrInit = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool ok = false;
    IShellLinkW* link = nullptr;
    if (SUCCEEDED(::CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_IShellLinkW, (void**)&link)) && link) {
        std::wstring exe = exePath();
        std::wstring dir = exe.substr(0, exe.find_last_of(L"\\/"));
        link->SetPath(exe.c_str());
        link->SetWorkingDirectory(dir.c_str());
        IPersistFile* pf = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, (void**)&pf)) && pf) {
            ok = SUCCEEDED(pf->Save(lnk.c_str(), TRUE));
            pf->Release();
        }
        link->Release();
    }
    if (hrInit == S_OK || hrInit == S_FALSE) ::CoUninitialize();
    return ok;
}
}
