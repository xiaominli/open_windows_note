#include "ui/NoteListView.h"
#include "ui/TextPrompt.h"
#include "app/NoteWindowHost.h"
#include "data/NoteStore.h"
#include "domain/DateTimeText.h"
#include <ctime>
#include <map>
#include <set>

static uint32_t typeMarkerColor(own::NoteType t) {
    switch (t) {
        case own::NoteType::Checklist: return 0x3060E0;
        case own::NoteType::Drawing:   return 0x30A030;
        default:                       return 0xE0C020;   // 富文本=黄
    }
}
// UTF-8(存储层) ↔ 宽字符(Unicode GDI/菜单) 转换。直接把 UTF-8 当 ANSI 会乱码。
static CString u8ToWide(const std::string& s) {
    if (s.empty()) return CString();
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    CString w;
    if (n > 0) { ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.GetBuffer(n), n); w.ReleaseBuffer(n); }
    return w;
}
static std::string wideToU8(const CString& w) {
    if (w.IsEmpty()) return std::string();
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w, w.GetLength(), nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0) ::WideCharToMultiByte(CP_UTF8, 0, w, w.GetLength(), &s[0], n, nullptr, nullptr);
    return s;
}
void CNoteListView::Attach(SWTableScrollViewWnd* table, own::NoteStore* store, INoteWindowHost* host) {
    m_table = table; m_store = store; m_host = host;
    m_table->setTableScrollViewCallback(this);
    // 列： [标记] 标题 分组 标签 更新
    m_cols.push_back(new TABLE_VIEW_COLUMN_INFO((char*)"",   28.f, 2, 0));
    m_cols.push_back(new TABLE_VIEW_COLUMN_INFO((char*)"\xE6\xA0\x87\xE9\xA2\x98", 220.f, 0, 1)); // 标题
    m_cols.push_back(new TABLE_VIEW_COLUMN_INFO((char*)"\xE5\x88\x86\xE7\xBB\x84",  90.f, 0, 0)); // 分组
    m_cols.push_back(new TABLE_VIEW_COLUMN_INFO((char*)"\xE6\xA0\x87\xE7\xAD\xBE", 120.f, 0, 0)); // 标签
    m_cols.push_back(new TABLE_VIEW_COLUMN_INFO((char*)"\xE6\x9B\xB4\xE6\x96\xB0", 120.f, 0, 1)); // 更新
    m_table->setColumnInfos("note_list", m_cols);
    reload();
}
void CNoteListView::setSearch(const std::string& text) { m_search = text; reload(); }
void CNoteListView::reload() {
    if (!m_store) return;
    own::NoteQuery q; q.search = m_search;
    auto notes = m_store->query(q);
    own::sortNoteRows(notes, m_sortKey, m_sortOrder);
    std::map<int64_t, std::string> gname;
    for (const auto& g : m_store->allGroups()) gname[g.id] = g.name;
    std::set<int64_t> remNotes;   // 一次查询代替每行 remindersOfNote（N+1）
    for (const auto& rem : m_store->enabledReminders()) remNotes.insert(rem.noteId);
    int64_t now = (int64_t)time(nullptr);
    m_rows.clear();
    for (const auto& n : notes) {
        Row r; r.note = n;
        r.title = own::noteTitleText(n);
        if (remNotes.count(n.id)) r.title = "\xE2\x8F\xB0 " + r.title;   // ⏰ 有提醒
        auto it = gname.find(n.groupId);
        r.group = (n.groupId != 0 && it != gname.end()) ? it->second : "";
        std::string tags;
        for (const auto& t : m_store->tagsOfNote(n.id)) { if (!tags.empty()) tags += ","; tags += t.name; }
        r.tags = tags;
        r.updated = own::formatRelativeTime(now, n.updatedAt);
        m_rows.push_back(std::move(r));
    }
    if (m_table) { m_table->setTotalRowCount((int)m_rows.size()); m_table->Invalidate(FALSE); }
}
int64_t CNoteListView::rowNoteId(int row) const {
    if (row >= 1 && row <= (int)m_rows.size()) return m_rows[row-1].note.id;
    return 0;
}
void CNoteListView::onTableScrollViewDrawCell(HDC hdc, SWTableScrollViewWnd*, int row, int col, CRect rect, int) {
    if (row < 1 || row > (int)m_rows.size()) return;
    const Row& r = m_rows[row-1];
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, RGB(0xE0,0xE0,0xE0));
    if (col == 0) {                                   // 类型色块
        uint32_t c = typeMarkerColor(r.note.type);
        CRect sw(rect.left + rect.Width()/2 - 5, rect.top + rect.Height()/2 - 5,
                 rect.left + rect.Width()/2 + 5, rect.top + rect.Height()/2 + 5);
        HBRUSH b = ::CreateSolidBrush(RGB((c>>16)&0xFF,(c>>8)&0xFF,c&0xFF));
        RECT rr = sw; ::FillRect(hdc, &rr, b); ::DeleteObject(b);
        return;
    }
    const std::string* txt = nullptr;
    if (col == 1) txt = &r.title; else if (col == 2) txt = &r.group;
    else if (col == 3) txt = &r.tags; else if (col == 4) txt = &r.updated;
    if (!txt) return;
    CRect tr(rect.left + 6, rect.top, rect.right - 4, rect.bottom);
    CString wtxt = u8ToWide(*txt);
    ::DrawTextW(hdc, wtxt, wtxt.GetLength(), tr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}
