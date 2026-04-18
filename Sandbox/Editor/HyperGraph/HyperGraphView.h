////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   HyperGraphView.h
//  Version:     v1.00
//  Created:     21/3/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __HyperGraphView_h__
#define __HyperGraphView_h__
#pragma once

#include "HyperGraph.h"
#include <GdiPlus.h>

class CHyperGraphView;
class CHyperGraphNode;
class CPropertyCtrl;

#define WM_HYPERGRAPHVIEW_SELECTION_CHANGE WM_USER+1

//////////////////////////////////////////////////////////////////////////
class CHyperGraphViewRenameEdit : public CEdit
{
public:
	DECLARE_MESSAGE_MAP()

public:
	void SetView( CHyperGraphView *pView ) { m_pView = pView; };

protected:
	afx_msg UINT OnGetDlgCode() { return CEdit::OnGetDlgCode() | DLGC_WANTALLKEYS|DLGC_WANTMESSAGE; }
	afx_msg void OnKillFocus(CWnd* pWnd);
	afx_msg void OnEditKillFocus();

	virtual BOOL PreTranslateMessage(MSG* pMsg);

	CHyperGraphView *m_pView;
};

//////////////////////////////////////////////////////////////////////////
//
// CHyperGraphView is a window where all graph nodes are drawn on.
// View also provides an user interface to manipulate these nodes.
//
//////////////////////////////////////////////////////////////////////////
class CHyperGraphView : public CView, public IHyperGraphListener
{
	DECLARE_DYNAMIC(CHyperGraphView)
public:
	CHyperGraphView();
	~CHyperGraphView();

	// Creates view window.
	BOOL Create( DWORD dwStyle,const RECT &rect,CWnd *pParentWnd,UINT nID );

	// Assign graph to the view.
	void SetGraph( CHyperGraph *pGraph );
	CHyperGraph* GetGraph() const { return m_pGraph; }

	void SetPropertyCtrl( CPropertyCtrl *propsWnd );

	// Transform local rectangle into the view rectangle.
	CRect LocalToViewRect( const Gdiplus::RectF &localRect ) const;
	Gdiplus::RectF ViewToLocalRect( const CRect &viewRect ) const;

	CPoint LocalToView( Gdiplus::PointF point );
	Gdiplus::PointF ViewToLocal( CPoint point );

	CHyperNode* CreateNode( const CString &sNodeClass,CPoint point );

	// Retrieve all nodes in give view rectangle, return true if any found.
	bool GetNodesInRect( const CRect &viewRect,std::vector<CHyperNode*> &nodes,bool bFullInside=false );
	// Retrieve all selected nodes in graph, return true if any selected.
	enum SelectionSetType
	{
		SELECTION_SET_ONLY_PARENTS,
		SELECTION_SET_NORMAL,
		SELECTION_SET_INCLUDE_RELATIVES
	};
	bool GetSelectedNodes( std::vector<CHyperNode*> &nodes, SelectionSetType setType=SELECTION_SET_NORMAL );

	void ClearSelection();

	// scroll/zoom pToNode (of current graph) into view and optionally select it
	void ShowAndSelectNode(CHyperNode* pToNode, bool bSelect=true);

	// Invalidates hypergraph view.
	void InvalidateView(bool bComplete=false);

	//////////////////////////////////////////////////////////////////////////
	// IHyperGraphListener implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void OnHyperGraphEvent( IHyperNode *pNode,EHyperGraphEvent event );
	//////////////////////////////////////////////////////////////////////////

	void SetComponentViewMask(uint32 mask)	{	m_componentViewMask = mask; }

	CHyperNode* GetNodeAtPoint( CPoint point );

protected:
	DECLARE_MESSAGE_MAP()

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnDraw(CDC* pDC) {};
	virtual void PostNcDestroy();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

	afx_msg void OnCommandDelete();
	afx_msg void OnCommandDeleteKeepLinks();
	afx_msg void OnCommandCopy();
	afx_msg void OnCommandPaste();
	afx_msg void OnCommandPasteWithLinks();
	afx_msg void OnCommandCut();
	afx_msg void OnCommandFitAll();
	afx_msg void OnSelectEntity();
	afx_msg void OnToggleMinimize();

