#include "app/NoteApp.h"
#include "app/AppPaths.h"
#include "app/DbBootstrap.h"
#include "data/NoteStore.h"
#include <gdiplus.h>
#include <string>
#include <ctime>

CNoteApp theApp;   // the one and only application object; MFC supplies WinMain

BOOL CNoteApp::InitInstance() {
    CWinApp::InitInstance();
    AfxInitRichEdit2();   // RichEdit20W 注册，供 CTextContentView 使用

    // 单实例：已有实例则通知其新建一条 note 后退出
    m_singleton = ::CreateMutex(nullptr, FALSE, _T("open_windows_note_singleton"));
    if (m_singleton && ::GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND h = ::FindWindow(nullptr, _T("OwnAppHost"));
        if (h) ::PostMessage(h, WM_HOTKEY, CAppHostWindow::kHotkeyNew, 0);
        return FALSE;
    }

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

    m_store = std::make_unique<own::NoteStore>(m_db);

    // 装配 host 回调：新建热键 → 插入一条 note 并弹窗；退出热键 → 结束消息循环
    m_host.onNewNote = [this]{
        own::Note n; n.type = own::NoteType::RichText; n.visible = true;
        n.plainText = "new note";
        int64_t id = m_store->insertNote(n, (int64_t)time(nullptr));
        auto full = m_store->getNote(id);
        if (full) createAndShowNote(*full);
    };
    m_host.onNewChecklist = [this]{
        own::Note n; n.type = own::NoteType::Checklist; n.visible = true;
        int64_t id = m_store->insertNote(n, (int64_t)time(nullptr));
        auto full = m_store->getNote(id);
        if (full) createAndShowNote(*full);
    };
    m_host.onQuit = []{ ::PostQuitMessage(0); };

    if (!m_host.Create())
        return FALSE;
    m_pMainWnd = &m_host;   // hidden host keeps the message loop alive

    // 显示已有可见 note；首启为空则建欢迎 note
    own::NoteQuery q; q.onlyVisible = true;
    auto notes = m_store->query(q);
    if (notes.empty()) {
        own::Note w; w.visible = true;
        w.plainText = "welcome - Ctrl+Alt+N new / Ctrl+Alt+Q quit";
        int64_t id = m_store->insertNote(w, (int64_t)time(nullptr));
        auto full = m_store->getNote(id);
        if (full) notes.push_back(*full);
    }
    for (const auto& n : notes) createAndShowNote(n);
    return TRUE;
}

void CNoteApp::createAndShowNote(const own::Note& seed) {
    auto w = std::make_unique<CNoteWindow>();
    if (w->Create(seed, m_store.get()))
        m_notes.push_back(std::move(w));
}

int CNoteApp::ExitInstance() {
    m_notes.clear();
    if (m_gdiplusToken) Gdiplus::GdiplusShutdown(m_gdiplusToken);
    if (m_singleton) ::CloseHandle(m_singleton);
    return CWinApp::ExitInstance();
}
