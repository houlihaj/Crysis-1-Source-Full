/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:	Include file for Windows (XP/Vista) registry setup and integration
							routines. (including game explorer integration and XP-Vista update)

-------------------------------------------------------------------------
History:
- 27:01:2006   10:54 : Created by Jan Müller

*************************************************************************/
#ifndef SETUP_BASE_INCLUDE_
#define SETUP_BASE_INCLUDE_


//#define _CRTDBG_MAP_ALLOC

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#undef IEntity
#include <rpcsal.h>
#include <gameux.h>
#include <strsafe.h>
#include <shlobj.h>

char* ConvertLPWSTRToLPSTR (LPWSTR lpwszStrIn, LPSTR pszOut)
{
	if (lpwszStrIn != NULL)
	{
		int nInputStrLen = int(wcslen (lpwszStrIn));

		// Double NULL Termination
		int nOutputStrLen = WideCharToMultiByte (CP_ACP, 0, lpwszStrIn, nInputStrLen, NULL, 0, 0, 0) + 2;
		pszOut = new char [nOutputStrLen];

		if (pszOut)
		{
			memset (pszOut, 0x00, nOutputStrLen);
			WideCharToMultiByte(CP_ACP, 0, lpwszStrIn, nInputStrLen, pszOut, nOutputStrLen, 0, 0);
		}
	}
	return pszOut;
}

HRESULT ConvertStringToGUID( const WCHAR* strSrc, GUID* pGuidDest )
{
	UINT aiTmp[10];

	if( swscanf( strSrc, L"{%8X-%4X-%4X-%2X%2X-%2X%2X%2X%2X%2X%2X}",
		&pGuidDest->Data1, 
		&aiTmp[0], &aiTmp[1], 
		&aiTmp[2], &aiTmp[3],
		&aiTmp[4], &aiTmp[5],
		&aiTmp[6], &aiTmp[7],
		&aiTmp[8], &aiTmp[9] ) != 11 )
	{
		ZeroMemory( pGuidDest, sizeof(GUID) );
		return E_FAIL;
	}
	else
	{
		pGuidDest->Data2       = (USHORT) aiTmp[0];
		pGuidDest->Data3       = (USHORT) aiTmp[1];
		pGuidDest->Data4[0]    = (BYTE) aiTmp[2];
		pGuidDest->Data4[1]    = (BYTE) aiTmp[3];
		pGuidDest->Data4[2]    = (BYTE) aiTmp[4];
		pGuidDest->Data4[3]    = (BYTE) aiTmp[5];
		pGuidDest->Data4[4]    = (BYTE) aiTmp[6];
		pGuidDest->Data4[5]    = (BYTE) aiTmp[7];
		pGuidDest->Data4[6]    = (BYTE) aiTmp[8];
		pGuidDest->Data4[7]    = (BYTE) aiTmp[9];
		return S_OK;
	}
}

//creates a link (game explorer tasks)
HRESULT CreateLink(LPCSTR lpszPathObj, LPCSTR lpszPathLink, LPCSTR lpszDesc)
{
	HRESULT hres;
	IShellLink* psl;

	// Get a pointer to the IShellLink interface.
	hres = CoInitialize( 0 );
	hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
		IID_IShellLink, (LPVOID*)&psl);
	if (SUCCEEDED(hres))
	{
		IPersistFile* ppf;

		// Set the path to the shortcut target and add the description.
		psl->SetPath(lpszPathObj);
		psl->SetDescription(lpszDesc);

		// Query IShellLink for the IPersistFile interface for saving the
		// shortcut in persistent storage.
		hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);

		if (SUCCEEDED(hres))
		{
			WCHAR wsz[MAX_PATH];

			// Ensure that the string is Unicode.
			MultiByteToWideChar(CP_ACP, 0, lpszPathLink, -1, wsz, MAX_PATH);

			// Save the link by calling IPersistFile::Save.
			hres = ppf->Save(wsz, TRUE);
			ppf->Release();
		}
		psl->Release();
	}
	return hres;
}

