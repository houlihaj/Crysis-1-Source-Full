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

#include "StdAfx.h"
#include "Resource.h"
#include "HyperGraphView.h"
#include "HyperGraphManager.h"
#include "Controls\MemDC.h"
#include "Controls\PropertyCtrl.h"

#include "HyperGraphDialog.h"
#include "clipboard.h"

#include "FlowGraphNode.h"
#include "CommentNode.h"

#include "FlowGraphManager.h"
#include "FlowGraph.h"
#include "GameEngine.h"

#include <Gdiplus.h>
#include ".\hypergraphview.h"
#include "Objects/Entity.h"

#define GET_GDI_COLOR(x) (Gdiplus::Color(GetRValue(x),GetGValue(x),GetBValue(x)))

//////////////////////////////////////////////////////////////////////////
#define BACKGROUND_COLOR Gdiplus::Color(140,140,140)
#define GRID_COLOR Gdiplus::Color(145,145,145)
#define GRID_SIZE 10
#define ARROW_COLOR GET_GDI_COLOR(gSettings.hyperGraphColors.colorArrow)
#define ARROW_SEL_COLOR_IN GET_GDI_COLOR(gSettings.hyperGraphColors.colorInArrowHighlighted)
#define ARROW_SEL_COLOR_OUT GET_GDI_COLOR(gSettings.hyperGraphColors.colorOutArrowHighlighted)
#define ARROW_DIS_COLOR GET_GDI_COLOR(gSettings.hyperGraphColors.colorArrowDisabled)

#define MIN_ZOOM 0.01f
#define MAX_ZOOM 100.0f

//////////////////////////////////////////////////////////////////////////
// CHyperGraphViewRenameEdit implementation.
//////////////////////////////////////////////////////////////////////////
BEGIN_MESSAGE_MAP (CHyperGraphViewRenameEdit,CEdit)
	ON_WM_GETDLGCODE()
	ON_WM_KILLFOCUS()
	ON_CONTROL_REFLECT(EN_KILLFOCUS, OnEditKillFocus)
END_MESSAGE_MAP ()

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphViewRenameEdit::PreTranslateMessage(MSG* msg)
{
	if (msg->message == WM_LBUTTONDOWN || msg->message == WM_RBUTTONDOWN)
	{
		CPoint pnt;
		GetCursorPos(&pnt);
		CRect rc;
		GetWindowRect(rc);
		if (!rc.PtInRect(pnt))
		{
			// Accept edit.
			m_pView->OnAcceptRename();
			return TRUE;
		}
	}
	else if (msg->message == WM_KEYDOWN)
	{
		switch (msg->wParam)
		{
		case VK_RETURN:
			{
				GetAsyncKeyState(VK_CONTROL);
				if (!GetAsyncKeyState(VK_CONTROL))
				{
					// Send accept message.
					m_pView->OnAcceptRename();
					return TRUE;
				}

				/*
				// Add carriege return into the end of string.
				const char *szStr = "\n";
				int len = GetWindowTextLength();
				SendMessage( EM_SETSEL,len,-1 );
				SendMessage( EM_SCROLLCARET, 0,0 );
				SendMessage( EM_REPLACESEL, FALSE, (LPARAM)szStr );
				*/
			}
			break;

		case VK_ESCAPE:
			// Cancel edit.
			m_pView->OnCancelRename();
			return TRUE;
			break;
		}
	}
	return CEdit::PreTranslateMessage(msg);
}

void CHyperGraphViewRenameEdit::OnKillFocus(CWnd *pWnd)
{
	// Send accept
	m_pView->OnAcceptRename();
	CEdit::OnKillFocus(pWnd);
}

void CHyperGraphViewRenameEdit::OnEditKillFocus()
{
	// Send accept
	m_pView->OnAcceptRename();
}

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CHyperGraphView,CWnd)

BEGIN_MESSAGE_MAP (CHyperGraphView, CWnd)
	ON_WM_MOUSEMOVE()
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_MOUSEWHEEL()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MBUTTONDBLCLK()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	ON_WM_SETCURSOR()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()

	ON_COMMAND(ID_EDIT_DELETE,OnCommandDelete)
	ON_COMMAND(ID_EDIT_COPY,OnCommandCopy)
	ON_COMMAND(ID_EDIT_PASTE,OnCommandPaste)
	ON_COMMAND(ID_EDIT_PASTE_WITH_LINKS,OnCommandPasteWithLinks)
	ON_COMMAND(ID_EDIT_CUT,OnCommandCut)
	//ON_COMMAND(ID_MINIMIZE,OnToggleMinimize)
	ON_COMMAND(ID_EDIT_DELETE_KEEP_LINKS,OnCommandDeleteKeepLinks)
END_MESSAGE_MAP ()

struct GraphicsHelper
{
	GraphicsHelper( CWnd * pWnd ) : m_pWnd(pWnd), m_pDC(pWnd->GetDC()), m_pGr(new Gdiplus::Graphics(m_pDC->GetSafeHdc()))
	{
	}
	~GraphicsHelper()
	{
		delete m_pGr;
		m_pWnd->ReleaseDC(m_pDC);
	}

	operator Gdiplus::Graphics *()
	{
		return m_pGr;
	}

	CWnd * m_pWnd;
	CDC * m_pDC;
	Gdiplus::Graphics * m_pGr;
};

//////////////////////////////////////////////////////////////////////////
CHyperGraphView::CHyperGraphView()
{
	m_pGraph = 0;
	m_mouseDownPos = CPoint(0,0);
	m_scrollOffset = CPoint(0,0);
	m_rcSelect.SetRect(0,0,0,0);
	m_mode = NothingMode;

	m_pPropertiesCtrl = 0;
	m_zoom = 1.0f;
	m_bSplineArrows = true;
	m_bHighlightIncomingEdges = false;
	m_bHighlightOutgoingEdges = false;
	m_componentViewMask = 0; // will be set by dialog
	m_bRefitGraphInOnSize = false;
}

