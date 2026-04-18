// ObjectTypeBrowser.cpp : implementation file
//

#include "stdafx.h"
#include "ObjectTypeBrowser.h"
#include "ObjectCreateTool.h"
#include "Objects\ObjectManager.h"

/////////////////////////////////////////////////////////////////////////////
// ObjectTypeBrowser dialog

ObjectTypeBrowser::ObjectTypeBrowser(CWnd* pParent /*=NULL*/)
	: CDialog(ObjectTypeBrowser::IDD, pParent)
{
	//{{AFX_DATA_INIT(ObjectTypeBrowser)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	m_createTool = 0;

	Create( IDD,pParent );
}


void ObjectTypeBrowser::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(ObjectTypeBrowser, CDialog)
	ON_MESSAGE(XTPWM_TASKPANEL_NOTIFY, OnTaskPanelNotify)
	ON_WM_SIZE()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL ObjectTypeBrowser::OnInitDialog()
{
	CRect rc;
	GetClientRect(rc);
	m_wndTaskPanel.Create( WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,rc,this,1 );

	m_wndTaskPanel.SetBehaviour(xtpTaskPanelBehaviourToolbox);
	m_wndTaskPanel.SetAnimation(xtpTaskPanelAnimationNo);
	m_wndTaskPanel.SetExpandable(FALSE);
	//m_wndTaskPanel.SetCustomTheme( new CObjectTypeBrowserTheme );
	m_wndTaskPanel.SetTheme(xtpTaskPanelThemeToolboxWhidbey);
	m_wndTaskPanel.SetHotTrackStyle(xtpTaskPanelHighlightItem);
	m_wndTaskPanel.SetSelectItemOnFocus(TRUE);
	m_wndTaskPanel.AllowDrag(FALSE);

	m_wndTaskPanel.GetPaintManager()->m_rcGroupOuterMargins.SetRect(2,2,2,2);
	m_wndTaskPanel.GetPaintManager()->m_rcGroupInnerMargins.SetRect(0,0,0,0);
	m_wndTaskPanel.GetPaintManager()->m_rcItemOuterMargins.SetRect(0,0,0,0);
	m_wndTaskPanel.GetPaintManager()->m_rcItemInnerMargins.SetRect(1,1,1,1);
	m_wndTaskPanel.GetPaintManager()->m_rcControlMargins.SetRect(2,0,2,0);

	m_wndTaskPanel.GetPaintManager()->m_rcImageLayoutIconPadding.SetRect(0,0,0,0);
	m_wndTaskPanel.GetPaintManager()->m_rcItemIconPadding.SetRect(0,0,0,0);
	m_wndTaskPanel.GetPaintManager()->m_rcGroupIconPadding.SetRect(0,0,0,0);

	m_wndTaskPanel.GetPaintManager()->m_nGroupSpacing = 0;

	m_wndTaskPanel.GetPaintManager()->m_bOfficeHighlight = TRUE;
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// ObjectTypeBrowser message handlers

void ObjectTypeBrowser::SetCategory( CObjectCreateTool *createTool,const CString &category )
{
	assert( createTool != 0 );
	m_createTool = createTool;
	m_category = category;
	std::vector<CString> types;
	GetIEditor()->GetObjectManager()->GetClassTypes( category,types );

	m_lastSel = -1;

	/*
	m_list.ResetContent();
	for (int i = 0; i < types.size(); i++)
	{
		int item = m_list.AddString( types[i] );
		m_list.SetItemHeight( item,16 );
	}
	if (types.size() > 0)
	{
		m_lastSel = 0;
		m_list.SetCurSel( 0 );
		m_createTool->StartCreation( types[0] );
	}
	*/

	m_wndTaskPanel.GetGroups()->Clear();
	CXTPTaskPanelGroup* pFolder = m_wndTaskPanel.AddGroup(1);
	pFolder->ShowCaption(FALSE);
	pFolder->SetExpanded(TRUE);
	CXTPTaskPanelGroupItem *pItem;
	for (int i = 0; i < types.size(); i++)
	{
		pItem = pFolder->AddLinkItem(i); pItem->SetCaption( types[i] );
	}
	//m_wndTaskPanel.SetFocusedItem(pItemFirst);
}

void ObjectTypeBrowser::OnSelchangeList() 
{
	// TODO: Add your control notification handler code here
	int sel = m_list.GetCurSel();
	if (sel >= 0 && m_lastSel != sel)
	{
		m_lastSel = sel;
		CString type;
		m_list.GetText( sel,type );

		// Start creating object of this type.
		if (m_createTool)
			m_createTool->StartCreation( type );
	}
}

//////////////////////////////////////////////////////////////////////////
LRESULT ObjectTypeBrowser::OnTaskPanelNotify(WPARAM wParam, LPARAM lParam)
{
	switch(wParam)
	{
	case XTP_TPN_CLICK:
		{
			CXTPTaskPanelGroupItem* pItem = (CXTPTaskPanelGroupItem*)lParam;
			if (pItem)
			{
				m_lastSel = pItem->GetID();
				CString type = pItem->GetCaption();
				// Start creating object of this type.
				if (m_createTool)
					m_createTool->StartCreation( type );
			}
		}
		break;
	}
	return 0;
}

void ObjectTypeBrowser::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);
	
	if (m_list)
	{
		m_list.SetWindowPos( NULL,0,0,cx-8,cy-10,SWP_NOMOVE );
	}
	if (m_wndTaskPanel)
	{
		CRect rc;
		GetClientRect(rc);
		m_wndTaskPanel.MoveWindow(rc);
	}
}
