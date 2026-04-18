#include "stdafx.h"
#include "Launcher.h"

#include "LauncherWindow.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


CLauncher gTheApp;


BEGIN_MESSAGE_MAP(CLauncher, CWinApp)
END_MESSAGE_MAP()


CLauncherWindow* CLauncher::GetWindow()
{
	return m_pLauncherWindow;
}


BOOL CLauncher::InitInstance()
{
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();
	AfxEnableControlContainer();

	m_pLauncherWindow = new CLauncherWindow();
	m_pMainWnd = m_pLauncherWindow;
	INT_PTR nResponse = m_pLauncherWindow->DoModal();
	return FALSE;
}
