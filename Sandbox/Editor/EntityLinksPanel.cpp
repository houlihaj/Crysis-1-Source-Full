// EntityLinksPanel.cpp : implementation file
//

#include "stdafx.h"
#include "EntityLinksPanel.h"
#include "Objects\Entity.h"
#include "Objects\ObjectManager.h"
#include "StringDlg.h"

#include "CryEditDoc.h"
#include "Mission.h"
#include "MissionScript.h"
#include "EntityPrototype.h"

//////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CEntityLinksPanel dialog


CEntityLinksPanel::CEntityLinksPanel(CWnd* pParent /*=NULL*/)
: CXTResizeDialog(CEntityLinksPanel::IDD, pParent)
{
	m_entity = 0;
	m_currentLinkIndex = -1;
}


void CEntityLinksPanel::DoDataExchange(CDataExchange* pDX)
{
	CXTResizeDialog::DoDataExchange(pDX);

	//DDX_Control(pDX, IDC_REMOVE, m_removeButton);
	DDX_Control(pDX, IDC_ADD, m_pickButton);
	DDX_Control(pDX, IDC_LINKS, m_links);
}


BEGIN_MESSAGE_MAP(CEntityLinksPanel, CXTResizeDialog)
	//ON_COMMAND( IDC_REMOVE,OnBnClickedRemove )
	ON_NOTIFY(NM_RCLICK, IDC_LINKS, OnRclickLinks)
	ON_NOTIFY(NM_DBLCLK, IDC_LINKS, OnDblClickLinks)
	ON_NOTIFY(LVN_BEGINLABELEDIT,IDC_LINKS,OnBeginLabelEdit)
	ON_NOTIFY(LVN_ENDLABELEDIT,IDC_LINKS,OnEndLabelEdit)
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL CEntityLinksPanel::OnInitDialog() 
{
	CXTResizeDialog::OnInitDialog();

	//m_pickButton.SetPushedBkColor( RGB(255,255,0) );
	m_pickButton.SetPickCallback( this,_T("Pick Target Entity"),RUNTIME_CLASS(CEntity) );

	SetResize( IDC_LINKS,SZ_HORRESIZE(1) );

	CRect rc;
	GetClientRect(rc);
	int w = rc.Width()/2+20;

	m_links.SetExtendedStyle( LVS_EX_FLATSB|LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES );
	m_links.InsertColumn(0,"Link Name",LVCFMT_LEFT,w,0);
	m_links.InsertColumn(1,"Entity",LVCFMT_LEFT,w,1);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


/////////////////////////////////////////////////////////////////////////////
// CEntityLinksPanel message handlers
void CEntityLinksPanel::SetEntity( CEntity *entity )
{
	assert( entity );

	m_entity = entity;
	ReloadLinks();
}

void CEntityLinksPanel::ReloadLinks()
{
	//m_removeButton.EnableWindow(FALSE);
	if (!m_entity)
		return;

	m_currentLinkIndex = -1;

	m_links.DeleteAllItems();
	LVITEM lvi;
	ZeroStruct(lvi);
	for (int i = 0; i < m_entity->GetEntityLinkCount(); i++)
	{
		CEntityLink &link = m_entity->GetEntityLink(i);

		lvi.mask = LVIF_PARAM|LVIF_TEXT;
		lvi.iItem = i;
		lvi.iSubItem = 0;
		lvi.pszText = const_cast<char*>((const char*)link.name);
		lvi.lParam = (LPARAM)i;
		int idItem = m_links.InsertItem( &lvi );

		if (link.target)
		{
			m_links.SetItem( idItem,1,LVIF_TEXT,link.target->GetName(),0,0,0,0 );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityLinksPanel::OnBnClickedRemove() 
{
	if (m_links.GetSelectedCount() > 0)
	{
		int nSelItem = m_links.GetSelectionMark();
		if (nSelItem < 0)
			return;
		DWORD_PTR nCurrentIndex = m_links.GetItemData(nSelItem);
		{
			CUndo undo("Remove Entity Link");
			m_entity->RemoveEntityLink( nCurrentIndex );
		}
		ReloadLinks();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityLinksPanel::OnPick( CBaseObject *picked )
{
	CEntity *pickedEntity = (CEntity*)picked;
	if (!pickedEntity)
		return;

	CEntityLink *pLink = 0;

	CString linkName = "NewLink";
	// Replace previous link.
	if (m_currentLinkIndex >= 0 && m_currentLinkIndex < m_entity->GetEntityLinkCount())
	{
		linkName = m_entity->GetEntityLink(m_currentLinkIndex).name;
		m_entity->RemoveEntityLink(m_currentLinkIndex);
	}

	m_entity->AddEntityLink( linkName,pickedEntity->GetId() );
	ReloadLinks();
}

void CEntityLinksPanel::OnCancelPick()
{
	m_currentLinkIndex = -1;
}

#define CMD_PICK      1
#define CMD_PICK_NEW  2
#define CMD_DELETE    3
#define CMD_RENAME    4

void CEntityLinksPanel::OnRclickLinks(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NMITEMACTIVATE *pNMItem = (NMITEMACTIVATE*)pNMHDR;
	DWORD_PTR nIndex = -1;
	if (pNMItem->iItem >= 0)
	{
		nIndex = m_links.GetItemData(pNMItem->iItem);
	}

	CEntityLink *pLink = 0;

	if (nIndex >= 0 && nIndex < m_entity->GetEntityLinkCount())
		pLink = &m_entity->GetEntityLink(nIndex);
	
	CMenu menu;
	menu.CreatePopupMenu();

	m_currentLinkIndex = -1;
	
	if (pLink)
	{
		menu.AppendMenu( MF_STRING,CMD_PICK,"Change Target Entity" );
		menu.AppendMenu( MF_STRING,CMD_RENAME,"Rename Link" );
		menu.AppendMenu( MF_STRING,CMD_DELETE,"Delete Link" );

		menu.AppendMenu( MF_SEPARATOR,0,"" );
		menu.AppendMenu( MF_STRING,CMD_PICK_NEW,"Pick New Target" );
	}
	else
	{
		menu.AppendMenu( MF_STRING,CMD_PICK,"Pick New Target" );
	}

	CPoint p;
	::GetCursorPos( &p );
	int res = ::TrackPopupMenuEx( menu.GetSafeHmenu(),TPM_LEFTBUTTON|TPM_RETURNCMD,p.x,p.y,GetSafeHwnd(),NULL );
	switch (res)
	{
	case CMD_PICK:
		m_currentLinkIndex = nIndex;
		m_pickButton.OnClicked();
		break;
	case CMD_PICK_NEW:
		m_currentLinkIndex = -1;
		m_pickButton.OnClicked();
		break;
	case CMD_DELETE:
		OnBnClickedRemove();
		break;
	case CMD_RENAME:
		m_links.EditLabel(pNMItem->iItem);
		break;
	}

	*pResult = TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CEntityLinksPanel::OnDblClickLinks(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NMITEMACTIVATE *pNMItem = (NMITEMACTIVATE*)pNMHDR;
	if (pNMItem->iItem < 0)
		return;
	DWORD_PTR nIndex = m_links.GetItemData(pNMItem->iItem);
	if (m_entity)
	{
		if (nIndex >= 0 && nIndex < m_entity->GetEntityLinkCount())
		{
			CEntityLink &link = m_entity->GetEntityLink(nIndex);
			if (link.target)
			{
				// Select entity.
				CUndo undo( "Select Object(s)" );
				GetIEditor()->ClearSelection();
				GetIEditor()->SelectObject(link.target);
			}
		}
	}

	
	*pResult = TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CEntityLinksPanel::OnBeginLabelEdit(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NMLVDISPINFO *pNMItem = (NMLVDISPINFO*)pNMHDR;
	if (pNMItem->item.iSubItem == 0)
		*pResult = FALSE;
	else
		*pResult = TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CEntityLinksPanel::OnEndLabelEdit(NMHDR* pNMHDR, LRESULT* pResult) 
{
	*pResult = TRUE;

	NMLVDISPINFO *pNMItem = (NMLVDISPINFO*)pNMHDR;
	if (pNMItem->item.iSubItem != 0)
		return;
	
	if (!pNMItem->item.pszText)
		return;

	DWORD_PTR nIndex = m_links.GetItemData(pNMItem->item.iItem);
	if (m_entity)
	{
		if (nIndex >= 0 && nIndex < m_entity->GetEntityLinkCount())
		{
			m_entity->RenameEntityLink(nIndex,pNMItem->item.pszText);
		}
	}
}
