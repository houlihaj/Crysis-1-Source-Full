// LUADBG.cpp : Defines the entry point for the application.
//
#include "StdAfx.h"

#ifdef WIN32

#include "LUADBG.h"
#include "_TinyRegistry.h"
#include "_TinyFileEnum.h"
#include "_TinyBrowseFolder.h"
#include "AboutWnd.h"
#include "GoToWnd.h"
#include <ICryPak.h>
#include <CryPath.h>
#include <IHardwareMouse.h>
#include <algorithm>

#include <direct.h>
#ifndef WIN64
#include "Shlwapi.h"
#pragma comment (lib, "Shlwapi.lib")
#endif

_TINY_DECLARE_APP();

extern HANDLE gDLLHandle;

//////////////////////////////////////////////////////////////////////////
// CSourceEdit
//////////////////////////////////////////////////////////////////////////
BOOL CSourceEdit::Create(DWORD dwStyle,DWORD dwExStyle,const RECT* pRect,_TinyWindow *pParentWnd,ULONG nID)
{
	_TinyRect erect;
	erect.left=BORDER_SIZE;
	erect.right=pRect->right;
	erect.top=0;
	erect.bottom=pRect->bottom;
	if(!_TinyWindow::Create(NULL,_T("asd"),WS_CHILD|WS_VISIBLE,0,pRect,pParentWnd,nID))
		return FALSE;
	if(!m_wEditWindow.Create(IDC_SOURCE,WS_VISIBLE|WS_CHILD|ES_WANTRETURN|WS_HSCROLL|WS_VSCROLL|ES_AUTOHSCROLL|ES_AUTOVSCROLL|ES_MULTILINE|ES_NOHIDESEL,WS_EX_CLIENTEDGE,&erect,this))
		return FALSE;
	m_wEditWindow.SetFont((HFONT) GetStockObject(ANSI_FIXED_FONT));

	/*
	// Set 1 tab.
	PARAFORMAT pf;
	memset(&pf,0,sizeof(pf));
	pf.cbSize = sizeof(pf);
	pf.dwMask = PFM_TABSTOPS;
	m_wEditWindow.SendMessage(EM_GETPARAFORMAT, (WPARAM)0,(LPARAM)&pf );
	pf.cTabCount = 2;
	m_wEditWindow.SendMessage(EM_SETPARAFORMAT, (WPARAM)0,(LPARAM)&pf );
	*/

	HDC hDC = GetDC(m_hWnd);
	m_hFont = CreateFont(-::MulDiv(9, GetDeviceCaps(hDC, LOGPIXELSY), 72), 0, 0, 0, 
		FW_DONTCARE, FALSE, FALSE, FALSE,
		ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
		DEFAULT_PITCH, "Verdana"); //Tahoma
	ReleaseDC(m_hWnd,hDC);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
LRESULT CSourceEdit::OnEraseBkGnd(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam){return 1;};


//////////////////////////////////////////////////////////////////////////
LRESULT CSourceEdit::OnPaint(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
{
	_TinyRect rect;
	PAINTSTRUCT ps;
	HDC hDC;
	BeginPaint(m_hWnd,&ps);
	hDC=::GetDC(m_hWnd);
	int w,h;
	UINT i;
	int iLine;
	const char *pszFile = NULL;
	/////////////////////////////
	GetClientRect(&rect);
	w=rect.right-rect.left;
	h=rect.bottom-rect.top;
	_TinyRect rc(0,0,BORDER_SIZE,h);
	::DrawFrameControl(hDC, &rc, DFC_BUTTON, DFCS_BUTTONPUSH | DFCS_PUSHED);
	::SetBkMode(hDC,TRANSPARENT);
	POINT pt;
	m_wEditWindow.GetScrollPos(&pt);
	int iFirstLine = m_wEditWindow.GetFirstVisibleLine() + 1;
	TEXTMETRIC tm;
	m_wEditWindow.GetTextMetrics(&tm);


	//int iFontY=tm.tmHeight-tm.tmDescent;
	int iFontY=tm.tmHeight;
	int y=-pt.y%iFontY+3;
	char sTemp[10];
	HRGN hOldRgn=::CreateRectRgn(0,0,0,0);
	HRGN hClippedRgn=::CreateRectRgn(0,0,BORDER_SIZE,h - 1);
	::GetWindowRgn(m_hWnd,hOldRgn);
	::SelectClipRgn(hDC,hClippedRgn);
	HGDIOBJ hPrevFont = ::SelectObject(hDC, m_hFont);
	COLORREF prevClr;
	while(y<h)
	{
		sprintf(sTemp,"%d",iFirstLine);
		if (iFirstLine == m_iLineMarkerPos)
			prevClr = ::SetTextColor(hDC,RGB(255,0,0));
		::TextOut(hDC,2,y,sTemp,(int)strlen(sTemp));
		if (iFirstLine == m_iLineMarkerPos)
			::SetTextColor(hDC,prevClr);
		y+=iFontY;
		iFirstLine++;
	}
	iFirstLine = m_wEditWindow.GetFirstVisibleLine();

	HBRUSH brsh;
	COLORREF crColor = 0x00BBBBBB;  // Grey colour for disabled
	// What debug mode are we in?
	if (ICVar *pDebug = gEnv->pConsole->GetCVar("lua_debugger"))
		if (pDebug->GetIVal() == eLDM_FullDebug)
			crColor = 0x0000009F;
	
	brsh = ::CreateSolidBrush(crColor);
	::SelectObject(hDC, brsh); 

	BreakPoints* pBreakPoints = m_pLuaDbg->GetBreakPoints();
	for (i=0; i < (UINT)pBreakPoints->size(); i++)
	{
		BreakPoint &bp = (*pBreakPoints)[i];
		if (bp.nLine && strlen(bp.sSourceFile) > 0)
		{
			iLine = bp.nLine;
			if (stricmp(bp.sSourceFile,m_strSourceFile) == 0)
			{
				INT iY = (-pt.y % iFontY + 3) + (iLine - iFirstLine - 1) * iFontY + 7;
				Ellipse(hDC, 41 - 6, iY - 6, 41 + 6, iY + 6);
			}
		}
	}
	::DeleteObject(brsh);

	// Draw current line marker
	brsh = ::CreateSolidBrush(0x00FFFF00);
	::SelectObject(hDC, brsh);
	POINT sTriangle[4];
	sTriangle[0].x = 30;
	sTriangle[0].y = -2;
	sTriangle[1].x = 46;
	sTriangle[1].y = 5;
	sTriangle[2].x = 30;
	sTriangle[2].y = 12;
	sTriangle[3].x = 30;
	sTriangle[3].y = 0;
	
	for (i=0; i<4; i++)
		sTriangle[i].y += (-pt.y % iFontY + 3) + (m_iLineMarkerPos - iFirstLine - 1) * iFontY + 2;
	Polygon(hDC, sTriangle, 4);
	
	// Breakpoints
	::DeleteObject(brsh);


	::SelectObject(hDC, hPrevFont);
	::SelectClipRgn(hDC,hOldRgn);
	::DeleteObject(hOldRgn);
	::DeleteObject(hClippedRgn);
	//::DeleteObject(hBrush);
	/////////////////////////////
	EndPaint(m_hWnd,&ps);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
LRESULT CSourceEdit::OnSize(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
{
	int w=LOWORD(lParam);
	int h=HIWORD(lParam);
	_TinyRect erect;
	erect.left=BORDER_SIZE;
	erect.right=w-BORDER_SIZE;
	erect.top=0;
	erect.bottom=h;
	m_wEditWindow.SetWindowPos(BORDER_SIZE,0,w-BORDER_SIZE,h,0);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
LRESULT CSourceEdit::OnScroll(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
{
	_TinyRect rect;
	GetClientRect(&rect);

	rect.top=0;
	rect.left=0;
	rect.right=BORDER_SIZE;
	return InvalidateRect(&rect);
}

//////////////////////////////////////////////////////////////////////////
CLUADbg::CLUADbg( CScriptSystem *pScriptSystem )
{
	m_pIScriptSystem = NULL;
	m_pIPak = NULL;
	m_pIVariable = NULL;
	m_hRoot = NULL;
	m_pTreeToAdd = NULL;
	m_pIPak = gEnv->pCryPak;
	m_pIScriptSystem = pScriptSystem;

	m_wScriptWindow.SetLuaDbg(this);
	m_wScriptWindow.SetScriptSystem(m_pIScriptSystem); 

	m_bQuitMsgLoop = false;
	m_bForceBreak = false;
	m_breakState = bsNoBreak;
	m_nCallLevelUntilBreak = 0;
	m_bCppCallstackEnabled = false;

	LoadBreakpoints( "breakpoints.lst" );

	_Tiny_InitApp((HINSTANCE)::GetModuleHandle(NULL), (HINSTANCE) gDLLHandle, NULL, NULL, IDI_SMALL);
}

CLUADbg::~CLUADbg()
{
	_TinyRegistry cTReg;
	_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "MainHorzSplitter", m_wndMainHorzSplitter.GetSplitterPos()));
 	_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "SrcEditSplitter", m_wndSrcEditSplitter.GetSplitterPos()));
	_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "WatchSplitter", m_wndWatchSplitter.GetSplitterPos()));
	_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "WatchCallstackSplitter", m_wndWatchCallstackSplitter.GetSplitterPos()));
	_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "Fullscreen", ::IsZoomed(m_hWnd) ? 1 : 0));
}

