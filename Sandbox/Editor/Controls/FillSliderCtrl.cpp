////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CFillSliderCtrl.cpp
//  Version:     v1.00
//  Created:     24/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FillSliderCtrl.h"

IMPLEMENT_DYNCREATE(CFillSliderCtrl,CSliderCtrlEx)

/////////////////////////////////////////////////////////////////////////////
// CFillSliderCtrl

CFillSliderCtrl::CFillSliderCtrl()
{
	m_bFilled = true;
	m_mousePos.SetPoint(0,0);
}

CFillSliderCtrl::~CFillSliderCtrl()
{
}

BEGIN_MESSAGE_MAP(CFillSliderCtrl, CSliderCtrlEx)
	ON_WM_PAINT()
	ON_WM_MOUSEMOVE()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_ENABLE()
	ON_WM_SETFOCUS()
	ON_WM_SIZE()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////////////////
BOOL CFillSliderCtrl::OnEraseBkgnd(CDC* pDC)
{
	if (!m_bFilled)
		return __super::OnEraseBkgnd(pDC);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::OnPaint() 
{
	if (!m_bFilled)
	{
		__super::OnPaint();
		return;
	}

	CPaintDC dc(this); // device context for painting

	float val = m_value;
	
	CRect rc;
	GetClientRect(rc);
	rc.top += 1;
	rc.bottom -= 1;
	float pos = (val-m_min) / fabs(m_max-m_min);
	int splitPos = pos * rc.Width();

	if (splitPos < rc.left)
		splitPos = rc.left;
	if (splitPos > rc.right)
		splitPos = rc.right;

	// Paint filled rect.
	CRect fillRc = rc;
	fillRc.right = splitPos;

	COLORREF startColour = RGB(100, 100, 100);
	COLORREF endColour = RGB(180, 180, 180);
	if (!IsWindowEnabled())
	{
		startColour = RGB(190, 190, 190);
		endColour = RGB(200, 200, 200);
	}
	XTPPaintManager()->GradientFill( &dc,fillRc,startColour,endColour,FALSE );


	// Paint empty rect.
	CRect emptyRc = rc;
	emptyRc.left = splitPos+1;
	emptyRc.IntersectRect(emptyRc,rc);
	COLORREF colour = RGB(255, 255, 255);
	if (!IsWindowEnabled())
		colour = RGB(230, 230, 230);
	CBrush brush(colour);
	dc.FillRect(emptyRc, &brush);

	if (IsWindowEnabled())
	{
		// Draw black marker at split position.
		CPen *pBlackPen = CPen::FromHandle((HPEN)GetStockObject(BLACK_PEN));
		CPen *pOldPen = dc.GetCurrentPen();
		dc.SelectObject(pBlackPen);
		dc.MoveTo( CPoint(splitPos,rc.top) );
		dc.LineTo( CPoint(splitPos,rc.bottom) );
		dc.SelectObject(pOldPen);
	}

	// Do not call __super::OnPaint() for painting messages
}


//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::OnLButtonDown(UINT nFlags, CPoint point) 
{
	if (!m_bFilled)
	{
		__super::OnLButtonDown(nFlags,point);
		return;
	}

	if (!IsWindowEnabled())
		return;

	CWnd *parent = GetParent();
	if (parent)
	{
		::SendMessage( parent->GetSafeHwnd(),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(),WMU_FS_LBUTTONDOWN),(LPARAM)GetSafeHwnd() );
	}	

	m_bUndoStarted = false;
	// Start undo.
	if (m_bUndoEnabled)
	{
		GetIEditor()->BeginUndo();
		m_bUndoStarted = true;
	}

	ChangeValue(point.x,false);

	m_mousePos = point;
	SetCapture();
	m_bDragging = true;
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if (!m_bFilled)
	{
		__super::OnLButtonUp(nFlags,point);
		return;
	}

	if (!IsWindowEnabled())
		return;

	bool bLButonDown = false;
	m_bDragging = false;
	if (GetCapture() == this)
	{
		bLButonDown = true;
		ReleaseCapture();
	}

	if (bLButonDown && m_bUndoStarted)
	{
		if (GetIEditor()->IsUndoRecording())
			GetIEditor()->AcceptUndo( m_undoText );
		m_bUndoStarted = false;
	}
	m_bDragging = false;

	CWnd *parent = GetParent();
	if (parent)
	{
		::SendMessage( parent->GetSafeHwnd(),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(),WMU_FS_LBUTTONUP),(LPARAM)GetSafeHwnd() );
	}	
}

void CFillSliderCtrl::OnMouseMove(UINT nFlags, CPoint point) 
{
	if (!m_bFilled)
	{
		__super::OnMouseMove(nFlags,point);
		return;
	}

	if (!IsWindowEnabled())
		return;

	if (point == m_mousePos)
		return;
	m_mousePos = point;

	if (m_bDragging)
	{
		m_bDragging = true;

		ChangeValue(point.x,true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::ChangeValue( int sliderPos,bool bTracking )
{
	if (m_bLocked)
		return;

	CRect rc;
	GetClientRect(rc);
	if (sliderPos < rc.left)
		sliderPos = rc.left;
	if (sliderPos > rc.right)
		sliderPos = rc.right;
	
	float pos = (float)sliderPos / rc.Width();
	m_value = m_min + pos*fabs(m_max-m_min);

	NotifyUpdate(bTracking);
	RedrawWindow();
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::SetValue( float val )
{
	__super::SetValue( val );
	if (m_hWnd && m_bFilled)
		RedrawWindow();
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::NotifyUpdate( bool tracking )
{
	if (m_noNotify)
		return;

	if (tracking && m_bUndoEnabled && CUndo::IsRecording())
	{
		m_bLocked = true;
		GetIEditor()->RestoreUndo();
		m_bLocked = false;
	}

	if (m_updateCallback)
		m_updateCallback(this);

	if (m_hWnd)
	{
		CWnd *parent = GetParent();
		if (parent)
		{
			::SendMessage( parent->GetSafeHwnd(),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(),WMU_FS_CHANGED),(LPARAM)GetSafeHwnd() );
		}
	}
	m_lastUpdateValue = m_value;
}

//////////////////////////////////////////////////////////////////////////
void CFillSliderCtrl::SetFilledLook( bool bFilled )
{
	m_bFilled = bFilled;
}
