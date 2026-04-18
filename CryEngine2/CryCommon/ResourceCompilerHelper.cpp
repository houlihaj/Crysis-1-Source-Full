// #include <CryModuleDefs.h>

#include "StdAfx.h"

#undef eCryModule
#define eCryModule eCryM_System

#undef RC_EXECUTABLE
#define RC_EXECUTABLE			"rc.exe"

#include "ResourceCompilerHelper.h"
#include "LineStreamBuffer.h"


#pragma warning (disable:4312)


#if defined(WIN32) || defined(WIN64)
#include <windows.h>		
#include <shellapi.h> //ShellExecute()

// pseudo-variable that represents the DOS header of the module
EXTERN_C IMAGE_DOS_HEADER __ImageBase;


//static bool g_bWindowQuit;
//static CResourceCompilerHelper *g_pThis=0;
/*
static const uint32 IDC_hWndRCPath	= 100;
static const uint32 IDC_hWndPickRCPath	=	101;
static const uint32 IDC_hWndTest	=	102;
static const uint32 IDC_hBtnShowWindow	=	103;
static const uint32 IDC_hBtnHideCustom	=	104;
static const uint32 IDC_hBtnPrefer32Bit	=	105;
static const uint32 IDC_hBtnStatic	=	106;
*/

//////////////////////////////////////////////////////////////////////////
CResourceCompilerHelper::CResourceCompilerHelper(const char* moduleName) : 
	m_bErrorFlag(false)
{
	m_settingsManager = new CEngineSettingsManager(moduleName);
}


//////////////////////////////////////////////////////////////////////////
string CResourceCompilerHelper::GetRootPath(bool pullFromRegistry)
{
	return pullFromRegistry ? m_settingsManager->GetRootPath() : m_settingsManager->GetValue<string>("ENG_RootPath");
}

//////////////////////////////////////////////////////////////////////////
void CResourceCompilerHelper::ResourceCompilerUI( void* hParent )
{
	m_settingsManager->CallSettingsDialog(hParent);
}


class ResourceCompilerLineHandler
{
public:
	ResourceCompilerLineHandler(IResourceCompilerListener* listener): m_listener(listener) {}
	void HandleLine(const char* line)
	{
		if (m_listener)
		{
			// Check the first character to see if it's a warning or error.
			IResourceCompilerListener::MessageSeverity severity = IResourceCompilerListener::MessageSeverity_Info;
			switch (line[0])
			{
			case 'E': severity = IResourceCompilerListener::MessageSeverity_Error; break;
			case 'W': severity = IResourceCompilerListener::MessageSeverity_Warning; break;
			}

			m_listener->OnRCMessage(severity, line + 2); // +2 to skip the prefix.
		}
	}

private:
	IResourceCompilerListener* m_listener;
};