TBBUTTON tbButtonsAdd [] = 
{
	{ 0, ID_DEBUG_RUN, TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },
	{ 1, ID_DEBUG_BREAK, 0, BTNS_BUTTON, { 0 }, 0L, 0 },
	{ 2, ID_DEBUG_STOP, 0, BTNS_BUTTON, { 0 }, 0L, 0 },

	{ 0, 0, TBSTATE_ENABLED, BTNS_SEP, { 0 }, 0L, 0 },
	
	{ 3, ID_DEBUG_STEPINTO, TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },
	{ 4, ID_DEBUG_STEPOVER, TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },
	{ 5, ID_DEBUG_TOGGLEBREAKPOINT, TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },
	{ 6, ID_DEBUG_DISABLE, TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },

	{ 0, 0, TBSTATE_ENABLED, BTNS_SEP, { 0 }, 0L, 0 },

	{ 7, IDM_ABOUT, TBSTATE_ENABLED, BTNS_BUTTON, { 0 }, 0L, 0 },
};


//! add a backslash if needed
inline void MyPathAddBackslash( char* szPath )
{
	if(szPath[0]==0) 
		return;

	size_t nLen = strlen(szPath);

	if (szPath[nLen-1] == '\\')
		return;

	if (szPath[nLen-1] == '/')
	{
		szPath[nLen-1] = '\\';
		return;
	}

	szPath[nLen] = '\\';
	szPath[nLen+1] = '\0';
}



LRESULT CLUADbg::RegSanityCheck(bool bForceDelete)
{
	_TinyRegistry cTReg;

	// The registry often gets corrupted, hence check and possibly delete it 
	if ( !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "MainHorzSplitter", 10, 1000)
		|| !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "SrcEditSplitter", 10, 1000)
		|| !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "WatchSplitter", 10, 1000)
		|| !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "WatchCallstackSplitter", 10, 1000)

		|| !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "XOrigin", 10, 1000)
		|| !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "YOrigin", 10, 1000)
		|| !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "XSize", 10, 1000)
		|| !cTReg.CheckNumber("Software\\Tiny\\LuaDebugger\\", "YSize", 10, 1000)
		|| bForceDelete )
	{
		// One of the checks failed, so lets delete all the subkeys
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "MainHorzSplitter");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "SrcEditSplitter");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "WatchSplitter");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "WatchCallstackSplitter");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "XOrigin");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "YOrigin");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "XSize");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "YSize");
		cTReg.DeleteValue("Software\\Tiny\\LuaDebugger\\", "Fullscreen");
	}

	return 0;
}


LRESULT CLUADbg::SetupSplitters(void)
{
	_TinyRegistry cTReg;
	_TinyRect wrect;
	GetClientRect(&wrect);

	// Read splitter window locations from registry
	DWORD dwVal;
	cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "MainHorzSplitter", dwVal, 150);
	m_wndMainHorzSplitter.SetSplitterPos(dwVal);
	m_wndMainHorzSplitter.Reshape();
	cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "SrcEditSplitter", dwVal, 190);
	m_wndSrcEditSplitter.SetSplitterPos(dwVal);
	m_wndSrcEditSplitter.Reshape();
	cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "WatchSplitter", dwVal, wrect.right / 3 * 2);
	m_wndWatchSplitter.SetSplitterPos(dwVal);
	m_wndWatchSplitter.Reshape();
	cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "WatchCallstackSplitter", dwVal, wrect.right / 3);
	m_wndWatchCallstackSplitter.SetSplitterPos(dwVal);
	m_wndWatchCallstackSplitter.Reshape();

	return 1;
}