//////////////////////////////////////////////////////////////////////////
CHyperGraphView::~CHyperGraphView()
{
	SetGraph( NULL );
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::SetPropertyCtrl( CPropertyCtrl *propsCtrl )
{
	m_pPropertiesCtrl = propsCtrl;

	if (m_pPropertiesCtrl)
	{
		m_pPropertiesCtrl->SetUpdateCallback( functor(*this,&CHyperGraphView::OnUpdateProperties) );
		m_pPropertiesCtrl->EnableUpdateCallback(true);
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphView::Create( DWORD dwStyle,const RECT &rect,CWnd *pParentWnd,UINT nID )
{
	if (!m_hWnd)
	{
		//////////////////////////////////////////////////////////////////////////
		// Create window.
		//////////////////////////////////////////////////////////////////////////
		CRect rcDefault(0,0,100,100);
		LPCTSTR lpClassName = AfxRegisterWndClass(CS_DBLCLKS|CS_HREDRAW|CS_VREDRAW,	AfxGetApp()->LoadStandardCursor(IDC_ARROW), NULL, NULL);
		VERIFY( CreateEx( NULL,lpClassName,"HyperGraphView",dwStyle,rcDefault, pParentWnd, nID));

		if (!m_hWnd)
			return FALSE;
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::PostNcDestroy()
{
}

void CHyperGraphView::OnDestroy() 
{
	if (m_pGraph)
		m_pGraph->RemoveListener(this);

	CWnd::OnDestroy();
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphView::PreTranslateMessage(MSG* pMsg)
{
	if (!m_tooltip.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		m_tooltip.Create( this );
		m_tooltip.SetDelayTime( 1000 );
		m_tooltip.SetMaxTipWidth(600);
		m_tooltip.AddTool( this,"",rc,1 );
		m_tooltip.Activate(FALSE);
	}
	m_tooltip.RelayEvent(pMsg);

	BOOL bResult = __super::PreTranslateMessage( pMsg );
	return bResult;
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) 
{
	float multiplier = 0.1f * 1.0f / 120.0f;

	// zoom around the mouse cursor position, first get old mouse pos
	GetCursorPos(&pt);
	ScreenToClient(&pt);
	Gdiplus::PointF oldZoomFocus = ViewToLocal(pt);

	// zoom
	m_zoom += multiplier * zDelta;
	m_zoom = CLAMP( m_zoom, MIN_ZOOM, MAX_ZOOM );

	// determine where the focus has moved as part of the zoom
	CPoint newZoomFocus = LocalToView(oldZoomFocus);

	// adjust offset based on displacement and update windows stuff
	m_scrollOffset-= (newZoomFocus - pt);
	SetScrollPos( SB_HORZ,-m_scrollOffset.x );
	SetScrollPos( SB_VERT,-m_scrollOffset.y );
	SetScrollExtents();
	Invalidate();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize( nType,cx,cy );

	//marcok: the following code handles the case when we've tried to fit the graph to view whilst not knowing the correct size of the client rect
	if (m_bRefitGraphInOnSize)
	{
		m_bRefitGraphInOnSize = false;
		OnCommandFitAll();
	}

	SetScrollExtents();
}

//////////////////////////////////////////////////////////////////////////
BOOL CHyperGraphView::OnEraseBkgnd(CDC* pDC) 
{
	/*
	RECT rect;
	CBrush cFillBrush;

	// Get the rect of the client window
	GetClientRect(&rect);

	// Create the brush
	cFillBrush.CreateSolidBrush(BACKGROUND_COLOR);

	// Fill the entire client area
	pDC->FillRect(&rect, &cFillBrush);
*/
	return TRUE;
}

static bool LinesegRectSingle( Gdiplus::PointF * pWhere, const Lineseg& line1, float x1, float y1, float x2, float y2 )
{
	Vec3 a(x1,y1,0);
	Vec3 b(x2,y2,0);
	Lineseg line2(a,b);
	float t1, t2;
	if (Intersect::Lineseg_Lineseg2D(line1, line2, t1, t2))
	{
		Vec3 pos = line1.start + t1 * (line1.end - line1.start);
		pWhere->X = pos.x;
		pWhere->Y = pos.y;
		return true;
	}
	return false;
}

static void LinesegRectIntersection( Gdiplus::PointF * pWhere, EHyperEdgeDirection * pDir, const Lineseg& line, const Gdiplus::RectF& rect )
{
	if (LinesegRectSingle(pWhere, line, rect.X, rect.Y, rect.X + rect.Width, rect.Y))
		*pDir = eHED_Up;
	else if (LinesegRectSingle(pWhere, line, rect.X, rect.Y, rect.X, rect.Y + rect.Height))
		*pDir = eHED_Left;
	else if (LinesegRectSingle(pWhere, line, rect.X + rect.Width, rect.Y, rect.X + rect.Width, rect.Y + rect.Height))
		*pDir = eHED_Right;
	else if (LinesegRectSingle(pWhere, line, rect.X, rect.Y + rect.Height, rect.X + rect.Width, rect.Y + rect.Height))
		*pDir = eHED_Down;
	else
	{
		*pDir = eHED_Down;
		pWhere->X = rect.X + rect.Width * 0.5f;
		pWhere->Y = rect.Y + rect.Height * 0.5f;
	}
}

void CHyperGraphView::ReroutePendingEdges()
{
	std::vector<PendingEdges::iterator> eraseIters;

	for (PendingEdges::iterator iter = m_edgesToReroute.begin(); iter != m_edgesToReroute.end(); ++iter)
	{
		CHyperEdge * pEdge = *iter;
//		if (pEdge->nodeOut == -1)
//			DEBUG_BREAK;
		CHyperNode * pNodeOut = (CHyperNode*) m_pGraph->FindNode( pEdge->nodeOut );
		CHyperNode * pNodeIn = (CHyperNode*) m_pGraph->FindNode( pEdge->nodeIn );
		if ((!pNodeIn && m_bDraggingFixedOutput && pEdge != m_pDraggingEdge) || (!pNodeOut && !m_bDraggingFixedOutput && pEdge != m_pDraggingEdge))
			continue;
		
		Gdiplus::RectF rectOut, rectIn;
		if (pNodeIn)
		{
			if (!pNodeIn->GetAttachRect(pEdge->nPortIn, &rectIn))
				continue;
			rectIn.Offset( pNodeIn->GetPos() );
		}
		else if (m_bDraggingFixedOutput && pEdge == m_pDraggingEdge)
		{
			rectIn = Gdiplus::RectF( m_pDraggingEdge->pointIn.X, m_pDraggingEdge->pointIn.Y, 0, 0 );
		}
		else
		{
			continue;
		}

		if (pNodeOut)
		{
			if (!pNodeOut->GetAttachRect(pEdge->nPortOut + 1000, &rectOut))
				continue;
			rectOut.Offset( pNodeOut->GetPos() );
		}
		else if (!m_bDraggingFixedOutput && pEdge == m_pDraggingEdge)
		{
			rectOut = Gdiplus::RectF( m_pDraggingEdge->pointOut.X, m_pDraggingEdge->pointOut.Y, 0, 0 );
		}
		else
		{
			continue;
		}

		if (rectOut.IsEmptyArea() && rectIn.IsEmptyArea())
		{
			rectOut.GetLocation( &pEdge->pointOut );
			rectIn.GetLocation( &pEdge->pointIn );
			// hack... but it works
			pEdge->dirOut = eHED_Right;
			pEdge->dirIn = eHED_Left;
		}
		else if (!rectOut.IsEmptyArea() && !rectIn.IsEmptyArea())
		{
			Vec3 centerOut( rectOut.X + rectOut.Width * 0.5f, rectOut.Y + rectOut.Height * 0.5f, 0.0f );
			Vec3 centerIn( rectIn.X + rectIn.Width * 0.5f, rectIn.Y + rectIn.Height * 0.5f, 0.0f );
			Vec3 dir = centerOut - centerIn;
			dir.Normalize();
			dir *= 10.0f;
			std::swap(dir.x, dir.y);
			dir.x = -dir.x;
			centerIn += dir;
			centerOut += dir;
			Lineseg centerLine( centerOut, centerIn );
			LinesegRectIntersection( &pEdge->pointOut, &pEdge->dirOut, centerLine, rectOut );
			LinesegRectIntersection( &pEdge->pointIn, &pEdge->dirIn, centerLine, rectIn );
		}
		else if (!rectOut.IsEmptyArea() && rectIn.IsEmptyArea())
		{
			Vec3 centerOut( rectOut.X + rectOut.Width * 0.5f, rectOut.Y + rectOut.Height * 0.5f, 0.0f );
			Vec3 centerIn( rectIn.X + rectIn.Width * 0.5f, rectIn.Y + rectIn.Height * 0.5f, 0.0f );
			Lineseg centerLine( centerOut, centerIn );
			LinesegRectIntersection( &pEdge->pointOut, &pEdge->dirOut, centerLine, rectOut );
			rectIn.GetLocation( &pEdge->pointIn );
			pEdge->dirIn = eHED_Left;
		}
		else if (rectOut.IsEmptyArea() && !rectIn.IsEmptyArea())
		{
			Vec3 centerOut( rectOut.X + rectOut.Width * 0.5f, rectOut.Y + rectOut.Height * 0.5f, 0.0f );
			Vec3 centerIn( rectIn.X + rectIn.Width * 0.5f, rectIn.Y + rectIn.Height * 0.5f, 0.0f );
			Lineseg centerLine( centerOut, centerIn );
			LinesegRectIntersection( &pEdge->pointIn, &pEdge->dirIn, centerLine, rectIn );
			rectOut.GetLocation( &pEdge->pointOut );
			pEdge->dirOut = eHED_Right;
		}
		else
		{
			assert(false);
		}

		eraseIters.push_back( iter );
	}
	while (!eraseIters.empty())
	{
		m_edgesToReroute.erase(eraseIters.back());
		eraseIters.pop_back();
	}
}

void CHyperGraphView::RerouteAllEdges()
{
	std::vector<CHyperEdge*> edges;
	m_pGraph->GetAllEdges(edges);
	std::copy( edges.begin(), edges.end(), inserter(m_edgesToReroute, m_edgesToReroute.end()) );

	std::set<CHyperNode*> nodes;
	GraphicsHelper g(this);
	for (std::vector<CHyperEdge*>::const_iterator iter = edges.begin(); iter != edges.end(); ++iter)
	{
		CHyperNode * pNodeIn = (CHyperNode*) m_pGraph->FindNode( (*iter)->nodeIn );
		CHyperNode * pNodeOut = (CHyperNode*) m_pGraph->FindNode( (*iter)->nodeOut );
		if (pNodeIn && pNodeOut)
		{
			nodes.insert( pNodeIn );
			nodes.insert( pNodeOut );
		}
	}
	for (std::set<CHyperNode*>::const_iterator iter = nodes.begin(); iter != nodes.end(); ++iter)
	{
		(*iter)->Draw( this, *(Gdiplus::Graphics*)g, false );
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnPaint()
{
	CPaintDC PaintDC(this); // device context for painting

	CRect rc = PaintDC.m_ps.rcPaint;
	Gdiplus::Bitmap backBufferBmp(rc.Width(),rc.Height());

	Gdiplus::Graphics mainGr( PaintDC.GetSafeHdc() );

	Gdiplus::Graphics* pBackBufferGraphics = Gdiplus::Graphics::FromImage(&backBufferBmp);
	Gdiplus::Graphics &gr = *pBackBufferGraphics;

	gr.TranslateTransform( m_scrollOffset.x - rc.left, m_scrollOffset.y - rc.top );
	gr.ScaleTransform( m_zoom, m_zoom );
	gr.SetSmoothingMode(Gdiplus::SmoothingModeNone);
	gr.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit );
	//gr.SetTextRenderingHint(Gdiplus::TextRenderingHintSystemDefault);

	CRect rect = PaintDC.m_ps.rcPaint;
	Gdiplus::RectF clip = ViewToLocalRect(rect);

	// Create the brush
//	Gdiplus::SolidBrush fillBrush(BACKGROUND_COLOR);
	gr.Clear( BACKGROUND_COLOR );

//	Gdiplus::Pen border( Gdiplus::Color(0,255,0), 5.0f );
//	gr.DrawRectangle( &border, clip );

//	gr.FillRectangle( &fillBrush, clip );

	gr.SetSmoothingMode(Gdiplus::SmoothingModeNone);
	DrawGrid( gr,rect );
	if (m_pGraph)
	{
		gr.SetSmoothingMode(Gdiplus::SmoothingModeNone);
		DrawNodes( gr,rect );
		gr.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
		ReroutePendingEdges();
		DrawEdges( gr,rect );

		if (m_pDraggingEdge)
		{
			DrawArrow( gr, m_pDraggingEdge, 0 );
		}
	}

	 mainGr.DrawImage(&backBufferBmp,rc.left,rc.top,rc.Width(),rc.Height());
	 delete pBackBufferGraphics;

//	if (!m_edgesToReroute.empty())
//		Invalidate();
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraphView::HitTestEdge( CPoint pnt,CHyperEdge* &pOutEdge,int &nIndex )
{
	if (!m_pGraph)
		return false;

	Gdiplus::PointF mousePt = ViewToLocal(pnt);

	std::vector<CHyperEdge*> edges;
	m_pGraph->GetAllEdges( edges );
	{
		for (int i = 0; i < edges.size(); i++)
		{
			CHyperEdge *pEdge = edges[i];
			for (int j = 0; j < 4; j++)
			{
				float dx = pEdge->cornerPoints[j].X - mousePt.X;
				float dy = pEdge->cornerPoints[j].Y - mousePt.Y;

				if ((dx*dx + dy*dy) < 7)
				{
					pOutEdge = pEdge;
					nIndex = j;
					return true;
				}
			}
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnLButtonDown(UINT nFlags, CPoint point) 
{
	if (!m_pGraph)
		return;

	m_pHitEdge = 0;

	SetFocus();
	// Save the mouse down position
	m_mouseDownPos = point;

	bool bAltClick = CheckVirtualKey(VK_MENU);
	bool bCtrlClick = (nFlags & MK_CONTROL);
	bool bShiftClick = (nFlags & MK_SHIFT);

	Gdiplus::PointF mousePt = ViewToLocal(point);

	/*
	for (std::map<_smart_ptr<CHyperEdge>, Gdiplus::RectF>::const_iterator iter = m_edgeHelpers.begin(); iter != m_edgeHelpers.end(); ++iter)
	{
		if (iter->second.Contains(mousePt))
		{
			CMenu menu;
			menu.CreatePopupMenu();
			menu.AppendMenu(MF_STRING, 1, "Remove");
			if (iter->first->IsEditable())
				menu.AppendMenu(MF_STRING, 2, "Edit");
			CPoint pt;
			GetCursorPos(&pt);
			switch (menu.TrackPopupMenuEx(TPM_RETURNCMD, pt.x, pt.y, this, NULL))
			{
			case 1:
				{
					CUndo undo( "Remove Graph Edge" );
					m_pGraph->RemoveEdge( iter->first );
				}
				break;
			case 2:
				{
					CUndo undo( "Edit Graph Edge" );
					m_pGraph->EditEdge( iter->first );
				}
				break;
			}
			return;
		}
	}
	*/

	CHyperEdge *pHitEdge = 0;
	int nHitEdgePoint = 0;
	if (HitTestEdge( point,pHitEdge,nHitEdgePoint ))
	{
		m_pHitEdge = pHitEdge;
		m_nHitEdgePoint = nHitEdgePoint;
		m_prevCornerW = m_pHitEdge->cornerW;
		m_prevCornerH = m_pHitEdge->cornerH;
		m_mode = EdgePointDragMode;
		CaptureMouse();
		return;
	}
	
	CHyperNode *pNode = GetNodeAtPoint( point );
	if (pNode)
	{
		int objectClicked = pNode->GetObjectAt( GraphicsHelper(this), ViewToLocal(point) );

		CHyperNodePort * pPort = NULL;
		static const int WAS_A_PORT = -10000;
		static const int WAS_A_SELECT = -10001;

		if (objectClicked >= eSOID_ShiftFirstOutputPort && objectClicked <= eSOID_ShiftLastOutputPort)
		{
			if (bShiftClick)
				objectClicked = eSOID_FirstOutputPort + (objectClicked - eSOID_ShiftFirstOutputPort);
			else if (bAltClick)
				objectClicked = eSOID_FirstOutputPort + (objectClicked - eSOID_ShiftFirstOutputPort);
			else
				objectClicked = eSOID_Background;
		}

		if ((bAltClick && (objectClicked < eSOID_FirstOutputPort && objectClicked > eSOID_LastOutputPort)) || bCtrlClick)
		{
			objectClicked = WAS_A_SELECT;
		}
		else if (objectClicked >= eSOID_FirstInputPort && objectClicked <= eSOID_LastInputPort && !pNode->GetInputs()->empty())
		{
			pPort = &pNode->GetInputs()->at(objectClicked-eSOID_FirstInputPort);
			objectClicked = WAS_A_PORT;
		}
		else if (objectClicked >= eSOID_FirstOutputPort && objectClicked <= eSOID_LastOutputPort && !pNode->GetOutputs()->empty())
		{
			pPort = &pNode->GetOutputs()->at(objectClicked-eSOID_FirstOutputPort);
			objectClicked = WAS_A_PORT;
		}
		switch (objectClicked)
		{
		case eSOID_InputTransparent:
			break;
		case WAS_A_SELECT:
		case eSOID_AttachedEntity:
		case eSOID_Background:
		case eSOID_Title:
			{
				bool bUnselect = bAltClick;
				if (!pNode->IsSelected() || bUnselect)
				{
					CUndo undo( "Select Graph Node(s)" );
					// Select this node if not selected yet.
					if (!bCtrlClick && !bAltClick)
						m_pGraph->UnselectAll();
					//pNode->SetSelected( !bUnselect );
					IHyperGraphEnumerator* pEnum = pNode->GetRelatedNodesEnumerator();
					for (IHyperNode* pRelative = pEnum->GetFirst(); pRelative; pRelative = pEnum->GetNext())
						static_cast<CHyperNode*>(pRelative)->SetSelected(!bUnselect);
					pEnum->Release();
					OnSelectionChange();
				}
				m_mode = MoveMode;
				m_moveHelper.clear();
				GetIEditor()->BeginUndo();
			}
			break;
		case eSOID_InputGripper:
			ShowPortsConfigMenu( point,true,pNode );
			break;
		case eSOID_OutputGripper:
			ShowPortsConfigMenu( point,false,pNode );
			break;
		case eSOID_Minimize:
			OnToggleMinimizeNode(pNode);
			break;

		default:
			//assert(false);
			break;

		case WAS_A_PORT:
			if (!pPort->bInput || pPort->nConnected <= 1)
			{
				if (!pNode->IsSelected())
				{
					CUndo undo( "Select Graph Node(s)" );
					// Select this node if not selected yet.
					m_pGraph->UnselectAll();
					pNode->SetSelected( true );
					OnSelectionChange();
				}

				// Undo must start before edge delete.
				GetIEditor()->BeginUndo();
				pPort->bSelected = true;

				// If trying to drag output port, disconnect input parameter and drag it.
				if (pPort->bInput)
				{
					CHyperEdge *pEdge =	m_pGraph->FindEdge( pNode,pPort );
					if (pEdge)
					{
						// Disconnect this edge.
						pNode = (CHyperNode*)m_pGraph->FindNode(pEdge->nodeOut);
						if (pNode)
						{
							pPort = &pNode->GetOutputs()->at(pEdge->nPortOut);
							InvalidateEdge(pEdge);
							m_pGraph->RemoveEdge( pEdge );
						}
						else
							pPort = 0;
						m_bDraggingFixedOutput = true;
					}
					else
					{
						m_bDraggingFixedOutput = false;
					}
				}
				else
				{
					m_bDraggingFixedOutput = true;
				}
				if (!pPort || !pNode)
				{
					GetIEditor()->CancelUndo();
					return;
				}

				m_mode = PortDragMode;

				m_pDraggingEdge = m_pGraph->CreateEdge();
				m_edgesToReroute.insert(m_pDraggingEdge);

				m_pDraggingEdge->nodeIn = -1;
				m_pDraggingEdge->nPortIn = -1;
				m_pDraggingEdge->nodeOut = -1;
				m_pDraggingEdge->nPortOut = -1;

				if (m_bDraggingFixedOutput)
				{
					m_pDraggingEdge->nodeOut = pNode->GetId();
					m_pDraggingEdge->nPortOut = pPort->nPortIndex;
					m_pDraggingEdge->pointIn = ViewToLocal(point);
				}
				else
				{
					m_pDraggingEdge->nodeIn = pNode->GetId();
					m_pDraggingEdge->nPortIn = pPort->nPortIndex;
					m_pDraggingEdge->pointOut = ViewToLocal(point);
				}
				pPort->bSelected = true;

				InvalidateNode(pNode,true);
			}
			else
			{
				CMenu menu;
				menu.CreatePopupMenu();
				std::vector<CHyperEdge*> edges;
				m_pGraph->GetAllEdges(edges);
				for (size_t i=0; i<edges.size(); ++i)
				{
					if (edges[i]->nodeIn == pNode->GetId() && 0 == stricmp(edges[i]->portIn, pPort->GetName()))
					{
						CString title;
						CHyperNode * pToNode = (CHyperNode*) m_pGraph->FindNode( edges[i]->nodeOut );
						title.Format("Remove %s:%s", pToNode->GetClassName(), edges[i]->portOut);
						menu.AppendMenu(MF_STRING, i+1, title);
					}
				}
				CPoint pt;
				GetCursorPos(&pt);
				int nEdge = menu.TrackPopupMenuEx(TPM_RETURNCMD, pt.x, pt.y, this, NULL);
				if (nEdge)
				{
					nEdge--;
					m_pGraph->RemoveEdge( edges[nEdge] );
				}
			}
			break;
		}
	}
	else
	{
		if (!bAltClick && !bCtrlClick)
		{
			CUndo undo( "Unselect Graph Node(s)" );
			m_pGraph->UnselectAll();
			OnSelectionChange();
		}
		m_mode = SelectMode;
	}

	CaptureMouse();

	//CWnd::OnLButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraphView::CheckVirtualKey( int virtualKey )
{
	GetAsyncKeyState(virtualKey);
	if (GetAsyncKeyState(virtualKey))
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if (!m_pGraph)
		return;

	ReleaseCapture();

	bool bAltClick = CheckVirtualKey(VK_MENU);
	bool bCtrlClick = (nFlags & MK_CONTROL);
	bool bShiftClick = (nFlags & MK_SHIFT);

	switch (m_mode)
	{
	case SelectMode:
		{
			if (!m_rcSelect.IsRectEmpty())
			{
				bool bUnselect = bAltClick;

				CUndo undo("Select Graph Node(s)");
				
				if (!bAltClick && !bCtrlClick)
				{
					if (m_pGraph)
						m_pGraph->UnselectAll();
				}

				std::vector<CHyperNode*> nodes;
				GetNodesInRect( m_rcSelect,nodes,true );
				for (int i = 0; i < nodes.size(); i++)
				{
					CHyperNode *pNode = nodes[i];
					IHyperGraphEnumerator* pEnum = pNode->GetRelatedNodesEnumerator();
					for (IHyperNode* pRelative = pEnum->GetFirst(); pRelative; pRelative = pEnum->GetNext())
						static_cast<CHyperNode*>(pRelative)->SetSelected(!bUnselect);
					//pNode->SetSelected(!bUnselect);
				}
				OnSelectionChange();
				Invalidate();
			}
		}
		break;
	case MoveMode:
		m_moveHelper.clear();
		GetIEditor()->AcceptUndo( "Move Graph Node(s)" );
		SetScrollExtents();
		Invalidate();
		break;
	case EdgePointDragMode:
		{

			
		}
		break;
	case PortDragMode:
		{
			if (CHyperNode * pNode = (CHyperNode*) m_pGraph->FindNode( m_pDraggingEdge->nodeIn ))
			{
				pNode->UnselectAllPorts();
			}
			if (CHyperNode * pNode = (CHyperNode*) m_pGraph->FindNode( m_pDraggingEdge->nodeOut ))
			{
				pNode->UnselectAllPorts();
			}

			CHyperNode *pTrgNode = GetNodeAtPoint( point );
			if (pTrgNode)
			{
				int objectClicked = pTrgNode->GetObjectAt( GraphicsHelper(this), ViewToLocal(point) );
				bool specialDrag = false;
				if (objectClicked >= eSOID_ShiftFirstOutputPort && objectClicked <= eSOID_ShiftLastOutputPort)
				{
					if (bShiftClick)
						objectClicked = eSOID_FirstInputPort + (objectClicked - eSOID_ShiftFirstOutputPort);
					else if (bAltClick)
					{
						objectClicked = eSOID_FirstInputPort + (objectClicked - eSOID_ShiftFirstOutputPort);
						specialDrag = true;
					}
					else
						objectClicked = eSOID_Background;
				}
				CHyperNodePort *pTrgPort = NULL;
				if (objectClicked >= eSOID_FirstInputPort && objectClicked <= eSOID_LastInputPort && !pTrgNode->GetInputs()->empty())
					pTrgPort = &pTrgNode->GetInputs()->at(objectClicked - eSOID_FirstInputPort);
				else if (objectClicked >= eSOID_FirstOutputPort && objectClicked <= eSOID_LastOutputPort && !pTrgNode->GetOutputs()->empty())
					pTrgPort = &pTrgNode->GetOutputs()->at(objectClicked - eSOID_FirstOutputPort);
				if (pTrgPort)
				{
					CHyperNode * pSrcNode = 0;
					CHyperNodePort * pSrcPort = 0;
					if (m_bDraggingFixedOutput)
					{
						pSrcNode = (CHyperNode*) m_pGraph->FindNode( m_pDraggingEdge->nodeOut );
						pSrcPort = &pSrcNode->GetOutputs()->at( m_pDraggingEdge->nPortOut );
					}
					else
					{
						pSrcNode = (CHyperNode*) m_pGraph->FindNode( m_pDraggingEdge->nodeIn );
						pSrcPort = &pSrcNode->GetInputs()->at( m_pDraggingEdge->nPortIn );
						std::swap(pSrcNode, pTrgNode);
						std::swap(pSrcPort, pTrgPort);
					}
					if (m_pGraph->CanConnectPorts( pSrcNode,pSrcPort,pTrgNode,pTrgPort ))
					{
						// Connect
						m_pGraph->ConnectPorts( pSrcNode,pSrcPort,pTrgNode,pTrgPort,specialDrag );
						InvalidateNode( pSrcNode,true );
						InvalidateNode( pTrgNode,true );
					}
				}
			}
			Invalidate();

			GetIEditor()->AcceptUndo( "Connect Graph Node" );
			m_pDraggingEdge = NULL;
		}
		break;
	}
	m_rcSelect.SetRect(0,0,0,0);

	if (m_mode == ScrollModeDrag)
	{
		Invalidate();
		GetIEditor()->AcceptUndo( "Connect Graph Node" );
		m_pDraggingEdge = NULL;
		m_mode = ScrollMode;
	}
	else
	{
		m_mode = NothingMode;
	}
	//CWnd::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnRButtonDown(UINT nFlags, CPoint point) 
{
	CHyperEdge *pHitEdge = 0;
	int nHitEdgePoint = 0;
	if (HitTestEdge( point,pHitEdge,nHitEdgePoint ))
	{
		CMenu menu;
		menu.CreatePopupMenu();
		menu.AppendMenu(MF_STRING, 1, _T("Remove"));
		if (pHitEdge->IsEditable())
			menu.AppendMenu(MF_STRING, 2, _T("Edit"));
		if (m_pGraph->IsFlowGraph())
			menu.AppendMenu(MF_STRING, 3, pHitEdge->enabled ? _T("Disable") : _T("Enable"));

		// makes only sense for non-AIActions and while fg system is updated
		if (m_pGraph->IsFlowGraph() && m_pGraph->GetAIAction() == 0 && GetIEditor()->GetGameEngine()->IsFlowSystemUpdateEnabled())
			menu.AppendMenu(MF_STRING, 4, _T("Simulate flow [Double click]"));

		CPoint pt;
		GetCursorPos(&pt);
		switch (menu.TrackPopupMenuEx(TPM_RETURNCMD, pt.x, pt.y, this, NULL))
		{
		case 1:
			{
				CUndo undo( "Remove Graph Edge" );
				m_pGraph->RemoveEdge( pHitEdge );
			}
			break;
		case 2:
			{
				CUndo undo( "Edit Graph Edge" );
				m_pGraph->EditEdge( pHitEdge );
			}
			break;
		case 3:
			{
				CUndo undo ( pHitEdge->enabled ? "Disable Edge" : "Enable Edge");
				m_pGraph->EnableEdge(pHitEdge, !pHitEdge->enabled);
				InvalidateEdge(pHitEdge);
			}
			break;
		case 4:
			SimulateFlow(pHitEdge);
			break;
		}
		return;
	}

	SetFocus();
	if (m_mode == PortDragMode)
	{
		m_mode = ScrollModeDrag;
	}
	else
	{
		if (m_mode == MoveMode)
			GetIEditor()->AcceptUndo( "Move Graph Node(s)" );

		m_mode = ScrollMode;
	}
	m_RButtonDownPos = point;
	m_mouseDownPos = point;
	SetCapture();

	CWnd::OnRButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnRButtonUp(UINT nFlags, CPoint point) 
{
	ReleaseCapture();
	// make sure to reset the mode, otherwise we can end up in scrollmode while the context menu is open
	if (m_mode == ScrollModeDrag)
	{
		m_mode = PortDragMode;
	}
	else
	{
		m_mode = NothingMode;
	}

	if (abs(m_RButtonDownPos.x-point.x)<3 && abs(m_RButtonDownPos.y-point.y)<3)
	{
		// Show context menu if right clicked on the same point.
		CHyperNode *pNode = GetNodeAtPoint( point );
		ShowContextMenu( point,pNode );
		return;
	}

//	CWnd::OnRButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnMButtonDown(UINT nFlags, CPoint point) 
{
	SetFocus();
	m_mode = ScrollMode;
	m_RButtonDownPos = point;
	m_mouseDownPos = point;
	SetCapture();

	CWnd::OnMButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnMButtonUp(UINT nFlags, CPoint point) 
{
	ReleaseCapture();

	m_mode = NothingMode;

	CWnd::OnMButtonDown(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnMButtonDblClk(UINT nFlags, CPoint point) 
{
	CWnd::OnMButtonDblClk(nFlags, point);
}


//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_pMouseOverNode)
		m_pMouseOverNode->UnselectAllPorts();
	m_pMouseOverNode = 0;

	switch (m_mode)
	{
	case MoveMode:
		{
			GetIEditor()->RestoreUndo();
			CPoint offset = point - m_mouseDownPos;
			MoveSelectedNodes( offset );
		}
		break;
	case ScrollMode:
		OnMouseMoveScrollMode(nFlags, point);
		break;
	case SelectMode:
		{
			// Update select rectangle.
			CRect rc( m_mouseDownPos.x,m_mouseDownPos.y,point.x,point.y );
			rc.NormalizeRect();
			CRect rcClient;
			GetClientRect(rcClient);
			rc.IntersectRect(rc,rcClient);

			CDC *dc = GetDC();
			dc->DrawDragRect( rc,CSize(1,1),m_rcSelect,CSize(1,1) );
			ReleaseDC(dc);
			m_rcSelect = rc;
		}
		break;
	case PortDragMode:
		OnMouseMovePortDragMode(nFlags, point);
		break;
	case EdgePointDragMode:
		if (m_pHitEdge)
		{
			if (m_bSplineArrows)
			{
				Gdiplus::PointF p1 = ViewToLocal(point);
				m_pHitEdge->cornerPoints[0] = p1;
				m_pHitEdge->cornerModified = true;
			}
			else
			{
				Gdiplus::PointF p1 = ViewToLocal(point);
				Gdiplus::PointF p2 = ViewToLocal(m_mouseDownPos);
				if (m_nHitEdgePoint < 2)
				{
					float w = p1.X - p2.X;
					m_pHitEdge->cornerW = m_prevCornerW + w;
					m_pHitEdge->cornerModified = true;
				}
				else
				{
					float h = p1.Y - p2.Y;
					m_pHitEdge->cornerH = m_prevCornerH + h;
					m_pHitEdge->cornerModified = true;
				}
			}
			Invalidate();
		}
		break;
	case ScrollModeDrag:
		OnMouseMoveScrollMode(nFlags, point);
		OnMouseMovePortDragMode(nFlags, point);
		break;
	default:
		{
			CHyperNodePort *pPort = 0;
			CHyperNode *pNode = GetNodeAtPoint( point );
			if (pNode)
			{
				m_pMouseOverNode = pNode;
				pPort = pNode->GetPortAtPoint( GraphicsHelper(this), ViewToLocal(point) );
				if (pPort)
				{
					pPort->bSelected = true;
					InvalidateNode(pNode,true);
				}
			}
			UpdateTooltip(pNode,pPort);
		}
	}

	CWnd::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnMouseMoveScrollMode(UINT nFlags, CPoint point)
{
	if (!((abs(m_RButtonDownPos.x-point.x)<3 && abs(m_RButtonDownPos.y-point.y)<3)))
	{
		CPoint offset = point - m_mouseDownPos;
		m_scrollOffset += offset;
		m_mouseDownPos = point;
		SetScrollPos( SB_HORZ,-m_scrollOffset.x );
		SetScrollPos( SB_VERT,-m_scrollOffset.y );
		SetScrollExtents();
		Invalidate(FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnMouseMovePortDragMode(UINT nFlags, CPoint point)
{
	InvalidateEdgeRect( LocalToView(m_pDraggingEdge->pointOut), LocalToView(m_pDraggingEdge->pointIn) );
	/*
	if (CHyperNode * pNode = (CHyperNode*) m_pGraph->FindNode( m_pDraggingEdge->nodeIn ))
	{
	pNode->GetInputs()->at(m_pDraggingEdge->nPortIn).bSelected = false;
	InvalidateNode( pNode );
	}
	if (CHyperNode * pNode = (CHyperNode*) m_pGraph->FindNode( m_pDraggingEdge->nodeOut ))
	InvalidateNode( pNode );
	*/

	CHyperNode *pNodeIn = (CHyperNode*) m_pGraph->FindNode( m_pDraggingEdge->nodeIn );
	CHyperNode *pNodeOut = (CHyperNode*) m_pGraph->FindNode( m_pDraggingEdge->nodeOut );
	if (pNodeIn)
	{
		CHyperNode::Ports *ports = pNodeIn->GetInputs();
		if(ports->size() > m_pDraggingEdge->nPortIn)
			ports->at(m_pDraggingEdge->nPortIn).bSelected = false;
	}

	if (m_bDraggingFixedOutput)
	{
		m_pDraggingEdge->pointIn = ViewToLocal(point);
		m_pDraggingEdge->nodeIn = -1;
	}
	else
	{
		m_pDraggingEdge->pointOut = ViewToLocal(point);
		m_pDraggingEdge->nodeOut = -1;
	}
	CHyperNode* pNode = GetNodeAtPoint( point );
	if (pNode)
	{
		if (CHyperNodePort * pPort = pNode->GetPortAtPoint( GraphicsHelper(this), m_pDraggingEdge->pointIn ))
		{
			if (m_bDraggingFixedOutput && pPort->bInput)
			{
				m_pDraggingEdge->nodeIn = pNode->GetId();
				m_pDraggingEdge->nPortIn = pPort->nPortIndex;
				pNode->GetInputs()->at(m_pDraggingEdge->nPortIn).bSelected = true;
			}
			else if (!m_bDraggingFixedOutput && !pPort->bInput)
			{
				m_pDraggingEdge->nodeOut = pNode->GetId();
				m_pDraggingEdge->nPortOut = pPort->nPortIndex;
				pNode->GetOutputs()->at(m_pDraggingEdge->nPortOut).bSelected = true;
			}
		}
	}
	m_edgesToReroute.insert(m_pDraggingEdge);

	if (pNode)
		InvalidateNode(pNode,true);
	if (pNodeIn)
		InvalidateNode(pNodeIn,true);
	if (pNodeOut)
	{
		if (m_pDraggingEdge->nPortOut >= 0 && m_pDraggingEdge->nPortOut < pNodeOut->GetOutputs()->size())
			pNodeOut->GetOutputs()->at(m_pDraggingEdge->nPortOut).bSelected = true;
		InvalidateNode(pNodeOut,true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::UpdateTooltip( CHyperNode *pNode,CHyperNodePort *pPort )
{
	if (m_tooltip.m_hWnd)
	{
		if (pNode && gSettings.bFlowGraphShowToolTip)
		{
			CString tip;
			if (pPort)
			{
				// Port tooltip.
				CString type;
				switch (pPort->pVar->GetType())
				{
				case IVariable::INT: type = "Integer"; break;
				case IVariable::FLOAT: type = "Float"; break;
				case IVariable::BOOL: type = "Boolean"; break;
				case IVariable::VECTOR: type = "Vector"; break;
				case IVariable::STRING: type = "String"; break;
				//case IVariable::VOID: type = "Void"; break;
				default:
					type = "Any"; break;
				}
				const char *desc = pPort->pVar->GetDescription();
				if (desc && *desc)
					tip.Format( "[%s] %s",(const char*)type,desc );
				else
					tip.Format( "[%s] %s",(const char*)type,(const char*)pNode->GetDescription() );
			}
			else
			{
				// Node tooltip.
				if (strcmp(pNode->GetClassName(), CCommentNode::GetClassType()) == 0)
				{
					tip.Format( "Name: %s\nClass: %s\n%s",
						pNode->GetName(),pNode->GetClassName(),pNode->GetDescription() );
				}
				else
				{	
					CFlowNode *pFlowNode = static_cast<CFlowNode*> (pNode);
					CString cat = pFlowNode->GetCategoryName();
					const uint32 usageFlags = pFlowNode->GetUsageFlags();
					// TODO: something with Usage flags
					tip.Format( "Name: %s\nClass: %s\nCategory: %s\nDescription: %s",
						pFlowNode->GetName(),pFlowNode->GetClassName(),cat.GetString(),pFlowNode->GetDescription() );
				}
			}
			//CString oldtip;
			//m_tooltip.GetText( oldtip,this,1 );
			//if (oldtip != tip)
			{
				m_tooltip.UpdateTipText( tip,this,1 );
				m_tooltip.Activate(TRUE);
			}
			//else
				//m_tooltip.Activate(FALSE);
		}
		else
		{
			m_tooltip.Activate(FALSE);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	CWnd::OnLButtonDblClk(nFlags, point);

	if (m_pGraph == 0)
		return;

	CHyperEdge *pHitEdge = 0;
	int nHitEdgePoint = 0;
	if (HitTestEdge( point,pHitEdge,nHitEdgePoint ))
	{
		// makes only sense for non-AIActions and while fg system is updated
		if (m_pGraph->IsFlowGraph() && m_pGraph->GetAIAction() == 0 && GetIEditor()->GetGameEngine()->IsFlowSystemUpdateEnabled())
		{
			SimulateFlow(pHitEdge);
		}
	}
	else
	{
		CHyperNode *pNode = GetNodeAtPoint( point );
		if (pNode && strcmp(pNode->GetClassName(), CCommentNode::GetClassType()) == 0)
		{
			RenameNode(pNode);
		}
		else
		{
			//int objectClicked = pNode->GetObjectAt( GraphicsHelper(this), ViewToLocal(point) );
			if (m_pGraph->IsFlowGraph())
				OnSelectEntity();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnRButtonDblClk(UINT nFlags, CPoint point) 
{
	CWnd::OnRButtonDblClk(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	bool processed = true;
	switch (nChar)
	{
	case 'F':
		m_bHighlightIncomingEdges = true;
		InvalidateView();
		break;
	case 'G':
		m_bHighlightOutgoingEdges = true;
		InvalidateView();
		break;
	case 'O':
		m_zoom += 0.3f; 
		m_zoom = CLAMP(m_zoom, MIN_ZOOM, MAX_ZOOM);
		InvalidateView();
		break;
	case 'P':
		m_zoom -= 0.3f;
		m_zoom = CLAMP(m_zoom, MIN_ZOOM, MAX_ZOOM);
		InvalidateView();
		break;
	default:
		CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	bool processed = false;
	if (nChar == 'F') {
		processed = true;
		m_bHighlightIncomingEdges = false;
		InvalidateView();
	} else if (nChar == 'G') {
		processed = true;
		m_bHighlightOutgoingEdges = false;
		InvalidateView();
	} else if (nChar == 'R') {
		processed = true;
		ForceRedraw();
	}
	if (!processed)
		CWnd::OnKeyUp(nChar, nRepCnt, nFlags);
}

//////////////////////////////////////////////////////////////////////////perforce
BOOL CHyperGraphView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if( m_mode == NothingMode )
	{
		CPoint point;
		GetCursorPos(&point);
		ScreenToClient(&point);

		CHyperEdge *pHitEdge = 0;
		int nHitEdgePoint = 0;
		if (HitTestEdge( point,pHitEdge,nHitEdgePoint ))
		{
			HCURSOR hCursor;
			hCursor = AfxGetApp()->LoadCursor(IDC_ARRWHITE);
			SetCursor(hCursor);
			return TRUE;
		}
	}

	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::InvalidateEdgeRect( CPoint p1,CPoint p2 )
{
	CRect rc(p1,p2);
	rc.NormalizeRect();
	rc.InflateRect(20,7,20,7);
	InvalidateRect(rc);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::InvalidateEdge( CHyperEdge *pEdge )
{
	m_edgesToReroute.insert(pEdge);
	InvalidateEdgeRect( LocalToView(pEdge->pointIn),LocalToView(pEdge->pointOut) );
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::InvalidateNode( CHyperNode *pNode,bool bRedraw )
{
	assert( pNode );
	if (!m_pGraph)
		return;

	CDC *pDC = GetDC();
	{
		Gdiplus::Graphics gr(pDC->GetSafeHdc());
		Gdiplus::RectF rc = pNode->GetRect();
		Gdiplus::SizeF sz = pNode->CalculateSize(gr);
		rc.Width = sz.Width;
		rc.Height = sz.Height;
		pNode->SetRect(rc);
		rc.Inflate(GRID_SIZE, GRID_SIZE);
		InvalidateRect( LocalToViewRect(rc),FALSE );

		if (bRedraw)
			pNode->Invalidate(true);

		// Invalidate all edges connected to this node.
		std::vector<CHyperEdge*> edges;
		if (m_pGraph->FindEdges( pNode,edges ))
		{
			// Invalidate all node edges.
			for (int i = 0; i < edges.size(); i++)
			{
				InvalidateEdge( edges[i] );
			}
		}
	}

	ReleaseDC(pDC);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::DrawEdges( Gdiplus::Graphics &gr,const CRect &rc )
{
	std::vector<CHyperEdge*> edges;
	m_pGraph->GetAllEdges( edges );

	m_edgeHelpers.clear();

	for (int i = 0; i < edges.size(); i++)
	{
		CHyperNode *nodeIn = (CHyperNode*)m_pGraph->FindNode(edges[i]->nodeIn);
		CHyperNode *nodeOut = (CHyperNode*)m_pGraph->FindNode(edges[i]->nodeOut);
		if (!nodeIn || !nodeOut)
			continue;
		CHyperNodePort *portIn = nodeIn->FindPort( edges[i]->portIn,true );
		CHyperNodePort *portOut = nodeOut->FindPort( edges[i]->portOut,false );
		if (!portIn || !portOut)
			continue;

		// Draw arrow.
		int selection = 0;
		selection |= (m_bHighlightIncomingEdges && nodeIn->IsSelected())  /* << 0 */ ;
		selection |= (m_bHighlightOutgoingEdges && nodeOut->IsSelected()) << 1;
		if(selection == 0)
			selection = (nodeIn->IsPortActivationModified(portIn))?4:0;
		Gdiplus::RectF box = DrawArrow( gr,edges[i],true,selection);
		m_edgeHelpers[edges[i]] = box;
		edges[i]->DrawSpecial( &gr, Gdiplus::PointF(box.X + box.Width/2, box.Y + box.Height/2) );
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::DrawNodes( Gdiplus::Graphics& gr,const CRect &rc )
{
	std::vector<CHyperNode*> nodes;
	GetNodesInRect( rc,nodes );
	for (int i = 0; i < nodes.size(); i++)
	{
		CHyperNode *pNode = nodes[i];
		CRect itemRect = LocalToViewRect(pNode->GetRect());

//		assert(false);
		pNode->Draw( this,gr,true );
	}
}

//////////////////////////////////////////////////////////////////////////
CRect CHyperGraphView::LocalToViewRect( const Gdiplus::RectF &localRect ) const
{
	Gdiplus::RectF temp = localRect;
	temp.X *= m_zoom;
	temp.Y *= m_zoom;
	temp.Width *= m_zoom;
	temp.Height *= m_zoom;
	temp.Offset( Gdiplus::PointF(m_scrollOffset.x, m_scrollOffset.y) );
	return CRect(temp.X-1.0f, temp.Y-1.0f, temp.GetRight()+1.05f, temp.GetBottom()+1.0f);
}

//////////////////////////////////////////////////////////////////////////
Gdiplus::RectF CHyperGraphView::ViewToLocalRect( const CRect &viewRect ) const
{
	Gdiplus::RectF rc( viewRect.left, viewRect.top, viewRect.Width(), viewRect.Height() );
	rc.Offset( -m_scrollOffset.x, -m_scrollOffset.y );
	rc.X /= m_zoom;
	rc.Y /= m_zoom;
	rc.Width /= m_zoom;
	rc.Height /= m_zoom;
	return rc;
}

//////////////////////////////////////////////////////////////////////////
CPoint CHyperGraphView::LocalToView( Gdiplus::PointF point )
{
	return CPoint(point.X*m_zoom+m_scrollOffset.x,point.Y*m_zoom+m_scrollOffset.y);
}

//////////////////////////////////////////////////////////////////////////
Gdiplus::PointF CHyperGraphView::ViewToLocal( CPoint point )
{
	return Gdiplus::PointF((point.x-m_scrollOffset.x)/m_zoom,(point.y-m_scrollOffset.y)/m_zoom);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnHyperGraphEvent( IHyperNode *pINode,EHyperGraphEvent event )
{
	CHyperNode *pNode = (CHyperNode*)pINode;
	switch (event)
	{
	case EHG_GRAPH_REMOVED:
		if (!m_pGraph->GetManager())
			SetGraph( NULL );
		break;
	case EHG_GRAPH_INVALIDATE:
		m_bHighlightOutgoingEdges = false;
		m_bHighlightIncomingEdges = false;
		m_edgesToReroute.clear();
		m_edgeHelpers.clear();
		m_pEditedNode = 0;
		m_pMouseOverNode = 0;
		m_moveHelper.clear();
		InvalidateView();
		OnSelectionChange();
		break;
	case EHG_NODE_ADD:
		InvalidateView();
		break;
	case EHG_NODE_DELETE:
		if (pINode == (CHyperNode*)m_pMouseOverNode)
			m_pMouseOverNode = 0;
		InvalidateView();
		break;
	case EHG_NODE_CHANGE:
		{
			InvalidateNode(pNode);
		}
		break;
	case EHG_NODE_CHANGE_DEBUG_PORT:
		InvalidateNode(pNode, true);
		if(m_pGraph && m_pGraph->FindNode(pNode->GetId()))
			OnPaint();
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::SetGraph( CHyperGraph *pGraph )
{
	if (m_pGraph == pGraph)
		return;

	m_bHighlightOutgoingEdges = false;
	m_bHighlightIncomingEdges = false;
	m_edgesToReroute.clear();
	m_edgeHelpers.clear();
	m_pEditedNode = 0;
	m_pMouseOverNode = 0;
	m_moveHelper.clear();

	if (m_pGraph)
	{
		// CryLogAlways("CHyperGraphView: (1) Removing as listener from 0x%p before switching to 0x%p", m_pGraph, pGraph);
		m_pGraph->RemoveListener(this);
		// CryLogAlways("CHyperGraphView: (2) Removing as listener from 0x%p before switching to 0x%p", m_pGraph, pGraph);
	}

	bool bFitAll = true;

	m_pGraph = pGraph;
	if (m_pGraph)
	{
		if (m_pGraph->GetViewPosition( m_scrollOffset,m_zoom ))
		{
			bFitAll = false;
			SetScrollPos( SB_HORZ,-m_scrollOffset.x );
			SetScrollPos( SB_VERT,-m_scrollOffset.y );
			InvalidateView();
		}

		RerouteAllEdges();
		m_pGraph->AddListener( this );
	}

	// Invalidate all view.
	if (m_hWnd && bFitAll)
	{
		OnCommandFitAll();
	}
	if (m_hWnd) {
		OnSelectionChange();
	}

	if(m_pGraph && m_pGraph->IsNodeActivationModified())
		OnPaint();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandFitAll()
{
	CRect rc;
	GetClientRect(rc);

	if (rc.IsRectEmpty())
	{
		m_bRefitGraphInOnSize = true;
		return;
	}
	m_scrollOffset.SetPoint(0,0);
	m_zoom = 1.0f;

	std::vector<CHyperNode*> nodes;
	GetNodesInRect( rc,nodes ); // Calc graph extents.

	CRect rcGraph = LocalToViewRect(m_graphExtents);

	float zoom = (float)max(rc.Width(),rc.Height()) / (max(rcGraph.Width(),rcGraph.Height()) + GRID_SIZE*4);
	if (zoom < 1)
		m_zoom = CLAMP( zoom, MIN_ZOOM, MAX_ZOOM ); 

	rcGraph = LocalToViewRect(m_graphExtents);

	if (rcGraph.Width() < rc.Width())
		m_scrollOffset.x += (rc.Width() - rcGraph.Width()) / 2;
	if (rcGraph.Height() < rc.Height())
		m_scrollOffset.y += (rc.Height() - rcGraph.Height()) / 2;

	m_scrollOffset.x += -rcGraph.left + GRID_SIZE;
	m_scrollOffset.y += -rcGraph.top + GRID_SIZE;

	SetScrollPos( SB_HORZ,-m_scrollOffset.x );
	SetScrollPos( SB_VERT,-m_scrollOffset.y );

	InvalidateView();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::ShowAndSelectNode(CHyperNode* pToNode, bool bSelect)
{
	assert (pToNode != 0);

	if (!m_pGraph)
		return;

	CRect rc;
	GetClientRect(rc);

	std::vector<CHyperNode*> nodes;
	GetNodesInRect( rc,nodes ); // Calc graph extents.

	CRect nodeRect = LocalToViewRect(pToNode->GetRect());

	// check if fully inside
	if (nodeRect.left >= rc.left &&
		nodeRect.top >= rc.top &&
		nodeRect.bottom <= rc.bottom &&
		nodeRect.right <= rc.right)
	{
		// fully inside
		// do nothing yet...
	}
	else
	{
		if (rc.IsRectEmpty())
		{
			m_bRefitGraphInOnSize = true;
		}
		else
		{
			m_scrollOffset.SetPoint(0,0);
			CRect rcGraph = LocalToViewRect(pToNode->GetRect());
			float zoom = (float)max(rc.Width(),rc.Height()) / (max(rcGraph.Width(),rcGraph.Height()) + GRID_SIZE*4);
			if (zoom < 1)
				m_zoom = CLAMP( zoom, MIN_ZOOM, MAX_ZOOM ); 
			rcGraph = LocalToViewRect(pToNode->GetRect());

			if (rcGraph.Width() < rc.Width())
				m_scrollOffset.x += (rc.Width() - rcGraph.Width()) / 2;
			if (rcGraph.Height() < rc.Height())
				m_scrollOffset.y += (rc.Height() - rcGraph.Height()) / 2;

			m_scrollOffset.x += -rcGraph.left + GRID_SIZE;
			m_scrollOffset.y += -rcGraph.top + GRID_SIZE;

			SetScrollPos( SB_HORZ,-m_scrollOffset.x );
			SetScrollPos( SB_VERT,-m_scrollOffset.y );
		}
	}

	if (bSelect)
	{
		IHyperGraphEnumerator *pEnum = m_pGraph->GetNodesEnumerator();
		for (IHyperNode *pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
		{
			CHyperNode *pNode = (CHyperNode*)pINode;
			if (pNode->IsSelected())
				pNode->SetSelected(false);
		}
		pToNode->SetSelected(true);

		OnSelectionChange();
		pEnum->Release();
	}
	InvalidateView();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::DrawGrid( Gdiplus::Graphics &gr,const CRect &updateRect )
{
	int gridStep = 10;

	float z = m_zoom;
	assert (z >= MIN_ZOOM);
	while (z < 0.99f)
	{
		z *= 2;
		gridStep *= 2;
	}

	// Draw grid line every 5 pixels.	
	Gdiplus::RectF updLocalRect = ViewToLocalRect(updateRect);
	float startX = updLocalRect.X - fmodf(updLocalRect.X, gridStep);
	float startY = updLocalRect.Y - fmodf(updLocalRect.Y, gridStep);
	float stopX = startX + updLocalRect.Width;
	float stopY = startY + updLocalRect.Height;

	Gdiplus::Pen gridPen( GRID_COLOR, 1.0f );

	// Draw vertical grid lines.
	for (float x = startX; x < stopX; x += gridStep)
		gr.DrawLine( &gridPen, Gdiplus::PointF(x,startY), Gdiplus::PointF(x,stopY) );

	// Draw horizontal grid lines.
	for (float y = startY; y < stopY; y += gridStep)
		gr.DrawLine( &gridPen, Gdiplus::PointF(startX,y), Gdiplus::PointF(stopX,y) );
}

//////////////////////////////////////////////////////////////////////////
Gdiplus::RectF CHyperGraphView::DrawArrow( Gdiplus::Graphics &gr,CHyperEdge *pEdge, bool helper, int selection)
{
	Gdiplus::PointF pout = pEdge->pointOut;
	Gdiplus::PointF pin = pEdge->pointIn;
	EHyperEdgeDirection dout = pEdge->dirOut;
	EHyperEdgeDirection din = pEdge->dirIn;

	struct 
	{
		float x1, y1;
		float x2, y2;
	}
	h[] =
	{
		{0,-0.5f,0,-0.5f},
		{0,0.5f,0,0.5f},
		{-0.5f,0,-0.5f,0},
		{0.5f,0,0.5f,0}
	};

	float dx = CLAMP(fabsf(pout.X - pin.X), 20.0f, 150.0f);
	float dy = CLAMP(fabsf(pout.Y - pin.Y), 20.0f, 150.0f);

	if (fabsf(pout.X - pin.X) < 0.0001f && fabsf(pout.Y - pin.Y) < 0.0001f)
		return Gdiplus::RectF(0,0,0,0);

	Gdiplus::PointF pnts[6];

	pnts[0] = Gdiplus::PointF(pout.X,pout.Y);
	pnts[1] = Gdiplus::PointF(pnts[0].X+h[dout].x1*dx,pnts[0].Y+h[dout].y1*dy);
	pnts[3] = Gdiplus::PointF(pin.X,pin.Y);
	pnts[2] = Gdiplus::PointF(pnts[3].X+h[din].x2*dx,pnts[3].Y+h[din].y2*dy);
	Gdiplus::PointF center = Gdiplus::PointF((pnts[1].X+pnts[2].X)*0.5f, (pnts[1].Y+pnts[2].Y)*0.5f);

	float zoom = m_zoom;
	if (zoom > 1)
		zoom = 1;

	Gdiplus::Color color;
	switch (selection)
	{
	case 1: // incoming
		color = ARROW_SEL_COLOR_IN;
		break;
	case 2: // outgoing
		color = ARROW_SEL_COLOR_OUT;
		break;
	case 3: // incoming+outgoing
		color = Gdiplus::Color(244,244,244);
		break;
	case 4: // debugging port activation
		color = PORT_DEBUGGING_COLOR;
		break;
	default:
		color = ARROW_COLOR;
		break;
	}
	Gdiplus::Pen pen( color );
	Gdiplus::SolidBrush  brush( color );
	Gdiplus::AdjustableArrowCap cap(5*zoom,6*zoom);

	pen.SetCustomEndCap( &cap );
	if (pEdge->enabled == false) {
		pen.SetColor(ARROW_DIS_COLOR);
		brush.SetColor(ARROW_DIS_COLOR);
		pen.SetWidth(2.0f);
		pen.SetDashStyle(Gdiplus::DashStyleDot);
	}

	float HELPERSIZE = 4.0f;
	Gdiplus::RectF helperRect( center.X-HELPERSIZE/2, center.Y-HELPERSIZE/2, HELPERSIZE, HELPERSIZE );

	if (m_bSplineArrows)
	{
		if (pEdge->cornerModified)
		{
			float cdx = pEdge->cornerPoints[0].X - center.X;
			float cdy = pEdge->cornerPoints[0].Y - center.Y;

			dx += cdx;
			dy += cdy;

			pnts[0] = Gdiplus::PointF(pout.X,pout.Y);
			pnts[1] = Gdiplus::PointF(pnts[0].X+h[dout].x1*dx,pnts[0].Y+h[dout].y1*dy);
			pnts[3] = Gdiplus::PointF(pin.X,pin.Y);
			pnts[2] = Gdiplus::PointF(pnts[3].X+h[din].x2*dx,pnts[3].Y+h[din].y2*dy);
		}
		else
			pEdge->cornerPoints[0] = center;

		gr.DrawBeziers( &pen,pnts, 4 );

		if (helper)
			gr.DrawEllipse( &pen, helperRect );
	}
	else
	{
		float w = 20 + pEdge->nPortOut*10;
		if (w > fabs((pout.X-pin.X)/2))
			w = fabs((pout.X-pin.X)/2);

		if (!pEdge->cornerModified)
		{
			pEdge->cornerW = w;
			pEdge->cornerH = 40;
		}
		w = pEdge->cornerW;
		float ph = pEdge->cornerH;

		if (pin.X >= pout.X)
		{
			pnts[0] = pout;
			pnts[1] = Gdiplus::PointF(pout.X+w,pout.Y);
			pnts[2] = Gdiplus::PointF(pout.X+w,pin.Y);
			pnts[3] = pin;
			gr.DrawLines( &pen,pnts,4 );

			pEdge->cornerPoints[0] = pnts[1];
			pEdge->cornerPoints[1] = pnts[2];
			pEdge->cornerPoints[2] = Gdiplus::PointF(0,0);
			pEdge->cornerPoints[3] = Gdiplus::PointF(0,0);
		}
		else
		{
			pnts[0] = pout;
			pnts[1] = Gdiplus::PointF(pout.X+w,pout.Y);
			pnts[2] = Gdiplus::PointF(pout.X+w,pout.Y+ph);
			pnts[3] = Gdiplus::PointF(pin.X-w,pout.Y+ph);
			pnts[4] = Gdiplus::PointF(pin.X-w,pin.Y);
			pnts[5] = pin;
			gr.DrawLines( &pen,pnts,6 );

			pEdge->cornerPoints[0] = pnts[1];
			pEdge->cornerPoints[1] = pnts[2];
			pEdge->cornerPoints[2] = pnts[3];
			pEdge->cornerPoints[3] = pnts[4];
		}
		if (helper)
		{
			gr.DrawRectangle( &pen,Gdiplus::RectF(pnts[1].X-HELPERSIZE/2,pnts[1].Y-HELPERSIZE/2,HELPERSIZE,HELPERSIZE ) );
			gr.DrawRectangle( &pen,Gdiplus::RectF(pnts[2].X-HELPERSIZE/2,pnts[2].Y-HELPERSIZE/2,HELPERSIZE,HELPERSIZE ) );
		}
	}

	return helperRect;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::CaptureMouse()
{
	if (GetCapture() != this)
	{
		SetCapture();
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::ReleaseMouse()
{
	if (GetCapture() == this)
	{
		ReleaseCapture();
	}
}

//////////////////////////////////////////////////////////////////////////
CHyperNode* CHyperGraphView::GetNodeAtPoint( CPoint point )
{
	if (!m_pGraph)
		return 0;
	std::vector<CHyperNode*> nodes;
	if (!GetNodesInRect( CRect(point.x,point.y,point.x,point.y),nodes ))
		return 0;

	// Return last node in the list.
	return nodes[nodes.size()-1];
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraphView::GetNodesInRect( const CRect &viewRect,std::vector<CHyperNode*> &nodes,bool bFullInside )
{
	if (!m_pGraph)
		return false;
	bool bFirst = true;
	m_graphExtents = Gdiplus::RectF(0,0,0,0);
	Gdiplus::RectF localRect = ViewToLocalRect(viewRect);
	IHyperGraphEnumerator *pEnum = m_pGraph->GetNodesEnumerator();
	nodes.clear();
	for (IHyperNode *pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
	{
		CHyperNode *pNode = (CHyperNode*)pINode;
		const Gdiplus::RectF &itemRect = pNode->GetRect();

		if (bFirst)
		{
			m_graphExtents = itemRect;
			bFirst = false;
		}
		else
		{
			Gdiplus::RectF rc = m_graphExtents;
			m_graphExtents.Union( m_graphExtents, rc, itemRect );
		}

		if (!localRect.IntersectsWith(itemRect))
			continue;
		if (bFullInside && !localRect.Contains(itemRect))
			continue;
		nodes.push_back( pNode );
	}
	pEnum->Release();

	return !nodes.empty();
}

//////////////////////////////////////////////////////////////////////////
bool CHyperGraphView::GetSelectedNodes( std::vector<CHyperNode*> &nodes, SelectionSetType setType )
{
	if (!m_pGraph)
		return false;

	bool onlyParents = false;
	bool includeRelatives = false;
	switch (setType)
	{
	case SELECTION_SET_INCLUDE_RELATIVES:
		includeRelatives = true;
		break;
	case SELECTION_SET_ONLY_PARENTS:
		onlyParents = true;
		break;
	case SELECTION_SET_NORMAL:
		break;
	}

	if (includeRelatives)
	{
		std::set<CHyperNode*> nodeSet;

		IHyperGraphEnumerator *pEnum = m_pGraph->GetNodesEnumerator();
		for (IHyperNode *pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
		{
			CHyperNode *pNode = (CHyperNode*)pINode;
			if (pNode->IsSelected())
			{
				//nodes.push_back( pNode );
				IHyperGraphEnumerator* pRelativesEnum = pNode->GetRelatedNodesEnumerator();
				for (IHyperNode *pRelative = pRelativesEnum->GetFirst(); pRelative; pRelative = pRelativesEnum->GetNext())
					nodeSet.insert(static_cast<CHyperNode*>(pRelative));
				pRelativesEnum->Release();
			}
		}
		pEnum->Release();

		nodes.clear();
		nodes.reserve(nodeSet.size());
		std::copy(nodeSet.begin(), nodeSet.end(), std::back_inserter(nodes));
	}
	else
	{
		IHyperGraphEnumerator *pEnum = m_pGraph->GetNodesEnumerator();
		nodes.clear();
		for (IHyperNode *pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
		{
			CHyperNode *pNode = (CHyperNode*)pINode;
			if (pNode->IsSelected() && (!onlyParents || pNode->GetParent() == 0))
				nodes.push_back( pNode );
		}
		pEnum->Release();
	}

	return !nodes.empty();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::ClearSelection()
{
	if (!m_pGraph)
		return;

	IHyperGraphEnumerator *pEnum = m_pGraph->GetNodesEnumerator();
	for (IHyperNode *pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
	{
		CHyperNode *pNode = (CHyperNode*)pINode;
		if (pNode->IsSelected())
			pNode->SetSelected(false);
	}
	OnSelectionChange();
	pEnum->Release();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::MoveSelectedNodes( CPoint offset )
{
	std::vector<CHyperNode*> nodes;
	if (!GetSelectedNodes( nodes ))
		return;

	Gdiplus::RectF bounds;

	for (int i = 0; i < nodes.size(); i++)
	{
		// Only move parent nodes.
		if (nodes[i]->GetParent() == 0)
		{
			if (m_moveHelper.find(nodes[i]) == m_moveHelper.end())
				m_moveHelper[nodes[i]] = nodes[i]->GetPos();

			if (!i)
				bounds = nodes[i]->GetRect();
			else
				Gdiplus::RectF::Union( bounds, bounds, nodes[i]->GetRect() );
			//InvalidateRect( LocalToViewRect(rect),FALSE );
			
			// Snap rectangle to the grid.
			Gdiplus::PointF pos = nodes[i]->GetPos();
			Gdiplus::PointF firstPos = m_moveHelper[nodes[i]];
			pos.X = firstPos.X + offset.x / m_zoom;
			pos.Y = firstPos.Y + offset.y / m_zoom;
			pos.X = floor(((float)pos.X/GRID_SIZE) + 0.5f) * GRID_SIZE;
			pos.Y = floor(((float)pos.Y/GRID_SIZE) + 0.5f) * GRID_SIZE;

			nodes[i]->SetPos( pos );
			InvalidateNode( nodes[i] );
			IHyperGraphEnumerator* pRelativesEnum = nodes[i]->GetRelatedNodesEnumerator();
			for (IHyperNode* pRelative = pRelativesEnum->GetFirst(); pRelative; pRelative = pRelativesEnum->GetNext())
			{
				CHyperNode* pRelativeNode = static_cast<CHyperNode*>(pRelative);
				Gdiplus::RectF::Union(m_graphExtents,m_graphExtents,pRelativeNode->GetRect());
				Gdiplus::RectF::Union( bounds, bounds, pRelativeNode->GetRect() );
			}
			pRelativesEnum->Release();
		}
	}
	CRect invRect = LocalToViewRect(bounds);
	invRect.InflateRect(32,32,32,32);
	InvalidateRect( invRect );
	SetScrollExtents();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnSelectEntity()
{
	std::vector<CHyperNode*> nodes;
	if (!GetSelectedNodes( nodes ))
		return;

	CUndo undo( "Select Object(s)" );
	GetIEditor()->ClearSelection();
	for (int i = 0; i < nodes.size(); i++)
	{
		// only can CFlowNode* if not a comment, argh...
		if (strcmp(nodes[i]->GetClassName(), CCommentNode::GetClassType()) != 0)
		{
			CFlowNode *pFlowNode = (CFlowNode*)nodes[i];
			if (pFlowNode->GetEntity())
			{
				GetIEditor()->SelectObject( pFlowNode->GetEntity() );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnToggleMinimize()
{
	std::vector<CHyperNode*> nodes;
	if (!GetSelectedNodes( nodes ))
		return;

	for (int i = 0; i < nodes.size(); i++)
	{
		OnToggleMinimizeNode(nodes[i]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnToggleMinimizeNode( CHyperNode *pNode )
{
	bool bMinimize = false;
	if (!bMinimize)
	{
		for (int i = 0; i < pNode->GetInputs()->size(); i++)
		{
			CHyperNodePort *pPort = &pNode->GetInputs()->at(i);
			if (pPort->bVisible && (pPort->nConnected==0))
			{
				bMinimize = true;
				break;
			}
		}
	}
	if (!bMinimize)
	{
		for (int i = 0; i < pNode->GetOutputs()->size(); i++)
		{
			CHyperNodePort *pPort = &pNode->GetOutputs()->at(i);
			if (pPort->bVisible && (pPort->nConnected==0))
			{
				bMinimize = true;
				break;
			}
		}
	}

	bool bVisible = !bMinimize;
	{
		for (int i = 0; i < pNode->GetInputs()->size(); i++)
		{
			CHyperNodePort *pPort = &pNode->GetInputs()->at(i);
			pPort->bVisible = bVisible;
		}
	}
	{
		for (int i = 0; i < pNode->GetOutputs()->size(); i++)
		{
			CHyperNodePort *pPort = &pNode->GetOutputs()->at(i);
			pPort->bVisible = bVisible;
		}
	}
	InvalidateNode(pNode,true);
	Invalidate();
}

#define ID_GRAPHVIEW_RENAME 1
#define ID_GRAPHVIEW_ADD_SELECTED_ENTITY 2
#define ID_GRAPHVIEW_ADD_DEFAULT_ENTITY 3
#define ID_GRAPHVIEW_TARGET_SELECTED_ENTITY 4
#define ID_GRAPHVIEW_TARGET_GRAPH_ENTITY 5
#define ID_GRAPHVIEW_TARGET_GRAPH_ENTITY2 6
#define ID_GRAPHVIEW_ADD_COMMENT 10
#define ID_GRAPHVIEW_FIT_TOVIEW 11
#define ID_GRAPHVIEW_SELECT_ENTITY 12
#define ID_GRAPHVIEW_SPLINES 13
#define ID_INPUTS_SHOW_ALL 20
#define ID_INPUTS_HIDE_ALL 21
#define ID_OUTPUTS_SHOW_ALL 22
#define ID_OUTPUTS_HIDE_ALL 23
#define BASE_NEW_NODE_CMD 10000
#define BASE_INPUTS_CMD 20000
#define BASE_OUTPUTS_CMD 21000

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::ShowContextMenu( CPoint point,CHyperNode *pNode )
{
	if (!m_pGraph)
		return;
	CMenu menu;
	menu.CreatePopupMenu();

	std::vector<CString> classes;
	std::vector<CMenu*> groups;
	CMenu classMenu;

	CPoint screenPoint = point;
	ClientToScreen(&screenPoint);

	if (pNode)
	{
		if (m_pGraph->GetAIAction() != 0)
		{
			menu.AppendMenu( MF_STRING,ID_GRAPHVIEW_TARGET_GRAPH_ENTITY,"Assign User entity" );
			menu.AppendMenu( MF_STRING,ID_GRAPHVIEW_TARGET_GRAPH_ENTITY2,"Assign Object entity" );
		}
		else if (pNode->CheckFlag(EHYPER_NODE_ENTITY))
		{
			menu.AppendMenu( MF_STRING,ID_GRAPHVIEW_TARGET_SELECTED_ENTITY,"Assign selected entity" );
			menu.AppendMenu( MF_STRING,ID_GRAPHVIEW_TARGET_GRAPH_ENTITY,"Assign graph entity" );
			// menu.AppendMenu( MF_STRING,ID_GRAPHVIEW_TARGET_GRAPH_ENTITY2,"Assign second graph entity" );  // alexl 10/11/05: currently there is no 2nd graph entity
		}
		menu.AppendMenu( MF_STRING,ID_GRAPHVIEW_RENAME,"Rename" );
		menu.AppendMenu( MF_SEPARATOR );

		CMenu inputsMenu;
		inputsMenu.CreatePopupMenu();
		CMenu outputsMenu;
		outputsMenu.CreatePopupMenu();

		menu.AppendMenu( MF_POPUP,(UINT_PTR)inputsMenu.GetSafeHmenu(),"Inputs" );
		menu.AppendMenu( MF_POPUP,(UINT_PTR)outputsMenu.GetSafeHmenu(),"Outputs" );

		//////////////////////////////////////////////////////////////////////////
		// Inputs.
		//////////////////////////////////////////////////////////////////////////
		for (int i = 0; i < pNode->GetInputs()->size(); i++)
		{
			CHyperNodePort *pPort = &pNode->GetInputs()->at(i);
			inputsMenu.AppendMenu( MF_STRING |
				((pPort->bVisible||pPort->nConnected!=0)?MF_CHECKED:0) |
				((pPort->nConnected!=0)?MF_GRAYED:0),
				BASE_INPUTS_CMD+i,pPort->GetHumanName() );
		}
		inputsMenu.AppendMenu( MF_SEPARATOR );
		inputsMenu.AppendMenu( MF_STRING,ID_INPUTS_SHOW_ALL,"Show All" );
		inputsMenu.AppendMenu( MF_STRING,ID_INPUTS_HIDE_ALL,"Hide All" );
		//////////////////////////////////////////////////////////////////////////
		
		//////////////////////////////////////////////////////////////////////////
		// Outputs.
		//////////////////////////////////////////////////////////////////////////
		for (int i = 0; i < pNode->GetOutputs()->size(); i++)
		{
			CHyperNodePort *pPort = &pNode->GetOutputs()->at(i);
			outputsMenu.AppendMenu( MF_STRING | 
				((pPort->bVisible||pPort->nConnected!=0)?MF_CHECKED:0) |
				((pPort->nConnected!=0)?MF_GRAYED:0),
				BASE_OUTPUTS_CMD+i,pPort->GetHumanName() );
		}
		outputsMenu.AppendMenu( MF_SEPARATOR );
		outputsMenu.AppendMenu( MF_STRING,ID_OUTPUTS_SHOW_ALL,"Show All" );
		outputsMenu.AppendMenu( MF_STRING,ID_OUTPUTS_HIDE_ALL,"Hide All" );
		//////////////////////////////////////////////////////////////////////////

		menu.AppendMenu( MF_SEPARATOR );
	}
	else
	{
		VERIFY( classMenu.CreatePopupMenu() );
		PopulateClassMenu( classMenu,classes,groups );
		menu.AppendMenu( MF_POPUP,(UINT_PTR)classMenu.GetSafeHmenu(),"Add Node" );
		menu.AppendMenu( MF_STRING,ID_GRAPHVIEW_ADD_SELECTED_ENTITY,"Add Selected Entity" );
		if (m_pGraph->GetAIAction() == 0)
		{
			menu.AppendMenu( MF_STRING,ID_GRAPHVIEW_ADD_DEFAULT_ENTITY,"Add Graph Default Entity" );
		}
		menu.AppendMenu( MF_STRING,ID_GRAPHVIEW_ADD_COMMENT,"Add Comment" );
		menu.AppendMenu( MF_SEPARATOR );
	}

	// not doing this right now -> ensuring consistent (=broken!) UI
	// CClipboard clipboard;
	// int pasteFlags = clipboard.IsEmpty() ? MF_GRAYED : 0;

	menu.AppendMenu( MF_STRING,ID_EDIT_CUT,"Cut" );
	menu.AppendMenu( MF_STRING,ID_EDIT_COPY,"Copy" );
	menu.AppendMenu( MF_STRING,ID_EDIT_PASTE,"Paste" );
	menu.AppendMenu( MF_STRING,ID_EDIT_PASTE_WITH_LINKS,"Paste with Links" );
	menu.AppendMenu( MF_STRING,ID_EDIT_DELETE,"Delete" );
	menu.AppendMenu( MF_SEPARATOR );
	menu.AppendMenu( MF_STRING,ID_FILE_EXPORTSELECTION,"Export Selected" );
	menu.AppendMenu( MF_STRING,ID_FILE_IMPORT,"Import" );
	menu.AppendMenu( MF_STRING,ID_GRAPHVIEW_SELECT_ENTITY,"Select Assigned Entity" );
	menu.AppendMenu( MF_STRING|(m_bSplineArrows?(MF_CHECKED):0),ID_GRAPHVIEW_SPLINES,"Show Spline Arrows" );
	menu.AppendMenu( MF_STRING,ID_GRAPHVIEW_FIT_TOVIEW,"Fit Graph to View" );

	int cmd = menu.TrackPopupMenu( TPM_RETURNCMD|TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_NONOTIFY,screenPoint.x,screenPoint.y,this );
	// Release group menus.
	for (int i = 0; i < groups.size(); i++)
	{
		delete groups[i];
	}
	if (cmd >= BASE_NEW_NODE_CMD && cmd < BASE_NEW_NODE_CMD+classes.size())
	{
		CString cls = classes[cmd-BASE_NEW_NODE_CMD];
		CreateNode( cls,point );
		return;
	}

	if (cmd >= BASE_INPUTS_CMD && cmd < BASE_INPUTS_CMD+1000)
	{
		if (pNode)
		{
			CUndo undo( "Graph Port Visibilty" );
			pNode->RecordUndo();
			int nPort = cmd - BASE_INPUTS_CMD;
			pNode->GetInputs()->at(nPort).bVisible = !pNode->GetInputs()->at(nPort).bVisible;
			InvalidateNode(pNode,true);
			Invalidate();
		}
		return;
	}
	if (cmd >= BASE_OUTPUTS_CMD && cmd < BASE_OUTPUTS_CMD+1000)
	{
		if (pNode)
		{
			CUndo undo( "Graph Port Visibilty" );
			pNode->RecordUndo();
			int nPort = cmd - BASE_OUTPUTS_CMD;
			pNode->GetOutputs()->at(nPort).bVisible = !pNode->GetOutputs()->at(nPort).bVisible;
			InvalidateNode(pNode,true);
			Invalidate();
		}
		return;
	}


	switch (cmd)
	{
	case ID_EDIT_CUT:
		OnCommandCut();
		break;
	case ID_EDIT_COPY:
		OnCommandCopy();
		break;
	case ID_EDIT_PASTE:
		InternalPaste(false, point);
		break;
	case ID_EDIT_PASTE_WITH_LINKS:
		InternalPaste(true, point);
		break;
	case ID_EDIT_DELETE:
		OnCommandDelete();
		break;
	case ID_GRAPHVIEW_RENAME:
		RenameNode( pNode );
		break;
	case ID_FILE_EXPORTSELECTION:
	case ID_FILE_IMPORT:
		if (GetParent())
			GetParent()->SendMessage( WM_COMMAND,MAKEWPARAM(cmd,0),NULL );
		break;
	case ID_GRAPHVIEW_ADD_SELECTED_ENTITY:
		{
			CreateNode( "selected_entity",point );
		}
		break;
	case ID_GRAPHVIEW_SPLINES:
		m_bSplineArrows = !m_bSplineArrows;
		Invalidate();
		break;
	case ID_GRAPHVIEW_ADD_DEFAULT_ENTITY:
		{
			CreateNode( "default_entity",point );
		}
		break;
	case ID_GRAPHVIEW_ADD_COMMENT:
		{
			CHyperNode *pNode = CreateNode( CCommentNode::GetClassType(), point );
			if (pNode)
				RenameNode( pNode );
		}
		break;
	case ID_GRAPHVIEW_FIT_TOVIEW:
		OnCommandFitAll();
		break;
	case ID_GRAPHVIEW_SELECT_ENTITY:
		OnSelectEntity();
		break;
	case ID_GRAPHVIEW_TARGET_SELECTED_ENTITY:
		{
			CFlowNode *pFlowNode = (CFlowNode*)pNode;
			pFlowNode->SetSelectedEntity();
			pFlowNode->Invalidate(true);
		}
		break;
	case ID_GRAPHVIEW_TARGET_GRAPH_ENTITY:
		{
			CFlowNode *pFlowNode = (CFlowNode*)pNode;
			pFlowNode->SetDefaultEntity();
			pFlowNode->Invalidate(true);
		}
		break;
	case ID_GRAPHVIEW_TARGET_GRAPH_ENTITY2:
		{
			CFlowNode *pFlowNode = (CFlowNode*)pNode;
			pFlowNode->SetFlag( EHYPER_NODE_GRAPH_ENTITY,false );
			pFlowNode->SetFlag( EHYPER_NODE_GRAPH_ENTITY2,true );
		}
		break;;
	case ID_OUTPUTS_SHOW_ALL:
	case ID_OUTPUTS_HIDE_ALL:
		if (pNode)
		{
			CUndo undo( "Graph Port Visibilty" );
			pNode->RecordUndo();
			for (int i = 0; i < pNode->GetOutputs()->size(); i++)
			{
				pNode->GetOutputs()->at(i).bVisible = (cmd == ID_OUTPUTS_SHOW_ALL) || (pNode->GetOutputs()->at(i).nConnected != 0);
			}
			InvalidateNode(pNode,true);
			Invalidate();
		}
		break;
	case ID_INPUTS_SHOW_ALL:
	case ID_INPUTS_HIDE_ALL:
		if (pNode)
		{
			CUndo undo( "Graph Port Visibilty" );
			pNode->RecordUndo();
			for (int i = 0; i < pNode->GetInputs()->size(); i++)
			{
				pNode->GetInputs()->at(i).bVisible = (cmd == ID_INPUTS_SHOW_ALL) || (pNode->GetInputs()->at(i).nConnected != 0);
			}
			InvalidateNode(pNode,true);
			Invalidate();
		}
		break;
	}
}

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

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::PopulateClassMenu( CMenu &menu,std::vector<CString> &classes,std::vector<CMenu*> &groups )
{
	NodeFilter filter(m_componentViewMask);
	std::vector<_smart_ptr<CHyperNode> > prototypes;
	m_pGraph->GetManager()->GetPrototypesEx( prototypes, true, functor_ret(filter, &NodeFilter::Visit) );
	classes.resize(0);
	classes.reserve(prototypes.size());
	for (std::vector<_smart_ptr<CHyperNode> >::iterator iter = prototypes.begin();
		iter != prototypes.end(); ++iter)
	{
		CString fullname = (*iter)->GetClassName();
		if (fullname.Find(':') >= 0)
		{
			classes.push_back(fullname);
		}
		else
		{
			CString name = "Misc:";
			name += fullname;
			name += "0x2410";
			classes.push_back(name);
		}
	}
	std::sort( classes.begin(),classes.end() );

	std::map<CString,CMenu*> groupMap;
	for (int i = 0; i < classes.size(); i++)
	{
		const CString& fullname = classes[i];
		CString group,node;
		if (fullname.Find(':') >= 0)
		{
			group = fullname.SpanExcluding(":");
			node = fullname.Mid(group.GetLength()+1);
			int marker = node.Find("0x2410");
			if (marker>0)
			{
				node = node.Left(marker);
				classes[i] = node;
			}
		}
		else
		{
			group = "_UNKNOWN_"; // should never happen
			node = fullname;
			// assert (false);
		}

		CMenu *pMenu = &menu;
		if (!group.IsEmpty())
		{
			pMenu = stl::find_in_map( groupMap,group,(CMenu*)0 );
			if (!pMenu)
			{
				pMenu = new CMenu;
				pMenu->CreatePopupMenu();
				menu.AppendMenu( MF_POPUP,(UINT_PTR)pMenu->GetSafeHmenu(),group );
				groups.push_back(pMenu);
				groupMap[group] = pMenu;
			}
		}
		pMenu->AppendMenu( MF_STRING,BASE_NEW_NODE_CMD+i,node );
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::ShowPortsConfigMenu( CPoint point,bool bInput,CHyperNode *pNode )
{
	CMenu menu;
	menu.CreatePopupMenu();

	CHyperNode::Ports *pPorts;
	if (bInput)
	{
		pPorts = pNode->GetInputs();
		menu.AppendMenu( MF_STRING|MF_GRAYED,0,"Inputs" );
	}
	else
	{
		pPorts = pNode->GetOutputs();
		menu.AppendMenu( MF_STRING|MF_GRAYED,0,"Outputs" );
	}
	menu.AppendMenu( MF_SEPARATOR );

	for (int i = 0; i < pPorts->size(); i++)
	{
		CHyperNodePort *pPort = &(*pPorts)[i];
		int flags = MF_STRING;
		if (pPort->bVisible || pPort->nConnected != 0)
			flags |= MF_CHECKED;
		if (pPort->nConnected != 0)
			flags |= MF_GRAYED;
		menu.AppendMenu( flags,i+1,pPort->pVar->GetName() );
	}
	menu.AppendMenu( MF_SEPARATOR );
	if (bInput)
	{
		menu.AppendMenu( MF_STRING,ID_INPUTS_SHOW_ALL,"Show All" );
		menu.AppendMenu( MF_STRING,ID_INPUTS_HIDE_ALL,"Hide All" );
	}
	else
	{
		menu.AppendMenu( MF_STRING,ID_OUTPUTS_SHOW_ALL,"Show All" );
		menu.AppendMenu( MF_STRING,ID_OUTPUTS_HIDE_ALL,"Hide All" );
	}

	ClientToScreen(&point);

	int cmd = menu.TrackPopupMenu( TPM_RETURNCMD|TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_NONOTIFY,point.x,point.y,this );
	switch (cmd)
	{
	case ID_OUTPUTS_SHOW_ALL:
	case ID_OUTPUTS_HIDE_ALL:
		if (pNode)
		{
			CUndo undo( "Graph Port Visibilty" );
			pNode->RecordUndo();
			for (int i = 0; i < pNode->GetOutputs()->size(); i++)
			{
				pNode->GetOutputs()->at(i).bVisible = (cmd == ID_OUTPUTS_SHOW_ALL) || (pNode->GetOutputs()->at(i).nConnected != 0);
			}
			InvalidateNode(pNode,true);
			Invalidate();
			return;
		}
		break;
	case ID_INPUTS_SHOW_ALL:
	case ID_INPUTS_HIDE_ALL:
		if (pNode)
		{
			CUndo undo( "Graph Port Visibilty" );
			pNode->RecordUndo();
			for (int i = 0; i < pNode->GetInputs()->size(); i++)
			{
				pNode->GetInputs()->at(i).bVisible = (cmd == ID_INPUTS_SHOW_ALL) || (pNode->GetInputs()->at(i).nConnected != 0);
			}
			InvalidateNode(pNode,true);
			Invalidate();
			return;
		}
	}
	if (cmd > 0)
	{
		CUndo undo( "Graph Port Visibilty" );
		pNode->RecordUndo();
		CHyperNodePort *pPort = &(*pPorts)[cmd-1];
		pPort->bVisible = !pPort->bVisible || (pPort->nConnected != 0);
	}
	InvalidateNode(pNode,true);
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::SetScrollExtents()
{
	static bool bNoRecurse = false;
	if (bNoRecurse)
		return;

	bNoRecurse = true;

	Gdiplus::RectF graphRect = (m_graphExtents);
	graphRect.Inflate( GRID_SIZE, GRID_SIZE );
//	graphRect.InflateRect( GRID_SIZE,GRID_SIZE,GRID_SIZE*2,GRID_SIZE*2 );
	// Update scroll.
	CRect rcClient;
	GetClientRect(rcClient);
	Gdiplus::RectF rect = ViewToLocalRect(rcClient);
	rect.Union(rect,rect,graphRect);
	CRect rc = LocalToViewRect(rect);
	

	//m_scrollMin = rect.left;
	//m_scrollMax = rect.right;
	//m_itemWidth = max - min;

	int nPage = rcClient.Width()/2 + 5;

	SCROLLINFO si;
	ZeroStruct(si);
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = rc.left;
	si.nMax = rc.right - nPage;// + m_leftOffset;
	//si.nMax = 10;// + m_leftOffset;
	si.nPage = nPage;
	si.nPos = -m_scrollOffset.x;
	SetScrollInfo( SB_HORZ,&si,TRUE );

	nPage = rcClient.Height()/2 + 5;

	ZeroStruct(si);
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = rc.top;
	si.nMax = rc.bottom - nPage;
	si.nPage = nPage;
	si.nPos = -m_scrollOffset.y;
	SetScrollInfo( SB_VERT,&si,TRUE );

	bNoRecurse = false;

	if (m_pGraph && !rcClient.IsRectEmpty())
		m_pGraph->SetViewPosition( m_scrollOffset,m_zoom );
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	SCROLLINFO si;
	GetScrollInfo( SB_HORZ,&si );

	// Get the minimum and maximum scroll-bar positions.
	int minpos = si.nMin;
	int maxpos = si.nMax;
	int nPage = si.nPage;

	// Get the current position of scroll box.
	int curpos = si.nPos;

	// Determine the new position of scroll box.
	switch (nSBCode)
	{
	case SB_LEFT:      // Scroll to far left.
		curpos = minpos;
		break;

	case SB_RIGHT:      // Scroll to far right.
		curpos = maxpos;
		break;

	case SB_ENDSCROLL:   // End scroll.
		break;

	case SB_LINELEFT:      // Scroll left.
		if (curpos > minpos)
			curpos--;
		break;

	case SB_LINERIGHT:   // Scroll right.
		if (curpos < maxpos)
			curpos++;
		break;

	case SB_PAGELEFT:    // Scroll one page left.
		if (curpos > minpos)
			curpos = max(minpos, curpos - (int)nPage);
		break;

	case SB_PAGERIGHT:      // Scroll one page right.
		if (curpos < maxpos)
			curpos = min(maxpos, curpos + (int)nPage);
		break;

	case SB_THUMBPOSITION: // Scroll to absolute position. nPos is the position
		curpos = nPos;      // of the scroll box at the end of the drag operation.
		break;

	case SB_THUMBTRACK:   // Drag scroll box to specified position. nPos is the
		curpos = nPos;     // position that the scroll box has been dragged to.
		break;
	}

	// Set the new position of the thumb (scroll box).
	SetScrollPos( SB_HORZ,curpos );

	m_scrollOffset.x = -curpos;
	Invalidate();

	CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	SCROLLINFO si;
	GetScrollInfo( SB_VERT,&si );

	// Get the minimum and maximum scroll-bar positions.
	int minpos = si.nMin;
	int maxpos = si.nMax;
	int nPage = si.nPage;

	// Get the current position of scroll box.
	int curpos = si.nPos;

	// Determine the new position of scroll box.
	switch (nSBCode)
	{
	case SB_LEFT:      // Scroll to far left.
		curpos = minpos;
		break;

	case SB_RIGHT:      // Scroll to far right.
		curpos = maxpos;
		break;

	case SB_ENDSCROLL:   // End scroll.
		break;

	case SB_LINELEFT:      // Scroll left.
		if (curpos > minpos)
			curpos--;
		break;

	case SB_LINERIGHT:   // Scroll right.
		if (curpos < maxpos)
			curpos++;
		break;

	case SB_PAGELEFT:    // Scroll one page left.
		if (curpos > minpos)
			curpos = max(minpos, curpos - (int)nPage);
		break;

	case SB_PAGERIGHT:      // Scroll one page right.
		if (curpos < maxpos)
			curpos = min(maxpos, curpos + (int)nPage);
		break;

	case SB_THUMBPOSITION: // Scroll to absolute position. nPos is the position
		curpos = nPos;      // of the scroll box at the end of the drag operation.
		break;

	case SB_THUMBTRACK:   // Drag scroll box to specified position. nPos is the
		curpos = nPos;     // position that the scroll box has been dragged to.
		break;
	}

	// Set the new position of the thumb (scroll box).
	SetScrollPos( SB_VERT,curpos );

	m_scrollOffset.y = -curpos;
	Invalidate();

	CWnd::OnVScroll(nSBCode, nPos, pScrollBar);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::RenameNode( CHyperNode *pNode )
{
	assert(pNode);
	m_pRenameNode = pNode;
	CRect rc = LocalToViewRect( pNode->GetRect() );
	rc.DeflateRect(1,1);
	rc.bottom = rc.top + 14;
	if (m_renameEdit.m_hWnd)
		m_renameEdit.DestroyWindow();
	
	m_renameEdit.SetView(this);
	int nEditFlags = WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL;
	if (strcmp(pNode->GetClassName(), CCommentNode::GetClassType()) == 0)
	{
		nEditFlags |= ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN;
		rc.bottom += 42;
	}
	m_renameEdit.Create( nEditFlags,rc,this,IDC_RENAME );
	m_renameEdit.SetWindowText( m_pRenameNode->GetName() );
	m_renameEdit.SetFont( CFont::FromHandle( (HFONT)gSettings.gui.hSystemFont) );
	m_renameEdit.SetCapture();
	m_renameEdit.SetSel(0,-1);
	m_renameEdit.SetFocus();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnAcceptRename()
{
	CString text;
	m_renameEdit.GetWindowText(text);
	if (m_pRenameNode)
	{
		text.Replace( "\n","\\n" );
		text.Replace( "\r","" );
		m_pRenameNode->SetName(text);
		InvalidateNode(m_pRenameNode);
	}
	m_pRenameNode = NULL;
	m_renameEdit.DestroyWindow();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCancelRename()
{
	m_pRenameNode = NULL;
	m_renameEdit.DestroyWindow();
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnSelectionChange()
{
	if (!m_pPropertiesCtrl)
		return;

	m_pEditedNode = 0;

	std::vector<CHyperNode*> nodes;
	GetSelectedNodes(nodes, SELECTION_SET_ONLY_PARENTS);
	if (nodes.size() == 1)
	{
		m_pPropertiesCtrl->RemoveAllItems();

		CHyperNode *pNode = nodes[0];
		m_pEditedNode = pNode;

		_smart_ptr<CVarBlock> pVarBlock = pNode->GetInputsVarBlock();
		if (pVarBlock)
		{
			m_pPropertiesCtrl->AddVarBlock( pVarBlock );
			//delete pVarBlock; // it's safe to delete here, Varblock because the PropertyCtrl still holds refs to the vars themselves, and so do the ports also
		}
	}
	else
	{
		m_pPropertiesCtrl->RemoveAllItems();
	} 

	CWnd *pWnd = GetParent();
	if (pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
	{
		((CHyperGraphDialog*)pWnd)->OnViewSelectionChange();
	}
}

//////////////////////////////////////////////////////////////////////////
CHyperNode* CHyperGraphView::CreateNode( const CString &sNodeClass,CPoint point )
{
	if (!m_pGraph)
		return 0;

	Gdiplus::PointF p = ViewToLocal(point);
	CHyperNode *pNode = NULL;
	{
		CUndo undo( "New Graph Node");
		m_pGraph->UnselectAll();
		pNode = (CHyperNode*)m_pGraph->CreateNode( sNodeClass, p );
	}
	if (pNode)
	{
		pNode->SetSelected(true);
		OnSelectionChange();
	}
	InvalidateView();
	return pNode;
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnUpdateProperties( IVariable *pVar )
{
	if (m_pEditedNode)
	{
		// TODO: ugliest way to solve this! I should find a better solution... [Dejan]
		if (pVar->GetDataType() == IVariable::DT_SOCLASS)
		{
			CString className;
			pVar->Get( className );
			CHyperNodePort* pHelperInputPort = m_pEditedNode->FindPort( "sohelper_helper", true );
			if ( !pHelperInputPort )
				pHelperInputPort = m_pEditedNode->FindPort( "sonavhelper_helper", true );
			if ( pHelperInputPort && pHelperInputPort->pVar )
			{
				CString helperName;
				pHelperInputPort->pVar->Get( helperName );
				int f = helperName.Find(':');
				if ( f <= 0 || className != helperName.Left(f) )
				{
					helperName = className + ':';
					pHelperInputPort->pVar->Set( helperName );
				}
			}
		}

		m_pEditedNode->OnInputsChanged();
		InvalidateNode( m_pEditedNode, true); // force redraw node as param values have changed
	}
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::InvalidateView(bool bComplete)
{
	std::vector<CHyperEdge*> edges;
	if ( m_pGraph )
		m_pGraph->GetAllEdges(edges);
	std::copy( edges.begin(), edges.end(), inserter(m_edgesToReroute, m_edgesToReroute.end()) );

	if (bComplete && m_pGraph)
	{
		IHyperGraphEnumerator *pEnum = m_pGraph->GetNodesEnumerator();
		for (IHyperNode *pINode = pEnum->GetFirst(); pINode; pINode = pEnum->GetNext())
		{
			CHyperNode *pNode = (CHyperNode*)pINode;
			pNode->Invalidate(true);
		}
		pEnum->Release();
	}

	Invalidate();
	SetScrollExtents();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::ForceRedraw()
{
	if (!m_pGraph)
		return;

	InvalidateView(true);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandDelete()
{
	if (!m_pGraph)
		return;

	CUndo undo( "HyperGraph Delete Node(s)"  );
	std::vector<CHyperNode*> nodes;
	if (!GetSelectedNodes( nodes, SELECTION_SET_INCLUDE_RELATIVES ))
		return;

	for (int i = 0; i < nodes.size(); i++)
	{
		m_pGraph->RemoveNode( nodes[i] );
	}
	OnSelectionChange();
	InvalidateView();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandDeleteKeepLinks()
{
	if (!m_pGraph)
		return;

	CUndo undo( "HyperGraph Hide Node(s)"  );
	std::vector<CHyperNode*> nodes;
	if (!GetSelectedNodes( nodes, SELECTION_SET_INCLUDE_RELATIVES ))
		return;

	for (int i = 0; i < nodes.size(); i++)
	{
		m_pGraph->RemoveNodeKeepLinks( nodes[i] );
	}
	OnSelectionChange();
	InvalidateView();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandCopy()
{
	if (!m_pGraph)
		return;

	std::vector<CHyperNode*> nodes;
	if (!GetSelectedNodes( nodes ))
		return;

	CClipboard clipboard;
	//clipboard.Put( )
		//clipboard.

	CHyperGraphSerializer serializer( m_pGraph, 0 );

	for (int i = 0; i < nodes.size(); i++)
	{
		CHyperNode *pNode = nodes[i];
		serializer.SaveNode( pNode, true );
	}
	if (nodes.size() > 0)
	{
		XmlNodeRef node = CreateXmlNode("Graph");
		serializer.Serialize( node,false );
		clipboard.Put( node,"Graph" );
	}
	InvalidateView();
}


//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandPaste()
{
	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);
	InternalPaste(false, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandPasteWithLinks()
{
	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);
	InternalPaste(true, point);
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::InternalPaste(bool bWithLinks, CPoint point)
{
	if (!m_pGraph)
		return;

	CClipboard clipboard;
	XmlNodeRef node = clipboard.Get();

	if (node != NULL && node->isTag("Graph"))
	{
		CUndo undo( "HyperGraph Paste Node(s)"  );
		m_pGraph->UnselectAll();

		CHyperGraphSerializer serializer( m_pGraph, 0 );
		serializer.SelectLoaded(true);
		serializer.Serialize( node,true,bWithLinks,true); // Paste==true -> don't screw up graph specific data if just pasting nodes

		std::vector<CHyperNode*> nodes;
		serializer.GetLoadedNodes( nodes );

		CRect rc;
		GetClientRect(rc);

		Gdiplus::PointF offset(100,100);
		if (point.x > 0 && point.x < rc.Width() && point.y > 0 && point.y < rc.Height())
		{
			Gdiplus::RectF bounds;
			// Calculate bounds.
			for (int i = 0; i < nodes.size(); i++)
			{
				if (i == 0)
					bounds = nodes[i]->GetRect();
				else
				{
					Gdiplus::RectF temp = bounds;
					bounds.Union(bounds,temp,nodes[i]->GetRect());
				}
			}
			Gdiplus::PointF locPoint = ViewToLocal(point);
			offset = Gdiplus::PointF(locPoint.X-bounds.X,locPoint.Y-bounds.Y);
		}
		// Offset all pasted nodes.
		for (int i = 0; i < nodes.size(); i++)
		{
			Gdiplus::RectF rc = nodes[i]->GetRect();
			rc.Offset(offset);
			nodes[i]->SetRect(rc);
		}
	}
	OnSelectionChange();
	InvalidateView();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::OnCommandCut()
{
	OnCommandCopy();
	OnCommandDelete();
}

//////////////////////////////////////////////////////////////////////////
void CHyperGraphView::SimulateFlow(CHyperEdge* pEdge)
{
	// re-set target node's port value
	assert (m_pGraph != 0);
	CHyperNode *pNode = (CHyperNode*) m_pGraph->FindNode(pEdge->nodeIn);
	assert (pNode != 0);
	CFlowNode* pFlowNode = (CFlowNode*) pNode;
	IFlowGraph* pIGraph = pFlowNode->GetIFlowGraph();
	const TFlowInputData* pValue = pIGraph->GetInputValue(pFlowNode->GetFlowNodeId(), pEdge->nPortIn);
	assert (pValue != 0);
	pIGraph->ActivatePort( SFlowAddress(pFlowNode->GetFlowNodeId(),pEdge->nPortIn,false), *pValue );
}

