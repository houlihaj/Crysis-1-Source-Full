#include "stdafx.h"
#include "LauncherWindow.h"

#include <IHardwareMouse.h>
#include "SceneStartup.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


#define ID_CLOSE_BUTTON			0x0002
#define APP_CAPTION				"Launcher"


BEGIN_MESSAGE_MAP(CLauncherWindow, CDialog)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_DESTROY()	
END_MESSAGE_MAP()


CLauncherWindow::CLauncherWindow(CWnd* pParent)	: CDialog(CLauncherWindow::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDI_LAUNCHER_APP);
	m_pViewPort = NULL;
}


#include <IMovieSystem.h>


BOOL CLauncherWindow::OnInitDialog()
{
	// dialog setup

	CDialog::OnInitDialog();
	SetIcon(m_hIcon, TRUE);	
	SetIcon(m_hIcon, FALSE);

	CRect r;
	GetClientRect(&r);

	ShowWindow(true);

	m_pViewPort = new CViewPort();
	m_pViewPort->Create(NULL, WS_CHILD | WS_VISIBLE | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, 
		CRect(5, 5, r.right-5, r.bottom-5), this, 0x302);
	m_pViewPort->Refresh();

	// setup framework
	if (!InitSystem(m_hWnd))
		return FALSE;

	// setup scene

	SetWindowText(APP_CAPTION " - Loading Level...");

	gEnv->pConsole->ShowConsole(false);
	gEnv->pInput->SetExclusiveMode(eDI_Keyboard, false, m_hWnd);

	gEnv->pConsole->ExecuteString("map MyHelloWorld");
	gEnv->p3DEngine->GetTimeOfDay()->SetTime(14, true);

	m_pViewPort->Initialize(m_pSceneStartup, &m_sceneController);	
	m_sceneController.Initialize(m_pSceneStartup, VT_CAMERA, m_pViewPort);
	m_sceneController.StartGameMode();

	SetWindowText(APP_CAPTION);

	return TRUE;
}


bool CLauncherWindow::InitSystem(HWND hWnd)
{
	HANDLE mutex = CreateMutex(NULL, TRUE, "CrytekApplication");

	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		if(MessageBox("There is already a CRYTEK application running.", "Error", MB_OK))
			return false;
	}

	m_pSceneStartup = new CSceneStartup();
	if (!m_pSceneStartup)
	{
		MessageBox( "Failed to create the GameStartup Interface!", "Error", MB_OK);
		CloseHandle(mutex);
		return false;
	}

	SSystemInitParams startupParams;
	memset(&startupParams, 0, sizeof(SSystemInitParams));

	startupParams.hWnd					= hWnd;
	startupParams.hInstance				= GetModuleHandle(0);	
	startupParams.pUserCallback			= this;
	startupParams.sLogFileName			= "UILauncher.log";
	startupParams.bIsUIApplication		= true;

	// this will enable CVars
	sprintf_s(startupParams.szSystemCmdLine, 256, "APP -devmode");

	if (!m_pSceneStartup->Init(startupParams))
	{
		MessageBox( "Failed to initialize CryEngine Framework!", "Error", MB_OK);
		CloseHandle(mutex);
		return false;
	}

	CloseHandle(mutex);
	return true;
}


void CLauncherWindow::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		m_sceneController.Update();
		m_pViewPort->Refresh();
	}
}


void CLauncherWindow::OnSize(UINT nType, int cx, int cy)
{
	CRect rect;
	GetClientRect(&rect);

	if (m_pViewPort)
		m_pViewPort->MoveWindow(5, 5, rect.right-10, rect.bottom-10); 
}


