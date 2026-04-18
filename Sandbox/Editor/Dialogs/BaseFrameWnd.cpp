
#include <StdAfx.h>
#include "BaseFrameWnd.h"

#define BASEFRAME_WINDOW_CLASSNAME "CBaseFrameWndClass"

BEGIN_MESSAGE_MAP(CBaseFrameWnd, CXTPFrameWnd)
	ON_WM_DESTROY()
	// XT Commands.
	ON_MESSAGE( XTPWM_DOCKINGPANE_NOTIFY, OnDockingPaneNotify )
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CBaseFrameWnd::CBaseFrameWnd()
{
	m_frameLayoutVersion = 0;

	WNDCLASS wndcls;
	HINSTANCE hInst = AfxGetInstanceHandle();
	if (!(::GetClassInfo(hInst, BASEFRAME_WINDOW_CLASSNAME, &wndcls)))
	{
		// otherwise we need to register a new class
		wndcls.style            = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wndcls.lpfnWndProc      = ::DefWindowProc;
		wndcls.cbClsExtra       = wndcls.cbWndExtra = 0;
		wndcls.hInstance        = hInst;
		wndcls.hIcon            = NULL;
		wndcls.hCursor          = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
		wndcls.hbrBackground    = (HBRUSH) (COLOR_3DFACE + 1);
		wndcls.lpszMenuName     = NULL;
		wndcls.lpszClassName    = BASEFRAME_WINDOW_CLASSNAME;
		if (!AfxRegisterClass(&wndcls))
		{
			AfxThrowResourceException();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CBaseFrameWnd::Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd )
{
	BOOL bRet = __super::Create( BASEFRAME_WINDOW_CLASSNAME,NULL,dwStyle,rect,pParentWnd );
	if (bRet == TRUE)
	{
		try
		{
			//////////////////////////////////////////////////////////////////////////
			// Initialize the command bars
			if (!InitCommandBars())
				return FALSE;

		}	catch (CResourceException *e)
		{
			e->Delete();
			return FALSE;
		}

		//////////////////////////////////////////////////////////////////////////
		// Install docking panes
		//////////////////////////////////////////////////////////////////////////
		GetDockingPaneManager()->InstallDockingPanes(this);
		GetDockingPaneManager()->SetTheme(xtpPaneThemeOffice2003);
		GetDockingPaneManager()->SetThemedFloatingFrames(TRUE);

		if (CMainFrame::GetDockingHelpers())
		{
			GetDockingPaneManager()->SetAlphaDockingContext(TRUE);
			GetDockingPaneManager()->SetShowDockingContextStickers(TRUE);
		}
		//////////////////////////////////////////////////////////////////////////

		bRet = OnInitDialog();
	}
	return bRet;
}

//////////////////////////////////////////////////////////////////////////
void CBaseFrameWnd::OnDestroy()
{
	if (!m_frameLayoutKey.IsEmpty())
	{
		SaveFrameLayout();
	}
	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CBaseFrameWnd::PostNcDestroy()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
BOOL CBaseFrameWnd::PreTranslateMessage(MSG* pMsg)
{
	// allow tooltip messages to be filtered
	if (__super::PreTranslateMessage(pMsg))
		return TRUE;

	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		// All keypresses are translated by this frame window

		::TranslateMessage(pMsg);
		::DispatchMessage(pMsg);

		return TRUE;
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CBaseFrameWnd::AutoLoadFrameLayout( const CString &name,int nVersion )
{
	m_frameLayoutVersion = nVersion;
	m_frameLayoutKey = name;
	LoadFrameLayout();
}

//////////////////////////////////////////////////////////////////////////
void CBaseFrameWnd::LoadFrameLayout()
{
	CXTRegistryManager regMgr;
	int paneLayoutVersion = regMgr.GetProfileInt(CString(_T("FrameLayout_"))+m_frameLayoutKey, _T("LayoutVersion_"), 0);
	if (paneLayoutVersion == m_frameLayoutVersion)
	{
		CXTPDockingPaneLayout layout(GetDockingPaneManager());
		if (layout.Load( CString(_T("FrameLayout_"))+m_frameLayoutKey ))
		{
			if (layout.GetPaneList().GetCount() > 0)
			{
				GetDockingPaneManager()->SetLayout(&layout);	
			}
		}
	}
	else
	{
		regMgr.WriteProfileInt(CString(_T("FrameLayout_"))+m_frameLayoutKey, _T("LayoutVersion_"), m_frameLayoutVersion);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBaseFrameWnd::SaveFrameLayout()
{
	CXTPDockingPaneLayout layout(GetDockingPaneManager());
	GetDockingPaneManager()->GetLayout( &layout );
	layout.Save( CString(_T("FrameLayout_"))+m_frameLayoutKey );
	
	CXTRegistryManager regMgr;
	regMgr.WriteProfileInt(CString(_T("FrameLayout_"))+m_frameLayoutKey, _T("LayoutVersion_"), m_frameLayoutVersion);
}