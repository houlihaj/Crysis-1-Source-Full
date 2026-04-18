////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ToolBoxBar.cpp
//  Version:     v1.00
//  Created:     19/11/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ToolBoxBar.h"
#include "EditTool.h"
#include "ExternalTools.h"

#define IDC_TASKPANEL 1
/////////////////////////////////////////////////////////////////////////////
// CToolBoxDialog dialog

CToolBoxDialog::CToolBoxDialog()
: CDialog(CToolBoxDialog::IDD)
{
}

//////////////////////////////////////////////////////////////////////////
void CToolBoxDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CToolBoxDialog, CDialog)
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_MESSAGE(XTPWM_TASKPANEL_NOTIFY, OnTaskPanelNotify)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CToolBoxDialog message handlers
BOOL CToolBoxDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();

	CRect rc;
	GetClientRect( rc );

	//m_imageList.Create( 16,16,ILC_COLOR32|ILC_MASK,1,4 );
	//m_imageList.Create( 32,32,ILC_COLOR32,0,4 );
	m_imageList.Create( IDB_TOOLBOX,16,1,RGB(255,0,255) );

	m_wndTaskPanel.Create( WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN,rc,this,IDC_TASKPANEL );

	m_wndTaskPanel.SetBehaviour(xtpTaskPanelBehaviourExplorer);
	//m_wndTaskPanel.SetTheme(xtpTaskPanelThemeListViewOffice2003);
	//m_wndTaskPanel.SetTheme(xtpTaskPanelThemeToolboxWhidbey);
	m_wndTaskPanel.SetTheme(xtpTaskPanelThemeNativeWinXP);
	m_wndTaskPanel.SetHotTrackStyle(xtpTaskPanelHighlightItem);
	m_wndTaskPanel.SetSelectItemOnFocus(TRUE);
	m_wndTaskPanel.AllowDrag(FALSE);
	
	m_wndTaskPanel.GetPaintManager()->m_rcGroupOuterMargins.SetRect(0,0,0,0);
	m_wndTaskPanel.GetPaintManager()->m_rcGroupInnerMargins.SetRect(0,0,0,0);
	m_wndTaskPanel.GetPaintManager()->m_rcItemOuterMargins.SetRect(0,0,0,0);
	m_wndTaskPanel.GetPaintManager()->m_rcItemInnerMargins.SetRect(1,1,1,1);
	m_wndTaskPanel.GetPaintManager()->m_rcControlMargins.SetRect(2,0,2,0);
	m_wndTaskPanel.GetPaintManager()->m_nGroupSpacing = 0;

//	m_wndTaskPanel.GetPaintManager()->m_bOfficeHighlight = TRUE;


	UpdateTools();

//	AutoLoadPlacement( "Dialogs\\ToolBox" );
	m_wndTaskPanel.SetImageList( &m_imageList );

	GetIEditor()->RegisterNotifyListener(this);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

//////////////////////////////////////////////////////////////////////////
void CToolBoxDialog::OnDestroy() 
{
	GetIEditor()->UnregisterNotifyListener(this);
	CDialog::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CToolBoxDialog::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);

	CRect rc;
	GetClientRect( rc );
	
	if (m_wndTaskPanel.m_hWnd)
		m_wndTaskPanel.MoveWindow( rc,TRUE );
}

