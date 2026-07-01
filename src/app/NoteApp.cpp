#include "app/NoteApp.h"

CNoteApp theApp;   // the one and only application object; MFC supplies WinMain

BOOL CNoteApp::InitInstance() {
    CWinApp::InitInstance();
    if (!m_host.Create())
        return FALSE;
    m_pMainWnd = &m_host;   // hidden host keeps the message loop alive
    return TRUE;
}

int CNoteApp::ExitInstance() {
    return CWinApp::ExitInstance();
}
