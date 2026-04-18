//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   AIDebugger.h
//  Version:     v1.00
//  Created:     08-03-2005 by Kirill Bulatsev
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#define OEMRESOURCE
#include "StdAfx.h"
#include "Controls/MemDC.h"
#include <math.h>
#include <stdlib.h>
#include <IAIAction.h>
#include <IAISystem.h>
#include <IAgent.h>

#include "Resource.h"
#include "IViewPane.h"
#include "Objects\Entity.h"
#include "Objects\SelectionGroup.h"
#include "Clipboard.h"

#include "ItemDescriptionDlg.h"

#include "AIManager.h"

#include "AIDebugger.h"
#include "AIDebuggerView.h"
#include "IAIRecorder.h"
#include <Cderr.h>



//////////////////////////////////////////////////////////////////////////
//
//Drawing helper
//
//////////////////////////////////////////////////////////////////////////
CAdjustDC::CAdjustDC(CDC *dc, int left, int top, int right, int bottom)
{
	m_dc = dc;
	m_dcState = dc->SaveDC();
	CPoint p = dc->GetViewportOrg();
	dc->SetViewportOrg(left,top);
	m_width = right - left;
	m_height = bottom - top;
}

CAdjustDC::CAdjustDC(CDC* dc, CRect &rect)
{
	m_dc = dc;
	m_dcState = dc->SaveDC();
	CPoint p = dc->GetViewportOrg();
	dc->SetViewportOrg(rect.left,rect.top);
	m_width = rect.right - rect.left;
	m_height = rect.bottom - rect.top;
}

CAdjustDC::~CAdjustDC()
{
	m_dc->RestoreDC(m_dcState);
}


//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CAIDebugger,CXTPFrameWnd)

BEGIN_MESSAGE_MAP(CAIDebugger, CXTPFrameWnd)
	ON_WM_DESTROY()
	ON_WM_CLOSE()
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MBUTTONDBLCLK()
	ON_WM_MOUSEWHEEL()
	
	ON_COMMAND(ID_FILE_LOAD33869, FileLoad)
	ON_COMMAND(ID_FILE_SAVE33868, FileSave)
	ON_COMMAND(ID_FILE_LOADAS, FileLoadAs)
	ON_COMMAND(ID_FILE_SAVEAS, FileSaveAs)



END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
class CAIDebuggerViewClass : public TRefCountBase<IViewPaneClass>
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_VIEWPANE; };
	virtual REFGUID ClassID()
	{
		// {2D185A30-4487-4d1c-A076-E2A5F3367760}
		static const GUID guid = {0x2d185a30, 0x4487, 0x4d1c, { 0xa0, 0x76, 0xe2, 0xa5, 0xf3, 0x36, 0x77, 0x60 } };

		return guid;
	}
	virtual const char* ClassName() { return "AI Debugger"; };
	virtual const char* Category() { return "AI Debugger"; };
	//////////////////////////////////////////////////////////////////////////
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAIDebugger); };
	virtual const char* GetPaneTitle() { return _T("AI Debugger"); };
	virtual EDockingDirection GetDockingDirection() { return DOCK_FLOAT; };
	virtual CRect GetPaneRect() { return CRect(10,250,710,500); };
	virtual bool SinglePane() { return true; };
	virtual bool WantIdleUpdate() { return true; };
};


#define AIDEBUGGER_CLASSNAME "AIDebugger"

//////////////////////////////////////////////////////////////////////////
void CAIDebugger::RegisterViewClass()
{
	GetIEditor()->GetClassFactory()->RegisterClass( new CAIDebuggerViewClass );
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CAIDebugger::CAIDebugger(CWnd* pParent /*=NULL*/):
m_pView(NULL)
{
	GetIEditor()->RegisterNotifyListener(this);

	// Create view instance
	m_pView = new CAIDebuggerView();

	WNDCLASS wndcls;
	HINSTANCE hInst = AfxGetInstanceHandle();
	if (!(::GetClassInfo(hInst, AIDEBUGGER_CLASSNAME, &wndcls)))
	{
		// otherwise we need to register a new class
		//wndcls.style            = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wndcls.style            = CS_DBLCLKS;
		wndcls.lpfnWndProc      = ::DefWindowProc;
		wndcls.cbClsExtra       = wndcls.cbWndExtra = 0;
		wndcls.hInstance        = hInst;
		wndcls.hIcon            = NULL;
		wndcls.hCursor          = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
		wndcls.hbrBackground    = (HBRUSH) (COLOR_3DFACE + 1);
		//wndcls.hbrBackground    = 0;
		wndcls.lpszMenuName     = NULL;
		wndcls.lpszClassName    = AIDEBUGGER_CLASSNAME;
		if (!AfxRegisterClass(&wndcls))
		{
			AfxThrowResourceException();
		}
	}
	CRect rc(0,0,0,0);
	BOOL bRes = Create( WS_CHILD|WS_VISIBLE,rc,AfxGetMainWnd() );
	if (!bRes)
		return;

	OnInitDialog();
}

//////////////////////////////////////////////////////////////////////////
BOOL CAIDebugger::Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd )
{
	return __super::Create( AIDEBUGGER_CLASSNAME,"",dwStyle,rect,pParentWnd );
}


