////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   HyperGraphDialog.h
//  Version:     v1.00
//  Created:     21/3/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Resource.h"
#include <IAIAction.h>
#include <IAISystem.h>
#include "Ai/AIManager.h"
#include "HyperGraphDialog.h"
#include "IViewPane.h"
#include "MainFrm.h"
#include "HyperGraphManager.h"

#include "FlowGraphManager.h"
#include "FlowGraph.h"
#include "FlowGraphNode.h"
#include "FlowGraphVariables.h"
#include "Objects/Entity.h"
#include "Objects/PrefabObject.h"
#include "GameEngine.h"
#include "StringDlg.h"
#include "GenericSelectItemDialog.h"
#include "FlowGraphSearchCtrl.h"
#include "CommentNode.h"
#include "FlowGraphProperties.h"

namespace
{
	const int FlowGraphLayoutVersion = 0x0002; // bump this up on every substantial pane layout change
}

#define GRAPH_FILE_FILTER "Graph XML Files (*.xml)|*.xml"

#define IDW_HYPERGRAPH_RIGHT_PANE  AFX_IDW_CONTROLBAR_FIRST+10
#define IDW_HYPERGRAPH_TREE_PANE  AFX_IDW_CONTROLBAR_FIRST+11
#define IDW_HYPERGRAPH_COMPONENTS_PANE  AFX_IDW_CONTROLBAR_FIRST+12
#define IDW_HYPERGRAPH_SEARCH_PANE  AFX_IDW_CONTROLBAR_FIRST+13
#define IDW_HYPERGRAPH_SEARCHRESULTS_PANE  AFX_IDW_CONTROLBAR_FIRST+14

#define HYPERGRAPH_DIALOGFRAME_CLASSNAME "HyperGraphDialog"

#define IDC_HYPERGRAPH_COMPONENTS 1
#define IDC_HYPERGRAPH_GRAPHS 2

#define ID_PROPERTY_GROUP 1
#define ID_NODE_INFO_GROUP 2
#define ID_GRAPH_INFO_GROUP 3

#define ID_NODE_INFO 100
#define ID_GRAPH_INFO 110

namespace {
	const char* g_EntitiesFolderName = "Entities";
	const char* g_AIActionsFolderName = "AI Actions";

	void GetPrettyName(CHyperGraph* pGraph, CString& name, CString& groupName)
	{
		if (!pGraph->IsFlowGraph()) 
		{
			name = "";
			groupName = "";
			return;
		}
		CFlowGraph *pFlowGraph = static_cast<CFlowGraph*> (pGraph);
		CEntity *pEntity = pFlowGraph->GetEntity();
		IAIAction *pAIAction = pFlowGraph->GetAIAction();
		const CRuntimeClass* pPrefabClass = RUNTIME_CLASS(CPrefabObject);
		if (pEntity)
		{
			if (pEntity->IsInGroup())
			{
				CGroup* pObjGroup = pEntity->GetGroup();
				if (pObjGroup)
				{
					groupName = pObjGroup->GetName();
					name = pEntity->GetName();
				}
				else
				{
					name = "";
					groupName = "";
				}
			}
			else
			{
				name = pEntity->GetName();
				if (!pFlowGraph->GetGroupName().IsEmpty())
					groupName = pFlowGraph->GetGroupName();
				else
					groupName = g_EntitiesFolderName;
			}
		}
		else if (pAIAction)
		{
			name = pAIAction->GetName();
			if (!pFlowGraph->GetGroupName().IsEmpty())
				groupName = pFlowGraph->GetGroupName();
			else
				groupName = g_AIActionsFolderName;
		}
		else
		{
			name = pFlowGraph->GetName();
			groupName = "Files";
		}
	}

}

//////////////////////////////////////////////////////////////////////////
//
// CHyperGraphsTreeCtrl
//
//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP(CHyperGraphsTreeCtrl, CXTTreeCtrl)
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
CHyperGraphsTreeCtrl::CHyperGraphsTreeCtrl()
{
	m_bReloadGraphs = false;
	m_bIgnoreReloads = false;
	GetIEditor()->GetFlowGraphManager()->AddListener(this);
	int nSize = sizeof(&CHyperGraphsTreeCtrl::OnObjectEvent);
	GetIEditor()->GetObjectManager()->AddObjectEventListener( functor(*this,&CHyperGraphsTreeCtrl::OnObjectEvent) );
}

