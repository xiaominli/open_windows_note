#pragma once
#include <afxwin.h>
#include <memory>
#include <vector>
#include "app/AppHostWindow.h"
#include "data/Database.h"
#include "data/NoteStore.h"
#include "ui/NoteWindow.h"

// MFC application object. Single global instance `theApp` provides WinMain.
class CNoteApp : public CWinApp {
public:
    BOOL InitInstance() override;
    int  ExitInstance() override;
private:
    void createAndShowNote(const own::Note& seed);

    own::Database  m_db;
    std::unique_ptr<own::NoteStore> m_store;
    CAppHostWindow m_host;
    ULONG_PTR      m_gdiplusToken = 0;
    HANDLE         m_singleton = nullptr;
    std::vector<std::unique_ptr<CNoteWindow>> m_notes;
};
