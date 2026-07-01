#include "app/NoteApp.h"
#include "app/AppPaths.h"
#include "app/DbBootstrap.h"
#include "data/NoteStore.h"
#include <gdiplus.h>
#include <string>

CNoteApp theApp;   // the one and only application object; MFC supplies WinMain

BOOL CNoteApp::InitInstance() {
    CWinApp::InitInstance();

    std::string path = own::resolveDbPathWin();
    std::string err;
    if (!own::openDatabaseAtPath(path, m_db, &err)) {
        CStringA msg = ("\xE6\x97\xA0\xE6\xB3\x95\xE6\x89\x93\xE5\xBC\x80\xE6\x95\xB0\xE6\x8D\xAE\xE5\xBA\x93:\n"  // 无法打开数据库
                        + path + "\n" + err).c_str();
        AfxMessageBox(CString(msg));
        return FALSE;
    }

    Gdiplus::GdiplusStartupInput gsi;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gsi, nullptr);

    if (!m_host.Create())
        return FALSE;
    m_pMainWnd = &m_host;   // hidden host keeps the message loop alive

    // 显示所有 visible=1 的 note
    m_store = std::make_unique<own::NoteStore>(m_db);
    own::NoteQuery q; q.onlyVisible = true;
    for (const auto& n : m_store->query(q)) {
        auto w = std::make_unique<CNoteWindow>();
        if (w->Create(n, m_store.get()))
            m_notes.push_back(std::move(w));
    }
    return TRUE;
}

int CNoteApp::ExitInstance() {
    m_notes.clear();
    if (m_gdiplusToken) Gdiplus::GdiplusShutdown(m_gdiplusToken);
    return CWinApp::ExitInstance();
}
