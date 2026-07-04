#include "app/MainFrame.h"
#include "data/NoteStore.h"
#include "ui/UiFont.h"
static const int kToolbarH = 32;
static const int kSearchH = 28;
static const int kBtnNewTextId = 0x3401, kBtnNewChecklistId = 0x3402, kBtnNewDrawingId = 0x3403;
BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_SIZE()
    ON_WM_CLOSE()
    ON_BN_CLICKED(kBtnNewTextId, OnNewTextClicked)
    ON_BN_CLICKED(kBtnNewChecklistId, OnNewChecklistClicked)
    ON_BN_CLICKED(kBtnNewDrawingId, OnNewDrawingClicked)
END_MESSAGE_MAP()
bool CMainFrame::Create(own::NoteStore* store, INoteWindowHost* host) {
    m_store = store; m_host = host;
    if (!CFrameWnd::Create(nullptr, _T("open_windows_note"), WS_OVERLAPPEDWINDOW,
                           CRect(200, 200, 200 + 640, 200 + 480)))
        return false;
    CRect rc; GetClientRect(&rc);
    // 新建工具栏：文本/清单/涂鸦（接线见 CNoteApp）
    CFont* bf = CFont::FromHandle(own_ui::uiFont(14));
    m_btnNewText.Create(_T("\x65B0\x5EFA\x6587\x672C"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,     // 新建文本
                        CRect(8, 4, 88, 28), this, kBtnNewTextId);
    m_btnNewChecklist.Create(_T("\x65B0\x5EFA\x6E05\x5355"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, // 新建清单
                        CRect(96, 4, 176, 28), this, kBtnNewChecklistId);
    m_btnNewDrawing.Create(_T("\x65B0\x5EFA\x6D82\x9E26"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,   // 新建涂鸦
                        CRect(184, 4, 264, 28), this, kBtnNewDrawingId);
    m_btnNewText.SetFont(bf); m_btnNewChecklist.SetFont(bf); m_btnNewDrawing.SetFont(bf);
    m_search.Create(this, CRect(0, kToolbarH, rc.Width(), kToolbarH + kSearchH));
    m_search.onChanged = [this](const std::string& s){ m_list.setSearch(s); };
    m_table.Create(nullptr, _T("table"), WS_CHILD | WS_VISIBLE,
                   CRect(0, kToolbarH + kSearchH, rc.Width(), rc.Height()), this, 0x3200);
    m_list.Attach(&m_table, m_store, m_host);
    layout();
    return true;
}
void CMainFrame::layout() {
    CRect rc; GetClientRect(&rc);
    m_search.Reposition(CRect(0, kToolbarH, rc.Width(), kToolbarH + kSearchH));
    if (m_table.GetSafeHwnd())
        m_table.MoveWindow(0, kToolbarH + kSearchH, rc.Width(), rc.Height() - kToolbarH - kSearchH);
}
void CMainFrame::OnSize(UINT t, int cx, int cy) { CFrameWnd::OnSize(t, cx, cy); layout(); }
void CMainFrame::OnClose() { ShowWindow(SW_HIDE); }   // 关闭=隐藏，不退出（退出走 Ctrl+Alt+Q）
void CMainFrame::OnNewTextClicked()      { if (onNewText) onNewText(); }
void CMainFrame::OnNewChecklistClicked() { if (onNewChecklist) onNewChecklist(); }
void CMainFrame::OnNewDrawingClicked()   { if (onNewDrawing) onNewDrawing(); }
void CMainFrame::ToggleShow() {
    if (IsWindowVisible()) ShowWindow(SW_HIDE);
    else { reloadList(); ShowWindow(SW_SHOW); SetForegroundWindow(); }
}
void CMainFrame::reloadList() { m_list.reload(); }