//////////////////////////////////////////////////////////////////////////
CAIDebugger::~CAIDebugger()
{ 
	// Delete view instance
	SAFE_DELETE(m_pView);

	GetIEditor()->UnregisterNotifyListener( this );
}

//////////////////////////////////////////////////////////////////////////
BOOL CAIDebugger::OnCmdMsg(UINT nID, int nCode, void* pExtra,AFX_CMDHANDLERINFO* pHandlerInfo)
{
	BOOL res = FALSE;
	return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
}

//////////////////////////////////////////////////////////////////////////
BOOL CAIDebugger::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}


//////////////////////////////////////////////////////////////////////////
void CAIDebugger::OnDestroy()
{
	/*
	CXTPDockingPaneLayout layout(GetDockingPaneManager());
	GetDockingPaneManager()->GetLayout( &layout );
	layout.Save(_T("FlowGraphLayout"));
	CFlowGraphSearchOptions::GetSearchOptions()->Serialize(false);
	CXTRegistryManager regMgr;
	DWORD mask = m_componentsViewMask;
	regMgr.WriteProfileDword(_T("FlowGraph"), _T("ComponentView"), &mask);

	*/
	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
BOOL CAIDebugger::OnInitDialog()
{
	try
	{    
		// Initialize the command bars
		if (!InitCommandBars())      
			return -1;
	}
	catch (CResourceException *e)
	{
		e->Delete();
		return -1;
	}

	// Get a pointer to the command bars object.
	CXTPCommandBars* pCommandBars = GetCommandBars();
	if(pCommandBars == NULL)
	{
		TRACE0("Failed to create command bars object.\n");
		return -1; 
	}

	// Add the menu bar
	CXTPCommandBar* pMenuBar = pCommandBars->SetMenu( _T("Menu Bar"),IDR_AIDEBUGGER_MENU );	
	pMenuBar->SetFlags(xtpFlagStretched);
	pMenuBar->EnableCustomization(FALSE);

	// Custom style
	ModifyStyleEx(WS_EX_CLIENTEDGE, 0);

	// Create view
	CRect rc(0,0,0,0);
	m_pView->Create( WS_CHILD|WS_CLIPCHILDREN|WS_VISIBLE|WS_VSCROLL,rc,this,AFX_IDW_PANE_FIRST );

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
// Called by the editor to notify the listener about the specified event.
void CAIDebugger::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch ( event )
	{
	case eNotify_OnEndSimulationMode:
	case eNotify_OnEndGameMode:             // Send when editor goes out of game mode.
		{
			UpdateAll();
			Invalidate();
		}
		break;
	case eNotify_OnEndSceneOpen:
		{
			UpdateAll();
			Invalidate();
		}
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
BOOL CAIDebugger::PreTranslateMessage( MSG* pMsg )
{
	// allow tooltip messages to be filtered
	if (__super::PreTranslateMessage(pMsg))
		return TRUE;

	if ( pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST )
	{
		// All keypresses are translated by this frame window

		::TranslateMessage( pMsg );
		::DispatchMessage( pMsg );

		return TRUE;
	}

	return FALSE;
}


//////////////////////////////////////////////////////////////////////////
LRESULT CAIDebugger::OnTaskPanelNotify(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case XTP_TPN_CLICK:
		{
			CXTPTaskPanelGroupItem* pItem = (CXTPTaskPanelGroupItem*)lParam;
			UINT nCmdID = pItem->GetID();
			SendMessage( WM_COMMAND, MAKEWPARAM(nCmdID, 0), 0 );
		}
		break;
	case XTP_TPN_RCLICK:
		break;
	}

	return 0;
}


//////////////////////////////////////////////////////////////////////////
void CAIDebugger::OnClose()
{
	__super::OnClose();
}

//////////////////////////////////////////////////////////////////////////
void CAIDebugger::PostNcDestroy()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////

void CAIDebugger::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

//////////////////////////////////////////////////////////////////////////
void CAIDebugger::RecalcLayout( BOOL bNotify )
{
	__super::RecalcLayout( );
}

//////////////////////////////////////////////////////////////////////////
void CAIDebugger::UpdateAll(void)
{
	m_pView->UpdateView();
	m_pView->Invalidate();
}


//////////////////////////////////////////////////////////////////////////

BOOL CAIDebugger::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	int t = 5;
	CWnd::OnMouseWheel(nFlags, zDelta, pt);
	m_pView->OnMouseWheel(nFlags, zDelta, pt);
	return FALSE;
}

void CAIDebugger::FileLoad(void)
{
	IAIRecorder *pRecorder = gEnv->pAISystem->GetIAIRecorder();
	if (!pRecorder->Load())
		gEnv->pLog->LogError("Failed to complete loading AI recording");
	else
	{
		gEnv->pLog->Log("Successfully loaded AI recording");
		UpdateAll();		
	}
}

void CAIDebugger::FileSave(void)
{
	IAIRecorder *pRecorder = gEnv->pAISystem->GetIAIRecorder();
	pRecorder->Save();
}

void CAIDebugger::FileSaveAs(void)
{
	char szFilters[] = "AI Recorder Files (*.rcd)|*.rcd| ";
	string sDefaultFileName = "AIRECORD.rcd";
	CAutoDirectoryRestoreFileDialog dlg(FALSE, "rcd", sDefaultFileName.c_str(), OFN_NOCHANGEDIR|OFN_NOREADONLYRETURN, szFilters);

	// Show the dialog
	if (dlg.DoModal() == IDOK) 
	{
		BeginWaitCursor();
		char ext[_MAX_EXT];
		_splitpath( dlg.GetPathName(),NULL,NULL,NULL,ext );
		if (stricmp(ext,".rcd") == 0)
		{
			IAIRecorder *pRecorder = gEnv->pAISystem->GetIAIRecorder();
			pRecorder->Save( dlg.GetPathName());
		}
		EndWaitCursor();
	}
	else
	{
		// Find out what really went wrong.
		switch (CommDlgExtendedError())
		{
		case 0:
			// The user cancelled the operation.
			break;

#define COMDLGERR(E) case E: CryMessageBox("Load dialog failed: " #E, "Dialog Failure", MB_OKCANCEL | MB_ICONERROR); break
			COMDLGERR(CDERR_DIALOGFAILURE);
			COMDLGERR(CDERR_FINDRESFAILURE);
			COMDLGERR(CDERR_INITIALIZATION);
			COMDLGERR(CDERR_LOADRESFAILURE);
			COMDLGERR(CDERR_LOADSTRFAILURE);
			COMDLGERR(CDERR_LOCKRESFAILURE);
			COMDLGERR(CDERR_MEMALLOCFAILURE);
			COMDLGERR(CDERR_MEMLOCKFAILURE);
			COMDLGERR(CDERR_NOHINSTANCE);
			COMDLGERR(CDERR_NOHOOK);
			COMDLGERR(CDERR_NOTEMPLATE);
			COMDLGERR(CDERR_STRUCTSIZE);
			COMDLGERR(FNERR_BUFFERTOOSMALL);
			COMDLGERR(FNERR_INVALIDFILENAME);
			COMDLGERR(FNERR_SUBCLASSFAILURE);
#undef COMDLGERR
		default:
			CryMessageBox("Load dialog failed: UNKNOWN", "Dialog Failure", MB_OKCANCEL | MB_ICONERROR);
			break;
		}
	}
	// Either way, retrieve the focus
	SetFocus();
}


void CAIDebugger::FileLoadAs(void)
{
	char szFilters[] = "AI Recorder Files (*.rcd)|*.rcd| ";
	string sDefaultFileName = "AIRECORD.rcd";
	CAutoDirectoryRestoreFileDialog dlg(TRUE, "rcd", sDefaultFileName.c_str(), OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST, szFilters);
	// Show the dialog
	if (dlg.DoModal() == IDOK) 
	{
		BeginWaitCursor();
		char ext[_MAX_EXT];
		_splitpath( dlg.GetPathName(),NULL,NULL,NULL,ext );
		if (stricmp(ext,".rcd") == 0)
		{
			IAIRecorder *pRecorder = GetISystem()->GetAISystem()->GetIAIRecorder();
			if (!pRecorder->Load( dlg.GetPathName() ))
				gEnv->pLog->LogError("Failed to complete loading AI recording");
			else
			{
				gEnv->pLog->Log("Successfully loaded AI recording");
				UpdateAll();		
			}
		}
		EndWaitCursor();
	}
	else
	{
		// Find out what really went wrong.
		switch (CommDlgExtendedError())
		{
		case 0:
			// The user cancelled the operation.
			break;

#define COMDLGERR(E) case E: CryMessageBox("Load dialog failed: " #E, "Dialog Failure", MB_OKCANCEL | MB_ICONERROR); break
			COMDLGERR(CDERR_DIALOGFAILURE);
			COMDLGERR(CDERR_FINDRESFAILURE);
			COMDLGERR(CDERR_INITIALIZATION);
			COMDLGERR(CDERR_LOADRESFAILURE);
			COMDLGERR(CDERR_LOADSTRFAILURE);
			COMDLGERR(CDERR_LOCKRESFAILURE);
			COMDLGERR(CDERR_MEMALLOCFAILURE);
			COMDLGERR(CDERR_MEMLOCKFAILURE);
			COMDLGERR(CDERR_NOHINSTANCE);
			COMDLGERR(CDERR_NOHOOK);
			COMDLGERR(CDERR_NOTEMPLATE);
			COMDLGERR(CDERR_STRUCTSIZE);
			COMDLGERR(FNERR_BUFFERTOOSMALL);
			COMDLGERR(FNERR_INVALIDFILENAME);
			COMDLGERR(FNERR_SUBCLASSFAILURE);
#undef COMDLGERR
		default:
			CryMessageBox("Load dialog failed: UNKNOWN", "Dialog Failure", MB_OKCANCEL | MB_ICONERROR);
			break;
		}
	}
	// Either way, retrieve the focus
	SetFocus();
}