//////////////////////////////////////////////////////////////////////////
LRESULT CToolBoxDialog::OnTaskPanelNotify(WPARAM wParam, LPARAM lParam)
{
	switch(wParam) {
	case XTP_TPN_CLICK:
		{
			CXTPTaskPanelGroupItem* pItem = (CXTPTaskPanelGroupItem*)lParam;
			UnselectAllExceptThisItem(pItem);
			UINT nCmdID = pItem->GetID();
			SelectCmd( nCmdID );
		}
		break;

	case XTP_TPN_RCLICK:
		break;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CToolBoxDialog::UpdateTools()
{
	//m_wndTaskPanel.Dele

	m_wndTaskPanel.GetGroups()->Clear();

	XmlParser parser;
	XmlNodeRef root = parser.parse( "Editor/ToolBox.xml");
	if (!root)
	{
		return;
	}

	m_commandMap.clear();

	int nGroup;
	int nCommand = 1;
	int nImage = -1;
	for (nGroup = 0; nGroup < root->getChildCount(); nGroup++)
	{
		XmlNodeRef groupNode = root->getChild(nGroup);
		if (!groupNode->isTag("Group"))
			continue;

		CXTPTaskPanelGroup* pFolder = m_wndTaskPanel.AddGroup(nGroup+1);
		pFolder->SetCaption( groupNode->getAttr("Name") );
		pFolder->SetItemLayout(xtpTaskItemLayoutDefault);

		for (int nItem = 0; nItem < groupNode->getChildCount(); nItem++)
		{
			XmlNodeRef itempNode = groupNode->getChild(nItem);
			if (!itempNode->isTag("Item"))
				continue;

			nImage = 0;
			CString iconFile;
			if (itempNode->getAttr( "Icon",iconFile ))
			{
				nImage = AddIconFile( iconFile );
			}

			CXTPTaskPanelGroupItem *pItem =  pFolder->AddLinkItem(nCommand,nImage);
			pItem->SetType(xtpTaskItemTypeLink);
			pItem->SetCaption( itempNode->getAttr("Name") );
			itempNode->getAttr( "Icon",iconFile );

			CString tool;
			itempNode->getAttr( "Tool",tool );

			ToolCmd cmd;
			cmd.name = tool;
			cmd.type = ETOOL_CMD_EDITTOOL;
			m_commandMap[nCommand] = cmd;

			nCommand++;
		}
	}

	{
		CXTPTaskPanelGroup* pFolder = m_wndTaskPanel.AddGroup(nGroup+1);
		pFolder->SetCaption( "User Cmds" );
		pFolder->SetItemLayout(xtpTaskItemLayoutDefault);

		CExternalToolsManager *pToolMgr = GetIEditor()->GetExternalToolsManager();
		for (int i = 0; i < pToolMgr->GetToolsCount(); i++)
		{
			CExternalTool *pTool = pToolMgr->GetTool(i);

			CXTPTaskPanelGroupItem *pItem =  pFolder->AddLinkItem(nCommand,1);
			pItem->SetType(xtpTaskItemTypeLink);
			pItem->SetCaption( pTool->m_title );

			ToolCmd cmd;
			cmd.name = pTool->m_title;
			cmd.type = ETOOL_CMD_USERCMD;
			m_commandMap[nCommand] = cmd;

			nCommand++;
		}
	}

	SelectCurrentTool();
}

//////////////////////////////////////////////////////////////////////////
void CToolBoxDialog::SelectCmd( int nCmdID )
{
	CommandMap::iterator it = m_commandMap.find(nCmdID);
	if (it != m_commandMap.end())
	{
		const ToolCmd &cmd = it->second;
		if (cmd.type == ETOOL_CMD_EDITTOOL)
		{
			GetIEditor()->SetEditTool( cmd.name );
		}
		else if (cmd.type == ETOOL_CMD_USERCMD)
		{
			GetIEditor()->GetExternalToolsManager()->ExecuteTool( cmd.name );
		}
	}
	SelectCurrentTool();
}

//////////////////////////////////////////////////////////////////////////
int CToolBoxDialog::AddIconFile( const char *filename )
{
	CImage image;
	if (CImageUtil::LoadImage( filename,image ))
	{
		//CImageUtil::ScaleToFit( )
		if (image.GetWidth() == 16 && image.GetHeight() == 16)
		{
			CBitmap bmp;

			VERIFY( bmp.CreateBitmap( image.GetWidth(),image.GetHeight(),1,32,image.GetData() ) );
			int nRes = m_imageList.Add( &bmp,RGB(255,0,255) );
			//ASSERT( nRes != -1 );
			nRes = 0;
			return nRes;
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
void CToolBoxDialog::UnselectAllExceptThisItem( CXTPTaskPanelGroupItem* pItem )
{
	CXTPTaskPanelGroups *pGroups = (CXTPTaskPanelGroups*)m_wndTaskPanel.GetGroups();
	for (int group = 0; group < pGroups->GetCount(); group++)
	{
		CXTPTaskPanelGroup* pGroup = (CXTPTaskPanelGroup*)pGroups->GetAt(group);
		pGroup->SetSelectedItem(pItem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CToolBoxDialog::UnselectAllExceptThisItemID( int nID )
{
	CXTPTaskPanelGroups *pGroups = (CXTPTaskPanelGroups*)m_wndTaskPanel.GetGroups();
	for (int group = 0; group < pGroups->GetCount(); group++)
	{
		CXTPTaskPanelGroup* pGroup = (CXTPTaskPanelGroup*)pGroups->GetAt(group);
		CXTPTaskPanelGroupItem *pItem = pGroup->FindItem(nID);
		pGroup->SetSelectedItem(pItem);
	}
}

//////////////////////////////////////////////////////////////////////////
void CToolBoxDialog::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch (event)
	{
	case eNotify_OnEditToolChange:
		SelectCurrentTool();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CToolBoxDialog::SelectCurrentTool()
{
	int nCmdId = -1;
	CEditTool *pTool = GetIEditor()->GetEditTool();
	if (pTool && pTool->GetClassDesc())
	{
		const char *sClassName = pTool->GetClassDesc()->ClassName();
		for (CommandMap::iterator it = m_commandMap.begin(); it != m_commandMap.end(); ++it)
		{
			if (stricmp(it->second.name,sClassName) == 0)
			{
				nCmdId = it->first;
				break;
			}
		}
	}
	UnselectAllExceptThisItemID(nCmdId);
}
