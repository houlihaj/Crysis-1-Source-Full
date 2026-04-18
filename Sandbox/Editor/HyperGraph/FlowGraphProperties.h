////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   FlowGraphProperties.h
//  Version:     v1.00
//  Created:     14/06/2006 by AlexL.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __FLOWGRAPH_PROPERTIES_H__
#define __FLOWGRAPH_PROPERTIES_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Controls/PropertyCtrl.h"
#include "FlowGraph.h"

class CHyperGraphDialog;
class CXTPTaskPanel;
class CHyperGraph;

class CFlowGraphProperties : public CWnd
{
public:
	DECLARE_DYNAMIC(CFlowGraphProperties)

	CFlowGraphProperties(CHyperGraphDialog* pParent, CXTPTaskPanel* pPanel);
	virtual ~CFlowGraphProperties();

	BOOL Create( DWORD dwStyle, const CRect& rc, CWnd* pParentWnd, UINT nID );

	void SetGraph(CHyperGraph* pGraph);
	void UpdateGraphProperties(CHyperGraph* pGraph);

	bool IsShowMultiPlayer()
	{
		return m_bShowMP;
	}

	void ShowMultiPlayer(bool bShow);

protected:
	DECLARE_MESSAGE_MAP()

	afx_msg void OnGraphDescriptionChange();
	afx_msg void OnDestroy();

	void Init(int nID);
	void FillProps();
	void ResizeProps(bool bHide=false);
	void OnVarChange(IVariable* pVar);

protected:
	CFlowGraph*        m_pGraph;
	CHyperGraphDialog* m_pParent;
	CXTPTaskPanel*     m_pTaskPanel;
	CXTPTaskPanelGroup*m_pGroup;
	CXTPTaskPanelGroupItem* m_pPropItem;
	CXTPTaskPanelGroupItem* m_pDescItem;

	CColorCtrl<CEdit> m_graphDescriptionEdit;
	CPropertyCtrl     m_graphProps;

	CSmartVariable<bool> m_varEnabled;
	CSmartVariableEnum<int> m_varMultiPlayer;

	bool m_bUpdate;
	bool m_bShowMP;
};

#endif
