#pragma once
#include <afxwin.h>
#include "domain/Models.h"
#include "ui/TitleBarLayout.h"
namespace own { class NoteStore; }
class CNoteWindow : public CWnd {
public:
    bool Create(const own::Note& note, own::NoteStore* store);
    int64_t noteId() const { return m_note.id; }
protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()
private:
    own::TitleBarRects layout() const;   // 用当前 client 尺寸算标题栏
    own::Note m_note;
    own::NoteStore* m_store = nullptr;
};
