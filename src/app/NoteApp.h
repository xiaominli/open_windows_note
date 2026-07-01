#pragma once
#include <afxwin.h>
#include <memory>
#include <vector>
#include "app/AppHostWindow.h"
#include "app/NoteWindowHost.h"
#include "app/MainFrame.h"
#include "data/Database.h"
#include "data/NoteStore.h"
#include "ui/NoteWindow.h"

// MFC application object. Single global instance `theApp` provides WinMain.
class CNoteApp : public CWinApp, public INoteWindowHost {
public:
    BOOL InitInstance() override;
    int  ExitInstance() override;

    void openOrFocusNote(int64_t id) override;
    void refreshNoteWindow(int64_t id) override;
    void closeNoteWindow(int64_t id) override;
    void setAllNotesVisible(bool show) override;
private:
    void createAndShowNote(const own::Note& seed);
    CNoteWindow* findNote(int64_t id);   // 在 m_notes 里查；无则 nullptr

    own::Database  m_db;
    std::unique_ptr<own::NoteStore> m_store;
    CAppHostWindow m_host;
    std::unique_ptr<CMainFrame> m_main;
    ULONG_PTR      m_gdiplusToken = 0;
    HANDLE         m_singleton = nullptr;
    bool           m_allShown = true;
    std::vector<std::unique_ptr<CNoteWindow>> m_notes;
};
