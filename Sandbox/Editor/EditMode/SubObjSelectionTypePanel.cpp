////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SubObjSelectionTypePanel.h
//  Version:     v1.00
//  Created:     26/11/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SubObjSelectionTypePanel.h"
#include "Objects\SubObjSelection.h"

//////////////////////////////////////////////////////////////////////////
/// CXTPTaskPanelOffice2003ThemePlain

class CSubObjSelectionTypePanelTheme : public CXTPTaskPanelPaintManager
{
public:
	CSubObjSelectionTypePanelTheme()
	{
		m_bOfficeHighlight = 2;
		RefreshMetrics();
	}
	void RefreshMetrics()
	{
		CXTPTaskPanelPaintManager::RefreshMetrics();

		m_eGripper = xtpTaskPanelGripperNone;	

		COLORREF clrBackground;
		COLORREF clr3DShadow;

		//clrBackground = XTPColorManager()->LightColor(GetSysColor(COLOR_3DFACE), GetSysColor(COLOR_WINDOW), 50);        
		clrBackground = GetSysColor(COLOR_3DFACE);//, GetSysColor(COLOR_WINDOW), 50);
		clr3DShadow = GetXtremeColor(COLOR_3DSHADOW);

		m_clrBackground = CXTPPaintManagerColorGradient(clrBackground, clrBackground); 

		m_groupNormal.clrClient = clrBackground;			
		m_groupNormal.clrHead = CXTPPaintManagerColorGradient(clr3DShadow, clr3DShadow);
		m_groupNormal.clrClientLink = m_groupNormal.clrClientLinkHot = RGB(0, 0, 0);

		m_groupSpecial = m_groupNormal;
	}
};

#define IDC_TASKPANEL 1
// CSubObjSelectionTypePanel dialog

IMPLEMENT_DYNAMIC(CSubObjSelectionTypePanel, CDialog)
CSubObjSelectionTypePanel::CSubObjSelectionTypePanel(CWnd* pParent /*=NULL*/)
	: CDialog(CSubObjSelectionTypePanel::IDD, pParent)
{
	Create( IDD,pParent );
}

CSubObjSelectionTypePanel::~CSubObjSelectionTypePanel()
{
}

void CSubObjSelectionTypePanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	
}


BEGIN_MESSAGE_MAP(CSubObjSelectionTypePanel, CDialog)
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_MESSAGE(XTPWM_TASKPANEL_NOTIFY, OnTaskPanelNotify)
END_MESSAGE_MAP()


// CSubObjSelectionTypePanel message handlers
BOOL CSubObjSelectionTypePanel::OnInitDialog()
{
	BOOL res = __super::OnInitDialog();

	CRect rc;
	GetClientRect( rc );

	m_imageList.Create( IDB_SUBOBJ_SELTYPE,16,1,RGB(255,0,255) );

	m_wndTaskPanel.Create( WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,rc,this,IDC_TASKPANEL );

	m_wndTaskPanel.SetBehaviour(xtpTaskPanelBehaviourToolbox);
	m_wndTaskPanel.SetAnimation(xtpTaskPanelAnimationNo);
	m_wndTaskPanel.SetExpandable(FALSE);
	m_wndTaskPanel.SetCustomTheme( new CSubObjSelectionTypePanelTheme );
	//m_wndTaskPanel.SetTheme(xtpTaskPanelThemeListViewOffice2003);
	//m_wndTaskPanel.SetTheme(xtpTaskPanelThemeToolboxWhidbey);
	//m_wndTaskPanel.SetTheme(xtpTaskPanelThemeNativeWinXP);
	//m_wndTaskPanel.SetTheme(xtpTaskPanelThemeToolbox);
	m_wndTaskPanel.SetHotTrackStyle(xtpTaskPanelHighlightItem);
	m_wndTaskPanel.SetSelectItemOnFocus(TRUE);
	m_wndTaskPanel.AllowDrag(FALSE);

	m_wndTaskPanel.GetPaintManager()->m_rcGroupOuterMargins.SetRect(2,2,2,2);
	m_wndTaskPanel.GetPaintManager()->m_rcGroupInnerMargins.SetRect(0,0,0,0);
	m_wndTaskPanel.GetPaintManager()->m_rcItemOuterMargins.SetRect(0,0,0,0);
	m_wndTaskPanel.GetPaintManager()->m_rcItemInnerMargins.SetRect(1,1,1,1);
	m_wndTaskPanel.GetPaintManager()->m_rcControlMargins.SetRect(2,0,2,0);
	m_wndTaskPanel.GetPaintManager()->m_nGroupSpacing = 0;

	//	m_wndTaskPanel.GetPaintManager()->m_bOfficeHighlight = TRUE;

	CXTPTaskPanelGroup* pFolder = m_wndTaskPanel.AddGroup(1);
	pFolder->ShowCaption(FALSE);
	pFolder->SetExpanded(TRUE);
	//pFolder->SetItemLayout(xtpTaskItemLayoutDefault);

	CXTPTaskPanelGroupItem *pItem,*pItemFirst;
	pItem = pFolder->AddLinkItem(1,0); pItem->SetCaption( "Vertex" ); pItemFirst = pItem;
	pItem = pFolder->AddLinkItem(2,1); pItem->SetCaption( "Edge" );
	pItem = pFolder->AddLinkItem(3,2); pItem->SetCaption( "Face" );
	pItem = pFolder->AddLinkItem(4,3); pItem->SetCaption( "Polygon" );

	m_wndTaskPanel.SetFocusedItem(pItemFirst);

	//	AutoLoadPlacement( "Dialogs\\ToolBox" );
	m_wndTaskPanel.SetImageList( &m_imageList,CSize(16,16) );
	
	return res;
}

//////////////////////////////////////////////////////////////////////////
void CSubObjSelectionTypePanel::SelectElemtType( int nIndex )
{
	if (CXTPTaskPanelGroup *pGroup = m_wndTaskPanel.FindGroup(1))
	{
		CXTPTaskPanelGroupItem *pItem = pGroup->FindItem(nIndex+1);
		m_wndTaskPanel.SetFocusedItem(pItem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSubObjSelectionTypePanel::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);

	CRect rc;
	GetClientRect( rc );

	if (m_wndTaskPanel.m_hWnd)
		m_wndTaskPanel.MoveWindow( rc,TRUE );
}

//////////////////////////////////////////////////////////////////////////
LRESULT CSubObjSelectionTypePanel::OnTaskPanelNotify(WPARAM wParam, LPARAM lParam)
{
	switch(wParam)
	{
	case XTP_TPN_CLICK:
		{
			CXTPTaskPanelGroupItem* pItem = (CXTPTaskPanelGroupItem*)lParam;
			UINT nCmdID = pItem->GetID();
			switch (nCmdID)
			{
			case 1:
				GetIEditor()->ExecuteCommand( "EditMode.SelectVertex" );
				break;
			case 2:
				GetIEditor()->ExecuteCommand( "EditMode.SelectEdge" );
				break;
			case 3:
				GetIEditor()->ExecuteCommand( "EditMode.SelectFace" );
				break;
			case 4:
				GetIEditor()->ExecuteCommand( "EditMode.SelectPolygon" );
				break;
			}
		}
		break;

	case XTP_TPN_RCLICK:
		break;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CSubObjSelectionTypePanel::OnCancel()
{
	GetIEditor()->SetEditTool(0);
}