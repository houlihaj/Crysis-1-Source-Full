#include "stdafx.h"
#include "AnimationGraphTester.h"
#include "AnimationGraphDialog.h"
#include "IEntitySystem.h"
#include "IScriptSystem.h"

#define ID_SELECTENTITY 51
#define ID_ADDFORCEDSTATE 52
#define ID_CLEARFORCEDSTATES 53

namespace
{

	int ShowQuickPopup( const std::vector<CString>& elems, CWnd * pWnd )
	{
		CMenu menu;
		menu.CreatePopupMenu();
		for (std::vector<CString>::const_iterator iter = elems.begin(); iter != elems.end(); ++iter)
			menu.AppendMenu( MF_STRING, iter - elems.begin() + 1, *iter );

		CPoint p;
		::GetCursorPos(&p);
		return menu.TrackPopupMenuEx( TPM_RETURNCMD|TPM_LEFTBUTTON|TPM_LEFTALIGN, p.x, p.y, pWnd, NULL ) - 1;
	}

}

void CAnimationGraphTester::Init( CAnimationGraphDialog * pParent )
{
	m_pParent = pParent;
	m_pGroup = 0;

	CRect rc(0,0,350,500);
	Create(WS_CHILD|/*WS_VISIBLE|*/WS_CLIPSIBLINGS|WS_CLIPCHILDREN, rc, pParent, 0);
	SetBehaviour(xtpTaskPanelBehaviourExplorer);
	SetTheme(xtpTaskPanelThemeNativeWinXP);
	SetSelectItemOnFocus(TRUE);
	AllowDrag(FALSE);
	SetAnimation( xtpTaskPanelAnimationNo );
	GetPaintManager()->m_rcGroupOuterMargins.SetRect(0,0,0,0);
	GetPaintManager()->m_rcGroupInnerMargins.SetRect(0,0,0,0);
	GetPaintManager()->m_rcItemOuterMargins.SetRect(0,0,0,0);
	GetPaintManager()->m_rcItemInnerMargins.SetRect(1,1,1,1);
	GetPaintManager()->m_rcControlMargins.SetRect(2,0,2,0);
	GetPaintManager()->m_nGroupSpacing = 0;

//	SetOwner(pParent);
}

void CAnimationGraphTester::Reload()
{
	if (m_pGroup)
	{
		m_pGroup->Remove();
		m_pGroup = 0;
	}

	CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph();
	if (!pGraph)
		return;

	m_pGroup = AddGroup(1);
	m_pGroup->SetCaption( "Testing" );
	m_pGroup->SetItemLayout(xtpTaskItemLayoutDefault);

	AddVerb( "Select entity", ID_SELECTENTITY );
	AddVerb( "Queue forced state", ID_ADDFORCEDSTATE );
//	AddVerb( "Clear forced states", ID_CLEARFORCEDSTATES );
}

void CAnimationGraphTester::AddVerb( CString name, int id )
{
	CXTPTaskPanelGroupItem * pGI = m_pGroup->AddLinkItem( id );
	pGI->SetType( xtpTaskItemTypeLink );
	pGI->SetCaption( name );
}

void CAnimationGraphTester::OnCommand( int cmd )
{
	switch (cmd)
	{
	case ID_SELECTENTITY:
		{
			std::vector<CString> ents;
			IEntityItPtr pEI = gEnv->pEntitySystem->GetEntityIterator();
			while (!pEI->IsEnd())
			{
				IEntity * pEnt = pEI->Next();
				if (!pEnt)
					continue;
				SmartScriptTable pTbl = pEnt->GetScriptTable();
				if (!pTbl)
					continue;
				if (!pTbl->HaveValue("actor"))
					continue;
				ents.push_back(pEnt->GetName());
			}
			std::sort(ents.begin(), ents.end());
			int i = ShowQuickPopup(ents, this);
			if (i >= 0)
			{
				ICVar * pVar = gEnv->pConsole->GetCVar("ag_debug");
				if (pVar)
					pVar->Set((const char *)ents[i]);
			}
		}
		break;
	case ID_ADDFORCEDSTATE:
		{
			std::vector<CString> states;
			CAnimationGraphPtr pGraph = m_pParent->GetAnimationGraph();
			for (CAnimationGraph::state_iterator iter = pGraph->StateBegin(); iter != pGraph->StateEnd(); ++iter)
				states.push_back( (*iter)->GetName() );
			std::sort(states.begin(), states.end());
			int i = ShowQuickPopup(states, this);
			if (i >= 0)
			{
				ICVar * pVar = gEnv->pConsole->GetCVar("ag_queue");
				if (pVar)
					pVar->Set((const char *)states[i]);
			}
		}
		break;
	}
}