LRESULT CLUADbg::OnCreate(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	_TinyRect erect;
	_TinyRect lrect;
	_TinyRect wrect;
	_TinyRegistry cTReg;

	RegSanityCheck();

	DWORD dwXOrigin=0,dwYOrigin=0,dwXSize=800,dwYSize=600;
	if(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "XOrigin",dwXOrigin))
	{
		cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "YOrigin",dwYOrigin);
		cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "XSize",dwXSize);
		cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "YSize",dwYSize);
	}
	
	SetWindowPos(dwXOrigin, dwYOrigin, dwXSize, dwYSize, SWP_NOZORDER | SWP_NOMOVE);
	// TODO: Make sure fullscreen flag is loaded and used
	// WS_MAXIMIZE
	// DWORD dwFullscreen = 0;
	// cTReg.ReadNumber("Software\\Tiny\\LuaDebugger\\", "Fullscreen", dwFullscreen);
	// if (dwFullscreen == 1)
    // ShowWindow(SW_MAXIMIZE);
	CenterOnScreen();
	GetClientRect(&wrect);

	erect=wrect;
	erect.top=0;
	erect.bottom=32;
	m_wToolbar.Create((ULONG_PTR) GetMenu(m_hWnd), WS_CHILD|WS_VISIBLE|TBSTYLE_TOOLTIPS, 0, &erect, this);	//AMD Port
	m_wToolbar.AddButtons(IDC_LUADBG, tbButtonsAdd, 10);
	
	m_wndStatus.Create(0, NULL, this);

	// Client area window
	_TinyVerify(m_wndClient.Create(NULL, _T(""), WS_CHILD | WS_VISIBLE, 0, &wrect, this));
	m_wndClient.NotifyReflection(TRUE);

	// Splitter dividing source/file and watch windows
	m_wndMainHorzSplitter.Create(&m_wndClient, NULL, NULL, true);

	// Divides source and file view
	m_wndSrcEditSplitter.Create(&m_wndMainHorzSplitter);

	// Divides two watch windows
	m_wndWatchSplitter.Create(&m_wndMainHorzSplitter);

	// Divides the watch window and the cllstack
	m_wndWatchCallstackSplitter.Create(&m_wndWatchSplitter);
	
	// Add all scripts to the file tree
	m_wFilesTree.Create(erect, &m_wndSrcEditSplitter, IDC_FILES);
	char szRootPath[_MAX_PATH];
	_getcwd(szRootPath, _MAX_PATH);
	MyPathAddBackslash(szRootPath);

	string sScriptsFolder;
	const char *szFolder=m_pIPak->GetAlias("SCRIPTS");
	if (szFolder)
		sScriptsFolder = PathUtil::GetGameFolder() + "/" + szFolder + "/";
	else
		sScriptsFolder = PathUtil::GetGameFolder() + "/SCRIPTS/";

	strcat(szRootPath, sScriptsFolder);
	m_wFilesTree.ScanFiles(szRootPath);

	_TinyVerify(m_wScriptWindow.Create(WS_VISIBLE | WS_CHILD | ES_WANTRETURN | WS_HSCROLL | WS_VSCROLL | 
		ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE, 0, &erect, &m_wndSrcEditSplitter));
	m_wScriptWindow.SendMessage(EM_SETEVENTMASK,0,ENM_SCROLL);

	m_wndFileViewCaption.Create("Scripts", &m_wFilesTree, &m_wndSrcEditSplitter);
	m_wndSourceCaption.Create("Source", &m_wScriptWindow, &m_wndSrcEditSplitter);
	m_wndSrcEditSplitter.SetFirstPan(&m_wndFileViewCaption);
	m_wndSrcEditSplitter.SetSecondPan(&m_wndSourceCaption);

	m_wLocals.Create(IDC_LOCALS,WS_CHILD|TVS_HASLINES|TVS_HASBUTTONS|TVS_LINESATROOT|WS_VISIBLE,WS_EX_CLIENTEDGE,&erect, &m_wndWatchSplitter);
	m_wWatch.Create(IDC_WATCH,WS_CHILD|WS_VISIBLE|TVS_HASLINES|TVS_HASBUTTONS|TVS_LINESATROOT,WS_EX_CLIENTEDGE,&erect, &m_wndWatchSplitter);
	m_wCallstack.Create(0,WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SHOWSELALWAYS,LVS_EX_TRACKSELECT|LVS_EX_FULLROWSELECT|WS_EX_CLIENTEDGE,&erect, &m_wndWatchSplitter);

	m_wndLocalsCaption.Create("Locals", &m_wLocals, &m_wndWatchSplitter);
	m_wndWatchCaption.Create("Watch", &m_wWatch, &m_wndWatchCallstackSplitter);
	m_wndCallstackCaption.Create("Callstack", &m_wCallstack, &m_wndWatchCallstackSplitter);

	m_wndWatchSplitter.SetFirstPan(&m_wndWatchCallstackSplitter);

	m_wndWatchCallstackSplitter.SetFirstPan(&m_wndWatchCaption);
	m_wndWatchCallstackSplitter.SetSecondPan(&m_wndCallstackCaption);

	m_wndWatchSplitter.SetSecondPan(&m_wndLocalsCaption);

	m_wndMainHorzSplitter.SetFirstPan(&m_wndSrcEditSplitter);
	m_wndMainHorzSplitter.SetSecondPan(&m_wndWatchSplitter);

	Reshape(wrect.right-wrect.left,wrect.bottom-wrect.top);

	SetupSplitters();

	m_wCallstack.InsertColumn(0, "Function", 300, 0);
	m_wCallstack.InsertColumn(1, "Line", 40, 1);
	m_wCallstack.InsertColumn(2, "File", 100, 2);

	//_TinyVerify(LoadFile("C:\\MASTERCD\\SCRIPTS\\Default\\Entities\\PLAYER\\BasicPlayer.lua"));
	//_TinyVerify(LoadFile("C:\\MASTERCD\\SCRIPTS\\Default\\Entities\\PLAYER\\player.lua"));
	// _TINY_CHECK_LAST_ERROR
	
	return 0;
}

