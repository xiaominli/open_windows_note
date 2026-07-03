#include "app/NoteApp.h"
#include "app/AppPaths.h"
#include "app/DbBootstrap.h"
#include "data/NoteStore.h"
#include "data/SettingsStore.h"
#include "data/BackupService.h"
#include "domain/BackupRules.h"
#include "domain/StickyRules.h"
#include "services/AutostartManager.h"
#include "ui/ReminderToast.h"
#include "ui/SettingsDialog.h"
#include "ui/TextContentView.h"
#include <afxdlgs.h>   // CFileDialog
#include <gdiplus.h>
#include <string>
#include <ctime>

static std::wstring u8ToW(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n > 0 ? n : 0, L'\0');
    if (n > 0) ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
static std::string wToU8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0) ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

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

    {
        own::SettingsStore st(m_db);
        CTextContentView::SetDefaultFontPt(st.getInt("default_font_pt", 10));   // 默认字号先于建窗生效
    }

    Gdiplus::GdiplusStartupInput gsi;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gsi, nullptr);

    m_store = std::make_unique<own::NoteStore>(m_db);

    // 装配 host 回调：新建热键 → 插入一条 note 并弹窗；退出热键 → 结束消息循环
    m_host.onNewNote = [this]{
        own::Note n; n.type = own::NoteType::RichText; n.visible = true;
        n.plainText = "new note";
        own::SettingsStore st(m_db);
        n.themeId = st.getInt("default_theme_id", 0);
        n.opacity = st.getInt("default_opacity", 255);
        int64_t id = m_store->insertNote(n, (int64_t)time(nullptr));
        auto full = m_store->getNote(id);
        if (full) createAndShowNote(*full);
        if (m_main) m_main->reloadList();
    };
    m_host.onNewChecklist = [this]{
        own::Note n; n.type = own::NoteType::Checklist; n.visible = true;
        own::SettingsStore st(m_db);
        n.themeId = st.getInt("default_theme_id", 0);
        n.opacity = st.getInt("default_opacity", 255);
        int64_t id = m_store->insertNote(n, (int64_t)time(nullptr));
        auto full = m_store->getNote(id);
        if (full) createAndShowNote(*full);
        if (m_main) m_main->reloadList();
    };
    m_host.onNewDrawing = [this]{
        own::Note n; n.type = own::NoteType::Drawing; n.visible = true;
        own::SettingsStore st(m_db);
        n.themeId = st.getInt("default_theme_id", 0);
        n.opacity = st.getInt("default_opacity", 255);
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
    m_host.onOpenSettings = [this]{
        own_ui::showSettingsDialog(m_db, *m_store, m_hotkeys, m_host.GetSafeHwnd());
    };
    m_host.onExportBackup = [this]{ doExportBackup(); };
    m_host.onImportBackup = [this]{ doImportBackup(); };

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

    // 贴窗便签即使当前不可见也要有窗participate（瞬态显隐需要窗存在）
    {
        own::NoteQuery qa; auto all = m_store->query(qa);
        for (const auto& n : all)
            if (!n.stickTarget.empty() && !findNote(n.id)) createAndShowNote(n);
    }
    m_sticky.onForeground = [this](const std::string& t, const std::string& c) {
        applyStickyVisibility(t, c);
    };
    m_sticky.start();                    // 失败即降级：贴窗静默不工作
    stickyInitialPass();

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

void CNoteApp::applyStickyVisibility(const std::string& titleU8, const std::string& classU8) {
    for (auto& w : m_notes) {
        if (!w) continue;
        const std::string& t = w->stickTarget();
        if (t.empty()) continue;
        w->setStickyVisible(own::matchesStickTarget(titleU8, classU8, t));
    }
}
void CNoteApp::stickyInitialPass() {
    HWND fg = ::GetForegroundWindow();
    if (!fg) return;
    DWORD pid = 0; ::GetWindowThreadProcessId(fg, &pid);
    if (pid == ::GetCurrentProcessId()) { applyStickyVisibility("", ""); return; }  // 自家前台：贴窗先藏
    wchar_t title[256]{}; int tl = ::GetWindowTextW(fg, title, 256);
    wchar_t cls[256]{};   int cl = ::GetClassNameW(fg, cls, 256);
    std::wstring wt(title, title + (tl > 0 ? tl : 0)), wc(cls, cls + (cl > 0 ? cl : 0));
    applyStickyVisibility(wToU8(wt), wToU8(wc));
}

void CNoteApp::doExportBackup() {
    for (auto& w : m_notes) if (w) w->flushNow();          // 备份含最新内容
    time_t now = time(nullptr);
    tm lt{}; localtime_s(&lt, &now);
    std::string name = own::defaultBackupName(lt.tm_year + 1900, lt.tm_mon + 1,
                                              lt.tm_mday, lt.tm_hour, lt.tm_min);
    CFileDialog dlg(FALSE, _T("db"), CString(name.c_str()),
                    OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
                    _T("SQLite \x6570\x636E\x5E93 (*.db)|*.db|\x5168\x90E8\x6587\x4EF6 (*.*)|*.*||"));  // 数据库/全部文件
    if (dlg.DoModal() != IDOK) return;
    std::string dest = wToU8((LPCWSTR)dlg.GetPathName());
    ::DeleteFileW(u8ToW(dest).c_str());   // 覆盖已在对话框确认；宽字符删除（中文路径安全）
    std::string err;
    if (own::exportBackup(m_db, dest, &err)) {
        AfxMessageBox(_T("\x5BFC\x51FA\x6210\x529F") + CString(_T("\x3002")));         // 导出成功。
    } else {
        AfxMessageBox(_T("\x5BFC\x51FA\x5931\x8D25\xFF1A") + CString(u8ToW(err).c_str()));    // 导出失败：
    }
}
void CNoteApp::doImportBackup() {
    CFileDialog dlg(TRUE, _T("db"), nullptr, OFN_FILEMUSTEXIST,
                    _T("SQLite \x6570\x636E\x5E93 (*.db)|*.db|\x5168\x90E8\x6587\x4EF6 (*.*)|*.*||"));  // 数据库/全部文件
    if (dlg.DoModal() != IDOK) return;
    std::string src = wToU8((LPCWSTR)dlg.GetPathName());
    std::string err;
    if (!own::validateBackupFile(src, &err)) {
        AfxMessageBox(_T("\x65E0\x6548\x7684\x5907\x4EFD\x6587\x4EF6\xFF1A") + CString(u8ToW(err).c_str()));  // 无效的备份文件：
        return;
    }
    if (AfxMessageBox(_T("\x5BFC\x5165\x5C06\x66FF\x6362\x5F53\x524D\x5168\x90E8\x6570\x636E\x5E76\x91CD\x542F\x5E94\x7528\xFF0C\x662F\x5426\x7EE7\x7EED\xFF1F"),  // 导入将替换当前全部数据并重启应用，是否继续？
                      MB_YESNO | MB_ICONWARNING) != IDYES)
        return;
    // 停掉会碰 store 的定时轮询，再拆窗、关库（顺序：先消费方后 DB）
    m_host.onReminderTick = []{};
    m_hotkeys.unregisterAll(m_host.GetSafeHwnd());     // 弹错误框时消息泵仍在转：热键/托盘不得再碰已拆的 store
    m_host.onNewNote = nullptr; m_host.onNewChecklist = nullptr; m_host.onNewDrawing = nullptr;
    m_host.onToggleAll = nullptr; m_host.onSetAllVisible = nullptr; m_host.onToggleManager = nullptr;
    m_host.onOpenSettings = nullptr; m_host.onExportBackup = nullptr; m_host.onImportBackup = nullptr;
    m_sticky.stop();                                        // 拆窗期间不再收前台回调
    m_notes.clear();                                       // 析构链走 flushContent 落盘
    if (m_main) { m_main->DestroyWindow(); m_main.reset(); }
    m_store.reset();
    m_db.close();
    std::string cur = own::resolveDbPathWin();
    std::wstring wCur = u8ToW(cur), wSrc = u8ToW(src), wBak = u8ToW(cur + ".bak"), wNew = u8ToW(cur + ".new");
    ::CopyFileW(wCur.c_str(), wBak.c_str(), FALSE);        // 现库兜底备份（失败不阻断——可能首启无库）
    if (!::CopyFileW(wSrc.c_str(), wNew.c_str(), FALSE)) { // 先落到临时文件，绝不直接覆盖现库
        AfxMessageBox(_T("\x5BFC\x5165\x5931\x8D25\xFF1A\x65E0\x6CD5\x590D\x5236\x5907\x4EFD\x6587\x4EF6\xFF0C\x539F\x5E93\x672A\x53D7\x5F71\x54CD\x3002\x5E94\x7528\x5373\x5C06\x9000\x51FA\x3002"));  // 导入失败：无法复制备份文件，原库未受影响。应用即将退出。
        ::PostQuitMessage(0);
        return;
    }
    if (!::MoveFileExW(wNew.c_str(), wCur.c_str(), MOVEFILE_REPLACE_EXISTING)) {  // 同卷 rename：要么旧库要么新库，不会截断
        ::DeleteFileW(wNew.c_str());
        AfxMessageBox(_T("\x66FF\x6362\x6570\x636E\x5E93\x5931\x8D25\xFF0C\x539F\x5E93\x4FDD\x6301\x4E0D\x53D8\xFF1B\x5982\x6709\x5F02\x5E38\x53EF\x7528 notes.db.bak \x6062\x590D\x3002\x5E94\x7528\x5373\x5C06\x9000\x51FA\x3002"));  // 替换数据库失败，原库保持不变；如有异常可用 notes.db.bak 恢复。应用即将退出。
        ::PostQuitMessage(0);
        return;
    }
    if (m_singleton) { ::CloseHandle(m_singleton); m_singleton = nullptr; }   // 先释放单例锁再拉新进程
    wchar_t exe[MAX_PATH]{};
    DWORD n = ::GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) { ::PostQuitMessage(0); return; }            // 路径异常：不拉起，直接退（库已替换成功）
    ::ShellExecuteW(nullptr, L"open", exe, nullptr, nullptr, SW_SHOWNORMAL);
    ::PostQuitMessage(0);
}

int CNoteApp::ExitInstance() {
    m_sticky.stop();
    m_hotkeys.unregisterAll(m_host.GetSafeHwnd());
    m_notes.clear();
    if (m_gdiplusToken) Gdiplus::GdiplusShutdown(m_gdiplusToken);
    if (m_singleton) ::CloseHandle(m_singleton);
    return CWinApp::ExitInstance();
}