	virtual void ShowContextMenu( CPoint point,CHyperNode *pNode );
	virtual void UpdateTooltip( CHyperNode *pNode,CHyperNodePort *pPort );

	void OnMouseMoveScrollMode(UINT nFlags, CPoint point);
	void OnMouseMovePortDragMode(UINT nFlags, CPoint point);

private:
	// Draw graph nodes in rectangle.
	void DrawNodes( Gdiplus::Graphics &gr,const CRect &rc );
	void DrawEdges( Gdiplus::Graphics &gr,const CRect &rc );
	// return the helper rectangle
	Gdiplus::RectF DrawArrow( Gdiplus::Graphics &gr,CHyperEdge *pEdge, bool helper, int selection=0);
	void ReroutePendingEdges();
	void RerouteAllEdges();

	//////////////////////////////////////////////////////////////////////////
	void MoveSelectedNodes( CPoint offset );

	void DrawGrid( Gdiplus::Graphics &gr,const CRect &updateRect );
	
	void PopulateClassMenu( CMenu &menu,std::vector<CString> &classes,std::vector<CMenu*> &groups );
	void ShowPortsConfigMenu( CPoint point,bool bInputs,CHyperNode *pNode );

	void InvalidateEdgeRect( CPoint p1,CPoint p2 );
	void InvalidateEdge( CHyperEdge *pEdge );
	void InvalidateNode( CHyperNode *pNode,bool bRedraw=false );
	void ForceRedraw();

	//////////////////////////////////////////////////////////////////////////
	void SetScrollExtents( const CRect &rect );
	void CaptureMouse();
	void ReleaseMouse();
	bool CheckVirtualKey( int virtualKey );
	void SetScrollExtents();
	//////////////////////////////////////////////////////////////////////////

	void RenameNode( CHyperNode *pNode );
	void OnAcceptRename();
	void OnCancelRename();
	void OnSelectionChange();
	void OnUpdateProperties( IVariable *pVar );
	void OnToggleMinimizeNode( CHyperNode *pNode );

	void InternalPaste(bool bWithLinks, CPoint point);

	bool HitTestEdge( CPoint pnt,CHyperEdge* &pEdge,int &nIndex );

	// Simulate a flow (trigger input of target node)
	void SimulateFlow(CHyperEdge* pEdge);


private:
	friend CHyperGraphViewRenameEdit;
	enum EOperationMode
	{
		NothingMode = 0,
		ScrollZoomMode,
		ScrollMode,
		ZoomMode,
		MoveMode,
		SelectMode,
		PortDragMode,
		EdgePointDragMode,
		ScrollModeDrag,
	};

	bool   m_bRefitGraphInOnSize;
	CPoint m_RButtonDownPos;
	CPoint m_mouseDownPos;
	CPoint m_scrollOffset;
	CRect m_rcSelect;
	EOperationMode m_mode;

	// Port we are dragging now.
	_smart_ptr<CHyperEdge> m_pDraggingEdge;
	bool m_bDraggingFixedOutput;

	_smart_ptr<CHyperEdge> m_pHitEdge;
	int m_nHitEdgePoint;
	float m_prevCornerW,m_prevCornerH;

	typedef std::set< _smart_ptr<CHyperEdge> > PendingEdges;
	PendingEdges m_edgesToReroute;

	std::map<_smart_ptr<CHyperNode>, Gdiplus::PointF> m_moveHelper;
	std::map<_smart_ptr<CHyperEdge>, Gdiplus::RectF> m_edgeHelpers;

	Gdiplus::RectF m_graphExtents;
	float m_zoom;

	bool m_bSplineArrows;
	bool m_bHighlightIncomingEdges;
	bool m_bHighlightOutgoingEdges;

	CHyperGraphViewRenameEdit m_renameEdit;
	_smart_ptr<CHyperNode> m_pRenameNode;
	_smart_ptr<CHyperNode> m_pEditedNode;

	_smart_ptr<CHyperNode> m_pMouseOverNode;

	CPropertyCtrl *m_pPropertiesCtrl;

	CToolTipCtrl m_tooltip;
	
	// Current graph.
	CHyperGraph* m_pGraph;

	uint32 m_componentViewMask; 

};

#endif // __HyperGraphView_h__
