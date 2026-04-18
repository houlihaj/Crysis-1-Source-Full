// TrackViewNodes.cpp : implementation file
//

#include "stdafx.h"
#include "TrackViewNodes.h"
#include "TrackViewKeyList.h"
#include "TrackViewUndo.h"
#include "StringDlg.h"

#include "Clipboard.h"

#include "IMovieSystem.h"

#include "Objects\ObjectManager.h"

// CTrackViewNodes

IMPLEMENT_DYNAMIC(CTrackViewNodes, CMultiTree)
CTrackViewNodes::CTrackViewNodes()
{
	m_keysCtrl = 0;
	m_sequence = 0;
}

CTrackViewNodes::~CTrackViewNodes()
{
}


BEGIN_MESSAGE_MAP(CTrackViewNodes, CMultiTree)
	ON_NOTIFY_REFLECT(TVN_ITEMEXPANDED, OnTvnItemexpanded)
	ON_WM_CREATE()
	ON_NOTIFY_REFLECT(NM_RCLICK, OnNMRclick)
	ON_WM_VSCROLL()
	ON_NOTIFY_REFLECT(TVN_SELCHANGED, OnTvnSelchanged)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnNMDblclk)
	ON_WM_KEYDOWN()
END_MESSAGE_MAP()

void CTrackViewNodes::RefreshNodes()
{
	SetSequence(m_sequence);
}

CTrackViewNodes::SItemInfo* CTrackViewNodes::GetSelectedNode()
{
	HTREEITEM hItem=GetSelectedItem();
	if (!hItem)
		return NULL;
	return (SItemInfo*)GetItemData(hItem);
}