//--------------------------------------------------------------------------------------
// Adds a game to the Game Explorer
//--------------------------------------------------------------------------------------
STDAPI AddToGameExplorer( WCHAR* strGDFBinPath, WCHAR *strGameInstallPath, 
												 GAME_INSTALL_SCOPE InstallScope)
{
	HRESULT hr = E_FAIL;
	bool    bCleanupCOM = false;
	BOOL    bHasAccess = FALSE;
	BSTR    bstrGDFBinPath = NULL;
	BSTR    bstrGameInstallPath = NULL;
	IGameExplorer* pFwGameExplorer = NULL;

	if( strGDFBinPath == NULL || strGameInstallPath == NULL )
	{
		assert( false );
		return E_INVALIDARG;
	}

	bstrGDFBinPath = SysAllocString( strGDFBinPath );
	bstrGameInstallPath = SysAllocString( strGameInstallPath );
	if( bstrGDFBinPath == NULL || bstrGameInstallPath == NULL )
	{
		hr = E_OUTOFMEMORY;
		goto LCleanup;
	}


	hr = CoInitialize( 0 );
	bCleanupCOM = SUCCEEDED(hr); 

	GUID m_GUID;
	CoCreateGuid( &m_GUID ); 
	printf("Created GUID \n");

	// Create an instance of the Game Explorer Interface
	hr = CoCreateInstance( __uuidof(GameExplorer), NULL, CLSCTX_INPROC_SERVER, 
		__uuidof(IGameExplorer), (void**) &pFwGameExplorer );

	if( FAILED(hr) || pFwGameExplorer == NULL )
	{
		printf("Failed or no game explorer created: \n");
		// Depending on GAME_INSTALL_SCOPE, write to:
		//      HKLM\Software\Microsoft\Windows\CurrentVersion\GameUX\GamesToFindOnWindowsUpgrade\{GUID}\
		// or
		//      HKCU\Software\Classes\Software\Microsoft\Windows\CurrentVersion\GameUX\GamesToFindOnWindowsUpgrade\{GUID}\
		// and write there these 2 string values: GDFBinaryPath and GameInstallPath 
		//
		HKEY hKeyGamesToFind = NULL, hKeyGame = NULL;
		LONG lResult;
		DWORD dwDisposition;
		if( InstallScope == GIS_CURRENT_USER )
			lResult = RegCreateKeyEx( HKEY_CURRENT_USER, "Software\\Classes\\Software\\Microsoft\\Windows\\CurrentVersion\\GameUX\\GamesToFindOnWindowsUpgrade", 
			0, NULL, 0, KEY_WRITE, NULL, &hKeyGamesToFind, &dwDisposition );
		else
			lResult = RegCreateKeyEx( HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\GameUX\\GamesToFindOnWindowsUpgrade", 
			0, NULL, 0, KEY_WRITE, NULL, &hKeyGamesToFind, &dwDisposition );

		if(lResult == ERROR_SUCCESS) 
		{
			wchar_t strGameInstanceGUID[256];
			StringFromGUID2(m_GUID, strGameInstanceGUID, 256);

			char* tmp = 0;
			lResult = RegCreateKeyEx( hKeyGamesToFind, ConvertLPWSTRToLPSTR(strGameInstanceGUID, tmp), 0, NULL, 0, KEY_WRITE, NULL, &hKeyGame, &dwDisposition );
			delete []tmp;
			if(lResult == ERROR_SUCCESS) 
			{
				printf("Setting up Windows XP registry for update to Vista");
				size_t nGDFBinPath = 0, nGameInstallPath = 0;
				LPSTR temp1 = 0;
				LPSTR temp2 = 0;
				StringCchLength( ConvertLPWSTRToLPSTR(strGDFBinPath, temp1), MAX_PATH, &nGDFBinPath );
				StringCchLength( ConvertLPWSTRToLPSTR(strGameInstallPath, temp2), MAX_PATH, &nGameInstallPath );
				delete []temp1;
				delete []temp2;
				RegSetValueEx( hKeyGame, "GDFBinaryPath", 0, REG_SZ, (BYTE*)strGDFBinPath, (DWORD)((nGDFBinPath + 1)*sizeof(WCHAR)) );
				RegSetValueEx( hKeyGame, "GameInstallPath", 0, REG_SZ, (BYTE*)strGameInstallPath, (DWORD)((nGameInstallPath + 1)*sizeof(WCHAR)) );

				//write GUID in registry ...
				HKEY hk;
				DWORD dwDisp;
				if(InstallScope == GIS_ALL_USERS)
					hr = RegCreateKeyEx(HKEY_LOCAL_MACHINE, "Software", 0, NULL, REG_OPTION_NON_VOLATILE,
					KEY_WRITE, NULL, &hk, &dwDisp);
				else
					hr = RegCreateKeyEx(HKEY_CURRENT_USER, "Software", 0, NULL, REG_OPTION_NON_VOLATILE,
					KEY_WRITE, NULL, &hk, &dwDisp);

				hr = RegCreateKeyEx(hk, "Crytek\\Crysis", 0, NULL, REG_OPTION_NON_VOLATILE,
					KEY_WRITE, NULL, &hk, &dwDisp);
				WCHAR guidStringW[256];
				StringFromGUID2(m_GUID, guidStringW, 256);
				LPSTR temp = 0;
				char * guidString = ConvertLPWSTRToLPSTR(guidStringW, temp);
				delete []temp;
				std::string guidS(guidString);
				hr = RegSetValueExA(hk, "GUID", 0, REG_SZ, (const BYTE*)(guidS.c_str()), guidS.size());
			}
			if( hKeyGame ) RegCloseKey( hKeyGame );
		}
		if( hKeyGamesToFind )
			RegCloseKey( hKeyGamesToFind );
	}
	else
	{			//Windows Vista Game Explorer path
		printf("Game explorer available \n");
		hr = pFwGameExplorer->VerifyAccess( bstrGDFBinPath, &bHasAccess );
		if( SUCCEEDED(hr) || bHasAccess )
		{
			printf("Game explorer access verified - adding game \n");
			hr = pFwGameExplorer->AddGame( bstrGDFBinPath, bstrGameInstallPath, 
				InstallScope, &m_GUID );	//add Crysis to game explorer

			if( SUCCEEDED(hr))	//now save GUID to the registry
			{
				printf("Writing game instance ID in registry ...\n");

				HKEY hk;
				DWORD dwDisp;
				if(InstallScope == GIS_ALL_USERS)
					hr = RegCreateKeyEx(HKEY_LOCAL_MACHINE, "Software", 0, NULL, REG_OPTION_NON_VOLATILE,
					KEY_WRITE, NULL, &hk, &dwDisp);
				else
					hr = RegCreateKeyEx(HKEY_CURRENT_USER, "Software", 0, NULL, REG_OPTION_NON_VOLATILE,
					KEY_WRITE, NULL, &hk, &dwDisp);

				hr = RegCreateKeyEx(hk, "Crytek\\Crysis", 0, NULL, REG_OPTION_NON_VOLATILE,
					KEY_WRITE, NULL, &hk, &dwDisp);
				WCHAR guidStringW[256];
				StringFromGUID2(m_GUID, guidStringW, 256);
				LPSTR temp = 0;
				char * guidString = ConvertLPWSTRToLPSTR(guidStringW, temp);
				delete []temp;
				std::string guidS(guidString);
				hr = RegSetValueExA(hk, "GUID", 0, REG_SZ, (const BYTE*)(guidS.c_str()), guidS.size());
			}
		}
		else
			printf("Game explorer access failed ..\n");
	}

LCleanup:
	if( bstrGDFBinPath ) SysFreeString( bstrGDFBinPath );
	if( bstrGameInstallPath ) SysFreeString( bstrGameInstallPath );
	if( pFwGameExplorer ) pFwGameExplorer->Release();
	if( bCleanupCOM ) CoUninitialize();

	return hr;
}