bool CLUADbg::LoadFile(const char *pszFile, bool bForceReload)
{
	FILE *hFile = NULL;
	char *pszScript = NULL, *pszFormattedScript = NULL;
	UINT iLength, iCmpBufPos, iSrcBufPos, iDestBufPos, iNumChars, iStrStartPos, iCurKWrd, i;
	char szCmpBuf[2048];
	bool bIsKeyWord;
	string strLuaFormatFileName = "@";
	char szMasterCD[_MAX_PATH];

	if(!pszFile)
		return false;

	// Create lua specific filename and check if we already have that file loaded
	_getcwd(szMasterCD, _MAX_PATH);
	if (strncmp(szMasterCD, pszFile, strlen(szMasterCD) - 1) == 0)
	{
		string sShortName = &pszFile[strlen(szMasterCD) + 1];
		sShortName.MakeLower();
		const char *shortname = sShortName;
		if (strnicmp(shortname,PathUtil::GetGameFolder(),strlen(PathUtil::GetGameFolder()))==0)
		{
			shortname += strlen(PathUtil::GetGameFolder())+1;
		}

		strLuaFormatFileName += shortname;

		strLuaFormatFileName.replace('\\','/');
		strLuaFormatFileName.MakeLower();
	}
	else
		strLuaFormatFileName += pszFile;

	if (bForceReload == false && m_wScriptWindow.GetSourceFile() == strLuaFormatFileName)
		return true;

	m_wScriptWindow.SetSourceFile(strLuaFormatFileName.c_str());

	
	// hFile = fopen(pszFile, "rb");
	hFile = m_pIPak->FOpen(pszFile, "rb");

	if (!hFile)
		return false;

	// set filename in window title
	{
		char str[_MAX_PATH];

		sprintf(str,"Lua Debugger [%s]",pszFile);
		SetWindowText(str);
	}

	// Get file size
	// fseek(hFile, 0, SEEK_END);
	m_pIPak->FSeek(hFile, 0, SEEK_END);
	// iLength = ftell(hFile);
	iLength = m_pIPak->FTell(hFile);
	// fseek(hFile, 0, SEEK_SET);
	m_pIPak->FSeek(hFile, 0, SEEK_SET);

    pszScript = new char [iLength + 1];
	pszFormattedScript = new char [512 + iLength * 3];

	// _TinyVerify(fread(pszScript, iLength, 1, hFile) == 1);
	_TinyVerify(m_pIPak->FRead(pszScript, iLength, hFile) == iLength);
	pszScript[iLength] = '\0';
	_TinyAssert(strlen(pszScript) == iLength);

	// RTF text, font Courier, green comments and blue keyword
	strcpy(pszFormattedScript, "{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033" \
		"{\\fonttbl{\\f0\\fmodern\\fprq1\\fcharset0 Courier New;}{\\f1\\fswiss\\fcharset0 Arial;}}" \
		"{\\colortbl ;\\red0\\green0\\blue255;\\red0\\green128\\blue0;\\red160\\green160\\blue160;}\\f0\\fs20");

	const char szKeywords[][32] =
	{
		"function",
		"do",
		"for",
		"end",
		"and",
		"or",
		"not",
		"while",
		"return",
		"if",
		"then",
		"else",
		"elseif",
		"self",
		"local",
		"in",
		"nil",
		"repeat",
		"until",
		"break",
	};

	// Format text with syntax coloring
	iDestBufPos = strlen(pszFormattedScript);

	iSrcBufPos = 0;
	while (pszScript[iSrcBufPos] != '\0')
	{
		// Scan next token
		iNumChars = 1;
		iStrStartPos = iSrcBufPos;
		while (pszScript[iSrcBufPos] != ' '  &&
			   pszScript[iSrcBufPos] != '\n' &&
			   pszScript[iSrcBufPos] != '\r' &&
			   pszScript[iSrcBufPos] != '\t' &&
			   pszScript[iSrcBufPos] != '\0' &&
			   pszScript[iSrcBufPos] != '('  &&
			   pszScript[iSrcBufPos] != ')'  &&
			   pszScript[iSrcBufPos] != '['  &&
			   pszScript[iSrcBufPos] != ']'  &&
			   pszScript[iSrcBufPos] != '{'  &&
			   pszScript[iSrcBufPos] != '}'  &&
			   pszScript[iSrcBufPos] != ','  &&
			   pszScript[iSrcBufPos] != '.'  &&
			   pszScript[iSrcBufPos] != ';'  &&
			   pszScript[iSrcBufPos] != ':'  &&
			   pszScript[iSrcBufPos] != '='  &&
			   pszScript[iSrcBufPos] != '==' &&
			   pszScript[iSrcBufPos] != '*'  &&
			   pszScript[iSrcBufPos] != '+'  &&
			   pszScript[iSrcBufPos] != '/'  &&
			   pszScript[iSrcBufPos] != '~'  &&
			   pszScript[iSrcBufPos] != '"')
		{
			iSrcBufPos++;	
			iNumChars++;

			// Special treatment of '-' to allow parsing of '--'
			if (pszScript[iSrcBufPos - 1] == '-' && pszScript[iSrcBufPos] != '-')
				break;
		}
		if (iNumChars == 1)
			iSrcBufPos++;
		else
			iNumChars--;

		// Copy token and add escapes
		iCmpBufPos = 0;
		for (i=iStrStartPos; i<iStrStartPos + iNumChars; i++)
		{
			_TinyAssert(i - iStrStartPos < sizeof(szCmpBuf));

			if (pszScript[i] == '{' || pszScript[i] == '}' || pszScript[i] == '\\')
			{
				// Add \ to mark it as non-escape character
				szCmpBuf[iCmpBufPos++] = '\\';
				szCmpBuf[iCmpBufPos++] = pszScript[i];
				szCmpBuf[iCmpBufPos] = '\0';
			}
			else
			{
				szCmpBuf[iCmpBufPos++] = pszScript[i];
				szCmpBuf[iCmpBufPos] = '\0';
			}
		}

		// Comment
		if (strncmp(szCmpBuf, "--", 2) == 0)
		{
			// Green
			strcat(pszFormattedScript, "\\cf2 ");
			
			iDestBufPos += 5;
			
			strcpy(&pszFormattedScript[iDestBufPos], szCmpBuf);
			iDestBufPos += strlen(szCmpBuf);

			// Parse until newline
			while (pszScript[iSrcBufPos] != '\n' && pszScript[iSrcBufPos] != '\0')
			{
				if (pszScript[iSrcBufPos] == '{' || pszScript[iSrcBufPos] == '}' || pszScript[iSrcBufPos] == '\\')
				{
					// Add \ to mark it as non-escape character
					pszFormattedScript[iDestBufPos++] = '\\';
					pszFormattedScript[iDestBufPos++] = pszScript[iSrcBufPos++];
				}
				else
				{
					pszFormattedScript[iDestBufPos++] = pszScript[iSrcBufPos++];
				}
			}
			iSrcBufPos++;
			pszFormattedScript[iDestBufPos] = '\0';

			// Add newline and restore color
			strcat(pszFormattedScript, "\\par\n");
			iDestBufPos += 5;
			strcat(pszFormattedScript, "\\cf0 ");
			iDestBufPos += 5;

			continue;
		}
		
		// String
		if (strncmp(szCmpBuf, "\"", 2) == 0)
		{
			// Gray
			strcat(pszFormattedScript, "\\cf3 ");
			iDestBufPos += 5;
			
			strcpy(&pszFormattedScript[iDestBufPos], szCmpBuf);
			iDestBufPos += strlen(szCmpBuf);

			// Parse until end string / newline
			while (pszScript[iSrcBufPos] != '\n' && pszScript[iSrcBufPos] != '\0' && pszScript[iSrcBufPos] != '"')
			{
				pszFormattedScript[iDestBufPos++] = pszScript[iSrcBufPos++];
			}
			iSrcBufPos++;
			pszFormattedScript[iDestBufPos] = '\0';

			// Add literal
			strcat(pszFormattedScript, "\"");
			iDestBufPos += 1;

			// Restore color
			strcat(pszFormattedScript, "\\cf0 ");
			iDestBufPos += 5;

			continue;
		}

		// Have we parsed a keyword ?
		bIsKeyWord = false;
		for (iCurKWrd=0; iCurKWrd<sizeof(szKeywords) / sizeof(szKeywords[0]); iCurKWrd++)
		{
			if (strcmp(szKeywords[iCurKWrd], szCmpBuf) == 0)
			{
				strcat(pszFormattedScript, "\\cf1 ");
				strcat(pszFormattedScript, szKeywords[iCurKWrd]);
				strcat(pszFormattedScript, "\\cf0 ");
				iDestBufPos += 5 + 5 + strlen(szKeywords[iCurKWrd]);
				bIsKeyWord = true;
			}
		}
		if (bIsKeyWord)
			continue;

		if (strcmp(szCmpBuf, "\n") == 0)
		{
			// Newline
			strcat(pszFormattedScript, "\\par ");
			iDestBufPos += 5;
		}
		else
		{
			// Anything else, just append
			iDestBufPos += strlen(szCmpBuf);
			strcat(pszFormattedScript, szCmpBuf);
		}
	}

	if (hFile)
		// fclose(hFile);
		m_pIPak->FClose(hFile);

	if (pszScript)
	{
		delete [] pszScript;
		pszScript = NULL;
	}

	/*
	hFile = fopen("C:\\Debug.txt", "w");
	fwrite(pszFormattedScript, 1, strlen(pszFormattedScript), hFile);
	fclose(hFile);
	*/

	m_wScriptWindow.SetText(pszFormattedScript);
	
	if (pszFormattedScript)
	{
		delete [] pszFormattedScript;
		pszFormattedScript = NULL;
	}

	return true;
}

