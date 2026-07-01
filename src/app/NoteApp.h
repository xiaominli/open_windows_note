#pragma once
#include <afxwin.h>
#include "app/AppHostWindow.h"

// MFC application object. Single global instance `theApp` provides WinMain.
class CNoteApp : public CWinApp {
public:
    BOOL InitInstance() override;
    int  ExitInstance() override;
private:
    CAppHostWindow m_host;
};
