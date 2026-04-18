#pragma once

#include <windows.h>
#include "_TinyMain.h"
#include "_TinySplitter.h"
#include "_TinyStatusBar.h"
#include "_TinyCaptionWindow.h"

#include "FileTree.h"

#include <IScriptSystem.h>

#include "..\ScriptSystem.h"
#include "..\resource.h"
#include <stdio.h>
#include <list>

#define IDC_SOURCE 666
#define IDC_LOCALS 667
#define IDC_WATCH 668
#define IDC_FILES 669

#define BORDER_SIZE 50

//////////////////////////////////////////////////////////////////////////
// Break Point
//////////////////////////////////////////////////////////////////////////
struct BreakPoint
{
	int nLine;
	string sSourceFile;

	BreakPoint() { nLine=-1; }
	BreakPoint( const BreakPoint& b )
	{
		nLine=b.nLine;
		sSourceFile=b.sSourceFile;
	}
	bool operator ==( const BreakPoint& b ) { return nLine == b.nLine && sSourceFile == b.sSourceFile; }
	bool operator !=( const BreakPoint& b ) { return nLine != b.nLine || sSourceFile != b.sSourceFile; }
};
typedef std::vector<BreakPoint> BreakPoints;
//////////////////////////////////////////////////////////////////////////

class CSourceEdit : public _TinyWindow
{
	public:
	CSourceEdit() { m_iLineMarkerPos = 1; m_hFont = 0; };
	~CSourceEdit() { if (m_hFont) DeleteObject(m_hFont); };

protected:
	_BEGIN_MSG_MAP(CSourceEdit)
		_MESSAGE_HANDLER(WM_SIZE,OnSize)
		_MESSAGE_HANDLER(WM_PAINT,OnPaint)
		_BEGIN_COMMAND_HANDLER()
			_BEGIN_CMD_EVENT_FILTER(IDC_SOURCE)
					_EVENT_FILTER(EN_VSCROLL,OnScroll)
					_EVENT_FILTER(EN_HSCROLL,OnScroll)
					_EVENT_FILTER(EN_UPDATE ,OnScroll)
			_END_CMD_EVENT_FILTER()
		_END_COMMAND_HANDLER()
	_END_MSG_MAP()

public:
	
	void SetLineMarker(UINT iLine) {  m_iLineMarkerPos = iLine; };

	void SetScriptSystem( CScriptSystem *pIScriptSystem ) { m_pIScriptSystem = pIScriptSystem; };
	void SetLuaDbg( CLUADbg *dbg ) { m_pLuaDbg = dbg; }
	
	BOOL Create(DWORD dwStyle=WS_VISIBLE,DWORD dwExStyle=0,const RECT* pRect=NULL,_TinyWindow *pParentWnd=NULL,ULONG nID=0);

	BOOL SetText(char *pszText)
	{
		EDITSTREAM strm;
		char ***pppText = new char **;
		*pppText = &pszText;
		strm.dwCookie = (DWORD_PTR) pppText;
		strm.dwError = 0;
		strm.pfnCallback = EditStreamCallback;
		m_wEditWindow.SendMessage(EM_LIMITTEXT, 0x7FFFFFF, 0);
		return m_wEditWindow.SendMessage(EM_STREAMIN, SF_RTF, (LPARAM) &strm);
	};

	friend DWORD CALLBACK EditStreamCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
	{
		char ***pppText = reinterpret_cast<char ***> (dwCookie);
		char **ppText = *pppText;
		LONG iLen = (LONG)strlen(* ppText) /*- 1*/;
		*pcb = __min(cb, iLen);
		if (*pcb == 0)
		{
			delete pppText;
			return 0;
		}
		memcpy(pbBuff, (* ppText), *pcb);
		*ppText += *pcb;
		return 0;
	};

	void SetSourceFile(const char *pszFileName)
	{
		m_strSourceFile = pszFileName;
	};

	const char * GetSourceFile() { return m_strSourceFile.c_str(); };

	// Return current editing line.
	int GetCurLine()
	{
		return m_wEditWindow.LineFromChar(m_wEditWindow.GetSel());
	}

	//////////////////////////////////////////////////////////////////////////
	void InvalidateEditWindow()
	{
		::InvalidateRect(m_wEditWindow.m_hWnd, NULL, FALSE);
	}

