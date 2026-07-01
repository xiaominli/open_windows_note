#include "app/MainFrame.h"
#include "data/NoteStore.h"
static const int kSearchH = 28;
BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_SIZE()
    ON_WM_CLOSE()
END_MESSAGE_MAP()
bool CMainFrame::Create(own::NoteStore* store, INoteWindowHost* host) {
    m_store = store; m_host = host;
    if (!CFrameWnd::Create(nullptr, _T("open_windows_note"), WS_OVERLAPPEDWINDOW,
                           CRect(200, 200, 200 + 640, 200 + 480)))
        return false;
    CRect rc; GetClientRect(&rc);
    m_search.Create(this, CRect(0, 0, rc.Width(), kSearchH));
    m_search.onChanged = [this](const std::string& s){ m_list.setSearch(s); };
    m_table.Create(nullptr, _T("table"), WS_CHILD | WS_VISIBLE, CRect(0, kSearchH, rc.Width(), rc.Height()), this, 0x3200);
    m_list.Attach(&m_table, m_store, m_host);
    layout();
    return true;
}
void CMainFrame::layout() {
    CRect rc; GetClientRect(&rc);
    m_search.Reposition(CRect(0, 0, rc.Width(), kSearchH));
    if (m_table.GetSafeHwnd()) m_table.MoveWindow(0, kSearchH, rc.Width(), rc.Height() - kSearchH);
}
void CMainFrame::OnSize(UINT t, int cx, int cy) { CFrameWnd::OnSize(t, cx, cy); layout(); }
void CMainFrame::OnClose() { ShowWindow(SW_HIDE); }   // 关闭=隐藏，不退出（退出走 Ctrl+Alt+Q）
void CMainFrame::ToggleShow() {
    if (IsWindowVisible()) ShowWindow(SW_HIDE);
    else { reloadList(); ShowWindow(SW_SHOW); SetForegroundWindow(); }
}
void CMainFrame::reloadList() { m_list.reload(); }
