////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   FlowGraphProperties.cpp
//  Version:     v1.00
//  Created:     14/06/2006 by AlexL.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////


#include "StdAfx.h"
#include "FlowGraphProperties.h"
#include "HyperGraphDialog.h"
#include "FlowGraph.h"

#define IDC_GRAPH_DESCRIPTION 5
#define IDC_GRAPH_PROPERTIES 6

IMPLEMENT_DYNAMIC( CFlowGraphProperties,CWnd )

CFlowGraphProperties::CFlowGraphProperties(CHyperGraphDialog* pParent, CXTPTaskPanel* pPanel) 
:	m_pParent(pParent),
	m_pTaskPanel(pPanel),
	m_pGraph(0),
	m_bUpdate(false),
	m_bShowMP(false)
{
}

CFlowGraphProperties::~CFlowGraphProperties()
{
}

BEGIN_MESSAGE_MAP(CFlowGraphProperties, CWnd)
	ON_EN_CHANGE( IDC_GRAPH_DESCRIPTION,OnGraphDescriptionChange )
	ON_WM_DESTROY ()
END_MESSAGE_MAP()

BOOL CFlowGraphProperties::Create( DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID )
{
	__super::Create(NULL, NULL, dwStyle, rc, pParentWnd, nID);
	Init(nID);
	return TRUE;
}

void CFlowGraphProperties::OnDestroy()
{
	CXTRegistryManager regMgr;
	regMgr.WriteProfileInt(_T("FlowGraph"), _T("ShowMP"), m_bShowMP ? 1 : 0);
	__super::OnDestroy();
}

void CFlowGraphProperties::Init(int nID)
{
	m_pGroup = m_pTaskPanel->AddGroup(nID);
	m_pGroup->SetCaption(_T("Graph Properties"));
	m_pGroup->SetItemLayout(xtpTaskItemLayoutDefault);

	m_graphProps.Create( WS_CHILD|WS_VISIBLE, CRect(0,0,100,100), this, IDC_GRAPH_PROPERTIES);
	m_graphProps.ModifyStyleEx( 0,WS_EX_CLIENTEDGE );
	m_graphProps.SetParent(m_pTaskPanel);
	m_graphProps.SetItemHeight(16);

	m_graphDescriptionEdit.Create( WS_CHILD|WS_VISIBLE|WS_BORDER|ES_MULTILINE,CRect(0,0,100,100),this,IDC_GRAPH_DESCRIPTION );
	m_graphDescriptionEdit.SetFont( CFont::FromHandle((HFONT)gSettings.gui.hSystemFont) );
	m_graphDescriptionEdit.SetTextColor(RGB(0,0,200) );
	m_graphDescriptionEdit.SetParent(m_pTaskPanel);

	m_pPropItem = m_pGroup->AddControlItem(m_graphProps.GetSafeHwnd());
	m_pGroup->AddTextItem(_T("Description:"));
	m_pDescItem = m_pGroup->AddControlItem(m_graphDescriptionEdit.GetSafeHwnd());

	m_varEnabled->SetDescription(_T("Enable/Disable the FlowGraph"));

	m_varMultiPlayer->SetDescription(_T("MultiPlayer Option of the FlowGraph (Default: ServerOnly)"));
	m_varMultiPlayer->AddEnumItem("ServerOnly", (int) CFlowGraph::eMPT_ServerOnly);
	m_varMultiPlayer->AddEnumItem("ClientOnly", (int) CFlowGraph::eMPT_ClientOnly);
	// m_varMultiPlayer->AddEnumItem("ClientServer", (int) CFlowGraph::eMPT_ClientServer);

	CXTRegistryManager regMgr;
	int showMP = regMgr.GetProfileInt(_T("FlowGraph"), _T("ShowMP"), 0);
	m_bShowMP = showMP != 0;

	ResizeProps(true);
	m_pTaskPanel->ExpandGroup(m_pGroup, TRUE);
}