	void ScrollToLine(UINT iLine)
	{
		m_wEditWindow.ScrollToLine(iLine);
	};

private:
	LRESULT OnEraseBkGnd(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnPaint(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnSize(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnScroll(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam);
	
protected:
	_TinyRichEdit m_wEditWindow;
	UINT m_iLineMarkerPos;
	CScriptSystem *m_pIScriptSystem;
	string m_strSourceFile;
	CLUADbg *m_pLuaDbg;

	HFONT m_hFont;
};

class CLUADbg : public _TinyFrameWindow, IScriptTableDumpSink
{
public:
	CLUADbg( CScriptSystem *pScriptSystem );
	virtual ~CLUADbg();

	// Forces break on any next line.
	void Break();
	bool InvokeDebugger( const char *sSourceFile, int nLine, const char *pszReason );
	bool LoadFile(const char *pszFile, bool bForceReload = false);
	void PlaceLineMarker(UINT iLine);
	void SetStatusBarText(const char *pszText);
	void GetStackAndLocals();
	string GetLineFromFile( const char *sFilename,int nLine );
	
	//////////////////////////////////////////////////////////////////////////
	// Breakpoints.
	//////////////////////////////////////////////////////////////////////////
	bool HaveBreakPointAt( const char *sSourceFile,int nLine );
	void AddBreakPoint( const char *sSourceFile,int nLine,bool bSave=true );
	void RemoveBreakPoint( const char *sSourceFile,int nLine );
	BreakPoints* GetBreakPoints() { return &m_breakPoints; };
	//////////////////////////////////////////////////////////////////////////

	void SaveBreakpoints( const char *filename );
	void LoadBreakpoints( const char *filename );


	// Debug hook callback.
	void OnDebugHook( lua_State *L,lua_Debug *ar );

	IScriptSystem * GetScriptSystem() { return m_pIScriptSystem; };

	// For callback use byIScriptTableDumpSink only, don't call directly
	void OnElementFound(const char *sName,ScriptVarType type);
	void OnElementFound(int nIdx,ScriptVarType type);

protected:
	_BEGIN_MSG_MAP(CMainWindow)
		_MESSAGE_HANDLER(WM_CLOSE,OnClose)
		_MESSAGE_HANDLER(WM_SIZE,OnSize)
		_MESSAGE_HANDLER(WM_CREATE,OnCreate)
		_MESSAGE_HANDLER(WM_ERASEBKGND,OnEraseBkGnd)
			_BEGIN_COMMAND_HANDLER()
			_COMMAND_HANDLER(IDM_EXIT, OnClose)
			_COMMAND_HANDLER(IDM_ABOUT, OnAbout)
			_COMMAND_HANDLER(ID_EDIT_GOTO, OnGoTo)
			_COMMAND_HANDLER(ID_DEBUG_RUN, OnDebugRun)
			_COMMAND_HANDLER(ID_DEBUG_BREAK, OnDebugBreak)
			_COMMAND_HANDLER(ID_DEBUG_TOGGLEBREAKPOINT, OnToggleBreakpoint)
			_COMMAND_HANDLER(ID_DEBUG_DELETEALLBREAKPOINTS, OnDeleteAllBreakpoints)
			_COMMAND_HANDLER(ID_DEBUG_STEPINTO, OnDebugStepInto)
			_COMMAND_HANDLER(ID_DEBUG_STEPOVER, OnDebugStepOver)
			_COMMAND_HANDLER(ID_DEBUG_STEPOUT, OnDebugStepOut)
			_COMMAND_HANDLER(ID_DEBUG_DISABLE, OnDebugDisable)
			_COMMAND_HANDLER(ID_DEBUG_ERRORS, OnDebugErrors)
			_COMMAND_HANDLER(ID_DEBUG_ENABLE, OnDebugEnable)
			_COMMAND_HANDLER(ID_DEBUG_ACTIVATEC, OnDebugActivateCppCallstack)
			_COMMAND_HANDLER(ID_VIEW_RESET_VIEW, OnResetView)
			_COMMAND_HANDLER(ID_FILE_RELOAD, OnFileReload)
		_END_COMMAND_HANDLER()
		_MESSAGE_HANDLER(WM_NOTIFY,OnNotify)
		
	_END_MSG_MAP()

	//
	LRESULT RegSanityCheck(bool bForceDelete = false);
	LRESULT SetupSplitters(void);
	LRESULT UpdateCheckboxes(HWND hWnd);
	LRESULT OnResetView(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnClose(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnCreate(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnSize(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnEraseBkGnd(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnAbout(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnGoTo(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnToggleBreakpoint(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnDeleteAllBreakpoints(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT OnDebugStepInto(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
	{
		bool bInLuaCode = ! m_pIScriptSystem->IsCallStackEmpty();
		if (bInLuaCode)
		{
			m_breakState = bsStepInto;
			m_nCallLevelUntilBreak = 0;
			m_bQuitMsgLoop = true;
		}
		return 1;
	};
	LRESULT OnDebugStepOver(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
	{
		bool bInLuaCode = ! m_pIScriptSystem->IsCallStackEmpty();
		if (bInLuaCode)
		{
			m_breakState = bsStepNext;
			m_nCallLevelUntilBreak = 0;
			m_bQuitMsgLoop = true;
		}
		return 1;
	};
	LRESULT OnDebugStepOut(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
	{
		bool bInLuaCode = ! m_pIScriptSystem->IsCallStackEmpty();
		if (bInLuaCode)
		{
			m_breakState = bsStepOut;
			m_nCallLevelUntilBreak = 0;
			m_bQuitMsgLoop = true;
		}
		return 1;
	};
	LRESULT OnDebugRun(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (m_breakState == bsStepNext || m_breakState == bsStepInto)
		{
			// Leave step-by-step debugging
			m_breakState = bsContinue;
		}
		OnClose(hWnd,message,wParam,lParam);
		return 1;
	};
	LRESULT OnDebugBreak(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
	{
		m_bForceBreak = true;
		m_bQuitMsgLoop = true;
		return 1;
	};
	LRESULT OnDebugDisable(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
	{
		// Disable debugging completely and close
		
		m_breakState = bsNoBreak;
		if (ICVar *pDebug = gEnv->pConsole->GetCVar("lua_debugger"))
			pDebug->Set( eLDM_NoDebug );
		UpdateCheckboxes(hWnd);
		m_wScriptWindow.InvalidateEditWindow();

		OnClose(hWnd,message,wParam,lParam);
		return 1;
	}
	LRESULT OnDebugEnable(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
	{
		// Enable full debugging

		m_breakState = bsContinue;
		if (ICVar *pDebug = gEnv->pConsole->GetCVar("lua_debugger"))
			pDebug->Set( eLDM_FullDebug );
		UpdateCheckboxes(hWnd);
		m_wScriptWindow.InvalidateEditWindow();

		return 1;
	}
	LRESULT OnDebugActivateCppCallstack(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
	{
		// Activate C++ Callstack display

		m_bCppCallstackEnabled = true;
		GetStackAndLocals();
		
		return 1;
	}
	LRESULT OnDebugErrors(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
	{
		// Disable breakpoints, just debug on errors

		m_breakState = bsNoBreak;
		if (ICVar *pDebug = gEnv->pConsole->GetCVar("lua_debugger"))
			pDebug->Set( eLDM_OnlyErrors );
		UpdateCheckboxes(hWnd);
		m_wScriptWindow.InvalidateEditWindow();

		OnClose(hWnd,message,wParam,lParam);
		return 1;
	}
	LRESULT OnNotify(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
	{
		NMHDR *n = (NMHDR *) lParam;
		if (n->code == NM_DBLCLK) {
			if(n->hwndFrom == m_wFilesTree.m_hWnd)
			{
				const char *pszFileName = m_wFilesTree.GetCurItemFileName();
				if(!pszFileName)
					return 1;
				LoadFile(pszFileName);
			}
			if(n->hwndFrom == m_wCallstack.m_hWnd)
			{
				char temp[512];
				int sel=m_wCallstack.GetSelection();
				if(sel!=-1)
				{
					m_wCallstack.GetItemText(sel,1,temp,sizeof(temp));
					int linenum=atoi(temp);
					if(linenum!=-1){
						m_wCallstack.GetItemText(sel,2,temp,sizeof(temp));
						LoadFile(temp);
						PlaceLineMarker(linenum);
						
					}
				}

				//jumping in the carrect func
			}
		}
		return 1;
	}
		
	LRESULT OnFileReload(HWND hWnd,UINT message, WPARAM wParam, LPARAM lParam)
	{
		string strFileName = m_wScriptWindow.GetSourceFile();

		if ((* strFileName.begin()) == '@')
			strFileName = strFileName.substr(1);

		LoadFile(strFileName.c_str(), true);
		return 1;
	}

	bool Reshape(int w, int h);
	HTREEITEM AddVariableToTree(const char *sName, ScriptVarType type, HTREEITEM hParent = NULL);
	void DumpTable(HTREEITEM hParent, IScriptTable *pITable, UINT& iRecursionLevel);

	CSourceEdit m_wScriptWindow;
	_TinyToolbar m_wToolbar;
	_TinyTreeView m_wLocals;
	_TinyTreeView m_wWatch;
	_TinyListView m_wCallstack;

	CFileTree m_wFilesTree;
	
	_TinyWindow m_wndClient;

	_TinyStatusBar m_wndStatus;

	_TinyCaptionWindow m_wndLocalsCaption;
	_TinyCaptionWindow m_wndWatchCaption;
	_TinyCaptionWindow m_wndCallstackCaption;
	_TinyCaptionWindow m_wndFileViewCaption;
	_TinyCaptionWindow m_wndSourceCaption;

	_TinySplitter m_wndMainHorzSplitter;
	_TinySplitter m_wndWatchSplitter;
	_TinySplitter m_wndWatchCallstackSplitter;
	_TinySplitter m_wndSrcEditSplitter;

	CScriptSystem *m_pIScriptSystem;
	ICryPak *m_pIPak;

	// Used to let the enumeration function access it
	IScriptTable *m_pIVariable;
	HTREEITEM m_hRoot;
	UINT m_iRecursionLevel;
	_TinyTreeView *m_pTreeToAdd;

	bool m_bQuitMsgLoop;

	//////////////////////////////////////////////////////////////////////////
	// Lua debugging related.
	//////////////////////////////////////////////////////////////////////////
	bool m_bForceBreak;
	BreakState m_breakState;
	int m_nCallLevelUntilBreak;
	bool m_bCppCallstackEnabled;

	BreakPoints m_breakPoints;
};