LRESULT CLUADbg::OnResetView(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
{
	RegSanityCheck(true);	// Force registry deletion
	SetupSplitters();			// Setup splitters from default values	
	return 1;
}

LRESULT CLUADbg::OnEraseBkGnd(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
{
	return 1;
}

LRESULT CLUADbg::OnClose(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
{
	// Quit();
	WINDOWINFO wi;
	if(::GetWindowInfo(m_hWnd,&wi))
	{
		_TinyRegistry cTReg;
		_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "XOrigin", wi.rcWindow.left));
		_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "YOrigin", wi.rcWindow.top));
		_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "XSize", wi.rcWindow.right-wi.rcWindow.left));
		_TinyVerify(cTReg.WriteNumber("Software\\Tiny\\LuaDebugger\\", "YSize", wi.rcWindow.bottom-wi.rcWindow.top));
	}
	m_bQuitMsgLoop = true;
	return 0;
}


LRESULT CLUADbg::OnSize(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
{

	int w=LOWORD(lParam);
	int h=HIWORD(lParam);
	Reshape(w,h);

	return 0;
}

LRESULT CLUADbg::OnAbout(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
{
	CAboutWnd wndAbout;
	wndAbout.DoModal(this);
	return 0;
}

LRESULT CLUADbg::OnGoTo(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
{
	CGoToWnd wndGoTo;
	wndGoTo.DoModal(this);
	return 0;
}

LRESULT CLUADbg::OnToggleBreakpoint(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
{
	const char *source = m_wScriptWindow.GetSourceFile();
	int line = m_wScriptWindow.GetCurLine();
	if (HaveBreakPointAt(source,line))
		RemoveBreakPoint(source,line);
	else
	{
		AddBreakPoint( source,line );
		// Chances are we want to stop on breakpoints then! 
		ICVar *pDebugMode = gEnv->pConsole->GetCVar("lua_debugger");
		if (pDebugMode) 
		{
			pDebugMode->Set(eLDM_FullDebug);
			UpdateCheckboxes(m_hWnd);
		}
	}
	m_wScriptWindow.InvalidateEditWindow();
	return 0;
}

LRESULT CLUADbg::OnDeleteAllBreakpoints(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
{
	m_breakPoints.clear();
	SaveBreakpoints( "breakpoints.lst" );
	return 0;
}

#define TOOLBAR_HEIGHT 28
#define STATUS_BAR_HEIGHT 20

bool CLUADbg::Reshape(int w,int h)
{
	/*
	int nWatchHeight=(((float)h)*WATCH_HEIGHT_MULTIPLIER);
	int nFilesWidth=(((float)h)*FILES_WIDTH_MULTIPLIER);
	m_wScriptWindow.SetWindowPos(nFilesWidth,TOOLBAR_HEIGHT,w-nFilesWidth,h-TOOLBAR_HEIGHT-nWatchHeight,SWP_DRAWFRAME);
	m_wLocals.SetWindowPos(0,h-nWatchHeight,w/2,nWatchHeight,SWP_DRAWFRAME);
	m_wFilesTree.SetWindowPos(nFilesWidth,TOOLBAR_HEIGHT,nFilesWidth,h-TOOLBAR_HEIGHT-nWatchHeight,SWP_DRAWFRAME);
	m_wWatch.SetWindowPos(w/2,h-nWatchHeight,w/2,nWatchHeight,SWP_DRAWFRAME);
	*/

	m_wndClient.Reshape(w, h - TOOLBAR_HEIGHT - STATUS_BAR_HEIGHT);
	m_wndClient.SetWindowPos(0, TOOLBAR_HEIGHT, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	m_wndMainHorzSplitter.Reshape(w, h - TOOLBAR_HEIGHT - STATUS_BAR_HEIGHT);
	m_wToolbar.SetWindowPos(0,0,w,TOOLBAR_HEIGHT,0);
	m_wndStatus.SetWindowPos(0, h -  STATUS_BAR_HEIGHT, w, STATUS_BAR_HEIGHT, NULL);

	return true;
}
 
//////////////////////////////////////////////////////////////////////////
void CLUADbg::PlaceLineMarker(UINT iLine)
{
	m_wScriptWindow.SetLineMarker(iLine);
	m_wScriptWindow.ScrollToLine(iLine-1);
}

//////////////////////////////////////////////////////////////////////////
LRESULT CLUADbg::UpdateCheckboxes(HWND hWnd)
{
	ELuaDebugMode mode = eLDM_NoDebug;

	if (ICVar *pDebug = gEnv->pConsole->GetCVar("lua_debugger"))
		mode = (ELuaDebugMode) pDebug->GetIVal();

	UINT modeItem;
	switch(mode) {
		case eLDM_FullDebug:
			modeItem = ID_DEBUG_ENABLE;
			break;
		case eLDM_OnlyErrors:
			modeItem = ID_DEBUG_ERRORS;
			break;
		default:
			// An invalid debugging mode
		case eLDM_NoDebug:
			modeItem = ID_DEBUG_DISABLE;
			break;
	}

	int success = CheckMenuRadioItem(GetMenu(m_hWnd), ID_DEBUG_DISABLE, ID_DEBUG_ENABLE, modeItem, MF_BYCOMMAND);
	if (!success)
		_TINY_CHECK_LAST_ERROR;
	return 1;
}


//////////////////////////////////////////////////////////////////////////
void CLUADbg::SetStatusBarText(const char *pszText)
{
	// TODO: Find out why setting the panel text using the
	//       dedicated message doesn't work. For some reason the
	//       text gets drawn once but never again.
	// m_wndStatus.SetPanelText(pszText);

	m_wndStatus.SetWindowText(pszText);
}

//////////////////////////////////////////////////////////////////////////
string CLUADbg::GetLineFromFile( const char *sFilename,int nLine )
{
	CCryFile file;

	if (!file.Open(sFilename,"rb"))
	{
		return "";
	}
	int nLen = file.GetLength();
	char *sScript = new char [nLen + 1];
	char *sString = new char [nLen + 1];
	file.ReadRaw( sScript,nLen );
	sScript[nLen] = '\0';

	int nCurLine = 1;
	string strLine;

	strcpy( sString,"" );

	const char *strLast = sScript+nLen;

	const char *str = sScript;
	while (str < strLast)
	{
		char *s = sString;
		while (str < strLast && *str != '\n' && *str != '\r')
		{
			*s++ = *str++;
		}
		*s = '\0';
		// Skip \n\r
		str += 2;

		if (nCurLine == nLine)
		{
			strLine = sString;
			strLine.replace( '\t',' ' );
			strLine.Trim();
			break;
		}
		nCurLine++;
	}

	delete []sString;
	delete []sScript;

	return strLine;
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::GetStackAndLocals()
{
	m_wCallstack.SendMessage( WM_SETREDRAW,(WPARAM)FALSE,(LPARAM)FALSE );
	m_wLocals.SendMessage( WM_SETREDRAW,(WPARAM)FALSE,(LPARAM)FALSE );
	m_wWatch.SendMessage( WM_SETREDRAW,(WPARAM)FALSE,(LPARAM)FALSE );

	///////////
	// Stack //
	///////////
	{
		std::vector<SLuaStackEntry> callstack;
		m_pIScriptSystem->GetCallStack( callstack );
		const char *pszText = NULL;
		int iItem=0;
		m_wCallstack.Clear();

		if (!callstack.empty())
		{
			for (int i = callstack.size()-1; i >= 0; i--)
			{
				SLuaStackEntry &le = callstack[i];

				string fileline;

				iItem = m_wCallstack.InsertItem(0, le.description.c_str(), 0);

				char s[64];
				itoa(le.line,s,10);
				m_wCallstack.SetItemText(iItem, 1, s);

				if (!le.source.empty())
				{
					if (_stricmp(le.source, "=C") == 0)
						m_wCallstack.SetItemText(iItem, 2, "Native C Function");
					else
					{
						string strLine = GetLineFromFile( le.source.c_str()+1,le.line );
						if (!strLine.empty())
						{
							string str = le.description + "  (" + strLine + ")";
							m_wCallstack.SetItemText( iItem,0,str.c_str() );
						}

						m_wCallstack.SetItemText(iItem, 2, le.source.c_str()+1 );
					}
				}
				else
					m_wCallstack.SetItemText(iItem, 2, "No Source");

			}
		}
		else
		{
			iItem = m_wCallstack.InsertItem(0, "No Lua callstack available", 0);
		}


		//////////////////////////////////////////////////////////////////////////
		// Get C++ call stack.
		//////////////////////////////////////////////////////////////////////////
		if (m_bCppCallstackEnabled)
		{
			const char* funcs[32];
			int nFuncCount = 32;
			GetISystem()->debug_GetCallStack( funcs,nFuncCount );
			if (nFuncCount > 0)
			{
				iItem = m_wCallstack.GetItemCount();
				iItem = m_wCallstack.InsertItem(iItem+1, "", 0);
				iItem = m_wCallstack.InsertItem(iItem+1, "-------------- C/C++ CallStack --------------", 0);
				iItem = m_wCallstack.InsertItem(iItem+1, "", 0);
				int nIndex = 1;
				for (int i = 8; i < nFuncCount; i++) // Skip some irrelevant functions
				{
					const char *s = funcs[i];
					// Skip all lua callstack...
					if (strstr(s,".c:") == 0)
					{
						m_wCallstack.InsertItem(iItem+nIndex, funcs[i], 0);
						nIndex++;
					}
				}
			}
		}
		else
		{
			iItem = m_wCallstack.GetItemCount();
			iItem = m_wCallstack.InsertItem(iItem+1, "", 0);
			iItem = m_wCallstack.InsertItem(iItem+1, "C/C++ callstack can be activated in menu", 0);
		}

	}

	////////////
	// Locals //
	////////////

	m_wLocals.Clear();
	m_pIVariable = m_pIScriptSystem->GetLocalVariables(1);
	m_pTreeToAdd = &m_wLocals;
	if (m_pIVariable)
	{
		m_hRoot = NULL;
		m_iRecursionLevel = 0;
		m_pIVariable->Dump((IScriptTableDumpSink *) this);
		m_pIVariable->Release();
		m_pIVariable = NULL;
	}
	else
		m_wLocals.AddItemToTree("No Locals Available");
	m_pTreeToAdd = NULL;

	
	///////////
	// Watch //
	///////////

	const char *pszText = NULL;

	m_wWatch.Clear();
	IScriptTable *pIScriptObj = m_pIScriptSystem->GetLocalVariables(1);

	string strWatchVar = "self";

	bool bVarFound = false;
	IScriptTable::Iterator iter = pIScriptObj->BeginIteration();
	while (pIScriptObj->MoveNext(iter))
	{
		if (iter.sKey)
		{
			pszText = iter.sKey;
			if (strWatchVar == pszText)
			{
				SmartScriptTable pIEntry(m_pIScriptSystem, true);
				if (iter.value.type == ANY_TTABLE)
					m_pIVariable = iter.value.table;
				else
					m_pIVariable = pIEntry;

				m_pTreeToAdd = &m_wWatch;

				if (m_pIVariable)
				{
					bVarFound = true;

					m_hRoot = NULL;
					m_iRecursionLevel = 0;

					// Dump only works for tables, in case of values call the sink directly
					if (iter.value.type == ANY_TTABLE)
						m_pIVariable->Dump((IScriptTableDumpSink *) this);
					else
					{
						m_pIVariable = pIScriptObj; // Value needs to be retrieved from parent table
						OnElementFound(pszText, iter.value.GetVarType() );
					}

					// No AddRef() !
					// m_pIVariable->Release();

					m_pIVariable = NULL;
				}
				
				m_pTreeToAdd = NULL;
			}
		}
	}
	pIScriptObj->EndIteration(iter);

	if (!bVarFound)
		m_wWatch.AddItemToTree(const_cast<LPSTR>((strWatchVar + " = ?").c_str())); // TODO: Cast...

	m_wLocals.SendMessage( WM_SETREDRAW,(WPARAM)TRUE,(LPARAM)FALSE );
	m_wWatch.SendMessage( WM_SETREDRAW,(WPARAM)TRUE,(LPARAM)FALSE );
	m_wCallstack.SendMessage( WM_SETREDRAW,(WPARAM)TRUE,(LPARAM)FALSE );
}

HTREEITEM CLUADbg::AddVariableToTree(const char *sName, ScriptVarType type, HTREEITEM hParent)
{
	char szBuf[2048];
	char szType[32];
	bool bRetrieved = false;
	int iIdx;

	if (m_pTreeToAdd == NULL)
	{
		_TinyAssert(m_pTreeToAdd !=	NULL);
		return 0;
	}

	switch (type)
	{
	case svtNull:
		strcpy(szType, "[nil]");
		break;
	case svtString:
		strcpy(szType, "[string]");
		break;
	case svtNumber:
		strcpy(szType, "[numeric]");
		break;
	case svtFunction:
		strcpy(szType, "[function]");
		break;
	case svtObject:
		strcpy(szType, "[table]");
		break;
	case svtUserData:
		strcpy(szType, "[user data]");
		break;
	case svtBool:
		strcpy(szType, "[bool]");
		break;
	default:
		strcpy(szType, "[unknown]");
		break;
	}

	if (type == svtString || type == svtNumber || type == svtBool)
	{
		float fValue = 0;
		char temp[128];
		const char *szContent = temp;
		if (sName[0] == '[')
		{
			strcpy(szBuf, &sName[1]);
			szBuf[strlen(szBuf) - 1] = '\0';
			iIdx = atoi(szBuf);
			if (type == svtString)
			{
				bRetrieved = m_pIVariable->GetAt(iIdx, szContent);
			}
			else if (type == svtNumber)
			{
				bRetrieved = m_pIVariable->GetAt(iIdx, fValue);
				sprintf( temp,"%g",fValue );
			}
			else if (type == svtBool)
			{
				bool bValue = false;
				bRetrieved = m_pIVariable->GetAt(iIdx, bValue);
				if (bValue)
					strcpy(temp,"true");
				else
					strcpy(temp,"false");
			}
		}
		else
		{
			float fVal = 0;
			if (type == svtString)
			{
        bRetrieved = m_pIVariable->GetValue(sName, szContent);
			}
			else if (type == svtNumber)
			{
				bRetrieved = m_pIVariable->GetValue(sName, fValue);
				sprintf( temp,"%g",fValue );
			}
			else if (type == svtBool)
			{
				bool bValue = false;
				bRetrieved = m_pIVariable->GetValue(sName, bValue);
				if (bValue)
					strcpy(temp,"true");
				else
					strcpy(temp,"false");
			}
		}

		if (bRetrieved)
		{
			if (type == svtString)
				sprintf(szBuf, "%s %s = \"%s\"", szType, sName, szContent);
			else
				sprintf(szBuf, "%s %s = %s", szType, sName, szContent);
		}
		else
			sprintf(szBuf, "%s %s = (Unknown)", szType, sName);
	}
	else
		sprintf(szBuf, "%s %s", szType, sName);

	HTREEITEM hNewItem = m_pTreeToAdd->AddItemToTree(szBuf, NULL, hParent);
	TreeView_SortChildren(m_pTreeToAdd->m_hWnd, hNewItem, FALSE);

	return hNewItem;
}

void CLUADbg::OnElementFound(const char *sName, ScriptVarType type)
{
	HTREEITEM hRoot = NULL;
	UINT iRecursionLevel = 0;
	SmartScriptTable pTable(m_pIScriptSystem, true);
	IScriptTable *pIOldTbl = NULL;
	HTREEITEM hOldRoot = NULL;

	if(!sName)sName="[table idx]";
	hRoot = AddVariableToTree(sName, type, m_hRoot);

	if (type == svtObject && m_iRecursionLevel < 5)
	{
		if (m_pIVariable->GetValue(sName, pTable))
		{
			pIOldTbl = m_pIVariable;
			hOldRoot = m_hRoot;
			m_pIVariable = pTable;
			m_hRoot = hRoot;
			m_iRecursionLevel++;
			pTable->Dump((IScriptTableDumpSink *) this);
			m_iRecursionLevel--;
			m_pIVariable = pIOldTbl;
			m_hRoot = hOldRoot;
		}
	}
}

void CLUADbg::OnElementFound(int nIdx,ScriptVarType type)
{
	char szBuf[32];
	sprintf(szBuf, "[%i]", nIdx);
	OnElementFound(szBuf, type);
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::Break()
{
	m_bForceBreak = true;
	m_nCallLevelUntilBreak = 0;
	m_breakState = bsNoBreak;
}

//////////////////////////////////////////////////////////////////////////
bool CLUADbg::InvokeDebugger(const char *pszSourceFile, int iLine, const char *pszReason)
{
	HACCEL hAccelerators = NULL;
	MSG msg;
	IScriptSystem *pIScriptSystem = NULL;

	pIScriptSystem = gEnv->pScriptSystem;

	if (!::IsWindow(m_hWnd))
	{
		_TinyVerify( Create(NULL, _T("Lua Debugger"), WS_OVERLAPPEDWINDOW, 0, NULL,
			NULL, (ULONG_PTR) LoadMenu(_Tiny_GetResourceInstance(), MAKEINTRESOURCE(IDC_LUADBG))));
	}
	UpdateCheckboxes(m_hWnd);
	if (!::IsWindow(m_hWnd))
		return false;

	if ((hAccelerators = LoadAccelerators(_Tiny_GetResourceInstance(), MAKEINTRESOURCE(IDR_LUADGB_ACCEL))) == NULL)
	{
		// No accelerators
	}

	// Make sure the debugger is displayed maximized when it was left like that last time
	// TODO: Maybe serialize this with the other window settings
	if (::IsZoomed(m_hWnd))
		::ShowWindow(m_hWnd, SW_MAXIMIZE);
	else
		::ShowWindow(m_hWnd, SW_NORMAL);
	::SetForegroundWindow(m_hWnd);
	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetIHardwareMouse())
		gEnv->pSystem->GetIHardwareMouse()->IncrementCounter();

	if (pszSourceFile && pszSourceFile[0] == '@')
	{
		LoadFile(&pszSourceFile[1]);
		iLine = __max(0, iLine);
		PlaceLineMarker(iLine);
	}

	if (pszReason)
		SetStatusBarText(pszReason);

	GetStackAndLocals();

	m_bQuitMsgLoop = false;
	while (GetMessage(&msg, NULL, 0, 0) && !m_bQuitMsgLoop) 
	{
		if (hAccelerators == NULL || TranslateAccelerator(m_hWnd, hAccelerators, &msg) == 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (!IsWindow(m_hWnd))
		DebugBreak();

	// Don't hide the window when the debugger will be triggered next frame anyway
	if (m_breakState != bsStepNext &&	m_breakState != bsStepInto && !m_bForceBreak)
	{
		::ShowWindow(m_hWnd, SW_HIDE);
	}

	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetIHardwareMouse())
		gEnv->pSystem->GetIHardwareMouse()->DecrementCounter();

	DestroyAcceleratorTable(hAccelerators);

	return (int) msg.wParam == 0;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CLUADbg::HaveBreakPointAt( const char *sSourceFile,int nLine )
{
	int nCount = m_breakPoints.size();
	for (int i = 0; i < nCount; i++)
	{
		if (strcmp(m_breakPoints[i].sSourceFile,sSourceFile) == 0)
			if (m_breakPoints[i].nLine == nLine) 
				return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::AddBreakPoint( const char *sSourceFile,int nLine,bool bSave )
{
	BreakPoint bp;
	bp.nLine = nLine;
	bp.sSourceFile = sSourceFile;
	stl::push_back_unique( m_breakPoints,bp );

	if (bSave)
		SaveBreakpoints( "breakpoints.lst" );
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::RemoveBreakPoint( const char *sSourceFile,int nLine )
{
	int nCount = m_breakPoints.size();
	for (int i = 0; i < nCount; i++)
	{
		if (m_breakPoints[i].nLine == nLine && strcmp(m_breakPoints[i].sSourceFile,sSourceFile) == 0)
		{
			m_breakPoints.erase( m_breakPoints.begin()+i );
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::SaveBreakpoints( const char *filename )
{
	if (m_breakPoints.size() > 0)
	{
		FILE *file = fopen( filename,"wt" );
		if (file)
		{
			int nCount = m_breakPoints.size();
			for (int i = 0; i < nCount; i++)
			{
				fprintf( file,"%d:%s\n",m_breakPoints[i].nLine,m_breakPoints[i].sSourceFile.c_str() );
			}
			fclose(file);
		}
	}
	else
		DeleteFile( filename );
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::LoadBreakpoints( const char *filename )
{
	FILE *file = fopen( filename,"rt" );
	if (file)
	{
		m_breakPoints.clear();
		char str[2048];
		int nLine;
		while (!feof(file))
		{
			if (fscanf(file,"%d:%s\n",&nLine,str) == 2)
			{
				AddBreakPoint(str,nLine,false);
			}
		}
		fclose(file);
	}
}

//////////////////////////////////////////////////////////////////////////
void CLUADbg::OnDebugHook( lua_State *L,lua_Debug *ar )
{
	switch (ar->event)
	{
	case LUA_HOOKCALL:
		// function call.
		m_nCallLevelUntilBreak++;
		break;

	case LUA_HOOKRET:
	case LUA_HOOKTAILRET:
		// return from function
		if (m_breakState == bsStepOut && m_nCallLevelUntilBreak <= 0)
			Break();
		else
			m_nCallLevelUntilBreak--;
		break;
	case LUA_HOOKLINE:
		lua_getinfo(L, "Sl", ar);

		if (m_bForceBreak)
		{
			m_nCallLevelUntilBreak = 0;
			m_breakState = bsNoBreak;
			m_bForceBreak = false;
			InvokeDebugger( ar->source,ar->currentline,"Break" );
			return;
		}

		switch (m_breakState)
		{
		case bsStepInto:
			InvokeDebugger( ar->source,ar->currentline,"StepInt" );
			break;
		case bsStepNext:
			if (m_nCallLevelUntilBreak <= 0)
				InvokeDebugger( ar->source,ar->currentline,"StepOver" );
			break;
		default:
			if (HaveBreakPointAt(ar->source,ar->currentline))
				InvokeDebugger( ar->source,ar->currentline,"BreakPoint" );
		}
	}
}

#endif //#WIN32