void CNoteListView::onTableScrollViewLeftMouseDblClick(SWTableScrollViewWnd*, int row, int) {
    int64_t id = rowNoteId(row);
    if (id && m_host) m_host->openOrFocusNote(id);
}
void CNoteListView::onTableScrollViewRightMouseClick(SWTableScrollViewWnd*, int row, int) {
    if (rowNoteId(row)) onContextMenu(row);
}
void CNoteListView::onTableScrollViewSortColumn(SWTableScrollViewWnd*, int col, int order) {
    if (col == 1) m_sortKey = own::NoteSortKey::Title;
    else if (col == 4) m_sortKey = own::NoteSortKey::Updated;
    else return;
    m_sortOrder = order; reload();
}
int CNoteListView::onTableScrollViewAutoAdjustColumnWdidth(SWTableScrollViewWnd*, int) { return 0; }

void CNoteListView::onContextMenu(int row) {
    int64_t id = rowNoteId(row);
    if (!id || !m_store) return;
    auto note = m_store->getNote(id);
    if (!note) return;

    CMenu menu; menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, 1, _T("\x6253\x5F00"));                    // 打开
    menu.AppendMenu(MF_STRING, 2, _T("\x9690\x85CF\x8BE5\x4FBF\x7B7E")); // 隐藏该便签

    // 改分组 子菜单：无分组=100，现有=200+i，新建=199
    CMenu grp; grp.CreatePopupMenu();
    grp.AppendMenu(MF_STRING, 100, _T("\x65E0\x5206\x7EC4"));             // 无分组
    auto groups = m_store->allGroups();
    for (size_t i = 0; i < groups.size() && i < 99; ++i)
        grp.AppendMenu(MF_STRING, 200 + (UINT)i, u8ToWide(groups[i].name));
    grp.AppendMenu(MF_SEPARATOR, 0, _T(""));
    grp.AppendMenu(MF_STRING, 199, _T("\x65B0\x5EFA\x5206\x7EC4\x2026")); // 新建分组…
    menu.AppendMenu(MF_POPUP, (UINT_PTR)grp.GetSafeHmenu(), _T("\x6539\x5206\x7EC4")); // 改分组

    // 加标签 子菜单：现有=300+i，新建=299
    CMenu tag; tag.CreatePopupMenu();
    auto tags = m_store->allTags();
    for (size_t i = 0; i < tags.size() && i < 99; ++i)
        tag.AppendMenu(MF_STRING, 300 + (UINT)i, u8ToWide(tags[i].name));
    if (!tags.empty()) tag.AppendMenu(MF_SEPARATOR, 0, _T(""));
    tag.AppendMenu(MF_STRING, 299, _T("\x65B0\x5EFA\x6807\x7B7E\x2026")); // 新建标签…
    menu.AppendMenu(MF_POPUP, (UINT_PTR)tag.GetSafeHmenu(), _T("\x52A0\x6807\x7B7E")); // 加标签

    // 设提醒 子菜单：预设=401..404，重复=410..413，取消=419
    auto rems = m_store->remindersOfNote(id);
    const own::Reminder* cur = nullptr;
    for (const auto& x : rems) if (x.enabled) { cur = &x; break; }
    CMenu rem; rem.CreatePopupMenu();
    rem.AppendMenu(MF_STRING, 401, _T("10 \x5206\x949F\x540E"));                 // 10 分钟后
    rem.AppendMenu(MF_STRING, 402, _T("1 \x5C0F\x65F6\x540E"));                  // 1 小时后
    rem.AppendMenu(MF_STRING, 403, _T("\x660E\x5929 9:00"));                     // 明天 9:00
    rem.AppendMenu(MF_STRING, 404, _T("\x81EA\x5B9A\x4E49\x65F6\x95F4\x2026"));  // 自定义时间…
    rem.AppendMenu(MF_SEPARATOR, 0, _T(""));
    UINT recBase = cur ? MF_STRING : (MF_STRING | MF_GRAYED);
    own::Recurrence curRec = cur ? cur->recurrence : own::Recurrence::None;
    rem.AppendMenu(recBase | ((cur && curRec == own::Recurrence::None) ? MF_CHECKED : 0),
                   410, _T("\x4E0D\x91CD\x590D"));                               // 不重复
    rem.AppendMenu(recBase | (curRec == own::Recurrence::Daily   ? MF_CHECKED : 0),
                   411, _T("\x6BCF\x5929"));                                     // 每天
    rem.AppendMenu(recBase | (curRec == own::Recurrence::Weekly  ? MF_CHECKED : 0),
                   412, _T("\x6BCF\x5468"));                                     // 每周
    rem.AppendMenu(recBase | (curRec == own::Recurrence::Monthly ? MF_CHECKED : 0),
                   413, _T("\x6BCF\x6708"));                                     // 每月
    rem.AppendMenu(MF_SEPARATOR, 0, _T(""));
    rem.AppendMenu(cur ? MF_STRING : (MF_STRING | MF_GRAYED),
                   419, _T("\x53D6\x6D88\x63D0\x9192"));                         // 取消提醒
    CString remLabel = _T("\x8BBE\x63D0\x9192");                                 // 设提醒
    if (cur) remLabel += _T(" (") + u8ToWide(own::formatLocalDateTime(cur->dueAt)) + _T(")");
    menu.AppendMenu(MF_POPUP, (UINT_PTR)rem.GetSafeHmenu(), remLabel);

    menu.AppendMenu(MF_STRING, 430,
        note->stickTarget.empty()
            ? _T("\x8D34\x5230\x7A97\x53E3\x2026")                                    // 贴到窗口…
            : _T("\x8D34\x5230\x7A97\x53E3\xFF08\x5DF2\x8BBE\xFF09\x2026"));          // 贴到窗口（已设）…

    menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
    menu.AppendMenu(MF_STRING, 3, _T("\x5220\x9664"));                    // 删除

    CPoint pt; ::GetCursorPos(&pt);
    int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, m_table);

    if (cmd == 1) { if (m_host) m_host->openOrFocusNote(id); }
    else if (cmd == 2) {
        m_store->updateFlags(id, note->opacity, note->pinned, note->rolledUp, false);
        if (m_host) m_host->closeNoteWindow(id);
        reload();
    }
    else if (cmd == 3) { if (m_host) m_host->closeNoteWindow(id); m_store->deleteNote(id); reload(); }
    else if (cmd == 100) { m_store->updateNoteGroup(note->id, 0); reload(); }
    else if (cmd >= 200 && cmd < 299) {
        size_t i = (size_t)(cmd - 200);
        if (i < groups.size()) { m_store->updateNoteGroup(note->id, groups[i].id); reload(); }
    }
    else if (cmd == 199) {
        CString name;
        if (own_ui::promptText(m_table, _T("\x65B0\x5EFA\x5206\x7EC4"), name)) {
            own::Group g; g.name = wideToU8(name);
            int64_t gid = m_store->upsertGroup(g);
            m_store->updateNoteGroup(note->id, gid); reload();
        }
    }
    else if (cmd >= 300 && cmd < 399) {
        size_t i = (size_t)(cmd - 300);
        if (i < tags.size()) { m_store->addTagToNote(id, tags[i].id); reload(); }
    }
    else if (cmd == 299) {
        CString name;
        if (own_ui::promptText(m_table, _T("\x65B0\x5EFA\x6807\x7B7E"), name)) {
            int64_t tid = m_store->upsertTag(wideToU8(name));
            m_store->addTagToNote(id, tid); reload();
        }
    }
    else if (cmd >= 401 && cmd <= 404) {
        int64_t now = (int64_t)time(nullptr);
        int64_t due = 0;
        bool have = false;
        if (cmd == 401)      { due = now + 600;  have = true; }
        else if (cmd == 402) { due = now + 3600; have = true; }
        else if (cmd == 403) { due = own::nextDayAt(now, 9, 0); have = true; }
        else {                                                    // 404 自定义时间…
            CString io = u8ToWide(own::formatLocalDateTime(cur ? cur->dueAt : now + 3600));
            if (own_ui::promptText(m_table,
                    _T("\x63D0\x9192\x65F6\x95F4 (YYYY-MM-DD HH:MM)"), io)) {    // 提醒时间
                if (own::parseLocalDateTime(wideToU8(io), due)) have = true;
                else AfxMessageBox(
                    _T("\x65F6\x95F4\x683C\x5F0F\x65E0\x6548\x3002\x5E94\x4E3A YYYY-MM-DD HH:MM")); // 时间格式无效。应为…
            }
        }
        if (have) {
            if (cur) {
                own::Reminder r = *cur;
                r.dueAt = due; r.snoozeUntil = 0; r.enabled = true;
                m_store->updateReminder(r);
            } else {
                own::Reminder r; r.noteId = id; r.dueAt = due;
                m_store->insertReminder(r);
            }
            reload();
        }
    }
    else if (cmd >= 410 && cmd <= 413 && cur) {
        own::Reminder r = *cur;
        r.recurrence = (own::Recurrence)(cmd - 410);
        m_store->updateReminder(r);
        reload();
    }
    else if (cmd == 419 && cur) { m_store->deleteReminder(cur->id); reload(); }
    else if (cmd == 430) {                          // 贴到窗口：输入标题子串或 class:类名；空=取消贴窗
        CString io = u8ToWide(note->stickTarget);
        if (own_ui::promptText(m_table, _T("\x7A97\x53E3\x6807\x9898\x5B50\x4E32\x6216 class:\x7C7B\x540D\xFF08\x7A7A=\x53D6\x6D88\xFF09"), io)) {  // 窗口标题子串或 class:类名（空=取消）
            std::string t = wideToU8(io);
            m_store->updateNoteStick(id, t);        // 只写 stick_target，不碰 blob
            if (m_host) {
                m_host->refreshNoteWindow(id);      // 开着则重开取新 target
                if (!t.empty()) m_host->openOrFocusNote(id);   // 没开则建窗以参与贴窗显隐
            }
            reload();
        }
    }
}
