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

#ifndef __HyperGraphDialog_h__
#define __HyperGraphDialog_h__
#pragma once

#include "ToolbarDialog.h"
#include "HyperGraphView.h"
#include "Controls/PropertyCtrl.h"

class CEntity;
class CPrefabObject;
class CFlowGraph;
class CFlowGraphSearchResultsCtrl;
class CFlowGraphSearchCtrl;
class CHyperGraphDialog;
class CFlowGraphProperties;

//////////////////////////////////////////////////////////////////////////
class CHyperGraphsTreeCtrl : public CXTTreeCtrl, public IHyperGraphManagerListener
{
public:
	CHyperGraphsTreeCtrl();
	~CHyperGraphsTreeCtrl();
	virtual BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
	virtual void OnHyperGraphManagerEvent( EHyperGraphEvent event );
	void SetCurrentGraph( CHyperGraph *pGraph );
	void Reload();
	void SetIgnoreReloads(bool bIgnore) { m_bIgnoreReloads = bIgnore; }
	HTREEITEM GetTreeItem(CHyperGraph* pGraph);

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);

	void OnObjectEvent( CBaseObject *pObject,int nEvent );

	CHyperGraph *m_pCurrentGraph;
	bool m_bReloadGraphs;
	bool m_bIgnoreReloads;
	CImageList m_imageList;

	std::map<CEntity*,HTREEITEM> m_mapEntityToItem;
	std::map<CPrefabObject*, HTREEITEM> m_mapPrefabToItem;
};

//////////////////////////////////////////////////////////////////////////
class CHyperGraphComponentsReportCtrl : public CXTPReportControl
{
	DECLARE_MESSAGE_MAP()
public:
	CHyperGraphComponentsReportCtrl();
	~CHyperGraphComponentsReportCtrl();

	CImageList* CreateDragImage(CXTPReportRow* pRow);
	void SetDialog (CHyperGraphDialog *pDialog) { m_pDialog = pDialog; }
	void Reload();
	void SetComponentFilterMask(uint32 mask) { m_mask = mask; }

protected:
	afx_msg void OnLButtonDown( UINT nFlags, CPoint point );
	afx_msg void OnLButtonUp( UINT nFlags, CPoint point );
	afx_msg void OnMouseMove( UINT nFlags, CPoint point );
	afx_msg void OnCaptureChanged( CWnd* );
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnReportColumnRClick( NMHDR* pNotifyStruct, LRESULT* result );
	afx_msg void OnReportItemDblClick( NMHDR* pNotifyStruct, LRESULT* result );

	afx_msg void OnDestroy();

	bool IsHeaderVisible();
	void SetHeaderVisible(bool bVisible);

protected:
	CImageList m_imageList;
	CHyperGraphDialog* m_pDialog;
	uint32 m_mask;
	CPoint m_ptDrag;
	bool m_bDragging;
	bool m_bDragEx;
	bool m_bHeaderVisible;
};

//////////////////////////////////////////////////////////////////////////
//
// Main Dialog for HyperGraph.
//
//////////////////////////////////////////////////////////////////////////
class CHyperGraphDialog : public CXTPFrameWnd, public IEditorNotifyListener, public IHyperGraphManagerListener
{
	DECLARE_DYNCREATE(CHyperGraphDialog)
public:
	static void RegisterViewClass();

	CHyperGraphDialog();
	~CHyperGraphDialog();

	BOOL Create( DWORD dwStyle,const RECT& rect,CWnd* pParentWnd );

	// Return pointer to the hyper graph view.
	CHyperGraphView* GetGraphView() { return &m_view; };

	CFlowGraphSearchCtrl* GetSearchControl() const { return m_pSearchOptionsView; }

	void SetGraph( CHyperGraph* pGraph );

	void OnViewSelectionChange();
	void ShowResultsPane(bool bShow=true, bool bFocus=true);
	void UpdateGraphProperties(CHyperGraph* pGraph);

	void OnComponentBeginDrag(CXTPReportRow* pRow, CPoint pt);

	enum { IDD = IDD_TRACKVIEWDIALOG };

protected:
	DECLARE_MESSAGE_MAP()