//--------------------------------------------------------------------------------------
// Removes a game from the Game Explorer
//--------------------------------------------------------------------------------------
STDAPI RemoveFromGameExplorer( GUID *pInstanceGUID )
{   
	HRESULT hr = E_FAIL;
	bool    bCleanupCOM = false;
	IGameExplorer* pFwGameExplorer = NULL;

	hr = CoInitialize( 0 );
	bCleanupCOM = SUCCEEDED(hr); 

	// Create an instance of the Game Explorer Interface
	hr = CoCreateInstance( __uuidof(GameExplorer), NULL, CLSCTX_INPROC_SERVER, 
		__uuidof(IGameExplorer), (void**) &pFwGameExplorer );
	if( FAILED(hr) || pFwGameExplorer == NULL )
	{
		hr = E_FAIL;
		goto LCleanup;
	}

	// Remove the game from the Game Explorer
	hr = pFwGameExplorer->RemoveGame( *pInstanceGUID );

LCleanup:
	if( pFwGameExplorer ) pFwGameExplorer->Release();
	if( bCleanupCOM ) CoUninitialize();

	return hr;
}

//this adds game tasks to the game explorer and returns true on success
bool CreateGameExplorerTasks(WCHAR *gameBinaryPath, GAME_INSTALL_SCOPE InstallScope)
{
	//first retrieve game GUID
	char guidString[256];

	HKEY hk;
	DWORD dwDisp;
	HRESULT hr;
	if(InstallScope == GIS_ALL_USERS)
		hr = RegCreateKeyEx(HKEY_LOCAL_MACHINE, "Software", 0, NULL, REG_OPTION_NON_VOLATILE,
		KEY_QUERY_VALUE, NULL, &hk, &dwDisp);
	else
		hr = RegCreateKeyEx(HKEY_CURRENT_USER, "Software", 0, NULL, REG_OPTION_NON_VOLATILE,
		KEY_QUERY_VALUE, NULL, &hk, &dwDisp);

	hr = RegCreateKeyEx(hk, "Crytek\\Crysis", 0, NULL, REG_OPTION_NON_VOLATILE,
		KEY_QUERY_VALUE, NULL, &hk, &dwDisp);

	//get the guid from the registry
	DWORD size = 256;
	DWORD type;
	hr = RegQueryValueExA(hk, "GUID", 0, &type, (BYTE*)guidString, &size);

	if(FAILED(hr))
		return false;

	char folderString[MAX_PATH];
	//get folder path ...
	if(InstallScope == GIS_CURRENT_USER)
		SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, LPSTR(&folderString));
	else if(InstallScope == GIS_ALL_USERS)
		SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, LPSTR(&folderString));
	else
		return false;

	string fullPath(folderString);		//this will be our game's task directory
	fullPath.append("\\Microsoft\\Windows\\GameExplorer\\");
	fullPath.append(guidString);

	string playTasks = fullPath;			//the full playTasks path
	playTasks.append("\\PlayTasks\\0\\");

	//create default play game task directory and all subdirectories ...
	int result = SHCreateDirectoryEx(NULL,playTasks.c_str(), NULL);

	//create support task's directory

	//add default double click link
	string defaultPlayTask = playTasks;
	defaultPlayTask.append("Play.lnk");

	char* temp = 0;
	char* binaryPath = ConvertLPWSTRToLPSTR(gameBinaryPath, temp);
	delete[] temp;

	if(FAILED(CreateLink(binaryPath, defaultPlayTask.c_str(), "Play Crysis!")))
		return false;

	//add further task links
	//->uninstall task?

	return true;
}