void CFlowGraphProperties::SetGraph(CHyperGraph* pGraph)
{
	if (pGraph && pGraph->IsFlowGraph())
	{
		m_pGraph = static_cast<CFlowGraph*> (pGraph);
	}
	else 
		m_pGraph = 0;

	if (m_pGraph)
	{
		FillProps();
		CString text = m_pGraph->GetDescription();
		text.Replace( "\\n","\r\n" );
		m_graphDescriptionEdit.SetWindowText( text );
	} else {
		m_graphProps.RemoveAllItems();
		m_graphDescriptionEdit.SetWindowText ("");
		ResizeProps(true);
	}
}

void CFlowGraphProperties::FillProps()
{
	if (m_pGraph == 0)
		return;

	m_graphProps.EnableUpdateCallback(false);
	m_varEnabled = m_pGraph->IsEnabled();
	m_varMultiPlayer = m_pGraph->GetMPType();
	m_graphProps.EnableUpdateCallback(true);

	CVarBlockPtr pVB = new CVarBlock;
	if (m_pGraph->GetAIAction() == 0)
	{
		pVB->AddVariable( m_varEnabled, "Enabled" );
		pVB->AddVariable( m_varMultiPlayer, "MultiPlayer Options");
		int flags = m_varMultiPlayer->GetFlags();
		if (m_bShowMP)
			flags &= ~IVariable::UI_DISABLED;
		else
			flags |=  IVariable::UI_DISABLED;
		m_varMultiPlayer->SetFlags(flags);
	}
	else
	{
		;
	}
	m_graphProps.RemoveAllItems();
	m_graphProps.AddVarBlock(pVB);
	m_graphProps.SetUpdateCallback( functor(*this,&CFlowGraphProperties::OnVarChange) );

	ResizeProps(pVB->IsEmpty());
}

void CFlowGraphProperties::ResizeProps(bool bHide)
{
	// m_pTaskPanel->ExpandGroup( m_pGroup,FALSE);
	CRect rc;
	m_pTaskPanel->GetClientRect(rc);
	// Resize to fit properties.
	int h = m_graphProps.GetVisibleHeight();
	m_graphProps.SetWindowPos( NULL,0,0,rc.right,h + 1 + 4,SWP_NOMOVE );
	m_pPropItem->SetControlHandle(m_graphProps.GetSafeHwnd());
	m_pPropItem->SetVisible(!bHide);
	m_pTaskPanel->Reposition(false);
	// m_pTaskPanel->ExpandGroup(m_pGroup, TRUE);
}

void CFlowGraphProperties::OnGraphDescriptionChange()
{
	CHyperGraph *pGraph = m_pGraph;
	if (!pGraph)
		return;

	CString str;
	m_graphDescriptionEdit.GetWindowText(str);

	str.Replace( "\r","" );
	str.Replace( "\n","\\n" );

	pGraph->SetDescription(str);
	pGraph->SetModified(true);
}

void CFlowGraphProperties::OnVarChange( IVariable *pVar )
{
	assert (m_pGraph != 0);
#if 0
	CString val;
	pVar->Get(val);
	CryLogAlways("Var %s changed to %s", pVar->GetName(), val.GetString());
#endif
	m_pGraph->SetEnabled(m_varEnabled);
	m_pGraph->SetMPType((CFlowGraph::EMultiPlayerType) (int) m_varMultiPlayer);

	m_bUpdate = true;
	m_pParent->UpdateGraphProperties(m_pGraph);
	m_bUpdate = false;
}

void CFlowGraphProperties::UpdateGraphProperties(CHyperGraph* pGraph)
{
	if (m_bUpdate)
		return;

	if (pGraph && pGraph->IsFlowGraph())
	{
		CFlowGraph* pFG = static_cast<CFlowGraph*> (pGraph);
		if (pFG == m_pGraph)
		{
			FillProps();
		}
	}
}

void CFlowGraphProperties::ShowMultiPlayer(bool bShow)
{
	if (m_bShowMP != bShow)
	{
		m_bShowMP = bShow;
		m_bUpdate = true;
		FillProps();
		m_bUpdate = false;
	}
}
