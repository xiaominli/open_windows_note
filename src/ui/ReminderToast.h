#pragma once
#include <afxwin.h>
#include <functional>
#include "domain/Models.h"
namespace own { class NoteStore; }
class INoteWindowHost;

// 自绘提醒通知窗：工作区右下角弹出、向上堆叠，[打开][贪睡10分][关闭]。
// 堆上自持有：DestroyWindow → PostNcDestroy → delete this。
class CReminderToast : public CWnd {
public:
    static bool show(const own::Reminder& r, const own::Note& note,
                     own::NoteStore* store, INoteWindowHost* host,
                     std::function<void(int64_t)> onClosed);
protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint pt);
    afx_msg void OnDestroy();
    void PostNcDestroy() override { delete this; }
    DECLARE_MESSAGE_MAP()
private:
    CRect btnRect(int i) const;      // 0=打开 1=贪睡 2=关闭
    void closeToast();               // onClosed 回调 + DestroyWindow
    own::Reminder m_rem;
    own::Note m_note;
    own::NoteStore* m_store = nullptr;
    INoteWindowHost* m_host = nullptr;
    std::function<void(int64_t)> m_onClosed;
    static int s_live;               // 存活通知数 → 堆叠槽位
};