BOOL CLauncherWindow::PreTranslateMessage(MSG* pMsg)
{
	ISystemEventDispatcher* sed = gEnv->pSystem->GetISystemEventDispatcher();
	LPARAM lParam = pMsg->lParam;
	WPARAM wParam = pMsg->wParam;

	if (gEnv) switch(pMsg->message)
	{
		// forward mouse/key events to CryEngine

	case WM_LBUTTONDOWN:
		m_sceneController.StartGameMode();

	case WM_KEYDOWN:
		if (wParam==VK_ESCAPE)
		{
			m_sceneController.StopGameMode();
			return 0;
		}
		if (wParam==VK_RETURN)
			return 0;
		break;

	case WM_CHAR:
		{
		/*	if (!m_sceneController.IsInGameMode())
				return 0;*/

			// forward chars to CryEngine
			if (gEnv && gEnv->pInput)
			{
				SInputEvent event;
				event.modifiers = gEnv->pInput->GetModifiers();
				event.deviceId = eDI_Keyboard;
				event.state = eIS_UI;
				event.value = 1.0f;
				event.pSymbol = 0;
				if (event.pSymbol)
					event.keyId = event.pSymbol->keyId;

				wchar_t tmp[2] = { 0 };
				MultiByteToWideChar(CP_ACP, 0, (char*)&wParam, 1, tmp, 2);
				event.timestamp = tmp[0];

				char szKeyName[4] = {0};
				if (wctomb(szKeyName, (WCHAR)wParam) != -1)
				{
					if (szKeyName[1]==0 && ((unsigned char)szKeyName[0])>=32)
					{
						event.keyName = szKeyName;
						gEnv->pInput->PostInputEvent(event);
					}
				}
			}
		}
		break;

		// command handling

	case WM_COMMAND:
		if (HandleCommand(pMsg->wParam))
			return 0;
		break;
	}

	return CDialog::PreTranslateMessage(pMsg);
}


bool CLauncherWindow::HandleCommand(WPARAM param)
{
	
	//m_pViewPort->Refresh();

	switch (param)
	{
	case ID_CLOSE_BUTTON:
		gEnv->pSystem->Quit();
		return true;

	case ID_SPECTATOR_1STPERSON:
		m_sceneController.SetViewTarget(VT_FIRST_PERSON);
		m_sceneController.StartGameMode();
		return true;

	case ID_VIEW_SPECTATOR:
		m_sceneController.SetViewTarget(VT_SPECTATOR);
		m_sceneController.StartGameMode();
		return true;

	case ID_VIEW_CAMERA:
		m_sceneController.SetViewTarget(VT_CAMERA);
		m_sceneController.StartGameMode();
		return true;

	case ID_FILE_OPEN:
		{
			char strFilter[] = { "CRY Files (*.cry)|*.cry|All Files (*.*)|*.*||" };
			CFileDialog* dlg = new CFileDialog(true, "*.cry", NULL, 0, strFilter, this);

			if (dlg->DoModal() == IDOK)
			{
				std::string fileName = dlg->GetFileName();
				char fn[_MAX_FNAME];
				_splitpath_s(fileName.c_str(), NULL, 0, NULL, 0, fn, _MAX_FNAME, NULL, 0);

				LoadLevel((char*)&fn);
			}
			return true;
		}

	case ID_FILE_QUIT:
		gEnv->pSystem->Quit();
		return true;

	case ID_AI_RELOAD:
		gEnv->pConsole->ExecuteString("#AIReload()");
		return true;
	}

	return false;
}


void CLauncherWindow::LoadLevel(const std::string &name)
{
	m_pSceneStartup->Reset(GAME_DLL_URL);
	gEnv->pConsole->ExecuteString((std::string("map ") + name).c_str());
}


void CLauncherWindow::OnDestroy()
{
	m_pSceneStartup->Shutdown();
}


bool CLauncherWindow::OnError( const char *szErrorString )
{
	::MessageBox(0, szErrorString, "Error in Application", MB_OK);
	return true;
}


void CLauncherWindow::OnInitProgress( const char *sProgressMsg )
{ 
	std::string text = APP_CAPTION " - ";
	text += std::string(sProgressMsg);

	SetWindowText(text.c_str());
}