//////////////////////////////////////////////////////////////////////////
CHyperGraphsTreeCtrl::~CHyperGraphsTreeCtrl()
{
//	GetIEditor()->GetAI()->SaveActionGraphs();
	GetIEditor()->GetFlowGraphManager()->RemoveListener(this);
	GetIEditor()->GetObjectManager()->RemoveObjectEventListener( functor(*this,&CHyperGraphsTreeCtrl::OnObjectEvent) );
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphsTreeCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	BOOL ret = __super::Create( dwStyle|WS_BORDER|TVS_HASBUTTONS|TVS_LINESATROOT|TVS_HASLINES|TVS_SHOWSELALWAYS,rect,pParentWnd,nID );

	CMFCUtils::LoadTrueColorImageList( m_imageList,IDB_HYPERGRAPH_TREE,16,RGB(255,0,255) );
	//m_imageList.Create( MAKEINTRESOURCE(IDB_HYPERGRAPH_TREE),16,1,RGB(255,0,255) );
	SetImageList( &m_imageList,TVSIL_NORMAL );
	return ret;
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphsTreeCtrl::OnEraseBkgnd(CDC* pDC)
{
	//if (m_bReloadGraphs)
	//	Reload();
	return __super::OnEraseBkgnd(pDC);
}

HTREEITEM FindItemByData( CTreeCtrl* pTreeCtrl, HTREEITEM hItem, DWORD_PTR data )
{
	while ( hItem )
	{
		if ( pTreeCtrl->GetItemData( hItem ) == data )
			return hItem;
		else
		{
			HTREEITEM temp = FindItemByData( pTreeCtrl, pTreeCtrl->GetChildItem( hItem ), data );
			if ( temp )
				return temp;
		}
		hItem = pTreeCtrl->GetNextSiblingItem(hItem);
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphsTreeCtrl::SetCurrentGraph( CHyperGraph *pCurrentGraph )
{
	m_pCurrentGraph = pCurrentGraph;
	if (m_hWnd)
	{
		CTreeCtrl *pTreeCtrl = this;

		HTREEITEM hItem = FindItemByData( pTreeCtrl, pTreeCtrl->GetRootItem(), (DWORD_PTR) pCurrentGraph );
		if ( hItem )
		{
			// all parent nodes have to be expanded first
			HTREEITEM hParent = hItem;
			while ( hParent = pTreeCtrl->GetParentItem( hParent ) )
				pTreeCtrl->Expand( hParent, TVE_EXPAND );

			pTreeCtrl->Select( hItem, TVGN_CARET );
			pTreeCtrl->EnsureVisible( hItem );
		}
	}
}

HTREEITEM CHyperGraphsTreeCtrl::GetTreeItem(CHyperGraph* pGraph)
{
	if (m_hWnd)
	{
		CTreeCtrl *pTreeCtrl = this;
		HTREEITEM hItem = FindItemByData( pTreeCtrl, pTreeCtrl->GetRootItem(), (DWORD_PTR) pGraph );
		return hItem;
	}
	return 0;
}

struct SHGTCReloadItem
{
	SHGTCReloadItem( CString name,  CString groupName, HTREEITEM parent,int nImage, CFlowGraph * pFlowGraph, CEntity * pEntity = NULL )
	{
		// this->name.Format("0x%p %s", pFlowGraph, name.GetString());
		this->name = name;
		this->groupName = groupName;
		this->parent = parent;
		this->pFlowGraph = pFlowGraph;
		this->pEntity = pEntity;
		this->nImage = nImage;
	}
	CString name;
	CString groupName;
	HTREEITEM parent;
	CFlowGraph * pFlowGraph;
	CEntity * pEntity;
	int nImage;

	bool operator<( const SHGTCReloadItem& rhs ) const
	{
		int cmpResult = groupName.CompareNoCase(rhs.groupName);
		if (cmpResult!=0)
			return cmpResult < 0;
		return name.CompareNoCase(rhs.name) < 0;
	}

	HTREEITEM Add( CHyperGraphsTreeCtrl* pCtrl )
	{
		HTREEITEM hItem = pCtrl->InsertItem( name, nImage, nImage, parent);
		pCtrl->SetItemData( hItem, DWORD_PTR(pFlowGraph) );
		/*
		if (!pFlowGraph->IsEnabled())
		{
		LOGFONT logfont;
		pCtrl->GetItemFont( hItem,&logfont );
		logfont.lfStrikeOut = 1;
		pCtrl->SetItemFont( hItem,logfont );
		}
		*/
		return hItem;
	}

};


//////////////////////////////////////////////////////////////////////////
void CHyperGraphsTreeCtrl::Reload()
{
	std::vector<SHGTCReloadItem> items;

	m_bReloadGraphs = false;
	SetRedraw(FALSE);
	DeleteAllItems();
	CFlowGraphManager *pMgr = GetIEditor()->GetFlowGraphManager();
	CTreeCtrl *pTreeCtrl = this;

	m_mapEntityToItem.clear();
	m_mapPrefabToItem.clear();

	HTREEITEM hActionsGroup = NULL;
	if ( GetISystem()->GetIFlowSystem() )
	{
		hActionsGroup = pTreeCtrl->InsertItem( g_AIActionsFolderName,1,1 );
		int i = 0;
		IAIAction* pAction;
		while ( pAction = gEnv->pAISystem->GetAIAction(i++) )
		{
			IFlowGraph* pActionGraph = pAction->GetFlowGraph();
		}
	}

	HTREEITEM hEntityGroup = pTreeCtrl->InsertItem( g_EntitiesFolderName,2,2 );
	HTREEITEM hSharedGroup = pTreeCtrl->InsertItem( "Files",2,2 );
	HTREEITEM hPrefabGroup = pTreeCtrl->InsertItem( "Prefabs", 2,2);
	const CRuntimeClass* pPrefabClass = RUNTIME_CLASS(CPrefabObject);

	pTreeCtrl->Expand(hEntityGroup,TVE_EXPAND);

	std::map<CString,HTREEITEM,stl::less_stricmp<CString> > groups[4];

	HTREEITEM hSelectedItem = 0;
	for (int i = 0; i < pMgr->GetFlowGraphCount(); i++)
	{
		CFlowGraph *pFlowGraph = pMgr->GetFlowGraph(i);
		CEntity *pEntity = pFlowGraph->GetEntity();
		IAIAction *pAIAction = pFlowGraph->GetAIAction();
		const char* szGroupName;

		if (pEntity)
		{
			if (pEntity->IsInGroup() || pEntity->CheckFlags(OBJFLAG_PREFAB))
			{
				CGroup* pObjGroup = pEntity->GetGroup();
				assert (pObjGroup != 0);
				if (pObjGroup == 0)
				{
					CryLogAlways("CHyperGraphsTreeCtrl::Reload(): Entity %p (%s) in Group but GroupPtr is 0",
						pEntity, pEntity->GetName().GetString());
					continue;
				}
				if (!pObjGroup->IsOpen())
					continue;

				if (pObjGroup->IsKindOf(pPrefabClass) || pEntity->CheckFlags(OBJFLAG_PREFAB))
				{
					CPrefabObject *pPrefab = (CPrefabObject*) pObjGroup;

					const CString& pfGroupName = pObjGroup ? pObjGroup->GetName() : "<unknown PREFAB>";
#if 0
					CryLogAlways("entity 0x%p name=%s group=%s",
						pEntity, 
						pEntity->GetName().GetString(), 
						pfGroupName.GetString());
#endif

					HTREEITEM hGroup = stl::find_in_map( groups[3],pfGroupName,NULL );
					if (!hGroup && !pfGroupName.IsEmpty())
					{
						hGroup = pTreeCtrl->InsertItem( pfGroupName,2,2,hPrefabGroup, TVI_SORT );
						groups[3][pfGroupName] = hGroup;
						m_mapPrefabToItem[pPrefab] = hGroup;
					}
					if (!hGroup)
					{
						hGroup = hPrefabGroup;
						szGroupName = "Prefabs";
					}
					else
						szGroupName = pfGroupName.GetString();
					items.push_back(SHGTCReloadItem(pEntity->GetName(), szGroupName, hGroup, pFlowGraph->IsEnabled() ? 0 : 4, pFlowGraph, pEntity));
				}
				else
				{
					// entity is in a group, but not in a prefab
					CryLogAlways("CHyperGraphsTreeCtrl::Reload(): Entity %p (%s) in Group but not a Prefab",
						pEntity, pEntity->GetName().GetString());

					HTREEITEM hGroup = stl::find_in_map( groups[0],pFlowGraph->GetGroupName(),NULL );
					if (!hGroup && !pFlowGraph->GetGroupName().IsEmpty())
					{
						hGroup = pTreeCtrl->InsertItem( pFlowGraph->GetGroupName(),2,2,hEntityGroup, TVI_SORT );
						groups[0][pFlowGraph->GetGroupName()] = hGroup;
					}
					if (!hGroup)
					{
						hGroup = hEntityGroup;
						szGroupName = g_EntitiesFolderName;
					}
					else
						szGroupName = pFlowGraph->GetGroupName().GetString();
					CString reloadName = "[";
					reloadName+=pObjGroup->GetName();
					reloadName+="] ";
					reloadName+=pEntity->GetName();
					items.push_back(SHGTCReloadItem(reloadName, szGroupName, hGroup, pFlowGraph->IsEnabled() ? 0 : 4, pFlowGraph, pEntity));
				}
			}
			else
			{
				HTREEITEM hGroup = stl::find_in_map( groups[0],pFlowGraph->GetGroupName(),NULL );
				if (!hGroup && !pFlowGraph->GetGroupName().IsEmpty())
				{
					hGroup = pTreeCtrl->InsertItem( pFlowGraph->GetGroupName(),2,2,hEntityGroup, TVI_SORT );
					groups[0][pFlowGraph->GetGroupName()] = hGroup;
				}
				if (!hGroup)
				{
					hGroup = hEntityGroup;
					szGroupName = g_EntitiesFolderName;
				}
				else
					szGroupName = pFlowGraph->GetGroupName().GetString();
				items.push_back(SHGTCReloadItem(pEntity->GetName(), szGroupName, hGroup, pFlowGraph->IsEnabled() ? 0 : 4, pFlowGraph, pEntity));
			}
		}
		else if (pAIAction)
		{
			if ( pAIAction->GetUserEntity() || pAIAction->GetObjectEntity() )
				continue;

			HTREEITEM hGroup = stl::find_in_map( groups[1],pFlowGraph->GetGroupName(),NULL );
			if (!hGroup && !pFlowGraph->GetGroupName().IsEmpty())
			{
				hGroup = pTreeCtrl->InsertItem( pFlowGraph->GetGroupName(),2,2,hActionsGroup, TVI_SORT );
				groups[1][pFlowGraph->GetGroupName()] = hGroup;
				szGroupName = pFlowGraph->GetGroupName().GetString();
			}
			if (!hGroup)
			{
				hGroup = hActionsGroup;
				szGroupName = g_AIActionsFolderName;
			}
			else
				szGroupName = pFlowGraph->GetGroupName().GetString();
			items.push_back(SHGTCReloadItem(pAIAction->GetName(), szGroupName, hGroup, 1, pFlowGraph));
		}
		else
		{
			HTREEITEM hGroup = stl::find_in_map( groups[2],pFlowGraph->GetGroupName(),NULL );
			if (!hGroup && !pFlowGraph->GetGroupName().IsEmpty())
			{
				hGroup = pTreeCtrl->InsertItem( pFlowGraph->GetGroupName(),2,2,hSharedGroup, TVI_SORT );
				groups[2][pFlowGraph->GetGroupName()] = hGroup;
				szGroupName = pFlowGraph->GetGroupName().GetString();
			}
			if (!hGroup)
			{
				hGroup = hSharedGroup;
				szGroupName = "Files";
			}
			else
				szGroupName = pFlowGraph->GetGroupName().GetString();
			items.push_back(SHGTCReloadItem(pFlowGraph->GetName(), szGroupName, hGroup, pFlowGraph->IsEnabled() ? 3 : 5, pFlowGraph));
		}
	}

	std::sort( items.begin(), items.end() );
	for (size_t i=0; i<items.size(); ++i)
	{
		HTREEITEM hItem = items[i].Add( this );
		if (items[i].pEntity)
		{
			m_mapEntityToItem[items[i].pEntity] = hItem;
		}
		if (m_pCurrentGraph == items[i].pFlowGraph)
		{
			hSelectedItem = hItem;
		}
	}

	if (hSelectedItem)
	{
		pTreeCtrl->SelectItem( hSelectedItem );
	}

	/*
	HTREEITEM hEntityItem = 0;
	CEntity *pPrev = 0;
	//////////////////////////////////////////////////////////////////////////
	for (std::multimap<CEntity*,CFlowGraph*>::iterator it = graphs.begin(); it != graphs.end(); ++it)
	{
	CEntity *pEntity = it->first;
	CFlowGraph* pFlowGraph = it->second;

	if (pPrev != pEntity && pEntity)
	{
	hEntityItem = pTreeCtrl->InsertItem( pEntity->GetName(),0,0 );
	}
	if (hEntityItem)
	{
	HTREEITEM hItem = pTreeCtrl->InsertItem( pFlowGraph->GetName(),1,1,hEntityItem );
	pTreeCtrl->SetItemData( hItem,(DWORD_PTR)pFlowGraph );
	//pTreeCtrl->InsertItem( "" );
	}
	}
	*/
	SetRedraw(TRUE);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphsTreeCtrl::OnHyperGraphManagerEvent( EHyperGraphEvent event )
{
	if (m_bIgnoreReloads == true)
		return;

	switch (event)
	{
	case EHG_GRAPH_ADDED:
	case EHG_GRAPH_REMOVED:
	case EHG_GRAPH_OWNER_CHANGE:
		m_bReloadGraphs = true;
		Reload();
		// Invalidate(TRUE);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphsTreeCtrl::OnObjectEvent( CBaseObject *pObject,int nEvent )
{
	switch (nEvent)
	{
	case CBaseObject::ON_RENAME:
		if (pObject->IsKindOf(RUNTIME_CLASS(CEntity)))
		{
			HTREEITEM hItem = stl::find_in_map( m_mapEntityToItem,(CEntity*)pObject,NULL );
			if (hItem)
			{
				// Rename this item.
				SetItemText( hItem,pObject->GetName() );
				HTREEITEM hParent = this->GetParentItem(hItem);
				if (hParent)
				{
					SortChildren(hParent);
				}
			}
		}
		else if (pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			HTREEITEM hItem = stl::find_in_map (m_mapPrefabToItem, (CPrefabObject*)pObject, NULL);
			if (hItem)
			{
				SetItemText( hItem,pObject->GetName() );
				HTREEITEM hParent = this->GetParentItem(hItem);
				if (hParent)
				{
					SortChildren(hParent);
				}
				CWnd *pWnd = GetIEditor()->FindView( "Flow Graph" );
				if (pWnd != 0 && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
				{
					((CHyperGraphDialog*)pWnd)->GetGraphView()->InvalidateView(true);
				}
			}
		}
		break;


	case CBaseObject::ON_ADD:
		if (pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			CPrefabObject* pPrefab = (CPrefabObject*) pObject;
			// get all children and see if one Entity is there, which we already know of.
			// not very efficient...
			struct Recurser
			{
				Recurser(std::map<CEntity*,HTREEITEM>& mapEntityToItem)
					: m_mapEntityToItem(mapEntityToItem), m_bHasFG(false), m_bAbort(false) {}
				inline void DoIt(CBaseObject* pObject)
				{
					if (pObject->IsKindOf(RUNTIME_CLASS(CEntity)) == false)
						return;

					HTREEITEM hEntityItem = stl::find_in_map( m_mapEntityToItem,(CEntity*)pObject,NULL );
					m_bHasFG = (hEntityItem != 0);
					m_bAbort = m_bHasFG;
				}
				void Recurse(CBaseObject* pObject)
				{
					DoIt(pObject);
					if (m_bAbort)
						return;
					int nChilds = pObject->GetChildCount();
					for (int i = 0; i < nChilds; ++i)
					{
						Recurse( pObject->GetChild(i));
						if (m_bAbort)
							return;
					}
				}
				std::map<CEntity*,HTREEITEM>& m_mapEntityToItem;
				bool m_bHasFG;
				bool m_bAbort;
			};

			Recurser recurser(m_mapEntityToItem);
			recurser.Recurse(pPrefab);
			if (recurser.m_bHasFG)
			{
				m_bReloadGraphs = true;
				Reload();
			}
		}
		break;
	}
}


//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP( CHyperGraphComponentsReportCtrl, CXTPReportControl )
	ON_WM_LBUTTONDOWN( )
	ON_WM_LBUTTONUP( )
	ON_WM_MOUSEMOVE( )
	ON_WM_CAPTURECHANGED( )
	ON_WM_DESTROY ( )
	ON_NOTIFY_REFLECT(XTP_NM_REPORT_HEADER_RCLICK, OnReportColumnRClick)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnReportItemDblClick)
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

class CMyPaintManager : public CXTPReportPaintManager
{
public:
	CMyPaintManager() : m_bHeaderVisible(true) {}

	virtual void DrawColumn(
		CDC* pDC, 
		CXTPReportColumn* pColumn, 
		CXTPReportHeader* pHeader, 
		CRect rcColumn, 
		BOOL bDrawExternal
		)
	{
		if (m_bHeaderVisible)
			__super::DrawColumn(pDC,pColumn,pHeader,rcColumn,bDrawExternal);
	}

	virtual int GetRowHeight(CDC* pDC, CXTPReportRow* pRow)
	{
		return __super::GetRowHeight(pDC,pRow) - 2;
	}

	virtual int GetHeaderHeight()
	{
		if (m_bHeaderVisible) 
			return __super::GetHeaderHeight();
		else
			return 0;
	}
	bool m_bHeaderVisible;
};

//////////////////////////////////////////////////////////////////////////
CHyperGraphComponentsReportCtrl::CHyperGraphComponentsReportCtrl()
{
	m_pDialog = 0;

	CMFCUtils::LoadTrueColorImageList( m_imageList,IDB_HYPERGRAPH_COMPONENTS,16,RGB(255,0,255) );
	SetImageList(&m_imageList);

	CXTPReportColumn *pNodeCol   = AddColumn(new CXTPReportColumn(0, _T("NodeClass"), 150, TRUE, XTP_REPORT_NOICON, TRUE, TRUE));
	CXTPReportColumn *pCatCol    = AddColumn(new CXTPReportColumn(1, _T("Category"), 50, TRUE, XTP_REPORT_NOICON, TRUE, TRUE));
	//CXTPReportColumn *pNodeCol    = AddColumn(new CXTPReportColumn(1, _T("Node"), 50, TRUE, XTP_REPORT_NOICON, TRUE, TRUE));
	//GetColumns()->GetGroupsOrder()->Add(pGraphCol);
	pNodeCol->SetTreeColumn(true);
	pNodeCol->SetSortable(FALSE);
	pCatCol->SetSortable(FALSE);
	GetColumns()->SetSortColumn(pNodeCol, true);
	GetReportHeader()->AllowColumnRemove(FALSE);
	ShadeGroupHeadings(FALSE);
	SkipGroupsFocus(TRUE);
	SetMultipleSelection(FALSE);

	m_bDragging = false;
	m_bDragEx = false;
	m_ptDrag.SetPoint(0,0);

	CXTPReportPaintManager* pPMgr = new CMyPaintManager();
	pPMgr->m_nTreeIndent = 0x0f;
	pPMgr->m_bShadeSortColumn = false;
	pPMgr->m_strNoItems = _T("Use View->Components menu\nto show items");
	pPMgr->SetGridStyle(FALSE, xtpGridNoLines);
	pPMgr->SetGridStyle(TRUE, xtpGridNoLines);
	SetPaintManager( pPMgr );

	CXTRegistryManager regMgr;
	int showHeader = regMgr.GetProfileInt(_T("FlowGraph"), _T("ComponentShowHeader"), 1);
	SetHeaderVisible(showHeader != 0);
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraphComponentsReportCtrl::IsHeaderVisible()
{
	return m_bHeaderVisible;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphComponentsReportCtrl::SetHeaderVisible(bool bVisible)
{
	CMyPaintManager* pPMgr = static_cast<CMyPaintManager*> (GetPaintManager());
	pPMgr->m_bHeaderVisible = bVisible;
	m_bHeaderVisible = bVisible;
	// when the header is hidden, we also hide the Category column
	GetColumns()->GetAt(1)->SetVisible(m_bHeaderVisible);
}


//////////////////////////////////////////////////////////////////////////
CHyperGraphComponentsReportCtrl::~CHyperGraphComponentsReportCtrl()
{
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphComponentsReportCtrl::OnDestroy()
{
	CXTRegistryManager regMgr;
	regMgr.WriteProfileInt(_T("FlowGraph"), _T("ComponentShowHeader"), IsHeaderVisible() ? 1 : 0);
	__super::OnDestroy();
}

#define ID_SORT_ASC  1
#define ID_SORT_DESC  2
#define ID_SHOWHIDE_HEADER 3

//////////////////////////////////////////////////////////////////////////
void CHyperGraphComponentsReportCtrl::OnRButtonDown(UINT nFlags, CPoint point) 
{
	CRect reportArea = GetReportRectangle();
	if (reportArea.PtInRect(point))
	{
		CPoint pt;
		GetCursorPos(&pt);

		CMenu menu;
		VERIFY(menu.CreatePopupMenu());
		// create main menu items
		CXTPReportColumns* pColumns = GetColumns();
		CXTPReportColumn* pCatColumn = pColumns->GetAt(1);

		// create main menu items
		menu.AppendMenu(MF_STRING, ID_SHOWHIDE_HEADER, IsHeaderVisible() ?  _T("Hide Header") : _T("Show Header"));

		// track menu
		int nMenuResult = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN |TPM_RIGHTBUTTON, pt.x, pt.y, this, NULL);

		// other general items
		switch (nMenuResult)
		{
		case ID_SHOWHIDE_HEADER:
			SetHeaderVisible(!IsHeaderVisible());
			Populate();
			break;
		}
	}
	else
	{
		__super::OnRButtonDown(nFlags, point);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphComponentsReportCtrl::OnReportColumnRClick( NMHDR* pNotifyStruct, LRESULT* result )
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	ASSERT(pItemNotify->pColumn);
	CPoint ptClick = pItemNotify->pt;
	CXTPReportColumn* pColumn = pItemNotify->pColumn;

	CMenu menu;
	VERIFY(menu.CreatePopupMenu());

	CXTPReportColumns* pColumns = GetColumns();
	CXTPReportColumn* pNodeClassColumn = pColumns->GetAt(0);
	CXTPReportColumn* pCatColumn = pColumns->GetAt(1);

	// create main menu items
	menu.AppendMenu(MF_STRING, ID_SORT_ASC, _T("Sort ascending"));
	menu.AppendMenu(MF_STRING, ID_SORT_DESC, _T("Sort descending"));
	menu.AppendMenu(MF_STRING, ID_SHOWHIDE_HEADER, IsHeaderVisible() ?  _T("Hide Header") : _T("Show Header"));

	// track menu
	int nMenuResult = CXTPCommandBars::TrackPopupMenu(&menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_LEFTALIGN |TPM_RIGHTBUTTON, ptClick.x, ptClick.y, this, NULL);

	// other general items
	switch (nMenuResult)
	{
	case ID_SORT_ASC:
	case ID_SORT_DESC:
		// only sort by node class
		if (pNodeClassColumn && pNodeClassColumn->IsSortable())
		{
			pColumns->SetSortColumn(pNodeClassColumn, nMenuResult == ID_SORT_ASC);
			Populate();
		}
		break;
	case ID_SHOWHIDE_HEADER:
		SetHeaderVisible(!IsHeaderVisible());
		Populate();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphComponentsReportCtrl::OnLButtonDown( UINT nFlags, CPoint point )
{
	if (!m_bDragging) {
		__super::OnLButtonDown(nFlags,point);

		CRect reportArea = GetReportRectangle();
		if (reportArea.PtInRect(point))
		{
			CXTPReportRow* pRow = HitTest( point );
			if( pRow) 
			{
				if (pRow->GetParentRow() != 0)
				{
					m_ptDrag = point;
					m_bDragEx   = true;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphComponentsReportCtrl::OnLButtonUp( UINT nFlags, CPoint point )
{
	m_bDragEx = false;

	if( !m_bDragging )
	{
		__super::OnLButtonUp(nFlags,point);
	}
	else
	{
		m_bDragging = false;
		ReleaseCapture();
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphComponentsReportCtrl::OnMouseMove( UINT nFlags, CPoint point )
{
	if (!m_bDragging)
	{
		__super::OnMouseMove(nFlags,point);

		const int dragX = ::GetSystemMetrics(SM_CXDRAG);
		const int dragY = ::GetSystemMetrics(SM_CYDRAG);
		if ( (m_ptDrag.x || m_ptDrag.y) && (abs(point.x - m_ptDrag.x) > dragX || abs(point.y - m_ptDrag.y) > dragY) )
		{
			CXTPReportRow* pRow = HitTest( m_ptDrag );
			if (pRow)
			{
				m_bDragging = true;
				SetCapture();
				point = m_ptDrag;
				ClientToScreen(&point);
				m_pDialog->OnComponentBeginDrag(pRow, point);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphComponentsReportCtrl::OnCaptureChanged( CWnd* pWnd)
{
	// Stop the dragging
	m_bDragging = false;
	m_bDragEx = false;
	m_ptDrag.SetPoint(0,0);
	__super::OnCaptureChanged(pWnd);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphComponentsReportCtrl::OnReportItemDblClick( NMHDR* pNotifyStruct, LRESULT* result )
{
	XTP_NM_REPORTRECORDITEM* pItemNotify = (XTP_NM_REPORTRECORDITEM*) pNotifyStruct;
	CXTPReportRow* pRow = pItemNotify->pRow;
	if (pRow)
	{
		pRow->SetExpanded(pRow->IsExpanded() ? FALSE : TRUE);
	}
}

namespace
{
	struct NodeFilter
	{
	public:
		NodeFilter(uint32 mask) : mask(mask) {}
		bool Visit (CHyperNode* pNode)
		{
			if (strcmp(pNode->GetClassName(), CCommentNode::GetClassType()) == 0)
				return false;
			CFlowNode *pFlowNode = static_cast<CFlowNode*> (pNode);
			if ((pFlowNode->GetCategory() & mask) == 0)
				return false;
			return true;

			// Only if the usage mask is set check if fulfilled -> this is an exclusive thing
			if ((mask&EFLN_USAGE_MASK) != 0 && (pFlowNode->GetUsageFlags() & mask) == 0)
				return false;
			return true;
		}
		uint32 mask;
	};
};

class CComponentRecord : public CXTPReportRecord
{
public:
	CComponentRecord(bool bIsGroup=true, const CString& name="")
		: m_name(name), m_bIsGroup(bIsGroup)
	{
	}
	bool IsGroup() const { return m_bIsGroup; }
	const CString& GetName() const { return m_name; }
protected:
	CString m_name;
	bool m_bIsGroup;
};

//////////////////////////////////////////////////////////////////////////
void CHyperGraphComponentsReportCtrl::Reload()
{
	std::map<CString,CComponentRecord*> groupMap;

	BeginUpdate();
	GetRecords()->RemoveAll();

	CFlowGraphManager *pMgr = GetIEditor()->GetFlowGraphManager();
	std::vector<_smart_ptr<CHyperNode> > prototypes;
	NodeFilter filter(m_mask);

	pMgr->GetPrototypesEx( prototypes, true, functor_ret(filter, &NodeFilter::Visit));
	for (int i = 0; i < prototypes.size(); i++)
	{
		const CHyperNode* pNode = prototypes[i];
		const CFlowNode* pFlowNode = static_cast<const CFlowNode*> (pNode);

		CString fullClassName = pFlowNode->GetUIClassName();

		CComponentRecord* pItemGroupRec = 0;
		int midPos = 0;
		int pos = 0;
		int len = fullClassName.GetLength();
		bool bUsePrototypeName = false; // use real prototype name or stripped from fullClassName

		CString nodeShortName;
		CString groupName = fullClassName.Tokenize( ":", pos);

		// check if a group name is given. if not, fake 'Misc:' group
		if (pos < 0 || pos >= len)
		{
			bUsePrototypeName = true; // re-fetch real prototype name
			fullClassName.Insert(0, "Misc:");
			pos = 0;
			len = fullClassName.GetLength();
			groupName = fullClassName.Tokenize( ":", pos);
		}

		CString pathName = ""; 
		while (pos >= 0 && pos < len)
		{
			pathName += groupName;
			pathName += ":";
			midPos = pos;

			CComponentRecord* pGroupRec = stl::find_in_map( groupMap,pathName,0 );
			if (pGroupRec == 0)
			{
				pGroupRec = new CComponentRecord();
				CXTPReportRecordItem* pNewItem = new CXTPReportRecordItemText(groupName);
				pNewItem->SetIconIndex(0);
				pGroupRec->AddItem(pNewItem);
				pGroupRec->AddItem(new CXTPReportRecordItemText(""));

				if (pItemGroupRec != 0)
				{
					pItemGroupRec->GetChilds()->Add(pGroupRec);
				}
				else
				{
					AddRecord(pGroupRec);
				}
				groupMap[pathName] = pGroupRec;
				pItemGroupRec = pGroupRec;
			}
			else
			{
				pItemGroupRec = pGroupRec;
			}

			// continue stripping 
			groupName = fullClassName.Tokenize(":", pos);
		};

		// short node name without ':'. used for display in last column
		nodeShortName = fullClassName.Mid(midPos);

		CComponentRecord* pRec = new CComponentRecord(false, pFlowNode->GetClassName());
		CXTPReportRecordItemText* pNewItem = new CXTPReportRecordItemText(nodeShortName);
		CXTPReportRecordItemText* pNewCatItem = new CXTPReportRecordItemText(pFlowNode->GetCategoryName());
		pNewItem->SetIconIndex(1);
		pRec->AddItem(pNewItem);
		pRec->AddItem(pNewCatItem);

		switch (pFlowNode->GetCategory())
		{
		case EFLN_APPROVED:
			//pNewItem->SetTextColor(RGB(20,200,20));
			//pNewCatItem->SetTextColor(RGB(20,200,20));
			//pNewItem->SetTextColor(RGB(120,120,124));
			//pNewCatItem->SetTextColor(RGB(120,120,124));
			pNewItem->SetTextColor(RGB(40,150,40));
			pNewCatItem->SetTextColor(RGB(40,150,40));
			break;
		case EFLN_ADVANCED:
			pNewItem->SetTextColor(RGB(40,40,220));
			pNewCatItem->SetTextColor(RGB(40,40,220));
			break;
		case EFLN_DEBUG:
			pNewItem->SetTextColor(RGB(220,180,20));
			pNewCatItem->SetTextColor(RGB(220,180,20));
			break;
		case EFLN_OBSOLETE:
			pNewItem->SetTextColor(RGB(255,0,0));
			pNewCatItem->SetTextColor(RGB(255,0,0));
			break;
		default:
			pNewItem->SetTextColor(RGB(0,0,0));
			pNewCatItem->SetTextColor(RGB(0,0,0));
			break;
		}

		assert (pItemGroupRec != 0);
		if (pItemGroupRec)
			pItemGroupRec->GetChilds()->Add(pRec);
		else
			AddRecord(pRec);
	}

	EndUpdate();
	Populate();
}

//////////////////////////////////////////////////////////////////////////
CImageList* CHyperGraphComponentsReportCtrl::CreateDragImage(CXTPReportRow* pRow)
{
	CXTPReportRecord* pRecord = pRow->GetRecord();
	if (pRecord == 0)
		return 0;

	CXTPReportRecordItem* pItem = pRecord->GetItem(0);
	if (pItem == 0)
		return 0;

	CXTPReportColumn* pCol = GetColumns()->GetAt(0);
	assert (pCol != 0);

	CClientDC	dc (this);
	CDC memDC;	
	if(!memDC.CreateCompatibleDC(&dc))
		return NULL;

	CRect rect;
	rect.SetRectEmpty();

	XTP_REPORTRECORDITEM_DRAWARGS drawArgs;
	drawArgs.pDC = &memDC;
	drawArgs.pControl = this;
	drawArgs.pRow = pRow;
	drawArgs.pColumn = pCol;
	drawArgs.pItem = pItem;
	drawArgs.rcItem = rect;
	XTP_REPORTRECORDITEM_METRICS metrics;
	pRow->GetItemMetrics(&drawArgs, &metrics);

	CString text = pItem->GetCaption(drawArgs.pColumn);

	CFont* pOldFont = memDC.SelectObject(GetFont());
	memDC.SelectObject(metrics.pFont);
	memDC.DrawText(text, &rect, DT_CALCRECT);

	HICON hIcon = m_imageList.ExtractIcon(pItem->GetIconIndex());
	int sx,sy;
	ImageList_GetIconSize(*&m_imageList, &sx, &sy);
	rect.right += sx+6;
	rect.bottom = sy > rect.bottom ? sy : rect.bottom;
	CRect boundingRect = rect;

	CBitmap bitmap;
	if(!bitmap.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height()))
		return NULL;

	CBitmap* pOldMemDCBitmap = memDC.SelectObject( &bitmap );
	memDC.SetBkColor(	metrics.clrBackground);
	memDC.FillSolidRect(&rect, metrics.clrBackground);
	memDC.SetTextColor(GetPaintManager()->m_clrWindowText);

	::DrawIconEx (*&memDC, rect.left+2, rect.top, hIcon, sx, sy, 0, NULL, DI_NORMAL); 

	rect.left += sx+4;
	rect.top -= 1;
	memDC.DrawText(text, &rect, 0);
	memDC.SelectObject( pOldFont );
	memDC.SelectObject( pOldMemDCBitmap );

	// Create image list
	CImageList* pImageList = new CImageList;
	pImageList->Create(boundingRect.Width(), boundingRect.Height(), 
		ILC_COLOR | ILC_MASK , 0, 1);
	pImageList->Add(&bitmap, metrics.clrBackground); // Here green is used as mask color

	::DestroyIcon(hIcon);
	bitmap.DeleteObject();
	return pImageList;
}

//////////////////////////////////////////////////////////////////////////
class CHyperGraphViewClass : public TRefCountBase<IViewPaneClass>
{
	//////////////////////////////////////////////////////////////////////////
	// IClassDesc
	//////////////////////////////////////////////////////////////////////////
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_VIEWPANE; };
	virtual REFGUID ClassID()
	{
		// {DE6E69F3-8B53-47de-89FF-11485852B6C9}
		static const GUID guid = { 0xde6e69f3, 0x8b53, 0x47de, { 0x89, 0xff, 0x11, 0x48, 0x58, 0x52, 0xb6, 0xc9 } };
		return guid;
	}
	virtual const char* ClassName() { return "Flow Graph"; };
	virtual const char* Category() { return "Flow Graph"; };
	//////////////////////////////////////////////////////////////////////////
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CHyperGraphDialog); };
	virtual const char* GetPaneTitle() { return _T("Flow Graph"); };
	virtual EDockingDirection GetDockingDirection() { return DOCK_FLOAT; };
	virtual CRect GetPaneRect() { return CRect(200,200,600,500); };
	virtual bool SinglePane() { return false; };
	virtual bool WantIdleUpdate() { return true; };
};

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::RegisterViewClass()
{
	GetIEditor()->GetClassFactory()->RegisterClass( new CHyperGraphViewClass );
}

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CHyperGraphDialog,CXTPFrameWnd)

BEGIN_MESSAGE_MAP(CHyperGraphDialog, CXTPFrameWnd)
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_WM_DESTROY()
	ON_WM_CLOSE()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_COMMAND( ID_FILE_NEW,OnFileNew )
	ON_COMMAND( ID_FILE_OPEN,OnFileOpen )
	ON_COMMAND( ID_FILE_SAVE,OnFileSave )
	ON_COMMAND( ID_FILE_SAVE_AS,OnFileSaveAs )
	ON_COMMAND( ID_FILE_NEWACTION,OnFileNewAction )
	ON_COMMAND( ID_FILE_IMPORT,OnFileImport )
	ON_COMMAND( ID_FILE_EXPORTSELECTION,OnFileExportSelection )
	ON_COMMAND( ID_EDIT_FG_FIND, OnFind )
	ON_COMMAND( ID_FLOWGRAPH_PLAY,OnPlay )
	ON_COMMAND( ID_FLOWGRAPH_STOP,OnStop )
	ON_COMMAND( ID_FLOWGRAPH_DEBUG,OnToggleDebug )
	ON_COMMAND( ID_FLOWGRAPH_DEBUG_ERASE,OnEraseDebug )
	//ON_COMMAND( ID_FLOWGRAPH_PAUSE,OnPause )
	ON_COMMAND( ID_FLOWGRAPH_STEP,OnStep )
	ON_UPDATE_COMMAND_UI(ID_FLOWGRAPH_DEBUG, OnUpdateDebug)
	ON_UPDATE_COMMAND_UI(ID_FLOWGRAPH_PLAY, OnUpdatePlay)
	//ON_UPDATE_COMMAND_UI(ID_FLOWGRAPH_PAUSE, OnUpdateStep)
#ifdef _DEBUG
	ON_COMMAND_EX_RANGE( ID_COMPONENTS_APPROVED, ID_COMPONENTS_OBSOLETE, OnComponentsViewToggle)
	ON_UPDATE_COMMAND_UI_RANGE( ID_COMPONENTS_APPROVED, ID_COMPONENTS_OBSOLETE, OnComponentsViewUpdate)
#else
	ON_COMMAND_EX_RANGE( ID_COMPONENTS_APPROVED, ID_COMPONENTS_DEBUG, OnComponentsViewToggle)
	ON_UPDATE_COMMAND_UI_RANGE( ID_COMPONENTS_APPROVED, ID_COMPONENTS_DEBUG, OnComponentsViewUpdate)
#endif
	ON_COMMAND_EX(ID_VIEW_GRAPHS, OnToggleBar )
	ON_COMMAND_EX(ID_VIEW_NODEINPUTS, OnToggleBar )
	ON_COMMAND_EX(ID_VIEW_COMPONENTS, OnToggleBar )
	ON_COMMAND_EX(ID_VIEW_SEARCHRESULTS, OnToggleBar )
	ON_UPDATE_COMMAND_UI(ID_VIEW_GRAPHS, OnUpdateControlBar )
	ON_UPDATE_COMMAND_UI(ID_VIEW_NODEINPUTS, OnUpdateControlBar )
	ON_UPDATE_COMMAND_UI(ID_VIEW_COMPONENTS, OnUpdateControlBar )
	ON_UPDATE_COMMAND_UI(ID_VIEW_SEARCHRESULTS, OnUpdateControlBar )

	ON_COMMAND_EX(ID_VIEW_MULTIPLAYER, OnMultiPlayerViewToggle)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MULTIPLAYER, OnMultiPlayerViewUpdate)

	ON_NOTIFY(TVN_SELCHANGED, IDC_HYPERGRAPH_GRAPHS, OnGraphsSelChanged)
	ON_NOTIFY(NM_RCLICK, IDC_HYPERGRAPH_GRAPHS, OnGraphsRightClick)
	ON_NOTIFY(NM_DBLCLK, IDC_HYPERGRAPH_GRAPHS, OnGraphsDblClick)

	ON_NOTIFY(XTP_LBN_SELCHANGE, ID_FG_BACK, OnNavListBoxControlSelChange)
	ON_NOTIFY(XTP_LBN_SELCHANGE, ID_FG_FORWARD, OnNavListBoxControlSelChange)
	ON_NOTIFY(XTP_LBN_POPUP, ID_FG_BACK, OnNavListBoxControlPopup)
	ON_NOTIFY(XTP_LBN_POPUP, ID_FG_FORWARD, OnNavListBoxControlPopup)
	ON_UPDATE_COMMAND_UI(ID_FG_BACK,    OnUpdateNav)
	ON_UPDATE_COMMAND_UI(ID_FG_FORWARD, OnUpdateNav)
	ON_COMMAND( ID_FG_FORWARD,OnNavForward )
	ON_COMMAND( ID_FG_BACK,OnNavBack )

	//////////////////////////////////////////////////////////////////////////
	// XT Commands.
	ON_MESSAGE(XTPWM_DOCKINGPANE_NOTIFY, OnDockingPaneNotify)
	ON_XTP_CREATECOMMANDBAR()
	ON_XTP_CREATECONTROL()
END_MESSAGE_MAP()
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CHyperGraphDialog::CHyperGraphDialog()
{
	GetIEditor()->RegisterNotifyListener( this );
	GetIEditor()->GetFlowGraphManager()->AddListener(this);

	m_pPropertiesItem = 0;
	m_pPropertiesFolder = NULL;
	m_pDragImage = NULL;
	m_pSearchResultsCtrl = 0;
	m_pSearchOptionsView = 0;
	m_pGraphProperties = 0;
	m_pNavGraph = 0;

	WNDCLASS wndcls;
	HINSTANCE hInst = AfxGetInstanceHandle();
	if (!(::GetClassInfo(hInst, HYPERGRAPH_DIALOGFRAME_CLASSNAME, &wndcls)))
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
		wndcls.lpszClassName    = HYPERGRAPH_DIALOGFRAME_CLASSNAME;
		if (!AfxRegisterClass(&wndcls))
		{
			AfxThrowResourceException();
		}
	}
	CRect rc(0,0,0,0);
	BOOL bRes = Create( WS_CHILD,rc,AfxGetMainWnd() );
	if (!bRes)
		return;
	ASSERT( bRes );

	OnInitDialog();
}

//////////////////////////////////////////////////////////////////////////
CHyperGraphDialog::~CHyperGraphDialog()
{
	SAFE_DELETE (m_pSearchResultsCtrl);
	SAFE_DELETE (m_pSearchOptionsView);
	SAFE_DELETE (m_pGraphProperties);

	if (GetIEditor()->GetFlowGraphManager())
		GetIEditor()->GetFlowGraphManager()->RemoveListener(this);

	GetIEditor()->UnregisterNotifyListener( this );
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd )
{
	return __super::Create( HYPERGRAPH_DIALOGFRAME_CLASSNAME,NULL,dwStyle,rect,pParentWnd );
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::OnCmdMsg(UINT nID, int nCode, void* pExtra,AFX_CMDHANDLERINFO* pHandlerInfo)
{
	BOOL res = FALSE;
	if (m_view.m_hWnd)
	{
		res = m_view.OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
		if (TRUE == res)
			return res;
	}

	/*
	CPushRoutingFrame push(this);
	return CWnd::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
	*/

	return __super::OnCmdMsg(nID,nCode,pExtra,pHandlerInfo);
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::OnInitDialog()
{
	LoadAccelTable( MAKEINTRESOURCE(IDR_HYPERGRAPH) );

	ModifyStyleEx( WS_EX_CLIENTEDGE, 0 );

	CRect rc;
	GetClientRect( &rc );

	try
	{
		//////////////////////////////////////////////////////////////////////////
		// Initialize the command bars
		if (!InitCommandBars())
			return -1;

	}	catch (CResourceException *e)
	{
		e->Delete();
		return -1;
	}

	// Get a pointer to the command bars object.
	CXTPCommandBars* pCommandBars = GetCommandBars();
	if(pCommandBars == NULL)
	{
		TRACE0("Failed to create command bars object.\n");
		return -1;      // fail to create
	}

	// Add the menu bar
	CXTPCommandBar* pMenuBar = pCommandBars->SetMenu( _T("Menu Bar"),IDR_HYPERGRAPH );
	ASSERT(pMenuBar);
	pMenuBar->SetFlags(xtpFlagStretched);
	pMenuBar->EnableCustomization(FALSE);

	//////////////////////////////////////////////////////////////////////////
	// Create standart toolbar.
	CXTPToolBar *pStdToolBar = pCommandBars->Add( _T("HyperGraph ToolBar"),xtpBarTop );
	VERIFY(pStdToolBar->LoadToolBar(IDR_FLOWGRAPH_BAR));
	//////////////////////////////////////////////////////////////////////////

	//m_cDlgToolBar.SetButtonStyle( m_cDlgToolBar.CommandToIndex(ID_TV_ADDKEY),TBBS_CHECKGROUP );

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

	CFlowGraphSearchOptions::GetSearchOptions()->Serialize(true);

	CXTRegistryManager regMgr;

	// FIXME: Platform independent uint32 -> dword ...
	DWORD compView;
	if (regMgr.GetProfileDword(_T("FlowGraph"), _T("ComponentView"), &compView) == false)
		compView = EFLN_APPROVED | EFLN_ADVANCED | EFLN_DEBUG;
	m_componentsViewMask = compView;
	m_componentsReportCtrl.SetComponentFilterMask(m_componentsViewMask);
	m_view.SetComponentViewMask(m_componentsViewMask);

	CreateLeftPane();
	CreateRightPane();
	UpdateComponentPaneTitle();

	// Create View.
	m_view.Create( WS_CHILD|WS_CLIPCHILDREN|WS_VISIBLE,rc,this,AFX_IDW_PANE_FIRST );

	int paneLayoutVersion = regMgr.GetProfileInt(_T("FlowGraph"), _T("FlowGraphLayoutVersion"), 0);
	if (paneLayoutVersion == FlowGraphLayoutVersion)
	{
		CXTPDockingPaneLayout layout(GetDockingPaneManager());
		if (layout.Load(_T("FlowGraphLayout"))) 
		{
			if (layout.GetPaneList().GetCount() > 0)
			{
				GetDockingPaneManager()->SetLayout(&layout);	
			}
		}
	}
	else
	{
		regMgr.WriteProfileInt(_T("FlowGraph"), _T("FlowGraphLayoutVersion"), FlowGraphLayoutVersion);
	}

	m_statusBar.Create(this);
	static UINT indicators[] = 
	{
		1,
	};
	m_statusBar.SetIndicators( indicators, sizeof(indicators)/sizeof(*indicators) );
	m_statusBar.SetPaneInfo(0,1, SBPS_STRETCH, 100);
	m_statusBar.ShowWindow(SW_HIDE);
	UpdateTitle(0);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::PostNcDestroy()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::PreTranslateMessage(MSG* pMsg)
{
	bool bFramePreTranslate = true;
	if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
	{
		CWnd* pWnd = CWnd::GetFocus();
		if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CEdit)))
			bFramePreTranslate = false;
	}

	if (bFramePreTranslate)
	{
		// allow tooltip messages to be filtered
		if (__super::PreTranslateMessage(pMsg))
			return TRUE;
	}

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
//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnPaint()
{
	CPaintDC PaintDC(this); // device context for painting
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);

	/*
	if (m_view.m_hWnd)
	{
	CRect rc;
	GetClientRect( &rc );

	rc.right -= 300;
	m_view.MoveWindow( rc );
	m_view.ShowWindow(SW_SHOW);
	}
	*/

	//	RecalcLayout();

	/*
	if (m_wndSplitter.GetSafeHwnd())
	{
	CRect rc;
	GetClientRect(&rc);
	m_wndSplitter.MoveWindow(&rc,FALSE);
	}
	RecalcLayout();
	*/
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnSetFocus(CWnd* pOldWnd)
{
	__super::OnSetFocus(pOldWnd);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnDestroy()
{
	CXTPDockingPaneLayout layout(GetDockingPaneManager());
	GetDockingPaneManager()->GetLayout( &layout );
	layout.Save(_T("FlowGraphLayout"));
	CFlowGraphSearchOptions::GetSearchOptions()->Serialize(false);
	CXTRegistryManager regMgr;
	DWORD mask = m_componentsViewMask;
	regMgr.WriteProfileDword(_T("FlowGraph"), _T("ComponentView"), &mask);

	ClearGraph();
	__super::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnClose()
{
	__super::OnClose();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::CreateRightPane()
{
	CRect rc(0,0,220,500);
	CXTPDockingPane* pDockPane = GetDockingPaneManager()->CreatePane( IDW_HYPERGRAPH_RIGHT_PANE,rc,dockRightOf);
	pDockPane->SetTitle( _T("Inputs") );

	m_wndTaskPanel.Create( WS_CHILD,rc,this,0 );

	m_wndTaskPanel.SetBehaviour(xtpTaskPanelBehaviourExplorer);
	//m_wndTaskPanel.SetBehaviour(xtpTaskPanelBehaviourList);
	//SetTheme(xtpTaskPanelThemeListViewOffice2003);
	//SetTheme(xtpTaskPanelThemeToolboxWhidbey);
	m_wndTaskPanel.SetTheme(xtpTaskPanelThemeNativeWinXP);
	m_wndTaskPanel.SetHotTrackStyle(xtpTaskPanelHighlightItem);
	m_wndTaskPanel.SetSelectItemOnFocus(TRUE);
	m_wndTaskPanel.AllowDrag(FALSE);

	m_wndTaskPanel.SetAnimation( xtpTaskPanelAnimationNo );

	m_wndTaskPanel.GetPaintManager()->m_rcGroupOuterMargins.SetRect(0,0,0,0);
	m_wndTaskPanel.GetPaintManager()->m_rcGroupInnerMargins.SetRect(0,0,0,0);
	m_wndTaskPanel.GetPaintManager()->m_rcItemOuterMargins.SetRect(0,0,0,0);
	m_wndTaskPanel.GetPaintManager()->m_rcItemInnerMargins.SetRect(1,1,1,1);
	m_wndTaskPanel.GetPaintManager()->m_rcControlMargins.SetRect(2,0,2,0);
	m_wndTaskPanel.GetPaintManager()->m_nGroupSpacing = 0;

	//////////////////////////////////////////////////////////////////////////
	// Create properties control.
	//////////////////////////////////////////////////////////////////////////
	m_wndProps.Create( WS_CHILD,rc,&m_wndTaskPanel );

	//m_wndProps.SetFlags( CPropertyCtrl::F_VS_DOT_NET_STYLE );
	m_wndProps.SetItemHeight( 16 );
	m_wndProps.ExpandAll();

	m_view.SetPropertyCtrl( &m_wndProps );
	//////////////////////////////////////////////////////////////////////////

	m_pPropertiesFolder = m_wndTaskPanel.AddGroup(ID_PROPERTY_GROUP);
	m_pPropertiesFolder->SetCaption( "Inputs" );
	m_pPropertiesFolder->SetItemLayout(xtpTaskItemLayoutDefault);

	m_pPropertiesItem =  m_pPropertiesFolder->AddControlItem(m_wndProps.GetSafeHwnd());
	m_pPropertiesItem->SetCaption( "Properties" );

	m_wndTaskPanel.ExpandGroup( m_pPropertiesFolder,FALSE );

	CXTPTaskPanelItem *pItem = 0;
	CXTPTaskPanelGroup *pNodeGroup = m_wndTaskPanel.AddGroup(ID_NODE_INFO_GROUP);
	pNodeGroup->SetCaption( "Selected Node Info" );

	pItem = pNodeGroup->AddLinkItem(ID_NODE_INFO);
	pItem->SetType(xtpTaskItemTypeText);

	m_pGraphProperties = new CFlowGraphProperties(this, &m_wndTaskPanel);
	m_pGraphProperties->Create(WS_CHILD|WS_BORDER,CRect(0,0,0,0),this,ID_GRAPH_INFO_GROUP);

	CXTPDockingPane* pSearchPane = GetDockingPaneManager()->CreatePane( IDW_HYPERGRAPH_SEARCH_PANE,CRect(0,0,200,250),dockBottomOf,pDockPane );
	pSearchPane->SetTitle( _T("Search") );
	m_pSearchOptionsView = new CFlowGraphSearchCtrl(this);

	CXTPDockingPane* pSearchResultsPane = GetDockingPaneManager()->CreatePane( IDW_HYPERGRAPH_SEARCHRESULTS_PANE,rc,dockBottomOf,pSearchPane );
	pSearchResultsPane->SetTitle( _T("SearchResults") );
	//pSearchResultsPane->SetMinTrackSize(CSize(100,50));

	m_pSearchResultsCtrl = new CFlowGraphSearchResultsCtrl(this);
	m_pSearchResultsCtrl->Create( WS_CHILD|WS_BORDER,CRect(0,0,0,0),this,0 );
	m_pSearchResultsCtrl->SetOwner(this);

	// set the results control for the search control
	m_pSearchOptionsView->SetResultsCtrl(m_pSearchResultsCtrl);
	m_pSearchOptionsView->SetOwner(this);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::CreateLeftPane()
{
	CRect rc(0,0,220,500);
	m_componentsReportCtrl.Create(WS_CHILD|WS_TABSTOP,rc,this,IDC_HYPERGRAPH_COMPONENTS );
	m_componentsReportCtrl.SetDialog(this);
	m_componentsReportCtrl.Reload();

	CXTPDockingPane* pComponentsPane = GetDockingPaneManager()->CreatePane( IDW_HYPERGRAPH_COMPONENTS_PANE,rc,dockLeftOf );
	pComponentsPane->SetTitle( _T("Components") );

	m_graphsTreeCtrl.Create( WS_CHILD|WS_TABSTOP,rc,this,IDC_HYPERGRAPH_GRAPHS );
	m_graphsTreeCtrl.Reload();

	CXTPDockingPane* pGraphsPane = GetDockingPaneManager()->CreatePane( IDW_HYPERGRAPH_TREE_PANE,rc,dockBottomOf,pComponentsPane );
	pGraphsPane->SetTitle( _T("Flow Graphs") );
}

/////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnViewSelectionChange()
{
	if (!m_pPropertiesItem)
		return;

	m_wndTaskPanel.ExpandGroup( m_pPropertiesFolder,FALSE );

	CRect rc;
	m_wndTaskPanel.GetClientRect(rc);
	// Resize to fit properties.
	int h = m_wndProps.GetVisibleHeight();
	m_wndProps.SetWindowPos( NULL,0,0,rc.right,h + 1,SWP_NOMOVE );

	m_wndProps.GetClientRect(rc);

	::GetClientRect(m_wndProps.GetSafeHwnd(), rc);

	m_pPropertiesItem->SetControlHandle( m_wndProps.GetSafeHwnd() );
	m_wndTaskPanel.Reposition(FALSE);

	m_wndTaskPanel.ExpandGroup( m_pPropertiesFolder,TRUE );

	//////////////////////////////////////////////////////////////////////////
	std::vector<CHyperNode*> nodes;
	m_view.GetSelectedNodes(nodes);
	if (nodes.size() <= 1)
	{
		CXTPTaskPanelGroupItem* pItem = 0;
		CXTPTaskPanelGroup *pGroup = m_wndTaskPanel.FindGroup(ID_NODE_INFO_GROUP);
		if (pGroup)
		{
			pItem = pGroup->FindItem(ID_NODE_INFO);
		}
		if (pItem)
		{
			if (nodes.size() == 1) {
				CString nodeInfo;
				CHyperNode *pNode = nodes[0];
				if (strcmp(pNode->GetClassName(), CCommentNode::GetClassType()) == 0)
				{
					nodeInfo.Format( "Name: %s\nClass: %s\n%s",
						pNode->GetName(),pNode->GetClassName(),pNode->GetDescription() );
				}
				else
				{	
					CFlowNode *pFlowNode = static_cast<CFlowNode*> (pNode);
					CString cat = pFlowNode->GetCategoryName();
					const uint32 usageFlags = pFlowNode->GetUsageFlags();
					// TODO: something with Usage flags
					if (gSettings.bFlowGraphShowNodeIDs)
						nodeInfo.Format( "Name: %s\nClass: %s\nNodeID=%d\nCategory: %s\n%s",
						pFlowNode->GetName(),pFlowNode->GetClassName(),pFlowNode->GetId(),cat.GetString(),pFlowNode->GetDescription() );
					else
						nodeInfo.Format( "Name: %s\nClass: %s\nCategory: %s\n%s",
						pFlowNode->GetName(),pFlowNode->GetClassName(),cat.GetString(),pFlowNode->GetDescription() );
				}
				pItem->SetCaption( nodeInfo );
			} else {
				pItem->SetCaption("");
			}
		}
	}
	else
	{
		//m_wndTaskPanel.GetGroups()->De
		//m_wndTaskPanel.GetGroups()->Remove( m_wndTaskPanel.FindGroup(ID_NODE_INFO_GROUP) );
	}
}

//////////////////////////////////////////////////////////////////////////
LRESULT CHyperGraphDialog::OnDockingPaneNotify(WPARAM wParam, LPARAM lParam)
{
	if (wParam == XTP_DPN_SHOWWINDOW)
	{
		// get a pointer to the docking pane being shown.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (!pwndDockWindow->IsValid())
		{
			switch (pwndDockWindow->GetID())
			{
			case IDW_HYPERGRAPH_RIGHT_PANE:
				pwndDockWindow->Attach(&m_wndTaskPanel);
				m_wndTaskPanel.ShowWindow(SW_SHOW);
				break;
			case IDW_HYPERGRAPH_TREE_PANE:
				pwndDockWindow->Attach(&m_graphsTreeCtrl);
				m_graphsTreeCtrl.ShowWindow(SW_SHOW);
				break;
			case IDW_HYPERGRAPH_COMPONENTS_PANE:
				pwndDockWindow->Attach(&m_componentsReportCtrl);
				m_componentsReportCtrl.ShowWindow(SW_SHOW);
				break;
			case IDW_HYPERGRAPH_SEARCH_PANE:
				pwndDockWindow->Attach(m_pSearchOptionsView);
				m_pSearchOptionsView->ShowWindow(SW_SHOW);
				break;
			case IDW_HYPERGRAPH_SEARCHRESULTS_PANE:
				pwndDockWindow->Attach(m_pSearchResultsCtrl);
				m_pSearchResultsCtrl->ShowWindow(SW_SHOW);
				break;
			default:
				return FALSE;
			}
		}
		return TRUE;
	}
	else if (wParam == XTP_DPN_CLOSEPANE)
	{
		// get a pointer to the docking pane being shown.
		CXTPDockingPane* pwndDockWindow = (CXTPDockingPane*)lParam;
		if (pwndDockWindow->IsValid())
		{
			switch (pwndDockWindow->GetID())
			{
			case IDW_HYPERGRAPH_RIGHT_PANE:
				break;
			}
		}
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::SetGraph( CHyperGraph *pGraph )
{
	// CryLogAlways("CHyperGraphDialog::SetGraph: Switching to 0x%p", pGraph);
	m_view.SetGraph( pGraph );
	m_graphsTreeCtrl.SetCurrentGraph( pGraph );
	m_pGraphProperties->SetGraph ( pGraph );
	UpdateTitle(pGraph);

	if (pGraph != 0)
	{
		/* If we want to show the title somewhere
		CString name, groupName;
		GetPrettyName(pGraph, name, groupName);
		title += groupName;
		title += ":";
		title += name;
		*/

		std::list<CHyperGraph*>::iterator iter = std::find(m_graphList.begin(), m_graphList.end(), pGraph);
		if (iter != m_graphList.end())
		{
			m_graphIterCur = iter;
		}
		else
		{
			// maximum entry 20
			while (m_graphList.size() > 20)
			{
				m_graphList.pop_front();
			}
			m_graphIterCur = m_graphList.insert(m_graphList.end(), pGraph);
		}
	}
	else
		m_graphIterCur = m_graphList.end();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::NewGraph()
{
	SetGraph( (CHyperGraph*)GetIEditor()->GetFlowGraphManager()->CreateGraph() );
}


//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileNew()
{
	ClearGraph();
	// Make a new graph.
	NewGraph();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileOpen()
{
	ClearGraph();
	CString filename;
	// if (CFileUtil::SelectSingleFile( EFILE_TYPE_ANY,filename,GRAPH_FILE_FILTER )) // game dialog
	if (CFileUtil::SelectFile ( GRAPH_FILE_FILTER, "", filename )) // native dialog
	{
		NewGraph();
		bool ok = m_view.GetGraph()->Load( filename );
		if (!ok)
		{
			Warning ("Cannot load FlowGraph from file '%s'.\nMake sure the file is actually a FlowGraph XML File.", filename.GetString());
		}
		m_view.GetGraph()->SetName(Path::GetFileName(filename));
		m_graphsTreeCtrl.Reload();
		m_view.InvalidateView(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileSave()
{
	CHyperGraph *pGraph = m_view.GetGraph();
	if (!pGraph)
		return;

	CString filename = pGraph->GetFilename();
	Path::ReplaceExtension(filename, ".xml");
	if (filename.IsEmpty())
	{
		OnFileSaveAs();
	}
	else
	{
		bool ok = pGraph->Save(filename);
		if (!ok)
		{
			Warning ("Cannot save FlowGraph to file '%s'.\nMake sure the path is correct.", filename.GetString());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileSaveAs()
{
	CHyperGraph *pGraph = m_view.GetGraph();
	if (!pGraph)
		return;

	CString filename = CString(pGraph->GetName()) + ".xml";
	CString dir = Path::GetPath(pGraph->GetFilename());
	if (CFileUtil::SelectSaveFile( GRAPH_FILE_FILTER,"xml",dir,filename ))
	{
		CWaitCursor wait;
		bool ok = pGraph->Save( filename );
		if (!ok)
		{
			Warning ("Cannot save FlowGraph to file '%s'.\nMake sure the path is correct.", filename.GetString());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileNewAction()
{
	CString filename;
	if ( GetIEditor()->GetAI()->NewAction(filename) )
	{
		IAIAction* pAction = gEnv->pAISystem->GetAIAction( PathUtil::GetFileName((const char*)filename) );
		if ( pAction )
		{
			CFlowGraphManager* pManager = GetIEditor()->GetFlowGraphManager();
			CFlowGraph* pFlowGraph = pManager->FindGraphForAction( pAction );
			assert( pFlowGraph );
			if ( pFlowGraph )
				pManager->OpenView( pFlowGraph );
		}
		else
			SetGraph( NULL );
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileImport()
{
	CHyperGraph *pGraph = m_view.GetGraph();
	if (!pGraph)
	{
		Warning ("Please first create a FlowGraph to which to import");
		return;
	}

	CString filename;
	// if (CFileUtil::SelectSingleFile( EFILE_TYPE_ANY,filename,GRAPH_FILE_FILTER )) // game dialog
	if (CFileUtil::SelectFile (GRAPH_FILE_FILTER, "", filename )) // native dialog
	{
		CWaitCursor wait;
		bool ok = m_view.GetGraph()->Import( filename );
		if (!ok)
		{
			Warning ("Cannot import file '%s'.\nMake sure the file is actually a FlowGraph XML File.", filename.GetString());
		}
		m_view.InvalidateView(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFileExportSelection()
{
	CHyperGraph *pGraph = m_view.GetGraph();
	if (!pGraph)
		return;

	std::vector<CHyperNode*> nodes;
	if (!m_view.GetSelectedNodes( nodes ))
	{
		Warning( "You must have selected graph nodes to export" );
		return;
	}

	CString filename = CString(pGraph->GetName()) + ".xml";
	CString dir = Path::GetPath(filename);
	if (CFileUtil::SelectSaveFile( GRAPH_FILE_FILTER,"xml",dir,filename ))
	{
		CWaitCursor wait;
		CHyperGraphSerializer serializer( pGraph, 0 );

		for (int i = 0; i < nodes.size(); i++)
		{
			CHyperNode *pNode = nodes[i];
			serializer.SaveNode( pNode,true );
		}
		if (nodes.size() > 0)
		{
			XmlNodeRef node = CreateXmlNode("Graph");
			serializer.Serialize( node,false );
			bool ok =	SaveXmlNode( node,filename );
			if (!ok)
			{
				Warning("FlowGraph not saved to file '%s'", filename.GetString());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnFind()
{
	CXTPDockingPane* pSearchPane = GetDockingPaneManager()->FindPane(IDW_HYPERGRAPH_SEARCH_PANE);
	assert (pSearchPane != 0);

	// show the pane
	GetDockingPaneManager()->ShowPane(pSearchPane);

	// goto combo box
	CWnd* pWnd = m_pSearchOptionsView->GetDlgItem(IDC_COMBO_FIND);
	assert (pWnd != 0);
	m_pSearchOptionsView->GotoDlgCtrl(pWnd);
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::OnComponentsViewToggle( UINT nID )
{
	switch (nID)
	{
	case ID_COMPONENTS_APPROVED:
		m_componentsViewMask ^= EFLN_APPROVED;
		break;
	case ID_COMPONENTS_ADVANCED:
		m_componentsViewMask ^= EFLN_ADVANCED;
		break;
	case ID_COMPONENTS_DEBUG:
		m_componentsViewMask ^= EFLN_DEBUG;
		break;
	//case ID_COMPONENTS_LEGACY:
	//	m_componentsViewMask ^= EFLN_LEGACY;
	//	break;
	//case ID_COMPONENTS_WIP:
	//	m_componentsViewMask ^= EFLN_WIP;
	//	break;
	//case ID_COMPONENTS_NOCATEGORY:
	//	m_componentsViewMask ^= EFLN_NOCATEGORY;
	//	break;
	case ID_COMPONENTS_OBSOLETE:
		m_componentsViewMask ^= EFLN_OBSOLETE;
		break;
	default:
		return FALSE;
	}
	UpdateComponentPaneTitle();
	m_componentsReportCtrl.SetComponentFilterMask(m_componentsViewMask);
	m_componentsReportCtrl.Reload();
	m_view.SetComponentViewMask(m_componentsViewMask);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnComponentsViewUpdate( CCmdUI *pCmdUI )
{
	uint32 flag;

	switch (pCmdUI->m_nID)
	{
	case ID_COMPONENTS_APPROVED:
		flag = EFLN_APPROVED;
		break;
	case ID_COMPONENTS_ADVANCED:
		flag = EFLN_ADVANCED;
		break;
	case ID_COMPONENTS_DEBUG:
		flag = EFLN_DEBUG;
		break;
	//case ID_COMPONENTS_LEGACY:
	//	flag = EFLN_LEGACY;
	//	break;
	//case ID_COMPONENTS_WIP:
	//	flag = EFLN_WIP;
	//	break;
	//case ID_COMPONENTS_NOCATEGORY:
	//	flag = EFLN_NOCATEGORY;
	//	break;
	case ID_COMPONENTS_OBSOLETE:
		flag = EFLN_OBSOLETE;
		break;
	default:
		return;
	}
	pCmdUI->SetCheck( (m_componentsViewMask & flag) ? 1 : 0);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::UpdateComponentPaneTitle()
{
	CString title = _T("Components");
	GetDockingPaneManager()->FindPane(IDW_HYPERGRAPH_COMPONENTS_PANE)->SetTitle(title);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::ClearGraph()
{
	/*
	CHyperGraph *pGraph = m_view.GetGraph();
	if (pGraph && pGraph->IsModified())
	{
		if (AfxMessageBox( _T("Graph was modified, Save changes?"),MB_YESNO|MB_ICONQUESTION ) == IDYES)
		{
			OnFileSave();
		}
	}
	*/
}

void CHyperGraphDialog::OnComponentBeginDrag(CXTPReportRow* pRow, CPoint pt)
{
	if (!pRow)
		return;
	CComponentRecord* pRec = static_cast<CComponentRecord*> (pRow->GetRecord());
	if (!pRec)
		return;
	if (pRec->IsGroup())
		return;

	m_pDragImage = m_componentsReportCtrl.CreateDragImage(pRow);
	if (m_pDragImage)
	{
		m_pDragImage->BeginDrag(0, CPoint(-10, -10));

		m_dragNodeClass = pRec->GetName();

		ScreenToClient(&pt);

		CRect rc;
		GetWindowRect( rc );

		CPoint p = pt;
		ClientToScreen( &p );
		p.x -= rc.left;
		p.y -= rc.top;

		m_componentsReportCtrl.RedrawControl();
		m_pDragImage->DragEnter( this,p );
		SetCapture();
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnMouseMove(UINT nFlags, CPoint point) 
{
	if (m_pDragImage)
	{
		CRect rc;
		GetWindowRect( rc );
		CPoint p = point;
		ClientToScreen( &p );
		p.x -= rc.left;
		p.y -= rc.top;
		m_pDragImage->DragMove( p );
		return;
	}

	__super::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if (m_pDragImage)
	{
		CPoint p;
		GetCursorPos( &p );

		::ReleaseCapture();
		m_pDragImage->DragLeave( this );
		m_pDragImage->EndDrag();
		m_pDragImage->DeleteImageList();
		// delete m_pDragImage; // FIXME: crashes 
		m_pDragImage = 0;

		CWnd *wnd = CWnd::WindowFromPoint( p );
		if (wnd == &m_view)
		{
			m_view.ScreenToClient(&p);
			m_view.CreateNode( m_dragNodeClass,p );
		}

		return;
	}
	__super::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnPlay()
{
	GetIEditor()->GetGameEngine()->EnableFlowSystemUpdate(true);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnStop()
{
	GetIEditor()->GetGameEngine()->EnableFlowSystemUpdate(false);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnToggleDebug()
{
	ICVar *pToggle = gEnv->pConsole->GetCVar("fg_iEnableFlowgraphNodeDebugging");
	if(pToggle)
	{
		pToggle->Set((pToggle->GetIVal()>0)?0:1);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnEraseDebug()
{
	std::list<CHyperGraph*>::iterator iter = m_graphList.begin();
	for (;iter != m_graphList.end();++iter)
	{
		CHyperGraph *pGraph = *iter;
		if(pGraph && pGraph->IsNodeActivationModified())
		{
			pGraph->ClearDebugPortActivation();

			IHyperGraphEnumerator *pEnum = pGraph->GetNodesEnumerator();
			for (IHyperNode *pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
			{
				CHyperNode *pNode = (CHyperNode*)pINode;
				pNode->Invalidate(true);
			}
			pEnum->Release();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnPause()
{
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnStep()
{
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnUpdatePlay( CCmdUI *pCmdUI )
{
	if (GetIEditor()->GetGameEngine()->IsFlowSystemUpdateEnabled())
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnUpdateStep( CCmdUI *pCmdUI )
{
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnUpdateDebug( CCmdUI *pCmdUI )
{
	ICVar *pToggle = gEnv->pConsole->GetCVar("fg_iEnableFlowgraphNodeDebugging");
	if(pToggle && pToggle->GetIVal() > 0)
		pCmdUI->SetCheck(1);
	else
		pCmdUI->SetCheck(0);
}

#define ID_GRAPHS_CHANGE_FOLDER 1
#define ID_GRAPHS_SELECT_ENTITY 2
#define ID_GRAPHS_REMOVE_GRAPH  3
#define ID_GRAPHS_ENABLE        4
#define ID_GRAPHS_ENABLE_ALL    5
#define ID_GRAPHS_DISABLE_ALL   6
#define ID_GRAPHS_RENAME_FOLDER 7

void CHyperGraphDialog::GraphUpdateEnable(CFlowGraph* pFlowGraph, HTREEITEM hItem)
{
	// m_graphsTreeCtrl.Reload();  
	// this is way too slow and deletes the current open view incl. opened folders
	// that's why we use the stuff below. the tree ctrl from above needs
	// a major revamp.
	CEntity *pOwnerEntity = pFlowGraph->GetEntity();
	if (pOwnerEntity !=0) {
		int img = pFlowGraph->IsEnabled() ? 0 : 4;
		m_graphsTreeCtrl.SetItemImage(hItem, img, img);
	} else if (pFlowGraph->GetAIAction() == 0) {
		int img = pFlowGraph->IsEnabled() ? 3 : 5;
		m_graphsTreeCtrl.SetItemImage(hItem, img, img);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnGraphsRightClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = TRUE;

	CPoint point;
	GetCursorPos(&point);
	m_graphsTreeCtrl.ScreenToClient(&point);

	CEntity *pOwnerEntity = 0;
	CHyperGraph *pGraph = 0;
	CFlowGraph *pFlowGraph = 0;
	UINT uFlags;
	HTREEITEM hItem = m_graphsTreeCtrl.HitTest( point,&uFlags );

	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		m_graphsTreeCtrl.Select( hItem,TVGN_CARET );
		pGraph = (CHyperGraph*)m_graphsTreeCtrl.GetItemData(hItem);
		if (pGraph)
			pFlowGraph = pGraph->IsFlowGraph() ? static_cast<CFlowGraph*> (pGraph) : 0;
		CString currentGroupName = m_graphsTreeCtrl.GetItemText(hItem);

		CMenu menu;
		menu.CreatePopupMenu();

		if (pFlowGraph)
		{
			pOwnerEntity = pFlowGraph->GetEntity();
			if (pFlowGraph->GetAIAction() == 0)
			{
				if (!pGraph->IsEnabled())
					menu.AppendMenu( MF_STRING,ID_GRAPHS_ENABLE,"Enable" );
				else
					menu.AppendMenu( MF_STRING,ID_GRAPHS_ENABLE,"Disable" );
			}
			menu.AppendMenu( MF_STRING,ID_GRAPHS_CHANGE_FOLDER,"Change Folder" );

			if (pOwnerEntity)
			{
				menu.AppendMenu( MF_STRING,ID_GRAPHS_SELECT_ENTITY,"Select Entity" );
				menu.AppendMenu( MF_SEPARATOR );
				menu.AppendMenu( MF_STRING,ID_GRAPHS_REMOVE_GRAPH,"Remove Graph" );
			}
		}
		else
		{
			HTREEITEM hParentItem = m_graphsTreeCtrl.GetParentItem(hItem);
			if (hParentItem != 0 && currentGroupName != g_AIActionsFolderName) // currently only for entity flowgraphs
				menu.AppendMenu(MF_STRING,ID_GRAPHS_RENAME_FOLDER,"RenameFolder/MoveGraphs");
			if (currentGroupName == g_EntitiesFolderName || 
				(hParentItem != 0 && m_graphsTreeCtrl.GetItemText(hParentItem) == g_EntitiesFolderName))
			{
				menu.AppendMenu(MF_STRING,ID_GRAPHS_ENABLE_ALL,"Enable All");
				menu.AppendMenu(MF_STRING,ID_GRAPHS_DISABLE_ALL,"Disable All");
			}
			//menu.AppendMenu( MF_STRING,IDC_NEW,"New Flow Graph" );
		}
		
		CPoint screenPoint;
		GetCursorPos(&screenPoint);

		int cmd = menu.TrackPopupMenu( TPM_RETURNCMD|TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_NONOTIFY,screenPoint.x,screenPoint.y,this );
		switch (cmd)
		{
		case ID_GRAPHS_ENABLE_ALL:
		case ID_GRAPHS_DISABLE_ALL:
			{
				bool enabled (cmd == ID_GRAPHS_ENABLE_ALL);
				m_graphsTreeCtrl.SetRedraw(FALSE);
				EnableItems(hItem, enabled, true);
				m_graphsTreeCtrl.SetRedraw(TRUE);
			}
			break;
		case ID_GRAPHS_RENAME_FOLDER:
			{
				bool bIsAction = false;
				CFlowGraphManager* pFlowGraphManager = GetIEditor()->GetFlowGraphManager();
				std::set<CString> groupsSet;
				pFlowGraphManager->GetAvailableGroups(groupsSet, bIsAction);

				CString groupName;
				bool bDoNew  = true;
				bool bChange = false;

				if (groupsSet.size() > 0)
				{
					std::vector<CString> groups (groupsSet.begin(), groupsSet.end());

					CGenericSelectItemDialog gtDlg(this);
					if (bIsAction == false)
						gtDlg.SetTitle(_T("Choose New Group for the Flow Graph"));
					else
						gtDlg.SetTitle(_T("Choose Group for the AIAction Graph"));

					gtDlg.PreSelectItem(currentGroupName);
					gtDlg.SetItems(groups);
					gtDlg.AllowNew(true);
					switch (gtDlg.DoModal())
					{
					case IDOK:
						groupName = gtDlg.GetSelectedItem();
						bChange = true;
						bDoNew  = false;
						break;
					case IDNEW:
						// bDoNew  = true; // is true anyway
						break;
					default:
						bDoNew = false;
						break;
					}
				}

				if (bDoNew)
				{
					CStringDlg dlg ( bIsAction == false ?
						_T("Enter Group Name for the Flow Graph") :
					_T("Enter Group Name for the AIAction Graph") );
					dlg.SetString(currentGroupName);
					if (dlg.DoModal() == IDOK)
					{
						bChange = true;
						groupName = dlg.GetString();
					}
				}

				if (bChange && groupName != currentGroupName)
				{
					m_graphsTreeCtrl.SetIgnoreReloads(true);
					m_graphsTreeCtrl.SetRedraw(FALSE);
					RenameItems(hItem, groupName, true);
					m_graphsTreeCtrl.SetRedraw(TRUE);
					m_graphsTreeCtrl.SetIgnoreReloads(false);
					m_graphsTreeCtrl.Reload();
				}

			}
			break;

		case ID_GRAPHS_SELECT_ENTITY:
			{
				if (pOwnerEntity)
				{
					CUndo undo( "Select Object(s)" );
					GetIEditor()->ClearSelection();
					GetIEditor()->SelectObject( pOwnerEntity );
				}
				/*
				CStringDlg dlg( "Rename Graph" );
				dlg.SetString( pGraph->GetName() );
				if (dlg.DoModal() == IDOK)
				{
					pGraph->SetName( dlg.GetString() );
					m_graphsTreeCtrl.SetItemText( hItem,dlg.GetString() );
				}
				*/
			}
			break;
		case ID_GRAPHS_REMOVE_GRAPH:
			{
				if (pOwnerEntity)
				{
					CUndo undo( "Remove Entity Graph" );

					CString str;
					str.Format( "Remove Flow Graph for Entity %s",(const char*)pOwnerEntity->GetName() );
					if (MessageBox( str,"Confirm",MB_OKCANCEL ) == IDOK)
					{
						pOwnerEntity->RemoveFlowGraph();
						m_graphsTreeCtrl.Reload();
						SetGraph(0);
					}
				}
			}
			break;
		case ID_GRAPHS_ENABLE:
			{
				if (pFlowGraph)
				{
					// Toggle enabled of graph.
					pFlowGraph->SetEnabled( !pGraph->IsEnabled() );
					// GraphUpdateEnable(pFlowGraph, hItem);
					UpdateGraphProperties(pGraph);
					m_graphsTreeCtrl.SetRedraw(TRUE);
				}
			}
			break;
		case ID_GRAPHS_CHANGE_FOLDER:
			{
				if (pFlowGraph)
				{
					/*
					CStringDlg dlg( "Change Graph Folder" );
					dlg.SetString( pGraph->GetGroupName() );
					if (dlg.DoModal() == IDOK)
					{
						pGraph->SetGroupName( dlg.GetString() );
						m_graphsTreeCtrl.Reload();
					}
					*/

					bool bIsAction = pFlowGraph->GetAIAction();
					CFlowGraphManager* pFlowGraphManager = (CFlowGraphManager*) pFlowGraph->GetManager();
					std::set<CString> groupsSet;
					pFlowGraphManager->GetAvailableGroups(groupsSet, bIsAction);

					CString groupName;
					bool bDoNew  = true;
					bool bChange = false;

					if (groupsSet.size() > 0)
					{
						std::vector<CString> groups (groupsSet.begin(), groupsSet.end());

						CGenericSelectItemDialog gtDlg(this);
						if (bIsAction == false)
							gtDlg.SetTitle(_T("Choose Group for the Flow Graph"));
						else
							gtDlg.SetTitle(_T("Choose Group for the AIAction Graph"));

						gtDlg.PreSelectItem(pFlowGraph->GetGroupName());
						gtDlg.SetItems(groups);
						gtDlg.AllowNew(true);
						switch (gtDlg.DoModal())
						{
						case IDOK:
							groupName = gtDlg.GetSelectedItem();
							bChange = true;
							bDoNew  = false;
							break;
						case IDNEW:
							// bDoNew  = true; // is true anyway
							break;
						default:
							bDoNew = false;
							break;
						}
					}

					if (bDoNew)
					{
						CStringDlg dlg ( bIsAction == false ?
							_T("Enter Group Name for the Flow Graph") :
							_T("Enter Group Name for the AIAction Graph") );
						dlg.SetString(pFlowGraph->GetGroupName());
						if (dlg.DoModal() == IDOK)
						{
							bChange = true;
							groupName = dlg.GetString();
						}
					}

					if (bChange && groupName != pGraph->GetGroupName())
					{
						pGraph->SetGroupName( groupName );
						m_graphsTreeCtrl.Reload();
					}
				}
			}
			break;
		case IDC_NEW:
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::EnableItems(HTREEITEM hItem, bool bEnable, bool bRecurse)
{
	HTREEITEM hChildItem = m_graphsTreeCtrl.GetChildItem(hItem);
	while (hChildItem != NULL)
	{
		if (m_graphsTreeCtrl.ItemHasChildren(hChildItem))
			EnableItems(hChildItem, bEnable, bRecurse);

		CHyperGraph* pGraph = (CHyperGraph*)m_graphsTreeCtrl.GetItemData(hChildItem);
		if (pGraph)
		{
			CFlowGraph* pFlowGraph = pGraph->IsFlowGraph() ? static_cast<CFlowGraph*> (pGraph) : 0;
			assert (pFlowGraph);
			if (pFlowGraph)
			{
				pFlowGraph->SetEnabled(bEnable);
				UpdateGraphProperties(pFlowGraph);
				// GraphUpdateEnable(pFlowGraph, hChildItem);
			}
		}
		hChildItem = m_graphsTreeCtrl.GetNextItem(hChildItem, TVGN_NEXT);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::RenameItems(HTREEITEM hItem, CString& newGroupName, bool bRecurse)
{
	HTREEITEM hChildItem = m_graphsTreeCtrl.GetChildItem(hItem);
	while (hChildItem != NULL)
	{
		if (m_graphsTreeCtrl.ItemHasChildren(hChildItem))
			RenameItems(hChildItem, newGroupName, bRecurse);

		CHyperGraph* pGraph = (CHyperGraph*)m_graphsTreeCtrl.GetItemData(hChildItem);
		if (pGraph)
		{
			CFlowGraph* pFlowGraph = pGraph->IsFlowGraph() ? static_cast<CFlowGraph*> (pGraph) : 0;
			assert (pFlowGraph);
			if (pFlowGraph)
			{
				pFlowGraph->SetGroupName(newGroupName);
				UpdateGraphProperties(pFlowGraph);
				// GraphUpdateEnable(pFlowGraph, hChildItem);
			}
		}
		hChildItem = m_graphsTreeCtrl.GetNextItem(hChildItem, TVGN_NEXT);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnGraphsDblClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = TRUE;

	CPoint point;
	GetCursorPos(&point);
	m_graphsTreeCtrl.ScreenToClient(&point);

	CEntity *pOwnerEntity = 0;
	CHyperGraph *pGraph = 0;

	UINT uFlags;
	HTREEITEM hItem = m_graphsTreeCtrl.HitTest( point,&uFlags );
	if ((hItem != NULL) && (TVHT_ONITEM & uFlags))
	{
		pGraph = (CHyperGraph*)m_graphsTreeCtrl.GetItemData(hItem);
		if (pGraph)
		{
			pOwnerEntity = ((CFlowGraph*)pGraph)->GetEntity();
		}
		if (pOwnerEntity)
		{
			CUndo undo( "Select Object(s)" );
			GetIEditor()->ClearSelection();
			GetIEditor()->SelectObject( pOwnerEntity );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnGraphsSelChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = TRUE;
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
	CHyperGraph *pGraph = (CHyperGraph*)pNMTreeView->itemNew.lParam;
	if (pGraph)
	{
		SetGraph( pGraph );
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::OnToggleBar( UINT nID )
{
	switch (nID)
	{
	case ID_VIEW_COMPONENTS:
		nID = IDW_HYPERGRAPH_COMPONENTS_PANE;
		break;
	case ID_VIEW_NODEINPUTS:
		nID = IDW_HYPERGRAPH_RIGHT_PANE;
		break;
	case ID_VIEW_GRAPHS:
		nID = IDW_HYPERGRAPH_TREE_PANE;
		break;
	case ID_VIEW_SEARCHRESULTS:
		nID = IDW_HYPERGRAPH_SEARCHRESULTS_PANE;
		break;
	default:
		return FALSE;
	}
	CXTPDockingPane *pBar = GetDockingPaneManager()->FindPane(nID);
	if (pBar)
	{
		if (pBar->IsClosed())
			GetDockingPaneManager()->ShowPane(pBar);
		else
			GetDockingPaneManager()->ClosePane(pBar);
		return TRUE;
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphDialog::OnMultiPlayerViewToggle( UINT nID )
{
	if (m_pGraphProperties)
		m_pGraphProperties->ShowMultiPlayer(!m_pGraphProperties->IsShowMultiPlayer());
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnMultiPlayerViewUpdate(CCmdUI* pCmdUI)
{
	if (m_pGraphProperties)
		pCmdUI->SetCheck(m_pGraphProperties->IsShowMultiPlayer());
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnUpdateControlBar(CCmdUI* pCmdUI)
{
	UINT nID = 0;
	switch (pCmdUI->m_nID)
	{
	case ID_VIEW_COMPONENTS:
		nID = IDW_HYPERGRAPH_COMPONENTS_PANE;
		break;
	case ID_VIEW_NODEINPUTS:
		nID = IDW_HYPERGRAPH_RIGHT_PANE;
		break;
	case ID_VIEW_GRAPHS:
		nID = IDW_HYPERGRAPH_TREE_PANE;
		break;
	case ID_VIEW_SEARCHRESULTS:
		nID = IDW_HYPERGRAPH_SEARCHRESULTS_PANE;
		break;
	default:
		return;
	}
	CXTPCommandBars* pCommandBars = GetCommandBars();
	if(pCommandBars != NULL)
	{
		CXTPToolBar *pBar = pCommandBars->GetToolBar(nID);
		if (pBar)
		{
			pCmdUI->SetCheck( (pBar->IsVisible()) ? 1 : 0 );
			return;
		}
	}
	CXTPDockingPane *pBar = GetDockingPaneManager()->FindPane(nID);
	if (pBar)
	{
		pCmdUI->SetCheck( (!pBar->IsClosed()) ? 1 : 0 );
		return;
	}
	pCmdUI->SetCheck(0);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::ShowResultsPane(bool bShow, bool bFocus)
{
	if (bShow) {
		CXTPDockingPane *pPane = GetDockingPaneManager()->FindPane(IDW_HYPERGRAPH_SEARCHRESULTS_PANE);
		assert (pPane != 0);
		GetDockingPaneManager()->ShowPane(pPane);
		if (bFocus)
			pPane->SetFocus();
	}
	else
		GetDockingPaneManager()->HidePane(IDW_HYPERGRAPH_SEARCHRESULTS_PANE);
}

//////////////////////////////////////////////////////////////////////////
// Called by the editor to notify the listener about the specified event.
void CHyperGraphDialog::OnEditorNotifyEvent( EEditorNotifyEvent event )
{
	switch ( event )
	{
	case eNotify_OnBeginSceneOpen:          // Sent when document is about to be opened.
		m_graphsTreeCtrl.SetIgnoreReloads(true);
		break;

	case eNotify_OnEndSceneOpen:            // Sent after document have been opened.
		m_graphsTreeCtrl.SetIgnoreReloads(false);
		m_graphsTreeCtrl.Reload();
		break;
	}
}

void CHyperGraphDialog::UpdateGraphProperties(CHyperGraph* pGraph)
{
	// TODO: real listener concept
	m_pGraphProperties->UpdateGraphProperties(pGraph);
	// TODO: real item update, not just this enable stuff
	if (pGraph->IsFlowGraph())
	{
		CFlowGraph* pFG = static_cast<CFlowGraph*> (pGraph);
		HTREEITEM hItem = m_graphsTreeCtrl.GetTreeItem(pFG);
		if (hItem)
		{
			GraphUpdateEnable(pFG, hItem);
		}
	}
}

void CHyperGraphDialog::UpdateTitle(CHyperGraph* pGraph)
{
	CString titleFormatter;
	if (pGraph)
	{
		titleFormatter += pGraph->GetFilename();
	}
	else
	{
		;
	}
	m_statusBar.SetPaneText(0, titleFormatter );
}

//////////////////////////////////////////////////////////////////////////
int CHyperGraphDialog::OnCreateCommandBar(LPCREATEBARSTRUCT lpCreatePopup)
{
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
int CHyperGraphDialog::OnCreateControl(LPCREATECONTROLSTRUCT lpCreateControl)
{
	CXTPToolBar* pToolBar = lpCreateControl->bToolBar? DYNAMIC_DOWNCAST(CXTPToolBar, lpCreateControl->pCommandBar): NULL;

	if ((lpCreateControl->nID == ID_FG_BACK || lpCreateControl->nID == ID_FG_FORWARD) && pToolBar)
	{
		CXTPControlPopup* pButtonNav = CXTPControlPopup::CreateControlPopup(xtpControlSplitButtonPopup);

		CXTPPopupToolBar* pNavBar = CXTPPopupToolBar::CreatePopupToolBar(GetCommandBars());
		pNavBar->EnableCustomization(FALSE);
		pNavBar->SetBorders(CRect(2, 2, 2, 2));
		pNavBar->DisableShadow();

		CXTPControlListBox* pControlListBox = (CXTPControlListBox*)pNavBar->GetControls()->Add(new CXTPControlListBox(), lpCreateControl->nID);
		pControlListBox->SetWidth(10);
		//pControlListBox->SetLinesMinMax(1, 6);
		pControlListBox->SetMultiplSel(FALSE);

		// CXTPControlStatic* pControlListBoxInfo = (CXTPControlStatic*)pUndoBar->GetControls()->Add(new CXTPControlStatic(), lpCreateControl->nID);
		// pControlListBoxInfo->SetWidth(140);


		pButtonNav->SetCommandBar(pNavBar);
		pNavBar->InternalRelease();

		lpCreateControl->pControl = pButtonNav;
		return TRUE;
	}

	return FALSE;
}

CXTPControlStatic* FindInfoControl(CXTPControl* pControl)
{
	CXTPCommandBar* pCommandBar = pControl->GetParent();

	for (int i = 0; i < pCommandBar->GetControls()->GetCount(); i++)
	{
		CXTPControlStatic* pControlStatic = DYNAMIC_DOWNCAST(CXTPControlStatic, pCommandBar->GetControl(i));
		if (pControlStatic && pControlStatic->GetID() == pControl->GetID())
		{
			return pControlStatic;
		}

	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnNavListBoxControlSelChange(NMHDR* pNMHDR, LRESULT* pRes)
{
	ASSERT(pNMHDR != NULL);
	ASSERT(pRes != NULL);

	CXTPControlListBox* pControlListBox = DYNAMIC_DOWNCAST(CXTPControlListBox, ((NMXTPCONTROL*)pNMHDR)->pControl);
	if (pControlListBox)
	{
		CListBox* pListBox = pControlListBox->GetListCtrl();
		int index = pListBox->GetCurSel();
		m_pNavGraph = index == LB_ERR ? 0 : reinterpret_cast<CHyperGraph*> (pListBox->GetItemDataPtr(index));

		/*
		CXTPControlStatic* pInfo = FindInfoControl(pControlListBox);
		if (pInfo)
		{
			CString str;
			str.Format(_T("Undo %i Actions"), pControlListBox->GetListCtrl()->GetSelCount());
			pInfo->SetCaption(str);
			pInfo->DelayRedrawParent();
		}
		*/

		*pRes = 1;
	}
}

int CalcWidth(CListBox* pListBox)
{
	int nMaxTextWidth = 0;
	int nScrollWidth = 0;

#if 0
	SCROLLINFO scrollInfo;
	memset(&scrollInfo, 0, sizeof(SCROLLINFO));
	scrollInfo.cbSize = sizeof(SCROLLINFO);
	scrollInfo.fMask  = SIF_ALL;
	pListBox->GetScrollInfo(SB_VERT, &scrollInfo, SIF_ALL);

	if(pListBox->GetCount() > 1 && ((int)scrollInfo.nMax >= (int)scrollInfo.nPage))
	{
		nScrollWidth = GetSystemMetrics(SM_CXVSCROLL);
	}
#endif

	CDC *pDC = pListBox->GetDC();
	if (pDC)
	{
		CFont *pOldFont = pDC->SelectObject(pListBox->GetFont());
		CString Text;
		const int nItems = pListBox->GetCount();
		for (int i = 0; i < nItems; ++i)
		{
			pListBox->GetText(i, Text);
			const int nTextWidth = pDC->GetTextExtent(Text).cx;
			if (nTextWidth > nMaxTextWidth)
				nMaxTextWidth = nTextWidth;
		}
		pDC->SelectObject(pOldFont);
		VERIFY(pListBox->ReleaseDC(pDC) != 0);
	}
	else
	{
		ASSERT(FALSE);
	}
	return nMaxTextWidth+nScrollWidth;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnNavListBoxControlPopup(NMHDR* pNMHDR, LRESULT* pRes)
{
	ASSERT(pNMHDR != NULL);
	ASSERT(pRes != NULL);
	CXTPControlListBox* pControlListBox = DYNAMIC_DOWNCAST(CXTPControlListBox, ((NMXTPCONTROL*)pNMHDR)->pControl);
	if (pControlListBox)
	{
		m_pNavGraph = 0;
		CListBox* pListBox = pControlListBox->GetListCtrl();
		pListBox->ResetContent();

		bool forward = pControlListBox->GetID() == ID_FG_FORWARD;
		CString name;
		CString groupName;

		if (forward)
		{
			std::list<CHyperGraph*>::iterator iterStart;
			std::list<CHyperGraph*>::iterator iterEnd;
			iterStart = m_graphIterCur;
			if (iterStart != m_graphList.end())
				++iterStart;
			iterEnd = m_graphList.end();
			while (iterStart != iterEnd)
			{
				GetPrettyName(*iterStart, name, groupName);
				groupName+=":";
				groupName+=name;
				int index = pListBox->AddString(groupName);
				pListBox->SetItemDataPtr(index, (*iterStart));
				++iterStart;
			}
		}
		else
		{
			std::list<CHyperGraph*>::iterator iterStart;
			std::list<CHyperGraph*>::iterator iterEnd;
			iterStart = m_graphIterCur;
			if (iterStart != m_graphList.begin())
			{
				iterEnd = m_graphList.begin();
				do 
				{
					--iterStart;
					GetPrettyName(*iterStart, name, groupName);
					groupName+=":";
					groupName+=name;
					int index = pListBox->AddString(groupName);
					pListBox->SetItemDataPtr(index, (*iterStart));
				}
				while (iterStart != iterEnd);
			}
		}

		int width = CalcWidth(pListBox)+4;
		pListBox->SetHorizontalExtent(width);
		pControlListBox->SetWidth(width);

		CXTPControlStatic* pInfo = FindInfoControl(pControlListBox);
		if (pInfo)
		{
			CString str;
			pInfo->SetCaption(_T("Undo 0 Actions"));
			pInfo->DelayRedrawParent();
		}

		*pRes = 1;
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnUpdateNav(CCmdUI* pCmdUI)
{
	BOOL enable = FALSE;

	switch (pCmdUI->m_nID)
	{
	case ID_FG_BACK:
		enable = m_graphList.size() > 0 && m_graphIterCur != m_graphList.begin();
		break;
	case ID_FG_FORWARD:
		enable = m_graphList.size() > 0 && m_graphIterCur != --m_graphList.end() && m_graphIterCur != m_graphList.end();
		break;
	default:
		// don't do anything
		return;
	}
	pCmdUI->Enable(enable);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnNavForward()
{
	if (m_pNavGraph != 0)
	{
		SetGraph(m_pNavGraph);
		m_pNavGraph = 0;
	}
	else if (m_graphList.size() > 0 && m_graphIterCur != -- m_graphList.end() && m_graphIterCur != m_graphList.end())
	{
		std::list<CHyperGraph*>::iterator iter = m_graphIterCur;
		++iter;
		CHyperGraph* pGraph = *(iter);
		SetGraph(pGraph);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnNavBack()
{
	if (m_pNavGraph != 0)
	{
		SetGraph(m_pNavGraph);
		m_pNavGraph = 0;
	}
	else if (m_graphList.size() > 0 && m_graphIterCur != m_graphList.begin())
	{
		std::list<CHyperGraph*>::iterator iter = m_graphIterCur;
		--iter;
		CHyperGraph* pGraph = *(iter);
		SetGraph(pGraph);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::UpdateNavHistory()
{
	// early return, when history is empty anyway [loading...]
	if (m_graphList.empty())
		return;

	CFlowGraphManager* pMgr = GetIEditor()->GetFlowGraphManager();
	const int fgCount = pMgr->GetFlowGraphCount();
	std::set<CHyperGraph*> graphs;
	for (int i=0; i<fgCount; ++i)
	{
		graphs.insert(pMgr->GetFlowGraph(i));
	}

	// validate history
	bool adjustCur = false;
	std::list<CHyperGraph*>::iterator iter = m_graphList.begin();
	while (iter != m_graphList.end())
	{
		CHyperGraph* pGraph = *iter;
		if (graphs.find(pGraph) == graphs.end())
		{
			// remove graph from history
			std::list<CHyperGraph*>::iterator toDelete = iter;
			++iter;
			if (m_graphIterCur == toDelete)
				adjustCur = true;

			m_graphList.erase(toDelete);
		}
		else
		{
			++iter;
		}
	}

	// adjust current iterator in case the current graph got deleted
	if (adjustCur)
	{
		m_graphIterCur = m_graphList.end();
	}
}


//////////////////////////////////////////////////////////////////////////
void CHyperGraphDialog::OnHyperGraphManagerEvent( EHyperGraphEvent event )
{
	switch (event)
	{
		case EHG_GRAPH_REMOVED:
			UpdateNavHistory();
			break;
	}
}
