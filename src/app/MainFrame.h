#pragma once
#include <afxwin.h>
#include <functional>
#include "ui/table/SWTableScrollViewWnd.h"
#include "ui/NoteListView.h"
#include "ui/SearchBox.h"
namespace own { class NoteStore; }
class INoteWindowHost;

// 管理器窗口：顶部新建工具栏 + 搜索框 + 便签自绘表格。关闭=隐藏（不退出）。
class CMainFrame : public CFrameWnd {
public:
    bool Create(own::NoteStore* store, INoteWindowHost* host);
    void ToggleShow();
    void reloadList();
    // 新建入口：由 CNoteApp 接线到既有热键回调（保持 MainFrame 不依赖 App）
    std::function<void()> onNewText, onNewChecklist, onNewDrawing;
protected:
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnClose();
    afx_msg void OnNewTextClicked();
    afx_msg void OnNewChecklistClicked();
    afx_msg void OnNewDrawingClicked();
    void PostNcDestroy() override {}   // 由 unique_ptr 拥有，禁止 MFC 默认 delete this
    DECLARE_MESSAGE_MAP()
private:
    void layout();
    own::NoteStore* m_store = nullptr;
    INoteWindowHost* m_host = nullptr;
    SWTableScrollViewWnd m_table;
    CNoteListView m_list;
    CSearchBox m_search;
    CButton m_btnNewText, m_btnNewChecklist, m_btnNewDrawing;
};