//////////////////////////////////////////////////////////////////////////
bool CResourceCompilerHelper::CallResourceCompiler( const char *szFileName, const char *szAdditionalSettings, IResourceCompilerListener* listener, bool mayShowWindow, bool bUseQuota, bool pullFromRegistry, bool silent,bool boNoUserDialog,const char *szWorkingDirectory)
{
	// make command for execution
	char szRemoteCmdLine[MAX_PATH*3];

	if(!szAdditionalSettings)
		szAdditionalSettings="";				// better than using default values - the compiler might mess that up

	string path = GetRootPath(pullFromRegistry);

	if (path.size()==0)
	{
		path=".";
	}

	char szRemoteDirectory[512];
	// we use /nooutput because the file name from Photoshop is temporary and not the one we want to use
	sprintf(szRemoteDirectory, "%s/Bin32/rc",path.c_str());

	const char *szHideCustom = ((m_settingsManager->GetValue<string>("HideCustom")=="true")|| boNoUserDialog) ? "" : " /userdialogcustom=0";

	// we use /nooutput because the file name from Photoshop is temporary and not the one we want to use

	if(!szFileName)
	{
		sprintf(szRemoteCmdLine, "%s/rc.exe",szRemoteDirectory);
	}
	else
	{
		if (bUseQuota)
		{
			sprintf(szRemoteCmdLine, "%s/rc.exe \"%s\" %s %s %s",szRemoteDirectory,szFileName,boNoUserDialog?"":"/userdialog=1",szAdditionalSettings,szHideCustom);
		}
		else
		{
			sprintf(szRemoteCmdLine, "%s/rc.exe %s %s %s %s",szRemoteDirectory,szFileName,boNoUserDialog?"":"/userdialog=1",szAdditionalSettings,szHideCustom);
		}
	}

	// Create a pipe to read the stdout of the RC.
	SECURITY_ATTRIBUTES saAttr;
	std::memset(&saAttr, 0, sizeof(saAttr));
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = 0;
	HANDLE hChildStdOutRd, hChildStdOutWr;
	CreatePipe(&hChildStdOutRd, &hChildStdOutWr, &saAttr, 0);
	SetHandleInformation(hChildStdOutRd, HANDLE_FLAG_INHERIT, 0); // Need to do this according to MSDN
	HANDLE hChildStdInRd, hChildStdInWr;
	CreatePipe(&hChildStdInRd, &hChildStdInWr, &saAttr, 0);
	SetHandleInformation(hChildStdInWr, HANDLE_FLAG_INHERIT, 0); // Need to do this according to MSDN

	STARTUPINFO si;
	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	si.dwX = 100;
	si.dwY = 100;
	si.hStdError = hChildStdOutWr;
	si.hStdOutput = hChildStdOutWr;
	si.hStdInput = hChildStdInRd;
	si.dwFlags = STARTF_USEPOSITION | STARTF_USESTDHANDLES;

	PROCESS_INFORMATION pi;
	ZeroMemory( &pi, sizeof(pi) );

	if( !CreateProcess( NULL, // No module name (use command line). 
		szRemoteCmdLine,				// Command line. 
		NULL,									  // Process handle not inheritable. 
		NULL,									  // Thread handle not inheritable. 
		TRUE,								  // Set handle inheritance to TRUE. 
		mayShowWindow && (m_settingsManager->GetValue<string>("ShowWindow")=="true")?0:CREATE_NO_WINDOW,	// creation flags. 
		NULL,									  // Use parent's environment block. 
		szWorkingDirectory?szWorkingDirectory:szRemoteDirectory,			// Set starting directory. 
		&si,										// Pointer to STARTUPINFO structure.
		&pi ))									  // Pointer to PROCESS_INFORMATION structure.
	{
	char szMessage[65535]="";
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,NULL,GetLastError(),0,szMessage,65354,NULL);
	GetCurrentDirectory(65534,szMessage);
		if (!silent)
			MessageBox(0,"ResourceCompiler was not found.\n\nPlease verify CryENGINE2 RootPath.","Error",MB_ICONERROR|MB_OK);
		return false;
	}

	// Close the pipe that writes to the child process, since we don't actually have any input for it.
	CloseHandle(hChildStdInWr);

	// Read all the output from the child process.
	CloseHandle(hChildStdOutWr);
	ResourceCompilerLineHandler lineHandler(listener);
	LineStreamBuffer lineBuffer(&lineHandler, &ResourceCompilerLineHandler::HandleLine);
	for (;;)
	{
		char buffer[2048];
		DWORD bytesRead;
		if (!ReadFile(hChildStdOutRd, buffer, sizeof(buffer) / sizeof(buffer[0]), &bytesRead, NULL) || bytesRead == 0)
			break; 
		lineBuffer.HandleText(buffer, bytesRead);
	} 

	// Wait until child process exits.
	WaitForSingleObject( pi.hProcess, INFINITE );

	// Close process and thread handles. 
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CResourceCompilerHelper::IsPrefer32Bit()
{ 
	return m_settingsManager->GetValue<string>("EDT_Prefer32Bit")=="true"; 
}


