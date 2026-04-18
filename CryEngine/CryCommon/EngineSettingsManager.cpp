#include "StdAfx.h"


// #include <CryModuleDefs.h>
#include "EngineSettingsManager.h"

#pragma warning (disable:4312)
#pragma warning(disable: 4244)

#if defined(WIN32) || defined(WIN64)

#include <assert.h>										// assert()
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>


// pseudo-variable that represents the DOS header of the module
EXTERN_C IMAGE_DOS_HEADER __ImageBase;


#define INFOTEXT		 "Please specify the directory of your CryENGINE2 installation (RootPath):"


static bool g_bWindowQuit;
static CEngineSettingsManager *g_pThis	= 0;
static const unsigned int IDC_hEditRootPath	= 100;
static const unsigned int IDC_hBtnBrowse		= 101;


BOOL BrowseForFolder(HWND hWnd, LPCTSTR szInitialPath, LPTSTR szPath, LPCTSTR szTitle);

// Desc: Static msg handler which passes messages to the application class.
LRESULT static CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{ 
	assert(g_pThis);

	if (uMsg == WM_INITDIALOG) 
	{
		int f = 0;
	}

	return g_pThis->HandleProc(hWnd, uMsg, wParam, lParam);
}



//////////////////////////////////////////////////////////////////////////
CEngineSettingsManager::CEngineSettingsManager(const char* moduleName, const char* iniFileName)
	: m_hWndParent(0)
{
	m_sModuleName = "";

	// std initialization
	RestoreDefaults();

	// try to load content from INI file
	if (moduleName != NULL)
	{
		m_sModuleName = string(moduleName);

		if (iniFileName==NULL)
		{
			// find INI filename located in module path
			HMODULE hInstance = GetModuleHandle(moduleName);
			char szFilename[_MAX_PATH];
			GetModuleFileName((HINSTANCE)&__ImageBase, szFilename, _MAX_PATH);
			char drive[_MAX_DRIVE];
			char dir[_MAX_DIR];
			char fname[_MAX_FNAME];
			_splitpath( szFilename,drive,dir,fname, NULL );
			_makepath( szFilename,drive,dir,fname, "INI" );
			m_sModuleFileName = string(szFilename);
		}
		else
			m_sModuleFileName = iniFileName;

		if (LoadValuesFromConfigFile(m_sModuleFileName.c_str()))
		{
			m_bGetDataFromRegistry = false;
			return;
		}
	}

	m_bGetDataFromRegistry = true;

	// load basic content from registry
	LoadEngineSettingsFromRegistry();
}


//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::RestoreDefaults()
{
	// Engine
	SetKey("ENG_RootPath", "");

	// RC
	SetKey("RC_ShowWindow", false);
	SetKey("RC_HideCustom", false);
	SetKey("RC_Parameters", "");

	// Editor
	SetKey("EDT_Prefer32Bit", false);
}


//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::SetRootPath( const string& szRootPath )
{ 
	int len = (int)szRootPath.length();
	char* path = new char[len+1];
	memcpy(path, szRootPath.c_str(), len+1);
	if (len>0 && (szRootPath[len-1]=='\\' || szRootPath[len-1]=='/'))
		path[len-1] = 0;

	SetKey("ENG_RootPath", string(path));
}