void CTrackViewNodes::AddNode( IAnimNode *node )
{
	AddNodeItem( node,TVI_ROOT );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewNodes::AddNodeItem( IAnimNode *node,HTREEITEM root )
{
	bool bNeedExpand = false;
	HTREEITEM MyRoot=root;
	CBaseObject *pBaseObject=GetIEditor()->GetObjectManager()->FindAnimNodeOwner(node);
	if (pBaseObject)
	{
		// connect to parent if it is in treeview already
		CBaseObject *pParent=pBaseObject->GetParent();
		if (pParent)
		{
			HTREEITEM hParent=GetChildNode(root, pParent->GetAnimNode());
			if (hParent!=NULL)
				MyRoot=hParent;
		}
	}
	int nNodeImage = 1;
	EAnimNodeType nodeType = node->GetType();
	switch (nodeType)
	{
	case ANODE_ENTITY:	   nNodeImage = 1; break;
	case ANODE_CAMERA:	   nNodeImage = 8; break;
	case ANODE_SCRIPTVAR:	 nNodeImage = 14; break;
	case ANODE_CVAR:       nNodeImage = 15; break;
	case ANODE_MATERIAL:   nNodeImage = 16; break;
	}
	HTREEITEM hItem = InsertItem( node->GetName(),nNodeImage,nNodeImage,MyRoot);
	//SetItemState(MyRoot, TVIS_BOLD, TVIS_BOLD);
	SetItemState(hItem, TVIS_BOLD, TVIS_BOLD);


	SItemInfo *pItemInfo = &(*m_itemInfos.insert(m_itemInfos.begin(),SItemInfo()));
	pItemInfo->node = node;
	pItemInfo->baseObj = pBaseObject;
	pItemInfo->track = 0;
	pItemInfo->paramId = 0;
	SetItemData( hItem,(DWORD_PTR)pItemInfo );

	if (MyRoot != root)
	{
		// Expand root object.
		Expand( MyRoot,TVE_EXPAND );
	}
	if (node->GetFlags()&ANODE_FLAG_EXPANDED)
	{
		bNeedExpand = true;
	}

	IAnimBlock *anim = node->GetAnimBlock();

	if (anim)
	{
		int type;
		IAnimTrack *track;
		IAnimNode::SParamInfo paramInfo;
		for (int i = 0; i < anim->GetTrackCount(); i++)
		{
			if (!anim->GetTrackInfo( i,type,&track ))
				continue;
			if (!track)
				continue;
			if (track->GetFlags() & ATRACK_HIDDEN) 
				continue;

			if (!node->GetParamInfoFromId(type,paramInfo))
				continue;

			int nImage = 13; // Default
			if (type == APARAM_FOV)
				nImage = 2;
			if (type == APARAM_POS)
				nImage = 3;
			if (type == APARAM_ROT)
				nImage = 4;
			if (type == APARAM_SCL)
				nImage = 5;
			if (type == APARAM_EVENT)
				nImage = 6;
			if (type == APARAM_VISIBLE)
				nImage = 7;
			if (type == APARAM_CAMERA)
				nImage = 8;
			if ((type >= APARAM_SOUND1) && (type <= APARAM_SOUND3))
				nImage = 9;
			if ((type >= APARAM_CHARACTER1) && (type <= APARAM_CHARACTER3))
				nImage = 10;
			if ((type >= APARAM_CHARACTER4) && (type <= APARAM_CHARACTER10))
				nImage = 10;
			if (type == APARAM_SEQUENCE)
				nImage = 11;
			if ((type >= APARAM_EXPRESSION1) && (type <= APARAM_EXPRESSION3))
				nImage = 12;
			if ((type >= APARAM_EXPRESSION4) && (type <= APARAM_EXPRESSION10))
				nImage = 12;
			if (type == APARAM_FLOAT_1)
				nImage = 13;

			HTREEITEM hTrackItem = InsertItem( paramInfo.name,nImage,nImage,hItem );

			SItemInfo *pItemInfo = &(*m_itemInfos.insert(m_itemInfos.begin(),SItemInfo()));
			pItemInfo->node = node;
			pItemInfo->paramId = type;
			pItemInfo->track = track;
			SetItemData( hTrackItem,(DWORD_PTR)pItemInfo );
		}
	}
	if (pBaseObject)
	{
		// connect children if they are in treeview already
		for (int i=0;i<pBaseObject->GetChildCount();i++)
		{
			CBaseObject *pChild=pBaseObject->GetChild(i);
			HTREEITEM hChild=GetChildNode(root, pChild->GetAnimNode());
			if (hChild!=NULL)
			{
				HTREEITEM hNewItem=CopyBranch(hChild, hItem);
				//SetItemState(hItem, TVIS_BOLD, TVIS_BOLD);
				SetItemState(hNewItem, TVIS_BOLD, TVIS_BOLD);
				DeleteItem(hChild);
				// If node have child nodes. expand it.
				bNeedExpand = true;
			}
		}
	}

	if (bNeedExpand)
	{
		// If node have child nodes. expand it.
		Expand( hItem,TVE_EXPAND );
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewNodes::SetKeyListCtrl( CTrackViewKeys *keysCtrl )
{
	m_keysCtrl = keysCtrl;
	//SyncKeyCtrl();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewNodes::SyncKeyCtrl()
{
	if (!m_keysCtrl)
		return;

	m_keysCtrl->ResetContent();

	if (!m_sequence)
		return;

	HTREEITEM hItem = GetFirstVisibleItem();
	while (hItem)
	{
		SItemInfo *pItemInfo = (SItemInfo*)GetItemData(hItem);
		if (pItemInfo != NULL && pItemInfo->track != NULL)
		{
			m_keysCtrl->AddItem( CTrackViewKeyList::Item(pItemInfo->node,pItemInfo->paramId,pItemInfo->track) );
		}
		else
		{
			m_keysCtrl->AddItem( CTrackViewKeyList::Item() );
		}

		hItem = GetNextVisibleItem(hItem);
	}
}
// CTrackViewNodes message handlers

void CTrackViewNodes::OnTvnItemexpanded(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	SItemInfo *pItemInfo = (SItemInfo*)GetItemData(pNMTreeView->itemNew.hItem);
	if (pItemInfo != NULL && pItemInfo->node != NULL)
	{
		IAnimNode *node = pItemInfo->node;
		if ((pNMTreeView->itemNew.state & TVIS_EXPANDED) || (pNMTreeView->itemNew.state & TVIS_EXPANDPARTIAL))
		{
			// Mark node expanded
			node->SetFlags( node->GetFlags()|ANODE_FLAG_EXPANDED );
		}
		else
		{
			// Mark node collapsed.
			node->SetFlags( node->GetFlags()&(~ANODE_FLAG_EXPANDED) );
		}
	}

	SyncKeyCtrl();

	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
int CTrackViewNodes::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMultiTree::OnCreate(lpCreateStruct) == -1)
		return -1;

	// Create the list
	CMFCUtils::LoadTrueColorImageList( m_cImageList,IDB_TRACKVIEW_NODES,16,RGB(255,0,255) );
	SetImageList( &m_cImageList,TVSIL_NORMAL );

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewNodes::CreateAnimNode( int type,const char *sName )
{
	GUID guid;
	CoCreateGuid( &guid );
	int nodeId = guid.Data1;
	IAnimNode *pAnimNode = GetIEditor()->GetSystem()->GetIMovieSystem()->CreateNode( type,nodeId );
	if (pAnimNode && m_sequence->AddNode(pAnimNode))
	{
		pAnimNode->SetName( sName );
		pAnimNode->CreateDefaultTracks();
	}
	RefreshNodes();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewNodes::SetSequence( IAnimSequence *seq )
{
	DeleteAllItems();
	m_sequence = seq;

	if (!m_sequence)
		return;

	// Create root item.
	HTREEITEM hRootItem = InsertItem( seq->GetName(),0,0,TVI_ROOT );
	SetItemData(hRootItem,(DWORD_PTR)0);
	SetItemState(hRootItem, TVIS_BOLD, TVIS_BOLD);

	for (int i = 0; i < seq->GetNodeCount(); i++)
	{
		IAnimNode *node = seq->GetNode(i);
		AddNodeItem( node,hRootItem );
	}
	Expand( hRootItem,TVE_EXPAND );
	SyncKeyCtrl();
}

void CTrackViewNodes::OnNMRclick(NMHDR *pNMHDR, LRESULT *pResult)
{
	CPoint point;

	SItemInfo *pItemInfo = 0;

	if (!m_sequence)
		return;

	// Find node under mouse.
	GetCursorPos( &point );
	ScreenToClient( &point );
	// Select the item that is at the point myPoint.
	UINT uFlags;
	HTREEITEM hItem = HitTest(point,&uFlags);
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		pItemInfo = (SItemInfo*)GetItemData(hItem);
	}

	// Create pop up menu.
	CMenu menu;
	CMenu menuAddTrack;
	
	menu.CreatePopupMenu();

	if (GetSelectedCount() > 1)
	{
		HTREEITEM hSelected = GetFirstSelectedItem();
		bool nodeSelected = false;

		for (int currentNode = 0; currentNode < GetSelectedCount(); currentNode++)
		{
			SItemInfo *pItemInfo = (SItemInfo*)GetItemData(hSelected);
			if (pItemInfo != NULL && pItemInfo->node != NULL && pItemInfo->track == NULL)
			{
				nodeSelected = true;
				break;
			}
			hSelected = GetNextSelectedItem(hSelected);
		}

		if (nodeSelected)
		{
			menu.AppendMenu( MF_STRING,602,"Copy Selected Nodes" );
			menu.AppendMenu( MF_STRING,603,"Copy Selected Nodes with Children" );
		}

		menu.AppendMenu( MF_STRING,10,"Remove Selected Nodes/Tracks" );
	}
	else
	{
	if (!pItemInfo || !pItemInfo->node)
	{
		menu.AppendMenu( MF_STRING,650,_T("Expand all") );
		menu.AppendMenu( MF_STRING,651,_T("Expand Entities") );
		menu.AppendMenu( MF_STRING,652,_T("Collapse Entities") );
		menu.AppendMenu( MF_STRING,653,_T("Expand Cameras") );
		menu.AppendMenu( MF_STRING,654,_T("Collapse Cameras") );
		menu.AppendMenu( MF_STRING,655,_T("Expand Materials") );
		menu.AppendMenu( MF_STRING,656,_T("Collapse Materials") );

		menu.AppendMenu( MF_STRING,500,_T("Add Selected Entity Node") );
		menu.AppendMenu( MF_STRING,501,_T("Add Scene Node") );
		menu.AppendMenu( MF_STRING,502,_T("Add Console Variable Node") );
		menu.AppendMenu( MF_STRING,503,_T("Add Script Variable Node") );
		menu.AppendMenu( MF_STRING,504,_T("Add Material Node") );

			menu.AppendMenu( MF_STRING,604,_T("Paste Node(s)") );
	}
	else
	{
		if (pItemInfo != NULL && pItemInfo->node != NULL && pItemInfo->track == 0)
		{
			menu.AppendMenu( MF_STRING,602,"Copy Node" );
			menu.AppendMenu( MF_STRING,603,"Copy Node with Children" );
			menu.AppendMenu( MF_STRING,10,"Remove Node" );
			menu.AppendMenu( MF_STRING,600,"Copy Selected Keys");
		}
		else
		{
			menu.AppendMenu( MF_STRING,600,"Copy Keys");
		}
		if (pItemInfo && pItemInfo->node != 0)
		{
			if (pItemInfo->node->GetFlags() & ANODE_FLAG_CAN_CHANGE_NAME)
			{
				menu.AppendMenu( MF_STRING,11,"Rename Node" );
			}
		}

		menu.AppendMenu( MF_STRING,601,"Paste Keys");
		menu.AppendMenu( MF_SEPARATOR,0,"" );
	}

	// add tracks menu
	menuAddTrack.CreatePopupMenu();
	bool bTracksToAdd=false;
	if (pItemInfo && pItemInfo->node != 0)
	{
		IAnimNode::SParamInfo paramInfo;
		// List`s which tracks can be added to animation node.
		for (int i = 0; i < pItemInfo->node->GetParamCount(); i++)
		{
			if (!pItemInfo->node->GetParamInfo( i,paramInfo ))
				continue;
				
			int flags = 0;
			IAnimTrack *track = pItemInfo->node->GetTrack( paramInfo.paramId );
			if (track)
			{
				continue;
				//flags |= MF_CHECKED;
			}

			menuAddTrack.AppendMenu( MF_STRING|flags,1000+paramInfo.paramId,paramInfo.name );
			bTracksToAdd=true;
		}
	}
	if (bTracksToAdd)
		menu.AppendMenu(MF_POPUP,(UINT_PTR)menuAddTrack.m_hMenu,"Add Track");

	// delete track menu
	if (pItemInfo != NULL && pItemInfo->node  != NULL && pItemInfo->track != NULL)
	{
		menu.AppendMenu(MF_STRING, 299, "Remove Track");
	}

	if (bTracksToAdd || (pItemInfo != NULL && pItemInfo->track != NULL))
		menu.AppendMenu( MF_SEPARATOR,0,"" );

	if (pItemInfo && pItemInfo->node != 0)
	{
		CString str;
		str.Format( "%s Tracks",pItemInfo->node->GetName() );
		menu.AppendMenu( MF_STRING|MF_DISABLED,0,str );

		// Show tracks in anim node.
		IAnimBlock *anim = pItemInfo->node->GetAnimBlock();
		if (anim)
		{
			IAnimNode::SParamInfo paramInfo;
			int type;
			IAnimTrack *track;
			for (int i = 0; i < anim->GetTrackCount(); i++)
			{
				if (!anim->GetTrackInfo( i,type,&track ))
					continue;
				if (!pItemInfo->node->GetParamInfoFromId( type,paramInfo ))
					continue;
			
				// change hidden flag for this track.
				int checked = MF_CHECKED;
				if (track->GetFlags() & ATRACK_HIDDEN)
				{
					checked = MF_UNCHECKED;
				}
				menu.AppendMenu( MF_STRING|checked,100+i,CString( "  " ) + paramInfo.name );
			}
		}
	}
	}

	GetCursorPos( &point );
	int cmd = menu.TrackPopupMenu( TPM_RETURNCMD|TPM_LEFTALIGN|TPM_LEFTBUTTON,point.x,point.y,this->GetParent()->GetParent() );
	
	//////////////////////////////////////////////////////////////////////////
	// Check Remove Node.
	if (cmd == 10)
	{
		RemoveNodes();
	}

	//////////////////////////////////////////////////////////////////////////
	// Add scene node.
	if (cmd == 500)
	{
		AddSelectedNodes();
	}
	//////////////////////////////////////////////////////////////////////////
	// Add scene node.
	if (cmd == 501)
	{
		m_sequence->AddSceneNode();
		RefreshNodes();
	}
	//////////////////////////////////////////////////////////////////////////
	// Add cvar node.
	if (cmd == 502)
	{
		CStringDlg dlg( _T("Console Variable Name") );
		if (dlg.DoModal() == IDOK && !dlg.GetString().IsEmpty())
		{
			CreateAnimNode( ANODE_CVAR,dlg.GetString() );
		}
	}
	//////////////////////////////////////////////////////////////////////////
	// Add script var node.
	if (cmd == 503)
	{
		CStringDlg dlg( _T("Script Variable Name") );
		if (dlg.DoModal() == IDOK && !dlg.GetString().IsEmpty())
		{
			CreateAnimNode( ANODE_SCRIPTVAR,dlg.GetString() );
		}
	}
	//////////////////////////////////////////////////////////////////////////
	// Add Material node.
	if (cmd == 504)
	{
		CStringDlg dlg( _T("Material Name") );
		if (dlg.DoModal() == IDOK && !dlg.GetString().IsEmpty())
		{
			CreateAnimNode( ANODE_MATERIAL,dlg.GetString() );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Check Rename Node.
	if (cmd == 11 && pItemInfo != NULL && pItemInfo->node != NULL)
	{
		// Rename node under cursor.
		CStringDlg dlg( _T("Rename Node") );
		dlg.SetString(pItemInfo->node->GetName());
		if (dlg.DoModal() == IDOK)
		{
			pItemInfo->node->SetName( dlg.GetString() );
			RefreshNodes();
		}
	}

	if (cmd >= 1000 && cmd < 2000)
	{
		if (pItemInfo)
		{
			IAnimNode *node = pItemInfo->node;
			IAnimBlock *anim = node->GetAnimBlock();
			if (anim)
			{
				CUndo undo("Create Anim Track");
				CUndo::Record( new CUndoAnimSequenceObject(m_sequence) );

				int paramId = cmd-1000;
				node->CreateTrack( paramId );
				RefreshNodes();
				ExpandNode(node);
			}
		}
	}

	if (cmd == 299)
	{
		if (pItemInfo)
		{
			IAnimNode *node = pItemInfo->node;
			IAnimBlock *anim = node->GetAnimBlock();
			if (anim)
			{
				if (AfxMessageBox("Are you sure you want to delete this track ? Undo will not be available !", MB_ICONQUESTION | MB_YESNO)==IDYES)
				{
					CUndo undo("Remove Anim Track");
					CUndo::Record( new CUndoAnimSequenceObject(m_sequence) );

					node->RemoveTrack(pItemInfo->track);
					RefreshNodes();
					ExpandNode(node);
				}
			}
		}
	}

	if (cmd >= 100 && cmd < 200)
	{
		if (pItemInfo)
		{
			IAnimNode *node = pItemInfo->node;
			IAnimBlock *anim = node->GetAnimBlock();
			if (anim)
			{
				CUndo undo("Modify Sequence");
				CUndo::Record( new CUndoAnimSequenceObject(m_sequence) );

				int type;
				IAnimTrack *track;
				for (int i = 0; i < anim->GetTrackCount(); i++)
				{
					if (!anim->GetTrackInfo( i,type,&track ))
						continue;

					if (cmd-100 == i)
					{
						// change hidden flag for this track.
						if (track->GetFlags() & ATRACK_HIDDEN)
							track->SetFlags( track->GetFlags() & ~ATRACK_HIDDEN );
						else
							track->SetFlags( track->GetFlags() | ATRACK_HIDDEN );
					}
					RefreshNodes();
					ExpandNode(  node );
					break;
				}
			}
		}
	}

	if(cmd==600)
	{
		m_keysCtrl->CopyKeys();
	}

	if(cmd==601)
	{
		SItemInfo * pSii = GetSelectedNode();
		m_keysCtrl->PasteKeys(pSii?pSii->node:0,0);
		return;
	}

	if(cmd==602)
	{
		CopyNodes(false);
	}
	if(cmd==603)
	{
		CopyNodes(true);
	}
	if(cmd==604)
	{
		PasteNodes();
	}

	if(cmd==650)
	{
		typedef std::list<IAnimNode*> t_Nodes;

		t_Nodes Nodes;

		for (ItemInfos::iterator it=m_itemInfos.begin(); it!=m_itemInfos.end(); ++it)
		{
			SItemInfo &ii = *it;
			if(ii.node)
			{
				Nodes.push_back(ii.node);
			}
		}
		for (t_Nodes::iterator itn = Nodes.begin(); itn!=Nodes.end(); ++ itn)
		{
			ExpandNode(*itn);
		}
		SyncKeyCtrl();
	}


	if(651<=cmd && cmd<=656)
	{
		typedef std::list<IAnimNode*> t_Nodes;

		t_Nodes Nodes;

		for (ItemInfos::iterator it=m_itemInfos.begin(); it!=m_itemInfos.end(); ++it)
		{
			SItemInfo &ii = *it;
			if(ii.node != NULL && (
				((cmd==651 || cmd==652) && ii.node->GetType()==ANODE_ENTITY) ||
				((cmd==653 || cmd==654) && ii.node->GetType()==ANODE_CAMERA) ||
				((cmd==655 || cmd==656) && ii.node->GetType()==ANODE_MATERIAL)
				))
			{
				Nodes.push_back(ii.node);
			}
		}

		HTREEITEM hCurrent = GetRootItem();
		for (t_Nodes::iterator itn = Nodes.begin(); itn!=Nodes.end(); ++ itn)
		{
			ExpandChildNode( GetNextItem(GetRootItem(),TVGN_CHILD),*itn, (cmd%2) ? true: false);
		}
		SyncKeyCtrl();
	}

	// processed
	*pResult = 1;
}

void CTrackViewNodes::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CMultiTree::OnVScroll(nSBCode, nPos, pScrollBar);

	SyncKeyCtrl();
}

void CTrackViewNodes::OnTvnSelchanged(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);

	if (m_keysCtrl && pNMTreeView->itemNew.hItem != NULL)
	{
		SItemInfo *pItemInfo = (SItemInfo*)GetItemData(pNMTreeView->itemNew.hItem);
		if (pItemInfo && pItemInfo->track != NULL && GetSelectedCount() <= 1)
		{
			for (int i = 0; i < m_keysCtrl->GetCount(); i++)
			{
				if (m_keysCtrl->GetTrack(i) == pItemInfo->track)
				{
					m_keysCtrl->SetCurSel(i);
					break;
				}
			}
		}
		else
		{
			m_keysCtrl->SetCurSel(-1);
		}
	}
	
	*pResult = 0;
}

//////////////////////////////////////////////////////////////////////////
HTREEITEM CTrackViewNodes::GetChildNode( HTREEITEM hItem,IAnimNode *node )
{
	// Look at all of the root-level items
	while (hItem != NULL)
	{
		SItemInfo *pItemInfo = (SItemInfo*)GetItemData(hItem);
		if (pItemInfo)
		{
			if (pItemInfo->node == node && pItemInfo->track == 0)
			{
				return hItem;
			}
		}
		HTREEITEM hChild = GetNextItem( hItem, TVGN_CHILD);
		HTREEITEM hRes=GetChildNode(hChild,node);
		if (hRes!=NULL)
			return hRes;
		hItem = GetNextItem( hItem, TVGN_NEXT);
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewNodes::ExpandChildNode( HTREEITEM hItem,IAnimNode *node, bool bExpand)
{
	// Look at all of the root-level items
	while (hItem != NULL)
	{
		SItemInfo *pItemInfo = (SItemInfo*)GetItemData(hItem);
		if (pItemInfo)
		{
			if (pItemInfo->node == node && pItemInfo->track == 0)
			{
				HTREEITEM hParent = GetParentItem(hItem);
				if (hParent != NULL)
					Expand(hParent, bExpand? TVE_EXPAND : TVE_COLLAPSE);

				Expand( hItem,bExpand? TVE_EXPAND : TVE_COLLAPSE );
				EnsureVisible(hItem);
				return true;
			}
		}
		HTREEITEM hChild = GetNextItem( hItem, TVGN_CHILD);
		if (ExpandChildNode(hChild,node, bExpand))
			return true;
		hItem = GetNextItem( hItem, TVGN_NEXT);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewNodes::ExpandNode( IAnimNode *node )
{
	// Look at all of the root-level items
	HTREEITEM hCurrent = GetRootItem();
	ExpandChildNode( GetNextItem(GetRootItem(),TVGN_CHILD),node );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewNodes::AddSelectedNodes()
{
	CUndo undo("Add Anim Node(s)");
	CUndo::Record( new CUndoAnimSequenceObject(m_sequence) );

	// Add selected nodes.
	CSelectionGroup *sel = GetIEditor()->GetSelection();
	for (int i = 0; i < sel->GetCount(); i++)
	{
		CBaseObject *obj = sel->GetObject(i);
		if (obj)
		{
			if (!obj->GetAnimNode())
				obj->EnableAnimNode(true);
			IAnimNode *pAnimNode = obj->GetAnimNode();
			
			if (pAnimNode)
			{
				if (!m_sequence->AddNode(pAnimNode))
					continue;

				IAnimBlock *pAnimBlock=pAnimNode->GetAnimBlock();
				if (pAnimBlock)		// lets add basic tracks
				{
					pAnimNode->CreateDefaultTracks();
				}
				// Force default parameters.
				pAnimNode->SetPos(0,obj->GetPos());
				pAnimNode->SetRotate(0,obj->GetRotation());
				pAnimNode->SetScale(0,obj->GetScale());
				pAnimNode->SetFlags( pAnimNode->GetFlags()|ANODE_FLAG_SELECTED );
			}
		}
	}
	RefreshNodes();
	for (int i = 0; i < sel->GetCount(); i++)
	{
		CBaseObject *obj = sel->GetObject(i);
		if (obj && obj->GetAnimNode())
		{
			ExpandNode(  obj->GetAnimNode() );
			obj->OnEvent( EVENT_UPDATE_TRACKGIZMO );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewNodes::AddSceneNodes()
{
	CUndo undo("Add Scene Node");
	CUndo::Record( new CUndoAnimSequenceObject(m_sequence) );

	IAnimNode *pSceneNode=m_sequence->AddSceneNode();
	if (!pSceneNode)
		AfxMessageBox("Scene-track already exists.", MB_ICONEXCLAMATION | MB_OK);
	else
	{
		pSceneNode->CreateDefaultTracks();
		RefreshNodes();
		ExpandNode(pSceneNode);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewNodes::RemoveNodes()
{
	char *description = NULL;

	if (GetSelectedCount() > 1)
	{
		description = "Remove Anim Nodes/Tracks";
	}
	else
	{
		description = "Remove Anim Node";
	}

	CUndo undo(description);
	CUndo::Record( new CUndoAnimSequenceObject(m_sequence) );

	bool nodeDeleted = false;
	bool trackDeleted = false;

	HTREEITEM hSelected = GetFirstSelectedItem();

	for (int currentNode = 0; currentNode < GetSelectedCount(); currentNode++)
	{
		SItemInfo *pItemInfo = (SItemInfo*)GetItemData(hSelected);
		if (pItemInfo != NULL && pItemInfo->node != NULL && pItemInfo->track != NULL)
		{
			pItemInfo->node->RemoveTrack(pItemInfo->track);
			trackDeleted = true;
		}
		hSelected = GetNextSelectedItem(hSelected);
	}

	hSelected = GetFirstSelectedItem();

	for (int currentNode = 0; currentNode < GetSelectedCount(); currentNode++)
	{
		SItemInfo *pItemInfo = (SItemInfo*)GetItemData(hSelected);
		if (pItemInfo != NULL && pItemInfo->node != NULL && pItemInfo->track == NULL)
		{
			m_sequence->RemoveNode(pItemInfo->node);
			nodeDeleted = true;
		}
		hSelected = GetNextSelectedItem(hSelected);
	}

	if (nodeDeleted == true || trackDeleted == true)
	{
		RefreshNodes();
	}
}

//////////////////////////////////////////////////////////////////////////
// Select object in Editor with Double click.
//////////////////////////////////////////////////////////////////////////
void CTrackViewNodes::OnNMDblclk(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;

	CPoint point;
	SItemInfo *pItemInfo = 0;

	if (!m_sequence)
		return;

	if (GetSelectedCount() > 1)
	{
		// Disable expanding on dblclick.
		*pResult = TRUE;

		return;
	}

	// Find node under mouse.
	GetCursorPos( &point );
	ScreenToClient( &point );
	// Select the item that is at the point myPoint.
	UINT uFlags;
	HTREEITEM hItem = HitTest(point,&uFlags);
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		pItemInfo = (SItemInfo*)GetItemData(hItem);
	}

	if (pItemInfo && pItemInfo->node != NULL)
	{
		// Double Clicked on node or track.
		// Make this object selected in Editor.
		CBaseObject *obj = GetIEditor()->GetObjectManager()->FindAnimNodeOwner(pItemInfo->node);
		if (obj)
		{
			GetIEditor()->ClearSelection();
			GetIEditor()->SelectObject(obj);

			// Disable expanding on dblclick.
			*pResult = TRUE;
		}
	}
}

void CTrackViewNodes::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_DELETE)
	{
		if (!m_sequence)
			return;

		if (GetSelectedCount() > 1)
		{
			if (AfxMessageBox("Delete Selected Nodes/Tracks?", MB_ICONQUESTION|MB_YESNO)==IDYES)
			{
				RemoveNodes();
			}
		}
		else
		{
		SItemInfo *pItemInfo = GetSelectedNode();
		if (pItemInfo)
		{
			if (pItemInfo->node  != NULL && pItemInfo->track != NULL)
			{
				// Delete track.
				if (AfxMessageBox("Delete Selected Track?", MB_ICONQUESTION|MB_YESNO)==IDYES)
				{
					IAnimNode *node = pItemInfo->node;

					CUndo undo("Remove Anim Track");
					CUndo::Record( new CUndoAnimSequenceObject(m_sequence) );

					node->RemoveTrack( pItemInfo->track );
					RefreshNodes();
					ExpandNode( node );
				}
			}
			else if (pItemInfo->node  != NULL && pItemInfo->track == 0)
			{
				// Delete node.
				if (AfxMessageBox("Delete Selected Animation Node?", MB_ICONQUESTION|MB_YESNO)==IDYES)
				{
					CUndo undo("Remove Anim Node");
					CUndo::Record( new CUndoAnimSequenceObject(m_sequence) );

					m_sequence->RemoveNode(pItemInfo->node);
					RefreshNodes();
				}
			}
		}
		}
		return;
	}

	if (nChar == 'C')
	{
		if(GetKeyState(VK_CONTROL))
			if(m_keysCtrl->CopyKeys())
				return;
	}
	
	if (nChar == 'V')
	{
		if(GetKeyState(VK_CONTROL))
		{
			SItemInfo * pSii = GetSelectedNode();
			if(m_keysCtrl->PasteKeys(pSii?pSii->node:0,0))
				return;
		}
	}


	CMultiTree::OnKeyDown(nChar, nRepCnt, nFlags);
}

bool CTrackViewNodes::CopyNodes(const bool copyChildren)
{
	XmlNodeRef animNodesRoot = CreateXmlNode("CopyAnimNodesRoot");
	HTREEITEM hSelected = GetFirstSelectedItem();

	for (int currentNode = 0; currentNode < GetSelectedCount(); currentNode++)
	{
		SItemInfo *pItemInfo = (SItemInfo*)GetItemData(hSelected);
		if (pItemInfo != NULL && pItemInfo->node != NULL && pItemInfo->track == NULL)
		{
			IAnimNode *pAnimNode = pItemInfo->node;
			IAnimBlock *pAnimBlock = pAnimNode->GetAnimBlock();

			if (pAnimBlock != NULL)
			{
	XmlNodeRef animNode = animNodesRoot->newChild("AnimNode");
	animNode->setAttr("Type",(int)pAnimNode->GetType());
	animNode->setAttr("Name",pAnimNode->GetName());
	animNode->setAttr("Id",pAnimNode->GetId());

	XmlNodeRef animBlockNode = animNode->newChild("AnimBlock");
	pAnimBlock->Serialize(pAnimNode,animBlockNode,false);
			}

	if (copyChildren == true)
	{
		HTREEITEM hAnimNode = GetNextItem(GetChildNode(GetRootItem(),pAnimNode),TVGN_CHILD);

		for (int i = 0; i < m_sequence->GetNodeCount(); i++)
		{
			IAnimNode *pNode = m_sequence->GetNode(i);
		
			if (GetChildNode(hAnimNode,pNode))
			{
				pAnimBlock = pNode->GetAnimBlock();

						if (pAnimBlock != NULL)
				{
				XmlNodeRef animNode = animNodesRoot->newChild("AnimNode");
				animNode->setAttr("Type",(int)pNode->GetType());
				animNode->setAttr("Name",pNode->GetName());
				animNode->setAttr("Id",pNode->GetId());

				XmlNodeRef animBlockNode = animNode->newChild("AnimBlock");
				pAnimBlock->Serialize(pNode,animBlockNode,false);
			}
		}
	}
			}
		}

		hSelected = GetNextSelectedItem(hSelected);
	}

	CClipboard clipboard;
	clipboard.Put(animNodesRoot,"Track view entity node");

	return true;
}

bool CTrackViewNodes::PasteNodes()
{
	CClipboard clipboard;
	if (clipboard.IsEmpty())
	{
		return false;
	}

	XmlNodeRef animNodesRoot = clipboard.Get();
	if (animNodesRoot == NULL || strcmp(animNodesRoot->getTag(),"CopyAnimNodesRoot"))
	{
		return false;
	}

	bool bSaveUndo = !CUndo::IsRecording();

	if (bSaveUndo)
	{
		GetIEditor()->BeginUndo();
	}

	CUndo::Record(new CUndoAnimSequenceObject(m_sequence));

	bool newNodeCreated = false;

	for (int child = 0; child < animNodesRoot->getChildCount(); child++)
	{
	int animNodeType;
	CString animNodeName;
	int animNodeId;

		XmlNodeRef animNode = animNodesRoot->getChild(child);
	animNode->getAttr("Type",animNodeType);
	animNode->getAttr("Name",animNodeName);
	animNode->getAttr("Id",animNodeId);

	IAnimNode *pAnimNode = NULL;

	if (animNodeType == ANODE_ENTITY || animNodeType == ANODE_CAMERA)
	{
		pAnimNode = GetIEditor()->GetSystem()->GetIMovieSystem()->GetNode(animNodeId);

		if (!pAnimNode || !m_sequence->AddNode(pAnimNode))
		{
				continue;
		}
	}
	else if (animNodeType == ANODE_SCENE)
	{
		pAnimNode = m_sequence->AddSceneNode();

		if (!pAnimNode)
		{
				continue;
		}
	}
	else
	{
		GUID guid;
		CoCreateGuid(&guid);
		int nodeId = guid.Data1;
		pAnimNode = GetIEditor()->GetSystem()->GetIMovieSystem()->CreateNode(animNodeType,nodeId);
		if (!pAnimNode || !m_sequence->AddNode(pAnimNode))
		{
				continue;
		}

		pAnimNode->SetName(animNodeName);
	}

	IAnimBlock *pAnimBlock = pAnimNode->GetAnimBlock();

	if (pAnimBlock != NULL)
	{
		pAnimBlock->Serialize(pAnimNode,animNode->getChild(0),true);
	}

		newNodeCreated = true;
	}

	if (newNodeCreated)
	{
	RefreshNodes();

	if (bSaveUndo)
	{
			GetIEditor()->AcceptUndo("Paste Anim Node(s)");
	}

	return true;
}

	return false;
}