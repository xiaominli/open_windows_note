#include "ui/NoteListView.h"
#include "app/NoteWindowHost.h"
#include "data/NoteStore.h"
#include <ctime>
#include <map>

static uint32_t typeMarkerColor(own::NoteType t) {
    switch (t) {
        case own::NoteType::Checklist: return 0x3060E0;
        case own::NoteType::Drawing:   return 0x30A030;
        default:                       return 0xE0C020;   // 富文本=黄
    }
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
    int64_t now = (int64_t)time(nullptr);
    m_rows.clear();
    for (const auto& n : notes) {
        Row r; r.note = n;
        r.title = own::noteTitleText(n);
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
    ::DrawTextA(hdc, txt->c_str(), (int)txt->size(), tr, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
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
    CMenu menu; menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING, 1, _T("\x6253\x5F00"));                    // 打开
    menu.AppendMenu(MF_STRING, 2, _T("\x9690\x85CF\x8BE5\x4FBF\x7B7E")); // 隐藏该便签
    menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
    menu.AppendMenu(MF_STRING, 3, _T("\x5220\x9664"));                    // 删除
    CPoint pt; ::GetCursorPos(&pt);
    int cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN, pt.x, pt.y, m_table);
    if (cmd == 1) { if (m_host) m_host->openOrFocusNote(id); }
    else if (cmd == 2) {
        auto n = m_store->getNote(id);
        if (n) {
            m_store->updateFlags(id, n->opacity, n->pinned, n->rolledUp, false);
            if (m_host) m_host->closeNoteWindow(id);
            reload();
        }
    }
    else if (cmd == 3) {
        if (m_host) m_host->closeNoteWindow(id);
        m_store->deleteNote(id);
        reload();
    }
}
