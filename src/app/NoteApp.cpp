#include "app/NoteApp.h"
#include "app/AppPaths.h"
#include "app/DbBootstrap.h"
#include "data/NoteStore.h"
#include "data/SettingsStore.h"
#include "services/AutostartManager.h"
#include "ui/ReminderToast.h"
#include <gdiplus.h>
#include <string>
#include <ctime>

CNoteApp theApp;   // the one and only application object; MFC supplies WinMain

BOOL CNoteApp::InitInstance() {
    CWinApp::InitInstance();
    AfxInitRichEdit2();               // RichEdit20W 注册（回落用）
    ::LoadLibraryW(L"Msftedit.dll");  // RichEdit 4.1 (RICHEDIT50W)：正确的 CJK/IME 输入

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
        if (m_main) m_main->reloadList();
    };
    m_host.onNewChecklist = [this]{
        own::Note n; n.type = own::NoteType::Checklist; n.visible = true;
        int64_t id = m_store->insertNote(n, (int64_t)time(nullptr));
        auto full = m_store->getNote(id);
        if (full) createAndShowNote(*full);
        if (m_main) m_main->reloadList();
    };
    m_host.onNewDrawing = [this]{
        own::Note n; n.type = own::NoteType::Drawing; n.visible = true;
        int64_t id = m_store->insertNote(n, (int64_t)time(nullptr));
        auto full = m_store->getNote(id);
        if (full) createAndShowNote(*full);
        if (m_main) m_main->reloadList();
    };
    m_host.onQuit = []{ ::PostQuitMessage(0); };
    m_host.onToggleAll = [this]{
        m_allShown = !m_allShown;
        setAllNotesVisible(m_allShown);
        if (m_main) m_main->reloadList();
    };

    // 管理器窗口：列出全部便签、搜索、右键操作
    m_main = std::make_unique<CMainFrame>();
    m_main->Create(m_store.get(), this);
    m_main->ShowWindow(SW_SHOW);
    m_host.onToggleManager = [this]{ if (m_main) m_main->ToggleShow(); };
    m_host.onSetAllVisible = [this](bool show){ setAllNotesVisible(show); if (m_main) m_main->reloadList(); };
    m_host.onToggleAutostart = []{ own_svc::autostartSetEnabled(!own_svc::autostartIsEnabled()); };
    m_host.isAutostartEnabled = []{ return own_svc::autostartIsEnabled(); };

    if (!m_host.Create())
        return FALSE;
    m_pMainWnd = &m_host;   // hidden host keeps the message loop alive

    // 全局热键：设置驱动、冲突安全地注册（派发仍在 host 的 OnHotKey）
    {
        own::SettingsStore settings(m_db);
        m_hotkeys.add(CAppHostWindow::kHotkeyNew,          "new",           "Ctrl+Alt+N");
        m_hotkeys.add(CAppHostWindow::kHotkeyNewChecklist, "new_checklist", "Ctrl+Alt+2");
        m_hotkeys.add(CAppHostWindow::kHotkeyNewDrawing,   "new_drawing",   "Ctrl+Alt+3");
        m_hotkeys.add(CAppHostWindow::kHotkeyManager,      "manager",       "Ctrl+Alt+M");
        m_hotkeys.add(CAppHostWindow::kHotkeyToggleAll,    "toggle_all",    "Ctrl+Alt+H");
        m_hotkeys.add(CAppHostWindow::kHotkeyQuit,         "quit",          "Ctrl+Alt+Q");
        m_hotkeys.loadAndRegister(m_host.GetSafeHwnd(), settings);
    }
    m_host.createTray();

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

    // 提醒：调度器接线 + host 30s 轮询 + 启动即查一次（错过的过期提醒开机即弹）
    m_reminders.attach(m_store.get());
    m_reminders.onFire = [this](const own::Reminder& r, const own::Note& n) {
        bool shown = CReminderToast::show(r, n, m_store.get(), this, [this](int64_t rid) {
            m_reminders.markResolved(rid);
            if (m_main) m_main->reloadList();   // ⏰ 前缀/时间随落库刷新
        });
        if (!shown) m_reminders.markResolved(r.id);   // 建窗失败：解除占用，下轮重试
    };
    m_host.onReminderTick = [this] { m_reminders.poll((int64_t)time(nullptr)); };
    m_host.startReminderTimer();
    m_reminders.poll((int64_t)time(nullptr));
    return TRUE;
}

void CNoteApp::createAndShowNote(const own::Note& seed) {
    auto w = std::make_unique<CNoteWindow>();
    if (w->Create(seed, m_store.get()))
        m_notes.push_back(std::move(w));
}

CNoteWindow* CNoteApp::findNote(int64_t id) {
    for (auto& w : m_notes) if (w && w->noteId() == id) return w.get();
    return nullptr;
}
void CNoteApp::openOrFocusNote(int64_t id) {
    if (CNoteWindow* w = findNote(id)) {
        w->ShowWindow(SW_SHOW);
        w->SetWindowPos(&CWnd::wndTopMost, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        return;
    }
    auto full = m_store->getNote(id);
    if (full) { full->visible = true; createAndShowNote(*full); }
}
void CNoteApp::closeNoteWindow(int64_t id) {
    for (auto it = m_notes.begin(); it != m_notes.end(); ++it) {
        if (*it && (*it)->noteId() == id) {
            if ((*it)->GetSafeHwnd()) (*it)->DestroyWindow();
            m_notes.erase(it);
            return;
        }
    }
}
void CNoteApp::refreshNoteWindow(int64_t id) {
    if (findNote(id)) { closeNoteWindow(id); openOrFocusNote(id); }
}
void CNoteApp::setAllNotesVisible(bool show) {
    own::NoteQuery q; auto all = m_store->query(q);
    for (const auto& n : all) {
        m_store->updateFlags(n.id, n.opacity, n.pinned, n.rolledUp, show);
        if (show) openOrFocusNote(n.id);
        else closeNoteWindow(n.id);
    }
}

int CNoteApp::ExitInstance() {
    m_hotkeys.unregisterAll(m_host.GetSafeHwnd());
    m_notes.clear();
    if (m_gdiplusToken) Gdiplus::GdiplusShutdown(m_gdiplusToken);
    if (m_singleton) ::CloseHandle(m_singleton);
    return CWinApp::ExitInstance();
}