	virtual void OnOK() {};
	virtual void OnCancel() {};

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void PostNcDestroy();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnDestroy();
	afx_msg void OnClose();
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnFileNew();
	afx_msg void OnFileOpen();
	afx_msg void OnFileSave();
	afx_msg void OnFileSaveAs();
	afx_msg void OnFileNewAction();
	afx_msg void OnFileImport();
	afx_msg void OnFileExportSelection();

	afx_msg LRESULT OnDockingPaneNotify(WPARAM wParam, LPARAM lParam);
	afx_msg void OnGraphsRightClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnGraphsDblClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnGraphsSelChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnPlay();
	afx_msg void OnStop();
	afx_msg void OnToggleDebug();
	afx_msg void OnEraseDebug();
	afx_msg void OnPause();
	afx_msg void OnStep();
	afx_msg void OnUpdatePlay( CCmdUI *pCmdUI );
	afx_msg void OnUpdateStep( CCmdUI *pCmdUI );
	afx_msg void OnUpdateDebug( CCmdUI *pCmdUI );
	afx_msg void OnFind();
	afx_msg BOOL OnComponentsViewToggle( UINT nID );
	afx_msg void OnComponentsViewUpdate( CCmdUI *pCmdUI );

	afx_msg BOOL OnToggleBar( UINT nID );
	afx_msg void OnUpdateControlBar(CCmdUI* pCmdUI);
	afx_msg BOOL OnMultiPlayerViewToggle (UINT nID);
	afx_msg void OnMultiPlayerViewUpdate (CCmdUI* pCmdUI);
	afx_msg int  OnCreateControl(LPCREATECONTROLSTRUCT lpCreateControl); 
	afx_msg int  OnCreateCommandBar(LPCREATEBARSTRUCT lpCreatePopup);
	afx_msg void OnNavListBoxControlSelChange(NMHDR* pNMHDR, LRESULT* pRes);
	afx_msg void OnNavListBoxControlPopup(NMHDR* pNMHDR, LRESULT* pRes);
	afx_msg void OnUpdateNav(CCmdUI* pCmdUI);
	afx_msg void OnNavForward();
	afx_msg void OnNavBack();

	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra,AFX_CMDHANDLERINFO* pHandlerInfo);

	void ClearGraph();
	void NewGraph();
	void CreateRightPane();
	void CreateLeftPane();
	CXTPDockingPaneManager* GetDockingPaneManager() { return &m_paneManager; }

	void GraphUpdateEnable(CFlowGraph* pFlowGraph, HTREEITEM hItem);
	void EnableItems(HTREEITEM hItem, bool bEnable, bool bRecurse);
	void RenameItems(HTREEITEM hItem, CString& newGroupName, bool bRecurse);

	void UpdateComponentPaneTitle();

	// Called by the editor to notify the listener about the specified event.
	void OnEditorNotifyEvent( EEditorNotifyEvent event );

	// Called by hypergraph manager (need this for nav back/forward)
	virtual void OnHyperGraphManagerEvent( EHyperGraphEvent event );
	
	void UpdateNavHistory();
	void UpdateTitle(CHyperGraph* pGraph);

private:
	CHyperGraphView m_view;

	CXTPTaskPanel m_wndTaskPanel;
	CPropertyCtrl m_wndProps;
	CStatusBar m_statusBar;

	CImageList m_treeImageList;
	CHyperGraphsTreeCtrl m_graphsTreeCtrl;
	CHyperGraphComponentsReportCtrl m_componentsReportCtrl;
	CImageList *m_pDragImage;
	CString m_dragNodeClass;
	CFlowGraphSearchResultsCtrl *m_pSearchResultsCtrl;
	CFlowGraphSearchCtrl *m_pSearchOptionsView;
	CFlowGraphProperties *m_pGraphProperties;

	CXTPTaskPanelGroup* m_pPropertiesFolder;
	CXTPTaskPanelGroupItem *m_pPropertiesItem;
	
	
	CXTPDockingPaneManager m_paneManager;
	uint32 m_componentsViewMask;

	std::list<CHyperGraph*> m_graphList;
	std::list<CHyperGraph*>::iterator m_graphIterCur;
	CHyperGraph* m_pNavGraph;
};

#endif // __HyperGraphDialog_h__