// strSavedGameExtension should begin with a period. ex: .ExampleSaveGame
// strLaunchPath should be enclosed in quotes.  ex: "%ProgramFiles%\ExampleGame\ExampleGame.exe"
// strCommandLineToLaunchSavedGame should be enclosed in quotes.  ex: "%1".  If NULL, it defaults to "%1"
//-----------------------------------------------------------------------------
STDAPI SetupRichSavedGames( WCHAR* strSavedGameExtension, WCHAR* strLaunchPath, 
														WCHAR* strCommandLineToLaunchSavedGame ) 
{
	HKEY hKey = NULL;
	LONG lResult;
	DWORD dwDisposition;
	WCHAR strExt[256];
	WCHAR strType[256];
	WCHAR strCmdLine[256];
	WCHAR strTemp[512];
	size_t nStrLength = 0;

	// Validate args 
	if( strLaunchPath == NULL || strSavedGameExtension == NULL )
	{
		assert( false );
		return E_INVALIDARG;
	}

	// Setup saved game extension arg - make sure there's a period at the start
	if( strSavedGameExtension[0] == L'.' )
	{
		StringCchCopyW( strExt, 256, strSavedGameExtension );
		StringCchPrintfW( strType, 256, L"%sType", strSavedGameExtension+1 );
	}
	else
	{
		StringCchPrintfW( strExt, 256, L".%s", strSavedGameExtension );
		StringCchPrintfW( strType, 256, L"%sType", strSavedGameExtension );
	}

	// Create default command line arg if none supplied
	if( strCommandLineToLaunchSavedGame )
		StringCchCopyW( strCmdLine, 256, strCommandLineToLaunchSavedGame );
	else
		StringCchCopyW( strCmdLine, 256, L"\"%1\"" );

	// Create file association & metadata regkeys
	lResult = RegCreateKeyExW( HKEY_CLASSES_ROOT, strExt, 0, NULL, 0, KEY_WRITE, NULL, &hKey, &dwDisposition );
	if( ERROR_SUCCESS == lResult ) 
	{
		// Create the following regkeys:
		//
		// [HKEY_CLASSES_ROOT\.ExampleGameSave]
		// (Default)="ExampleGameSaveFileType"
		//
		StringCchLengthW( strType, 256, &nStrLength );
		RegSetValueExW( hKey, L"", 0, REG_SZ, (BYTE*)strType, (DWORD)((nStrLength + 1)*sizeof(WCHAR)) );

		// Create the following regkeys:
		//
		// [HKEY_CLASSES_ROOT\.ExampleGameSave\ShellEx\{BB2E617C-0920-11d1-9A0B-00C04FC2D6C1}]
		// (Default)="{4E5BFBF8-F59A-4e87-9805-1F9B42CC254A}"
		//
		HKEY hSubKey = NULL;
		lResult = RegCreateKeyExW( hKey, L"ShellEx\\{BB2E617C-0920-11d1-9A0B-00C04FC2D6C1}", 0, NULL, 0, KEY_WRITE, NULL, &hSubKey, &dwDisposition );
		if( ERROR_SUCCESS == lResult ) 
		{
			StringCchPrintfW( strTemp, 512, L"{4E5BFBF8-F59A-4e87-9805-1F9B42CC254A}" );
			StringCchLengthW( strTemp, 256, &nStrLength );
			RegSetValueExW( hSubKey, L"", 0, REG_SZ, (BYTE*)strTemp, (DWORD)((nStrLength + 1)*sizeof(WCHAR)) );
		}
		if( hSubKey ) RegCloseKey( hSubKey );
	}
	else
	{
		char resultMessage[256];
		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, lResult, 0, resultMessage, 256, NULL);
		printf("Error: %s, could not create file association in registry!", resultMessage);
	}

	if( hKey ) 
		RegCloseKey( hKey );

	lResult = RegCreateKeyExW( HKEY_CLASSES_ROOT, strType, 0, NULL, 0, KEY_WRITE, NULL, &hKey, &dwDisposition );
	if( ERROR_SUCCESS == lResult ) 
	{
		// Create the following regkeys:
		//
		// [HKEY_CLASSES_ROOT\ExampleGameSaveFileType]
		// PreviewTitle="prop:System.Game.RichSaveName;System.Game.RichApplicationName"
		// PreviewDetails="prop:System.Game.RichLevel;System.DateChanged;System.Game.RichComment;System.DisplayName;System.DisplayType"
		//
		size_t nPreviewDetails = 0, nPreviewTitle = 0;
		WCHAR* strPreviewTitle = L"prop:System.Game.RichSaveName;System.Game.RichApplicationName";
		WCHAR* strPreviewDetails = L"prop:System.Game.RichLevel;System.DateChanged;System.Game.RichComment;System.DisplayName;System.DisplayType";
		StringCchLengthW( strPreviewTitle, 256, &nPreviewTitle );
		StringCchLengthW( strPreviewDetails, 256, &nPreviewDetails );
		RegSetValueExW( hKey, L"PreviewTitle", 0, REG_SZ, (BYTE*)strPreviewTitle, (DWORD)((nPreviewTitle + 1)*sizeof(WCHAR)) );
		RegSetValueExW( hKey, L"PreviewDetails", 0, REG_SZ, (BYTE*)strPreviewDetails, (DWORD)((nPreviewDetails + 1)*sizeof(WCHAR)) );

		// Create the following regkeys:
		//
		// [HKEY_CLASSES_ROOT\ExampleGameSaveFileType\Shell\Open\Command]
		// (Default)=""%ProgramFiles%\ExampleGame.exe" "%1""
		//
		HKEY hSubKey = NULL;
		lResult = RegCreateKeyExW( hKey, L"Shell\\Open\\Command", 0, NULL, 0, KEY_WRITE, NULL, &hSubKey, &dwDisposition );
		if( ERROR_SUCCESS == lResult ) 
		{
			StringCchPrintfW( strTemp, 512, L"%s %s", strLaunchPath, strCmdLine );
			StringCchLengthW( strTemp, 256, &nStrLength );
			RegSetValueExW( hSubKey, L"", 0, REG_SZ, (BYTE*)strTemp, (DWORD)((nStrLength + 1)*sizeof(WCHAR)) );
		}
		if( hSubKey ) RegCloseKey( hSubKey );
	}
	if( hKey ) RegCloseKey( hKey );

	// Create the following regkeys:
	//
	// [HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.ExampleGameSave]
	// (Default)="{ECDD6472-2B9B-4b4b-AE36-F316DF3C8D60}"
	//
	StringCchPrintfW( strTemp, 512, L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\%s", strExt );
	lResult = RegCreateKeyExW( HKEY_LOCAL_MACHINE, strTemp, 0, NULL, 0, KEY_WRITE, NULL, &hKey, &dwDisposition );
	if( ERROR_SUCCESS == lResult ) 
	{
		StringCchCopyW( strTemp, 512, L"{ECDD6472-2B9B-4B4B-AE36-F316DF3C8D60}" );
		StringCchLengthW( strTemp, 256, &nStrLength );
		RegSetValueExW( hKey, L"", 0, REG_SZ, (BYTE*)strTemp, (DWORD)((nStrLength + 1)*sizeof(WCHAR)) );
	}
	if( hKey ) RegCloseKey( hKey );

	return S_OK;
}

