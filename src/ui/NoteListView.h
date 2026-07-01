#pragma once
#include <vector>
#include <string>
#include "ui/table/SWTableScrollViewWnd.h"
#include "domain/Models.h"
#include "domain/NoteListFormat.h"
namespace own { class NoteStore; }
class INoteWindowHost;

// 便签列表适配器：把 SWTableScrollViewWnd 的虚拟表格回调映射到 NoteStore 数据。
class CNoteListView : public ISWTableScrollViewWndCallback {
public:
    void Attach(SWTableScrollViewWnd* table, own::NoteStore* store, INoteWindowHost* host);
    void setSearch(const std::string& text);
    void reload();
    int64_t rowNoteId(int row1based) const;   // 越界返回 0

    // ISWTableScrollViewWndCallback（6 个纯虚）
    void onTableScrollViewDrawCell(HDC hdc, SWTableScrollViewWnd* s, int row, int col, CRect rect, int align) override;
    void onTableScrollViewLeftMouseClick(SWTableScrollViewWnd*, int, int) override {}
    void onTableScrollViewRightMouseClick(SWTableScrollViewWnd*, int row, int col) override;
    void onTableScrollViewLeftMouseDblClick(SWTableScrollViewWnd*, int row, int col) override;
    void onTableScrollViewSortColumn(SWTableScrollViewWnd*, int col, int order) override;
    int  onTableScrollViewAutoAdjustColumnWdidth(SWTableScrollViewWnd*, int col) override;
protected:
    virtual void onContextMenu(int row);   // Task 8/10：弹右键菜单
    struct Row { own::Note note; std::string title, group, tags, updated; };
    std::vector<Row> m_rows;
    SWTableScrollViewWnd* m_table = nullptr;
    own::NoteStore* m_store = nullptr;
    INoteWindowHost* m_host = nullptr;
    std::string m_search;
    own::NoteSortKey m_sortKey = own::NoteSortKey::Updated;
    int m_sortOrder = -1;
    std::vector<TABLE_VIEW_COLUMN_INFO*> m_cols;
};