//////////////////////////////////////////////////////////////////////////
bool CResourceCompilerHelper::InvokeResourceCompiler( const char *szSrcFile, const char *szDestFile, const char *szDataFolder, const bool bWindow ) const
{
	bool bRet=true;

	// make command for execution
	char szRemoteCmdLine[512];
	char szMasterCDDir[256];
	char szDir[512];

	GetCurrentDirectory(256,szMasterCDDir);

	sprintf(szRemoteCmdLine, "Bin32/rc/%s \"%s/%s/%s\" /userdialog=0", RC_EXECUTABLE, szMasterCDDir,szDataFolder,szSrcFile);

	sprintf(szDir, "%s\\Bin32\\rc", szMasterCDDir);

	STARTUPINFO si;
	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	si.dwX = 100;
	si.dwY = 100;
	si.dwFlags = STARTF_USEPOSITION;

	PROCESS_INFORMATION pi;
	ZeroMemory( &pi, sizeof(pi) );

	if( !CreateProcess( NULL, // No module name (use command line). 
		szRemoteCmdLine,				// Command line. 
		NULL,             // Process handle not inheritable. 
		NULL,             // Thread handle not inheritable. 
		FALSE,            // Set handle inheritance to FALSE. 
		bWindow?0:CREATE_NO_WINDOW,	// creation flags. 
		NULL,             // Use parent's environment block. 
		szDir,					  // Set starting directory. 
		&si,              // Pointer to STARTUPINFO structure.
		&pi )             // Pointer to PROCESS_INFORMATION structure.
		) 
	{
		bRet=false;
	}

	// Wait until child process exits.
	WaitForSingleObject( pi.hProcess, INFINITE );

	// Close process and thread handles. 
	CloseHandle( pi.hProcess );
	CloseHandle( pi.hThread );

	return bRet;
}


//////////////////////////////////////////////////////////////////////////
string CResourceCompilerHelper::GetEditorExecutable()
{
	string editorExe = m_settingsManager->GetRootPath();

	if(editorExe.empty())
	{
		MessageBox(NULL, "Can't Find the Material Editor.\nPlease, setup correct CryENGINE2 root path in the engine settings dialog", "Error", MB_ICONERROR | MB_OK);
		ResourceCompilerUI(0);
		editorExe = m_settingsManager->GetRootPath();
		if(editorExe.empty())
			return editorExe;
	}

	if (m_settingsManager->GetValue<string>("EDT_Prefer32Bit")=="true")
		editorExe += "/Bin32/Editor.exe";
	else
		editorExe += "/Bin64/Editor.exe";
	return editorExe;
}


//////////////////////////////////////////////////////////////////////////
bool CResourceCompilerHelper::IsError() const
{
	return m_bErrorFlag;
}


//////////////////////////////////////////////////////////////////////////
string CResourceCompilerHelper::GetInputFilename( const char *szFilePath, const unsigned int dwIndex ) const
{
	const char *ext = GetExtension(szFilePath);

	if(ext)
	{
		if(stricmp(ext,".dds")==0)
		{
			switch(dwIndex)
			{
				case 0: return ReplaceExtension(szFilePath,".tif");	// index 0
//					case 1: return ReplaceExtension(szFilePath,".srf");	// index 1
				default: return "";	// last one
			}
		}
	}

	if(dwIndex)
		return "";				// last one

	return szFilePath;	// index 0
}


//////////////////////////////////////////////////////////////////////////
bool CResourceCompilerHelper::IsDestinationFormat( const char *szExtension ) const
{
	if(stricmp(szExtension,"dds")==0)		// DirectX surface format
		return true;

	return false;
}


//////////////////////////////////////////////////////////////////////////
bool CResourceCompilerHelper::IsSourceFormat( const char *szExtension ) const
{
	if(stricmp(szExtension,"tif")==0)			// Crytek resource compiler image input format
//		|| stricmp(szExtension,"srf")==0)		// Crytek surface formats (e.g. normalmap)
		return true;

	return false;
}


//////////////////////////////////////////////////////////////////////////
void* CResourceCompilerHelper::CallEditor( void* pParent, const char * pWndName, const char * pFlag )
{
	HWND hWnd = ::FindWindow(NULL, pWndName);
	if(hWnd)
		return hWnd;
	else
	{
		string editorExecutable = GetEditorExecutable();
		//MessageBox(0,editorExecutable.c_str(),pFlag,MB_OK);

		if (editorExecutable!="")		
		{
			// sandbox does not correct in material editor only mode, so reset this flag.
//			pFlag = "";

			INT_PTR hIns = (INT_PTR)ShellExecute(NULL, "open", editorExecutable.c_str(), pFlag, NULL, SW_SHOWNORMAL);
			if(hIns<=32)
			{
				MessageBox(0,"Editor.exe was not found.\n\nPlease verify CryENGINE2 root path.","Error",MB_ICONERROR|MB_OK);
				ResourceCompilerUI(pParent);
				editorExecutable = GetEditorExecutable();
				ShellExecute(NULL, "open", editorExecutable.c_str(), pFlag, NULL, SW_SHOWNORMAL);
			}
		}
	}
	return 0;
}

#endif //(WIN32) || (WIN64)