//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::HasKey(const string& sKey, bool bForcePull)
{
	bool bIsInCache = m_keyToValueMap.find(sKey)!=m_keyToValueMap.end();

	if ((!bIsInCache || bForcePull) && m_bGetDataFromRegistry )
	{
		char szKey[512] = {"Software\\Crytek\\Settings"};
		string sResult;

		RegKey key(szKey, false);
		if (key.pKey && GetRegValue(key.pKey, sKey, sResult))
		{
				SetKey(sKey, sResult);
				return true;
		}
		return false;
	}

	return bIsInCache;
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::SetKey(const string& key, const char* value)
{
	m_keyToValueMap[key] = value;
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::SetKey(const string& key, const string& value)
{
	m_keyToValueMap[key] = value;
}


//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::SetKey(const string& key, bool value)
{
	m_keyToValueMap[key] = value ? "true" : "false";
}


//////////////////////////////////////////////////////////////////////////
string CEngineSettingsManager::GetRootPath() 
{ 
	LoadEngineSettingsFromRegistry();
	return GetValue<string>("ENG_RootPath"); 
}


//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::CallSettingsDialog(void* pParent)
{
	HWND hParent = (HWND)pParent;

	string rootPath = GetRootPath();

	if (rootPath.empty())
	{
		CallRootPathDialog(hParent);
		return;
	}

	if (FindWindow(NULL,"CryENGINE2 Settings"))
		return;

	string params = string(m_sModuleName+" \""+m_sModuleFileName+"\"");
	HINSTANCE res = ::ShellExecute(hParent, "open", "Tools\\SettingsMgr.exe", params.c_str(), rootPath.c_str(), SW_SHOWNORMAL);

	if (res<(HINSTANCE)33)
	{
		MessageBox(hParent,"Could not execute CryENGINE2 Settings dialog.\nPlease verify RootPath.","Error",MB_OK|MB_ICONERROR);
		CallRootPathDialog(hParent);
		return;
	}

	Sleep(1000);
}


//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::CallRootPathDialog(void* pParent)
{
	HWND hParent = (HWND)pParent;

	g_bWindowQuit = false;
	g_pThis = this;

	const char *szWindowClass = "CRYENGINEROOTPATHUI";

	// Register the window class
	WNDCLASS wndClass = { 0, WndProc, 0, DLGWINDOWEXTRA, GetModuleHandle(0), 
		NULL, LoadCursor( NULL, IDC_ARROW ), (HBRUSH)COLOR_BTNSHADOW, NULL, szWindowClass };

	RegisterClass(&wndClass);

	bool bReEnableParent=false;

	if(IsWindowEnabled(hParent))
	{
		bReEnableParent=true;
		EnableWindow(hParent,false);
	}

	int cwX = CW_USEDEFAULT;
	int cwY = CW_USEDEFAULT;
	int cwW = 400+2*GetSystemMetrics(SM_CYFIXEDFRAME);
	int cwH = 92+2*GetSystemMetrics(SM_CYFIXEDFRAME)+GetSystemMetrics(SM_CYSMCAPTION);

	if (hParent!=NULL)
	{
		// center window over parent
		RECT rParentRect;
		GetWindowRect(hParent, &rParentRect);
		cwX = rParentRect.left + (rParentRect.right-rParentRect.left)/2 - cwW/2;
		cwY = rParentRect.top + (rParentRect.bottom-rParentRect.top)/2 - cwH/2;
	}

	// Create the window
	HWND hDialogWnd = CreateWindowEx( WS_EX_TOOLWINDOW|WS_EX_CONTROLPARENT,szWindowClass,"CryENGINE2 RootPath",WS_BORDER|WS_CAPTION|WS_SYSMENU|WS_VISIBLE, 
		cwX,cwY,cwW,cwH,hParent,NULL,GetModuleHandle(0),NULL);

	// ------------------------------------------

	LoadEngineSettingsFromRegistry();

	HINSTANCE hInst = GetModuleHandle(0);
	HGDIOBJ hDlgFont = GetStockObject (DEFAULT_GUI_FONT);

	// Engine Root Path

	HWND hStat0 = CreateWindow("STATIC",INFOTEXT, WS_CHILD | WS_VISIBLE,
		8,8,380,16, hDialogWnd,NULL, hInst, NULL);
	SendMessage(hStat0,WM_SETFONT,(WPARAM)hDlgFont,FALSE);

	HWND hWndRCPath = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT",GetValue<string>("ENG_RootPath").c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP| ES_READONLY,
		8,32,344,22, hDialogWnd,(HMENU)IDC_hEditRootPath, hInst, NULL);
	SendMessage(hWndRCPath,WM_SETFONT,(WPARAM)hDlgFont,FALSE);

	m_hBtnBrowse = CreateWindow("BUTTON","...", WS_CHILD | WS_VISIBLE,
		360,32,32,22, hDialogWnd,(HMENU)IDC_hBtnBrowse, hInst, NULL);
	SendMessage((HWND)m_hBtnBrowse,WM_SETFONT,(WPARAM)hDlgFont,FALSE);

	// std buttons

	HWND hWndOK = CreateWindow("BUTTON","OK", WS_CHILD | BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE | ES_LEFT | WS_TABSTOP,
		130,62,70,22, hDialogWnd,(HMENU)IDOK, hInst, NULL);
	SendMessage(hWndOK,WM_SETFONT,(WPARAM)hDlgFont,FALSE);

	HWND hWndCancel = CreateWindow("BUTTON","Cancel", WS_CHILD | WS_CHILD | WS_VISIBLE | ES_LEFT | WS_TABSTOP,
		210,62,70,22, hDialogWnd,(HMENU)IDCANCEL, hInst, NULL);
	SendMessage(hWndCancel,WM_SETFONT,(WPARAM)hDlgFont,FALSE);


	SetFocus(hWndRCPath);

	// ------------------------------------------

	{
		MSG msg;

		while(!g_bWindowQuit) 
		{
			GetMessage(&msg, NULL, 0, 0);

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	// ------------------------------------------

	DestroyWindow(hDialogWnd);
	UnregisterClass(szWindowClass,GetModuleHandle(0));

	if(bReEnableParent)
		EnableWindow(hParent,true);

	BringWindowToTop(hParent);

	g_pThis = NULL;
}


//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::SetParentDialog(unsigned long window)
{
	m_hWndParent = window;
}


//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::StoreData()
{
	if (m_bGetDataFromRegistry)
	{
		bool res = StoreEngineSettingsToRegistry();
		
		if (!res)
			MessageBox((HWND)m_hWndParent,"Could not store data to registry.","Error",MB_OK | MB_ICONERROR);
		return res;
	}

	// store data to INI file

	FILE* file;	
	fopen_s(&file, m_sModuleFileName.c_str(), "wb");
	if (file==NULL)
		return false;

	for(TKeyValueMap::iterator it=m_keyToValueMap.begin();it!=m_keyToValueMap.end();it++)
		fprintf_s(file, (it->first+" = "+it->second+"\r\n").c_str());

	fclose(file);

	return true;
}


//////////////////////////////////////////////////////////////////////////
string CEngineSettingsManager::Trim(string& str)
{
	int begin = 0;
	while (begin<str.length() && (str[begin]==' ' || str[begin]=='\r' || str[begin]=='\t' || str[begin]=='\n'))
		begin++;
	int end = int(str.length()-1);
	while (end>begin && (str[end]==' ' || str[end]=='\r' || str[end]=='\t' || str[end]=='\n'))
		end--;

	return str.substr(begin,end-begin+1);
}


//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::LoadValuesFromConfigFile(const char* szFileName)
{
	m_keyToValueMap.clear();

	// read file to memory

	FILE* file;	
	fopen_s(&file, szFileName, "rb");
	if (file==NULL)
		return false;

	fseek(file,0,SEEK_END);
	long size = ftell(file);
	fseek(file,0,SEEK_SET);
	char* data = new char[size+1];
	fread_s(data,size,1,size,file);
	fclose(file);

	// parse file for root path

	int start = 0, end = 0;
	while (end<size)
	{
		while (data[end]!='\n' && end<size)
			end++;

		memcpy(data,&data[start],end-start);
		data[end-start] = 0;
		start = end = end+1;

		string line(data);		
		int equalsOfs = int(line.find('='));
		if (equalsOfs>=0)
		{
			string key(Trim(line.substr(0, equalsOfs)));
			string value(Trim(line.substr(equalsOfs+1)));

			// Stay compatible to deprecated rootpath key
			if (key=="RootPath")
			{
				key = "ENG_RootPath";
				if(value[value.length()-1]=='\\' || value[value.length()-1]=='/')
					value = value.substr(0, value.length()-1);
			}

			m_keyToValueMap[key] = value;
		}
	}
	delete [] data;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::SetRegValue(void* key, const string& valueName, const string& value)
{
	return (ERROR_SUCCESS == RegSetValueEx((HKEY)key,(LPCSTR)valueName.c_str(), 0, REG_SZ, (BYTE*)value.c_str(), DWORD(value.length()+1)));
}


//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::SetRegValue(void* key, const string& valueName, bool value)
{
	DWORD dwVal = value;
	return (ERROR_SUCCESS == RegSetValueEx((HKEY)key,(LPCSTR)valueName.c_str(), 0, REG_DWORD, (BYTE *)&dwVal, sizeof(dwVal)));
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::SetRegValue(void* key, const string& valueName, int value)
{
	DWORD dwVal = value;
	return (ERROR_SUCCESS == RegSetValueEx((HKEY)key,(LPCSTR)valueName.c_str(), 0, REG_DWORD, (BYTE *)&dwVal, sizeof(dwVal)));
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetRegValue(void* key, const string& valueName, string& value)
{
	// Open the appropriate registry key
	TCHAR strPath[MAX_PATH];
	DWORD type, size = MAX_PATH;

	bool res = (ERROR_SUCCESS == RegQueryValueEx( (HKEY)key, (LPCSTR)valueName.c_str(), NULL, &type, (BYTE*)strPath, &size));
	if( res )
		value = string(strPath);

	return res;
}


//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetRegValue(void* key, const string& valueName, bool& value)
{
	// Open the appropriate registry key
	DWORD type, dwVal=0, size = sizeof(dwVal);
	bool res = (ERROR_SUCCESS == RegQueryValueEx( (HKEY)key, (LPCSTR)valueName.c_str(), NULL, &type, (BYTE*)&dwVal, &size));
	if( res )
	{
		value = dwVal!=0;
	}
	else
	{
		string valueStr;
		res = GetRegValue( key, valueName, valueStr );
		if ( res )
		{
			value = ( valueStr == "true" );
		}
	}
	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetRegValue(void* key, const string& valueName, int& value)
{
	// Open the appropriate registry key
	DWORD type, dwVal=0, size = sizeof(dwVal);

	bool res = (ERROR_SUCCESS == RegQueryValueEx( (HKEY)key, (LPCSTR)valueName.c_str(), NULL, &type, (BYTE*)&dwVal, &size));
	if( res )
		value = dwVal;

	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::StoreEngineSettingsToRegistry()
{
	if (!m_bGetDataFromRegistry)
		return true;

	bool bRet=true;

	// make sure the path in registry exists

	RegKey key1("Software\\Crytek", true);
	if (!key1.pKey)
	{
		RegKey key0("Software", true);
		HKEY hKey1;
		RegCreateKey((HKEY)key0.pKey, "Crytek", &hKey1);
		if(!hKey1)
			return false;
	}

	RegKey key2("Software\\Crytek\\Settings", true);
	if (!key2.pKey)
	{
		RegKey key1("Software\\Crytek", true);
		HKEY hKey2;
		RegCreateKey((HKEY)key1.pKey, "Settings", &hKey2);
		if(!hKey2)
			return false;
	}


	RegKey key("Software\\Crytek\\Settings", true);
	if (!key.pKey)
	{
		bRet = false;
	}
	else
	{
		// Engine Specific
		bRet &= SetRegValue(key.pKey,"ENG_RootPath", GetValue<string>("ENG_RootPath"));

		// ResourceCompiler Specific
		bRet &= SetRegValue(key.pKey,"RC_ShowWindow", GetValue<string>("RC_ShowWindow")=="true");
		bRet &= SetRegValue(key.pKey,"RC_HideCustom", GetValue<string>("RC_HideCustom")=="true");
		bRet &= SetRegValue(key.pKey,"RC_Parameters", GetValue<string>("RC_Parameters"));

		// Editor Specific
		bRet &= SetRegValue(key.pKey,"EDT_Prefer32Bit", GetValue<string>("EDT_Prefer32Bit")=="true");

		for(TKeyValueMap::iterator it=m_keyToValueMap.begin();it!=m_keyToValueMap.end();it++)
			bRet &= SetRegValue(key.pKey,it->first, it->second);
	}

	return bRet;
}


//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::LoadEngineSettingsFromRegistry()
{
	if (!m_bGetDataFromRegistry)
		return;

	char szKey[512] = {"Software\\Crytek\\Settings"};

	string sResult;
	bool bResult;

  // Engine Specific (Deprecated value)
	RegKey key(szKey, false);
	if (key.pKey)
	{
		if (GetRegValue(key.pKey, "RootPath", sResult))
			SetKey("ENG_RootPath",sResult);

		// Engine Specific 
		if (GetRegValue(key.pKey, "ENG_RootPath", sResult))
			SetKey("ENG_RootPath",sResult);

		// ResourceCompiler Specific
		if (GetRegValue(key.pKey, "RC_ShowWindow", bResult))
			SetKey("RC_ShowWindow",bResult);
		if (GetRegValue(key.pKey, "RC_HideCustom", bResult))
			SetKey("RC_HideCustom",bResult);
		if (GetRegValue(key.pKey, "RC_Parameters", sResult))
			SetKey("RC_Parameters",sResult);

		// Editor Specific
		if (GetRegValue(key.pKey, "EDT_Prefer32Bit", bResult))
			SetKey("EDT_Prefer32Bit",bResult);
	}
}


//////////////////////////////////////////////////////////////////////////
long CEngineSettingsManager::HandleProc(void* pWnd, long uMsg, long wParam, long lParam)
{
	HWND hWnd = (HWND)pWnd;

	switch(uMsg)
	{
	case WM_CREATE:
		break;

	case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
			case IDC_hBtnBrowse:
				{
					HWND hWndEdit = GetDlgItem(hWnd, IDC_hEditRootPath);
					LPSTR target[MAX_PATH];
					BrowseForFolder(hWnd,NULL, (LPTSTR)&target, INFOTEXT);
					SetWindowText(hWndEdit, (LPTSTR)&target);
				}
				break;

			case IDOK:
				{
					HWND hItemWnd = GetDlgItem(hWnd, IDC_hEditRootPath);
					char szPath[MAX_PATH];
					GetWindowText(hItemWnd, szPath, MAX_PATH);
					SetRootPath(szPath);
					StoreData();
				}

			case IDCANCEL:
				{
					g_bWindowQuit=true;
					break;
				}
			}
			break;
		}

	case WM_CLOSE:
		{
			g_bWindowQuit = true;
			break;
		}
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}


//////////////////////////////////////////////////////////////////////////
BOOL BrowseForFolder(HWND hWnd, LPCTSTR szInitialPath, LPTSTR szPath, LPCTSTR szTitle)
{
	TCHAR szDisplay[MAX_PATH];

	CoInitialize(NULL);

	BROWSEINFO bi = { 0 };
	bi.hwndOwner = hWnd;
	bi.pszDisplayName = szDisplay;
	bi.lpszTitle = szTitle;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpfn = NULL;
	bi.lParam = (LPARAM)szInitialPath;

	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);

	if (pidl != NULL)
	{
		BOOL retval = SHGetPathFromIDList(pidl, szPath);
		CoTaskMemFree(pidl);
		CoUninitialize();
		return TRUE;
	}

	szPath[0] = 0;
	CoUninitialize();
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::GetValueByRef(const string& key, string& value, bool bForcePull)
{
	if (HasKey(key, bForcePull))
		value = m_keyToValueMap[key];
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::GetValueByRef(const string& key, int& value, bool bForcePull)
{
	if (HasKey(key, bForcePull))
		value = strtol(m_keyToValueMap[key].c_str(), 0, 10);
}

//////////////////////////////////////////////////////////////////////////
CEngineSettingsManager::RegKey::RegKey(const string& key, bool writeable)
{
	HKEY  hKey;
	REGSAM access = (writeable ? KEY_WRITE : KEY_READ);
	LONG result = RegOpenKeyEx( HKEY_CURRENT_USER,key.c_str(),0, access, &hKey );
	pKey = hKey;
}

//////////////////////////////////////////////////////////////////////////
CEngineSettingsManager::RegKey::~RegKey()
{
	RegCloseKey((HKEY)pKey);
}

#endif //(WIN32) || (WIN64)