//-----------------------------------------------------------------------------
// Removes the registry keys to enable rich saved games.  
//
// strSavedGameExtension should begin with a period. ex: .ExampleSaveGame
//-----------------------------------------------------------------------------
STDAPI RemoveRichSavedGames( WCHAR* strSavedGameExtension ) 
{
	WCHAR strExt[256];
	WCHAR strType[256];
	WCHAR strTemp[512];

	// Validate args 
	if( strSavedGameExtension == NULL )
	{
		assert( false );
		return E_INVALIDARG;
	}

	// Setup saved game extension arg - make sure there's a period at the start
	if( strSavedGameExtension[0] == L'.' )
	{
		StringCchCopyW( strExt, 256, strSavedGameExtension );
		StringCchPrintfW( strType, 256, L"%sType", strSavedGameExtension+1 );
	}
	else
	{
		StringCchPrintfW( strExt, 256, L".%s", strSavedGameExtension );
		StringCchPrintfW( strType, 256, L"%sType", strSavedGameExtension );
	}

	// Delete the following regkeys:
	//
	// [HKEY_CLASSES_ROOT\.ExampleGameSave]
	// [HKEY_CLASSES_ROOT\ExampleGameSaveFileType]
	// [HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.ExampleGameSave]
	RegDeleteKeyW( HKEY_CLASSES_ROOT, strExt );
	RegDeleteKeyW( HKEY_CLASSES_ROOT, strType );
	StringCchPrintfW( strTemp, 512, L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\%s", strExt );
	RegDeleteKeyW( HKEY_LOCAL_MACHINE, strTemp );

	return S_OK;
}

//installs to game to the game explorer and registry (not copying files ...)
bool InstallGame( WCHAR* strGDFBinPath, WCHAR* strExecutablePath, WCHAR *strGameInstallPath, 
								 GAME_INSTALL_SCOPE InstallScope )
{
	HRESULT hr = AddToGameExplorer(strGDFBinPath, strGameInstallPath, InstallScope);
	if(hr != S_OK)
		return false;
	if(!strExecutablePath)
		strExecutablePath = strGDFBinPath;
	bool taskFoldersCreated = CreateGameExplorerTasks(strExecutablePath, InstallScope);
	hr = SetupRichSavedGames(L".CRYSISJMSF", strGDFBinPath, L"load %1");
	if(hr == S_OK && taskFoldersCreated)
		return true;
	return false;
}

//removes all entries
bool RemoveGame(GAME_INSTALL_SCOPE InstallScope)
{
	//first retrieve game GUID
	WCHAR guidString[256];

	HKEY hk;
	HKEY sub;
	DWORD dwDisp;
	HRESULT hr;
	if(InstallScope == GIS_ALL_USERS)
		hr = RegCreateKeyEx(HKEY_LOCAL_MACHINE, "Software", 0, NULL, REG_OPTION_NON_VOLATILE,
		KEY_QUERY_VALUE, NULL, &hk, &dwDisp);
	else
		hr = RegCreateKeyEx(HKEY_CURRENT_USER, "Software", 0, NULL, REG_OPTION_NON_VOLATILE,
		KEY_QUERY_VALUE, NULL, &hk, &dwDisp);

	hr = RegCreateKeyEx(hk, "Crytek", 0, NULL, REG_OPTION_NON_VOLATILE,
		KEY_QUERY_VALUE, NULL, &hk, &dwDisp);
	sub = hk;

	hr = RegCreateKeyEx(hk, "Crysis", 0, NULL, REG_OPTION_NON_VOLATILE,
		KEY_QUERY_VALUE, NULL, &hk, &dwDisp);

	//get the guid from the registry
	DWORD size = 256;
	DWORD type;
	hr = RegQueryValueExW(hk, L"GUID", 0, &type, (BYTE*)guidString, &size);

	if(hr!=S_OK)
	{
		printf("Could not find game GUID in registry - reinstall game by running this executable with one parameter..\n");
		return false;
	}

	GUID guid;
	hr = ConvertStringToGUID(guidString, &guid);

	//delete game in game explorer (windows vista only)
	hr = RemoveFromGameExplorer(&guid);

	if(hr != S_OK)		//probably WinxP - remove WinXP upgrade entries
	{
		HKEY hKeyGamesToFind;
		DWORD dwDisposition;
		if( InstallScope == GIS_CURRENT_USER )
			RegCreateKeyEx( HKEY_CURRENT_USER, "Software\\Classes\\Software\\Microsoft\\Windows\\CurrentVersion\\GameUX\\GamesToFindOnWindowsUpgrade", 
			0, NULL, 0, KEY_QUERY_VALUE, NULL, &hKeyGamesToFind, &dwDisposition );
		else
			RegCreateKeyEx( HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\GameUX\\GamesToFindOnWindowsUpgrade", 
			0, NULL, 0, KEY_QUERY_VALUE, NULL, &hKeyGamesToFind, &dwDisposition );

		hr = RegDeleteKeyW(hKeyGamesToFind, guidString);
	}

	//uninstall rich save game registration
	hr = RemoveRichSavedGames(L".CRYSISJMSF");

	//remove game registry entry
	hr = RegDeleteKeyW(sub, L"Crysis");

	//remove task directories
	char folderString[MAX_PATH];
	//get folder path ...
	if(InstallScope == GIS_CURRENT_USER)
		SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, LPSTR(&folderString));
	else if(InstallScope == GIS_ALL_USERS)
		SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, LPSTR(&folderString));
	else
		return false;

	char* temp = 0;
	char* guidA = ConvertLPWSTRToLPSTR(guidString, temp);
	delete[] temp;

	string fullPath(folderString);		//this will be our game's task directory
	fullPath.append("\\Microsoft\\Windows\\GameExplorer\\");
	fullPath.append(guidA);

	string playTasks = fullPath;			//the full playTasks path
	playTasks.append("\\PlayTasks\\");
	string defaultPlay = fullPath;
	defaultPlay.append("\\PlayTasks\\0\\");

	string tempTask = defaultPlay;
	tempTask.append("Play.lnk");
	remove(tempTask.c_str());

	BOOL worked = RemoveDirectoryA(defaultPlay.c_str());
	if(!worked)
		return false;
	worked = RemoveDirectoryA(playTasks.c_str());
	if(!worked)
		return false;

	//remove other directories ...

	worked = RemoveDirectoryA(fullPath.c_str());
	if(!worked)
		return false;
	return true;
}

#endif