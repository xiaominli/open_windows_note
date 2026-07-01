#pragma once
#include <afxwin.h>
#include "app/AppHostWindow.h"
#include "data/Database.h"

// MFC application object. Single global instance `theApp` provides WinMain.
class CNoteApp : public CWinApp {
public:
    BOOL InitInstance() override;
    int  ExitInstance() override;
private:
    own::Database  m_db;
    CAppHostWindow m_host;
};
