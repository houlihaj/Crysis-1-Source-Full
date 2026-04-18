////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowGraphSearchCtrl.h
//  Version:     v1.00
//  Created:     25/01/2006 by AlexL.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __FLOWGRAPH_SEARCH_CTRL_H__
#define __FLOWGRAPH_SEARCH_CTRL_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "FlowGraph.h"

// fwd declare
class CFlowGraphSearchResultsCtrl;
class CHyperGraphDialog;

class CFlowGraphSearchOptions
{
public:
	enum EFindLocation
	{
		eFL_Current = 0,
		eFL_AIActions,
		eFL_Entities,
		eFL_All
	};

	enum EFindSpecial
	{
		eFLS_None = 0,
		eFLS_NoEntity,
		eFLS_NoLinks,
		eFLS_Approved,
		eFLS_Advanced,
		eFLS_Debug,
		//eFLS_Legacy,
		//eFLS_WIP,
		//eFLS_NoCategory,
		eFLS_Obsolete,
	};

public:
	static CFlowGraphSearchOptions* GetSearchOptions();

	void Serialize(bool bLoading); // serialize to/from registry

public:
	BOOL		   m_bIncludePorts;
	BOOL		   m_bIncludeValues;
	BOOL       m_bIncludeEntities;
	BOOL       m_bIncludeIDs;
	BOOL       m_bExactMatch;
	CString    m_strFind;
	int        m_LookinIndex;
	int        m_findSpecial;

	CStringList m_lstFindHistory;

protected:
	CFlowGraphSearchOptions();
	CFlowGraphSearchOptions(const CFlowGraphSearchOptions&);
};

//////////////////////////////////////////////////////////////////////////
class CFlowGraphSearchCtrl : public CXTResizeDialog
{
	DECLARE_DYNAMIC(CFlowGraphSearchCtrl)
public:
	CFlowGraphSearchCtrl(CHyperGraphDialog* pDialog);
	virtual ~CFlowGraphSearchCtrl();

	enum { IDD = IDD_FG_SEARCH_OPTIONS };

	// called from CHyperGraphDialog...
	void SetResultsCtrl(CFlowGraphSearchResultsCtrl* pCtrl);

	void Find(const CString& searchString, bool bSetTextOnly=false, bool bSelectFirst=false, bool bTempExactMatch=false);

	typedef struct Result
	{
		Result(CFlowGraph* pGraph, HyperNodeID nodeId, const CString& context)
		{
			m_pGraph = pGraph;
			m_nodeId = nodeId;
			m_context = context;
		}
		CFlowGraph* m_pGraph;
		HyperNodeID m_nodeId;
		CString     m_context;
	};
	typedef std::vector<Result> ResultVec;

protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual void OnOK() {};
	virtual void OnCancel() {};

	void UpdateOptions();
	void DoTheFind(ResultVec& vec);
	void FindAll(bool bSelectFirst=false);

protected:
	CComboBox                m_cmbLookin;
	CComboBox                m_cmbFind;
	CComboBox                m_cmbSpecial;
	CFlowGraphSearchResultsCtrl *m_pResultsCtrl;
	CHyperGraphDialog       *m_pDialog;

	DECLARE_MESSAGE_MAP()

	afx_msg void OnButtonFindAll();
};

class CFlowGraphSearchResultsCtrl : public CXTPReportControl
{
public:
	enum { IDD_FG_SEARCH_RESULTS = 2410 };
	enum { IDD = IDD_FG_SEARCH_RESULTS };

	CFlowGraphSearchResultsCtrl(CHyperGraphDialog *pDialog);
	~CFlowGraphSearchResultsCtrl();

	void SelectFirstResult();
	void ShowSelectedResult();

	DECLARE_MESSAGE_MAP()

	afx_msg void OnReportItemDblClick( NMHDR* pNotifyStruct, LRESULT* result );
	afx_msg void OnReportItemHyperlink( NMHDR* pNotifyStruct, LRESULT* result );
	afx_msg void OnReportColumnRClick( NMHDR* pNotifyStruct, LRESULT* result );
	afx_msg void OnReportKeyDown(NMHDR * pNotifyStruct, LRESULT * result);


protected:
	void GroupBy(CXTPReportColumn *pColumn); // pColumn may be 0 -> ungroup
	CHyperGraphDialog       *m_pDialog;
};


#endif
