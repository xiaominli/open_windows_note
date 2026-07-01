#include "app/NoteApp.h"
#include "app/AppPaths.h"
#include "app/DbBootstrap.h"
#include <string>

CNoteApp theApp;   // the one and only application object; MFC supplies WinMain

BOOL CNoteApp::InitInstance() {
    CWinApp::InitInstance();

    std::string path = own::resolveDbPathWin();
    std::string err;
    if (!own::openDatabaseAtPath(path, m_db, &err)) {
        CStringA msg = ("\xE6\x97\xA0\xE6\xB3\x95\xE6\x89\x93\xE5\xBC\x80\xE6\x95\xB0\xE6\x8D\xAE\xE5\xBA\x93:\n"  // 无法打开数据库
                        + path + "\n" + err).c_str();
        AfxMessageBox(CString(msg));
        return FALSE;
    }

    if (!m_host.Create())
        return FALSE;
    m_pMainWnd = &m_host;   // hidden host keeps the message loop alive
    return TRUE;
}

int CNoteApp::ExitInstance() {
    return CWinApp::ExitInstance();
}
