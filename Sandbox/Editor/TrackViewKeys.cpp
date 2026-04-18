////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   TrackViewKeys.cpp
//  Version:     v1.00
//  Created:     23/8/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "TrackViewKeys.h"
#include "Controls\MemDC.h"

#include "TrackViewDialog.h"
#include "AnimationContext.h"
#include "TrackViewUndo.h"

#include "Clipboard.h"

//#include "IMovieSystem.h"

enum ETVMouseMode
{
	MOUSE_MODE_NONE = 0,
	MOUSE_MODE_SELECT = 1,
	MOUSE_MODE_MOVE,
	MOUSE_MODE_CLONE,
	MOUSE_MODE_DRAGTIME,
	MOUSE_MODE_DRAGSTARTMARKER,
	MOUSE_MODE_DRAGENDMARKER,
	MOUSE_MODE_PASTE_DRAGTIME,
};

#define KEY_TEXT_COLOR RGB(255,255,255)

// CTrackViewKeys

IMPLEMENT_DYNAMIC(CTrackViewKeys, CWnd)

CTrackViewKeys::CTrackViewKeys()
{
	m_pSequence = 0;
	m_wndTrack = NULL;
	m_bkgrBrush.CreateSolidBrush( GetSysColor(COLOR_3DFACE) );
	//m_bkgrBrushEmpty.CreateHatchBrush( HS_BDIAGONAL,GetSysColor(COLOR_3DFACE) );
	m_bkgrBrushEmpty.CreateSolidBrush( RGB(190,190,190) );
	m_timeBkgBrush.CreateSolidBrush(RGB(0xE0,0xE0,0xE0));
	m_timeHighlightBrush.CreateSolidBrush(RGB(0xFF,0x0,0x0));
	m_selectedBrush.CreateSolidBrush(RGB(200,200,230));
	//m_visibilityBrush.CreateSolidBrush( RGB(0,150,255) );
	m_visibilityBrush.CreateSolidBrush( RGB(120,120,255) );
	m_selectTrackBrush.CreateSolidBrush( RGB(100,190,255) );

	m_timeScale = 1;
	m_ticksStep = 10;

	m_bZoomDrag=false;
	m_bMoveDrag=false;

	m_bMouseOverKey = false;

	m_leftOffset = 0;
	m_scrollOffset = CPoint(0,0);
	m_bAnySelected = 0;
	m_mouseMode = MOUSE_MODE_NONE;
	m_currentTime = 40;
	m_rcSelect = CRect(0,0,0,0);
	m_keyTimeOffset = 0;
	m_currCursor = NULL;
	m_mouseActionMode = TVMODE_MOVEKEY;

	m_itemWidth = 1000;
	m_scrollMin = 0;
	m_scrollMax = 1000;
	m_itemHeight = 16;

	m_descriptionFont = new CFont();
	m_descriptionFont->CreateFont( 10,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
 		DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Verdana" );
}

CTrackViewKeys::~CTrackViewKeys()
{
	m_descriptionFont->DeleteObject();
	delete m_descriptionFont;
}


BEGIN_MESSAGE_MAP(CTrackViewKeys, CWnd)
	ON_WM_CREATE()
	ON_WM_MEASUREITEM_REFLECT()
	ON_WM_CTLCOLOR_REFLECT()
	ON_WM_SIZE()
	ON_WM_MOUSEWHEEL()
	ON_WM_HSCROLL()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_PAINT()
	ON_WM_SETCURSOR()
	ON_WM_ERASEBKGND()
	ON_WM_RBUTTONDOWN()
	ON_WM_KEYDOWN()
	ON_WM_RBUTTONUP()
END_MESSAGE_MAP()



// CTrackViewKeys message handlers

//////////////////////////////////////////////////////////////////////////
int CTrackViewKeys::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_imageList.Create( MAKEINTRESOURCE(IDB_TRACKVIEW_KEYS),14,0,RGB(255,0,255) );
	m_imgMarker.Create( MAKEINTRESOURCE(IDB_MARKER),8,0,RGB(255,0,255) );
	m_crsLeftRight = AfxGetApp()->LoadStandardCursor(IDC_SIZEWE);
	m_crsAddKey = AfxGetApp()->LoadCursor(IDC_ARROW_ADDKEY);
	m_crsCross = AfxGetApp()->LoadCursor(IDC_POINTER_OBJHIT);

	//InitializeFlatSB(GetSafeHwnd());

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::DrawTimeline( CDC *dc,const CRect &rcUpdate )
{
	CRect rc,temprc;
	
	bool recording = GetIEditor()->GetAnimation()->IsRecording();

	COLORREF lineCol = RGB(255,0,255);
	COLORREF textCol = RGB(0,0,0);
	if (recording)
	{
		lineCol = RGB(255,0,0);
		//textCol = RGB(255,255,255);
	}

	// Draw vertical line showing current time.
	{
		int x = TimeToClient(m_currentTime);
		if (x > m_rcClient.left && x < m_rcClient.right)
		{
			CPen pen( PS_SOLID,1,lineCol );
			CPen *prevPen = dc->SelectObject(&pen);
			dc->MoveTo( x,0 );
			dc->LineTo( x,m_rcClient.bottom );
			dc->SelectObject( prevPen );
		}
	}

	rc = m_rcTimeline;
	if (temprc.IntersectRect(rc,rcUpdate) == 0)
		return;

	/*
	if (recording)
	{
		dc->FillRect( rc,&m_timeHighlightBrush );
	}
	else
	*/
	{
		dc->FillRect( rc,&m_timeBkgBrush );
	}
	dc->Draw3dRect( rc,GetSysColor(COLOR_3DHILIGHT),GetSysColor(COLOR_3DDKSHADOW) );

	CPen *prevPen;
	CPen ltgray(PS_SOLID,1,RGB(90,90,90));
	CPen black(PS_SOLID,1,textCol);
	CPen redpen(PS_SOLID,1,lineCol );
	// Draw time ticks every tick step seconds.
	Range timeRange = m_timeRange;
	CString str;

	dc->SetTextColor( textCol );
	dc->SetBkMode( TRANSPARENT );
	dc->SelectObject( gSettings.gui.hSystemFont );

	dc->SelectObject(ltgray);

	Range VisRange=GetVisibleRange();
	int nNumberTicks=10;
	double step = (double)1.0 / m_ticksStep;
	for (double t = SnapTime(timeRange.start); t <= timeRange.end+step; t += step)
	{
		double st = SnapTime(t);
		if (st > timeRange.end)
			st = timeRange.end;
		if (st < VisRange.start)
			continue;
		if (st > VisRange.end)
			break;
		if (st < m_timeRange.start || st > m_timeRange.end)
			continue;
		int x = TimeToClient(st);
		dc->MoveTo(x,rc.bottom-2);

		int k = RoundFloatToInt(st * m_ticksStep);
		if (k % nNumberTicks == 0)
		{
			dc->SelectObject(black);
			dc->LineTo(x,rc.bottom-14);
			char str[32];
			sprintf( str,"%g",st );
			dc->TextOut( x+2,rc.top,str );
			dc->SelectObject(ltgray);
		}
		else
			dc->LineTo(x,rc.bottom-6);
	}

	// Draw time markers.
	int x;

	x=TimeToClient(m_timeMarked.start);
	m_imgMarker.Draw(dc, 1, CPoint(x, m_rcTimeline.bottom-9), ILD_TRANSPARENT);
	x=TimeToClient(m_timeMarked.end);
	m_imgMarker.Draw(dc, 0, CPoint(x-7, m_rcTimeline.bottom-9), ILD_TRANSPARENT);

	prevPen = dc->SelectObject(&redpen);
	x=TimeToClient(m_currentTime);
	dc->SelectObject( GetStockObject(NULL_BRUSH) );
	dc->Rectangle( x-3,rc.top,x+4,rc.bottom );

	dc->SelectObject(redpen);
	dc->MoveTo( x,rc.top ); dc->LineTo( x,rc.bottom );
	dc->SelectObject( GetStockObject(NULL_BRUSH) );
//	dc->Rectangle( x-3,rc.top,x+4,rc.bottom );

	dc->SelectObject(prevPen);
}

//////////////////////////////////////////////////////////////////////////
int CTrackViewKeys::TimeToClient( float time )
{
	int x = m_leftOffset - m_scrollOffset.x + time*m_timeScale;
	return x;
}

//////////////////////////////////////////////////////////////////////////
Range CTrackViewKeys::GetVisibleRange()
{
	Range r;
	r.start = (m_scrollOffset.x - m_leftOffset)/m_timeScale;
	r.end = r.start + (m_rcClient.Width())/m_timeScale;
	// Intersect range with global time range.
	r = m_timeRange & r;
	return r;
}

//////////////////////////////////////////////////////////////////////////
Range CTrackViewKeys::GetTimeRange( CRect &rc )
{
	Range r;
	r.start = (rc.left-m_leftOffset+m_scrollOffset.x)/m_timeScale;
	r.end = r.start + (rc.Width())/m_timeScale;

	r.start = SnapTime(r.start);
	r.end = SnapTime(r.end);
	// Intersect range with global time range.
	r = m_timeRange & r;
	return r;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::DrawTicks( CDC *dc,CRect &rc,Range &timeRange )
{
	// Draw time ticks every tick step seconds.
	CPen ltgray(PS_SOLID,1,RGB(90,90,90));
	CPen *prevPen = dc->SelectObject( &ltgray );
	Range VisRange=GetVisibleRange();
	int nNumberTicks=10;
	double step = 1.0 / m_ticksStep;
	for (double t = SnapTime(timeRange.start); t <= timeRange.end+step; t += step)
	{
		double st = SnapTime(t);
		if (st > timeRange.end)
			st = timeRange.end;
		if (st < VisRange.start)
			continue;
		if (st > VisRange.end)
			break;
		int x = TimeToClient(st);
		if (x < 0)
			continue;
		dc->MoveTo(x,rc.bottom-2);

		int k = RoundFloatToInt(st * m_ticksStep);
		if (k % nNumberTicks == 0)
		{
			dc->SelectObject( GetStockObject(BLACK_PEN) );
			dc->LineTo(x,rc.bottom-6);
			dc->SelectObject( ltgray );
		}
		else
			dc->LineTo(x,rc.bottom-4);
	}
	dc->SelectObject( prevPen );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::DrawKeys( IAnimTrack *track,CDC *dc,CRect &rc,Range &timeRange )
{
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::RedrawItem( int item )
{
	CRect rc;
	if (GetItemRect( item,rc ) != LB_ERR)
	{
		RedrawWindow( rc,NULL,RDW_INVALIDATE|RDW_ERASE );
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::MeasureItem(LPMEASUREITEMSTRUCT lpMIS)
{
	lpMIS->itemWidth = 1000;
  lpMIS->itemHeight = 16;
}

//////////////////////////////////////////////////////////////////////////
HBRUSH CTrackViewKeys::CtlColor(CDC* pDC, UINT nCtlColor)
{
	return m_bkgrBrush;

	// TODO:  Return a non-NULL brush if the parent's handler should not be called
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::SetTimeRange( float start,float end )
{
	/*
	if (m_timeMarked.start==m_timeRange.start)
		m_timeMarked.start=start;
	if (m_timeMarked.end==m_timeRange.end)
		m_timeMarked.end=end;
	if (m_timeMarked.end>end)
		m_timeMarked.end=end;
		*/
	if (m_timeMarked.start < start)
		m_timeMarked.start = start;
	if (m_timeMarked.end > end)
		m_timeMarked.end = end;

	m_realTimeRange.Set(start,end);
	m_timeRange.Set( start-1,end+1 );
	//SetHorizontalExtent( m_timeRange.Length() *m_timeScale + 2*m_leftOffset );
	
	SetHorizontalExtent( m_timeRange.start*m_timeScale-m_leftOffset,m_timeRange.end*m_timeScale-m_leftOffset );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::SetTimeScale( float timeScale )
{
	float fOldScale=m_timeScale;
	if (timeScale < 0.001f)
		timeScale = 0.001f;
	if (timeScale > 100000.0f)
		timeScale = 100000.0f;
	m_timeScale = timeScale;
	double fPixelsPerTick;

	int steps = 0;
	m_ticksStep = 10;
	do
	{
		fPixelsPerTick = (1.0/m_ticksStep)*(double)m_timeScale;
		if (fPixelsPerTick<6.0)
		{
			//if (m_ticksStep>=10)
				m_ticksStep /= 2;
		}
		if (m_ticksStep <= 0)
		{
			m_ticksStep = 1;
			break;
		}
		steps++;
	}	while (fPixelsPerTick < 6.0 && steps < 100);

	steps = 0;
	do 
	{
		fPixelsPerTick = (1.0/m_ticksStep)*(double)m_timeScale;
		if (fPixelsPerTick>=12.0)
		{
			m_ticksStep *= 2;
		}
		if (m_ticksStep <= 0)
		{
			m_ticksStep = 1;
			break;
		}
		steps++;
	} while (fPixelsPerTick>=12.0 && steps < 100);

	m_scrollOffset.x*=timeScale/fOldScale;
	Invalidate();

	//SetHorizontalExtent( m_timeRange.Length()*m_timeScale + 2*m_leftOffset );
	SetHorizontalExtent( m_timeRange.start*m_timeScale-m_leftOffset,m_timeRange.end*m_timeScale );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);

	GetClientRect(m_rcClient);

	if (m_offscreenBitmap.GetSafeHandle() != NULL)
		m_offscreenBitmap.DeleteObject();
	
	CDC *dc = GetDC();
	m_offscreenBitmap.CreateCompatibleBitmap( dc,m_rcClient.Width(),m_rcClient.Height() );
	ReleaseDC(dc);

	GetClientRect(m_rcTimeline);
	//m_rcTimeline.top = m_rcTimeline.bottom - m_itemHeight;
	m_rcTimeline.bottom = m_rcTimeline.top + m_itemHeight;

	SetHorizontalExtent( m_scrollMin,m_scrollMax );

	if (m_tooltip.m_hWnd)
	{
		m_tooltip.DelTool(this,1);
		m_tooltip.AddTool( this,"",m_rcClient,1 );
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CTrackViewKeys::PreTranslateMessage(MSG* pMsg)
{
	if (!m_tooltip.m_hWnd)
	{
		CRect rc;
		GetClientRect(rc);
		m_tooltip.Create( this );
		m_tooltip.SetDelayTime( TTDT_AUTOPOP,500 );
		m_tooltip.SetDelayTime( TTDT_INITIAL,0 );
		m_tooltip.SetDelayTime( TTDT_RESHOW,0 );
		m_tooltip.SetMaxTipWidth(600);
		m_tooltip.AddTool( this,"",rc,1 );
		m_tooltip.Activate(FALSE);
	}
	m_tooltip.RelayEvent(pMsg);

	return __super::PreTranslateMessage(pMsg);
}

//////////////////////////////////////////////////////////////////////////
BOOL CTrackViewKeys::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	//float z = m_timeScale + (zDelta/120.0f) * 1.0f;
	float z;
	if (zDelta>0)
		z = m_timeScale * 1.25f;
	else
		z = m_timeScale * 0.8f;
	SetTimeScale(z);
	return 1;
	//return CWnd::OnMouseWheel(nFlags, zDelta, pt);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
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

	m_scrollOffset.x = curpos;
	Invalidate();

	CWnd::OnHScroll(nSBCode, nPos, pScrollBar);
}

//////////////////////////////////////////////////////////////////////////
float CTrackViewKeys::SnapTime( float time )
{
	double t = floor( (double)time*m_ticksStep + 0.5);
	t = t / m_ticksStep;
	return t;
}

//////////////////////////////////////////////////////////////////////////
float CTrackViewKeys::TimeFromPoint( CPoint point )
{
	int x = point.x - m_leftOffset + m_scrollOffset.x;
	double t = (double)x / m_timeScale;
	return (float)SnapTime(t);
}

//////////////////////////////////////////////////////////////////////////
float CTrackViewKeys::TimeFromPointUnsnapped( CPoint point )
{
	int x = point.x - m_leftOffset + m_scrollOffset.x;
	double t = (double)x / m_timeScale;
	return t;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::AddItem( const Item &item )
{
	m_tracks.push_back(item);
	m_selected = -1;
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
const CTrackViewKeys::Item& CTrackViewKeys::GetItem( int item )
{
	return m_tracks[item];
}

//////////////////////////////////////////////////////////////////////////
IAnimTrack* CTrackViewKeys::GetTrack( int item )
{
	if (item < 0 || item >= GetCount())
		return 0;
	IAnimTrack *track = m_tracks[item].track;
	return track;
}

//////////////////////////////////////////////////////////////////////////
int CTrackViewKeys::KeyFromPoint( CPoint point )
{
	return -1;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::OnLButtonDown(UINT nFlags, CPoint point)
{
	CWnd::OnLButtonDown(nFlags, point);

	SetFocus();

	m_mouseDownPos = point;

	//int item = ItemFromPoint(point);
	//SetCurSel(item);

	if (m_mouseMode == MOUSE_MODE_PASTE_DRAGTIME)
	{
		GetIEditor()->AcceptUndo( "Paste Keys" );
		SetMouseCursor(NULL);
		ReleaseCapture();
		m_mouseMode = MOUSE_MODE_NONE;
		return;
	}

	if (m_rcTimeline.PtInRect(point))
	{
		// Clicked inside timeline.
		m_mouseMode = MOUSE_MODE_DRAGTIME;
		// If mouse over selected key, change cursor to left-right arrows.
		SetMouseCursor( m_crsLeftRight );
		SetCapture();
		
		SetCurrTime( TimeFromPoint(point) );
		return;
	}

	int key = KeyFromPoint(point);
	if (key >= 0)
	{
		int item = ItemFromPoint(point);
		IAnimTrack *track = GetTrack(item);

		// Store track undo.
		if(!(track->GetKeyFlags(key) & AKEY_SELECTED))
			RecordTrackUndo( GetItem(item) );

		if ((track->GetKeyFlags(key) & AKEY_SELECTED) == 0 && !(nFlags&MK_CONTROL))
		{
			RecordTrackUndo( GetItem(item) );
			UnselectAllKeys();
		}
		m_bAnySelected = true;
		m_keyTimeOffset = 0;
		track->SetKeyFlags( key,track->GetKeyFlags(key) | AKEY_SELECTED );
		if (nFlags & MK_SHIFT)
		{
			m_mouseMode = MOUSE_MODE_CLONE;
			SetMouseCursor( m_crsLeftRight );
		}
		else
		{
			m_mouseMode = MOUSE_MODE_MOVE;
			SetMouseCursor( m_crsLeftRight );
		}
		Invalidate();
		SetKeyInfo( track,key );
		return;
	}
	
	if (m_mouseActionMode == TVMODE_ADDKEY)
	{
		// Add key here.
		int item = ItemFromPoint(point);
		IAnimTrack *track = GetTrack(item);
		if (track)
		{
			RecordTrackUndo( GetItem(item) );
			track->CreateKey( TimeFromPoint(point) );
			Invalidate();
			UpdateAnimation();
		}
		return;
	}
	
	m_mouseMode = MOUSE_MODE_SELECT;
	SetCapture();
	if (m_bAnySelected && !(nFlags & MK_CONTROL))
	{
		// First unselect all buttons.
		UnselectAllKeys();
		Invalidate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_mouseMode == MOUSE_MODE_SELECT)
	{
		bool prevSelected = m_bAnySelected;
		// Check if any key are selected.
		m_rcSelect-=m_scrollOffset;
		SelectKeys( m_rcSelect );
		/*
		if (prevSelected == m_bAnySelected)
		Invalidate();
		else
		{
		CDC *dc = GetDC();
		dc->DrawDragRect( CRect(0,0,0,0),CSize(0,0),m_rcSelect,CSize(1,1) );
		ReleaseDC(dc);
		}
		*/
		Invalidate();
		m_rcSelect = CRect(0,0,0,0);
	}
	else if (m_mouseMode == MOUSE_MODE_DRAGTIME)
	{
		SetMouseCursor(NULL);
	}
	else if (m_mouseMode == MOUSE_MODE_PASTE_DRAGTIME)
	{
		GetIEditor()->AcceptUndo( "Paste Keys" );
		SetMouseCursor(NULL);
	}

	if (GetCapture() == this)
	{
		ReleaseCapture();
	}

	if (m_bAnySelected)
	{
		IAnimTrack *track = 0;
		int key = 0;
		if (FindSingleSelectedKey(track,key))
		{
			SetKeyInfo(track,key);
		}
	}
	m_keyTimeOffset = 0;

	//if (GetIEditor()->IsUndoRecording())
	//GetIEditor()->AcceptUndo( "Track Modify" );

	m_mouseMode = MOUSE_MODE_NONE;
	CWnd::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if (m_rcTimeline.PtInRect(point))
	{
		// Clicked inside timeline.
		return;
	}

	int key = KeyFromPoint(point);
	if (key >= 0)
	{
		int item = ItemFromPoint(point);
		IAnimTrack *track = GetTrack(item);
		UnselectAllKeys();
		track->SetKeyFlags( key,track->GetKeyFlags(key) | AKEY_SELECTED );
		m_bAnySelected = true;
		m_keyTimeOffset = 0;
		Invalidate();

		SetKeyInfo( track,key,true );
		return;
	}

	// Add key here.
	int item = ItemFromPoint(point);
	IAnimTrack *track = GetTrack(item);
	if (track)
	{
		RecordTrackUndo( GetItem(item) );
		track->CreateKey( TimeFromPoint(point) );
		Invalidate();
		UpdateAnimation();
	}
	
	m_mouseMode = MOUSE_MODE_NONE;
	CWnd::OnLButtonDblClk(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::OnMButtonDown(UINT nFlags, CPoint point)
{
	OnRButtonDown(nFlags,point);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::OnMButtonUp(UINT nFlags, CPoint point)
{
	OnRButtonUp(nFlags,point);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::OnRButtonDown(UINT nFlags, CPoint point)
{
	//CWnd::OnRButtonDown(nFlags, point);

	SetFocus();

	if (m_rcTimeline.PtInRect(point))
	{
		// Clicked inside timeline.
		// adjust markers.
		int nMarkerStart=TimeToClient(m_timeMarked.start);
		int nMarkerEnd=TimeToClient(m_timeMarked.end);
		if ((abs(point.x-nMarkerStart))<(abs(point.x-nMarkerEnd)))
		{
			SetStartMarker(TimeFromPoint(point));
			m_mouseMode = MOUSE_MODE_DRAGSTARTMARKER;
		}else
		{
			SetEndMarker(TimeFromPoint(point));
			m_mouseMode = MOUSE_MODE_DRAGENDMARKER;
		}
		SetCapture();
		return;
	}

	//int item = ItemFromPoint(point);
	//SetCurSel(item);

	m_mouseDownPos = point;

	if (nFlags & MK_SHIFT)	// alternative zoom
	{
		m_bZoomDrag=true;
		SetCapture();
		return;
	}

	int key = KeyFromPoint(point);
	if (key >= 0)
	{
		int item = ItemFromPoint(point);
		IAnimTrack *track = GetTrack(item);
		UnselectAllKeys();
		track->SetKeyFlags( key,track->GetKeyFlags(key) | AKEY_SELECTED );
		m_bAnySelected = true;
		m_keyTimeOffset = 0;
		Invalidate();

		SetKeyInfo( track,key,true );
	}else
	{
		m_bMoveDrag=true;
		SetCapture();
	}
}

void CTrackViewKeys::OnRButtonUp(UINT nFlags, CPoint point)
{
	m_bZoomDrag=false;
	m_bMoveDrag=false;

	if (GetCapture() == this)
	{
		ReleaseCapture();
	}
	m_mouseMode = MOUSE_MODE_NONE;
	//	CWnd::OnRButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::OnMouseMove(UINT nFlags, CPoint point)
{
	m_mouseOverPos = point;
	m_bMouseOverKey = false;
	if (m_bZoomDrag && (nFlags & MK_SHIFT))
	{
		SetTimeScale(m_timeScale*(1.0f+(point.x-m_mouseDownPos.x)*0.0025f));
		m_mouseDownPos=point;
		return;
	}else
		m_bZoomDrag=false;
	if (m_bMoveDrag)
	{
		m_scrollOffset.x+= m_mouseDownPos.x-point.x;
		if (m_scrollOffset.x < m_scrollMin)
			m_scrollOffset.x = m_scrollMin;
		if (m_scrollOffset.x > m_scrollMax)
			m_scrollOffset.x = m_scrollMax;
		m_mouseDownPos = point;
		// Set the new position of the thumb (scroll box).
		SetScrollPos( SB_HORZ,m_scrollOffset.x );
		Invalidate();
		SetMouseCursor( m_crsLeftRight );
		return;
	}
	if (m_mouseMode == MOUSE_MODE_SELECT)
	{
		SetMouseCursor(NULL);
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
	else if (m_mouseMode == MOUSE_MODE_MOVE)
	{
		SetMouseCursor(m_crsLeftRight);
		if (point.x < m_rcClient.left)
			point.x = m_rcClient.left;
		if (point.x > m_rcClient.right)
			point.x = m_rcClient.right;
		CPoint ofs = point - m_mouseDownPos;

		bool bSnapKeys = (nFlags & MK_CONTROL) == 0;

		if (m_mouseActionMode == TVMODE_SCALEKEY)
		{
			// Offset all selected keys back by previous offset.
			if (m_keyTimeOffset != 0)
				ScaleSelectedKeys( 1.0f/(1+m_keyTimeOffset),bSnapKeys );
		}
		else
		{
			bool bSlide = false;
			if (m_mouseActionMode == TVMODE_SLIDEKEY)
				bSlide = true;

			// Offset all selected keys back by previous offset.
			if (m_keyTimeOffset != 0)
				OffsetSelectedKeys( -m_keyTimeOffset,bSlide,bSnapKeys );
		}

		float newTime;
		float oldTime;
		if (!bSnapKeys)
		{
			newTime = TimeFromPointUnsnapped(point);

			int key = KeyFromPoint(m_mouseDownPos);
			if (key >= 0)
			{
				int item = ItemFromPoint(m_mouseDownPos);
				IAnimTrack *track = GetTrack(item);

				oldTime = track->GetKeyTime(key);
			}
			else
			{
			oldTime = TimeFromPointUnsnapped(m_mouseDownPos);
		}
		}
		else
		{
			newTime = TimeFromPoint(point);

			int key = KeyFromPoint(m_mouseDownPos);
			if (key >= 0)
			{
				int item = ItemFromPoint(m_mouseDownPos);
				IAnimTrack *track = GetTrack(item);

				oldTime = SnapTime(track->GetKeyTime(key));
			}
			else
			{
			oldTime = TimeFromPoint(m_mouseDownPos);
		}
		}
		m_realTimeRange.ClipValue( newTime );

		float timeOffset = newTime - oldTime;
		
		if (m_mouseActionMode == TVMODE_SCALEKEY)
		{
			float tscale = 0.005f;
			float tofs = ofs.x * tscale;
			// Offset all selected keys by this offset.
			ScaleSelectedKeys( 1+tofs,bSnapKeys );
			m_keyTimeOffset = tofs;
		}
		else
		{
			bool bSlide = false;
			if (m_mouseActionMode == TVMODE_SLIDEKEY)
				bSlide = true;

			// Offset all selected keys by this offset.
			OffsetSelectedKeys( timeOffset,bSlide,bSnapKeys );
			m_keyTimeOffset = timeOffset;
		}
		Invalidate();
	}
	else if (m_mouseMode == MOUSE_MODE_CLONE)
	{
		CloneSelectedKeys();
		m_mouseMode = MOUSE_MODE_MOVE;
	}
	else if (m_mouseMode == MOUSE_MODE_DRAGTIME)
	{
		CPoint p = point;
		if (p.x < m_rcClient.left)
			p.x = m_rcClient.left;
		if (p.x > m_rcClient.right)
			p.x = m_rcClient.right;
		if (p.y < m_rcClient.top)
			p.y = m_rcClient.top;
		if (p.y > m_rcClient.bottom)
			p.y = m_rcClient.bottom;

		bool bNoSnap = (nFlags & MK_CONTROL) != 0;
		float time = TimeFromPointUnsnapped(p);
		m_realTimeRange.ClipValue( time );
		if (!bNoSnap)
			time = SnapTime(time);
		SetCurrTime( time );
	}
	else if (m_mouseMode == MOUSE_MODE_DRAGSTARTMARKER)
	{
		CPoint p = point;
		if (p.x < m_rcClient.left)
			p.x = m_rcClient.left;
		if (p.x > m_rcClient.right)
			p.x = m_rcClient.right;
		if (p.y < m_rcClient.top)
			p.y = m_rcClient.top;
		if (p.y > m_rcClient.bottom)
			p.y = m_rcClient.bottom;

		bool bNoSnap = (nFlags & MK_CONTROL) != 0;
		float time = TimeFromPointUnsnapped(p);
		m_realTimeRange.ClipValue( time );
		if (!bNoSnap)
			time = SnapTime(time);
		SetStartMarker( time );
	}
	else if (m_mouseMode == MOUSE_MODE_DRAGENDMARKER)
	{
		CPoint p = point;
		if (p.x < m_rcClient.left)
			p.x = m_rcClient.left;
		if (p.x > m_rcClient.right)
			p.x = m_rcClient.right;
		if (p.y < m_rcClient.top)
			p.y = m_rcClient.top;
		if (p.y > m_rcClient.bottom)
			p.y = m_rcClient.bottom;

		bool bNoSnap = (nFlags & MK_CONTROL) != 0;
		float time = TimeFromPointUnsnapped(p);
		m_realTimeRange.ClipValue( time );
		if (!bNoSnap)
			time = SnapTime(time);
		SetEndMarker( time );
	}
	else if (m_mouseMode == MOUSE_MODE_PASTE_DRAGTIME)
	{
		CPoint p = point;
		if (p.x < m_rcClient.left)
			p.x = m_rcClient.left;
		if (p.x > m_rcClient.right)
			p.x = m_rcClient.right;

		bool bNoSnap = (nFlags & MK_CONTROL) != 0;
		float time0 = TimeFromPointUnsnapped(m_mouseDownPos);
		float time = TimeFromPointUnsnapped(p);
		if (!bNoSnap)
		{
			time = SnapTime(time);
			time0 = SnapTime(time0);
		}
		float fTimeOffset = time - time0;
		
		// Undo previous past keys.
		if (CUndo::IsRecording())
			GetIEditor()->RestoreUndo();
		PasteKeys( 0,fTimeOffset );
	}
	else
	{
		/*
		//////////////////////////////////////////////////////////////////////////
		// Change selection from mouse over.
		//////////////////////////////////////////////////////////////////////////
		int nNewSelected = ItemFromPoint(point);
		if (m_selected != nNewSelected)
		{
			InvalidateItem(m_selected);
			InvalidateItem(nNewSelected);
			m_selected = nNewSelected;
		}
		*/

		//////////////////////////////////////////////////////////////////////////
		if (m_mouseActionMode == TVMODE_ADDKEY)
		{
			SetMouseCursor(m_crsAddKey);
		}
		else
		{
			// No mouse mode.
			SetMouseCursor(NULL);
			int key = KeyFromPoint(point);
			if (key >= 0)
			{
				m_bMouseOverKey = true;
				int item = ItemFromPoint(point);
				IAnimTrack *track = GetTrack(item);
				if (track && track->GetKeyFlags(key) & AKEY_SELECTED)
				{
					// If mouse over selected key, change cursor to left-right arrows.
					SetMouseCursor(m_crsLeftRight);
				}
				else
				{
					SetMouseCursor(m_crsCross);
				}

				if (track)
				{
					ShowKeyTooltip( track,key,point );
				}
			}
			else
				m_tooltip.Activate(FALSE);
		}
	}

	CWnd::OnMouseMove(nFlags, point);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::OnPaint()
{
//	CPaintDC dc(this); // device context for painting
	// TODO: Add your message handler code here
//	CWnd::OnPaint();

	CPaintDC  PaintDC(this);
	CMemDC dc( PaintDC,&m_offscreenBitmap );

	dc->FillRect( &PaintDC.m_ps.rcPaint,&m_bkgrBrush );
	DrawControl( dc,PaintDC.m_ps.rcPaint );

	/*
	CRect rc = m_rcClient;
	for (int i = 0; i < GetCount(); i++)
	{
		IAnimTrack *track = GetTrack(i);
		if (!track)
			continue;
		
		float xoffset = 0;
		int y = (m_rcClient.bottom+m_rcClient.top)/2;
		dc->MoveTo( m_rcClient.left,y );
		// Draw first track spline.
		for (int x = m_rcClient.left; x < m_rcClient.right; x++)
		{
			float time = TimeFromPointUnsnapped(CPoint(x,y));
			Vec3 val;
			track->GetValue( time,val );
			if (x == m_rcClient.left)
				xoffset = val.x;
			dc->LineTo(x,y + val.x - xoffset);
		}
	}
	*/
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::DrawTrack( int item,CDC *dc,CRect &rcItem )
{
}


//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::DrawControl( CDC *dc,const CRect &rcUpdate )
{
	CRect rc;
	CRect rcTemp;

	// Draw all items.
	int count = GetCount();
	for (int i = 0; i < count; i++)
	{
		GetItemRect( i,rc );
		//if (rcTemp.IntersectRect(rc,rcUpdate) != 0)
		{
			DrawTrack( i,dc,rc );
		}
	}

	DrawTimeline( dc,rcUpdate );
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::UnselectAllKeys()
{
	for (int i = 0; i < GetCount(); i++)
	{
		IAnimTrack *track = GetTrack(i);
		if (!track)
			continue;

		for (int j = 0; j < track->GetNumKeys(); j++)
		{
			track->SetKeyFlags( j,track->GetKeyFlags(j) & ~AKEY_SELECTED );
		}
	}
	m_bAnySelected = false;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::SelectKeys( const CRect &rc )
{}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::DelSelectedKeys()
{
	// Cofirm.
	if (AfxMessageBox("Delete selected keys?",MB_OKCANCEL|MB_ICONQUESTION ) != IDOK)
		return;

	for (int i = 0; i < GetCount(); i++)
	{
		IAnimTrack *track = GetTrack(i);
		if (!track)
			continue;
		int j = 0;
		while (j < track->GetNumKeys())
		{
			if (track->GetKeyFlags(j)&AKEY_SELECTED)
			{
				track->RemoveKey(j);
			}
			else
				j++;
		}
	}
	Invalidate();
	UpdateAnimation();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::OffsetSelectedKeys( float timeOffset,bool bSlide,bool bSnap )
{

	for (int i = 0; i < GetCount(); i++)
	{
		IAnimTrack *track = GetTrack(i);
		if (!track)
			continue;

		int numKeys = track->GetNumKeys();

		if (bSlide)
		{
			bool move = false;
			for (int j = 0; j < numKeys; j++)
			{
				if (track->GetKeyFlags(j)&AKEY_SELECTED)
					move = true;
				if (move)
				{
					//float keyt = SnapTime(track->GetKeyTime(j) + timeOffset);
					float keyt = track->GetKeyTime(j) + timeOffset;
					if (bSnap)
						keyt = SnapTime(keyt);

					track->SetKeyTime( j,keyt );
				}
			}
		}
		else
		{
			for (int j = 0; j < numKeys; j++)
			{
				if (track->GetKeyFlags(j)&AKEY_SELECTED)
				{
					//float keyt = SnapTime(track->GetKeyTime(j) + timeOffset);
					float keyt = track->GetKeyTime(j) + timeOffset;
					if (bSnap)
						keyt = SnapTime(keyt);
					track->SetKeyTime( j,keyt );
				}
			}
		}
	}
	UpdateAnimation();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::ScaleSelectedKeys( float timeOffset,bool bSnapKeys )
{
	if (timeOffset <= 0)
		return;
	for (int i = 0; i < GetCount(); i++)
	{
		IAnimTrack *track = GetTrack(i);
		if (!track)
			continue;
		int numKeys = track->GetNumKeys();
		bool move = false;
		for (int j = 0; j < numKeys; j++)
		{
			if (track->GetKeyFlags(j)&AKEY_SELECTED)
				move = true;
			if (move)
			{
				float keyt = track->GetKeyTime(j) * timeOffset;
				if (bSnapKeys)
					keyt = SnapTime(keyt);
				track->SetKeyTime( j,keyt );
			}
		}
	}
	UpdateAnimation();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::CloneSelectedKeys()
{
	for (int i = 0; i < GetCount(); i++)
	{
		IAnimTrack *track = GetTrack(i);
		if (!track)
			continue;
		int numKeys = track->GetNumKeys();
		for (int j = 0; j < numKeys; j++)
		{
			int keyFlags = track->GetKeyFlags(j);
			if (!(keyFlags&AKEY_SELECTED))
				continue;

			int newKey = track->CloneKey(j);
			// Select new key.
			track->SetKeyFlags(newKey,keyFlags|AKEY_SELECTED );
			// Unselect cloned key.
			track->SetKeyFlags(j,keyFlags&(~AKEY_SELECTED) );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
BOOL CTrackViewKeys::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	if (m_currCursor != NULL)
	{
		return 0;
	}

	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

void CTrackViewKeys::SetMouseCursor( HCURSOR crs )
{
	m_currCursor = crs;
	if (m_currCursor != NULL)
		SetCursor(crs);
}

//////////////////////////////////////////////////////////////////////////
BOOL CTrackViewKeys::OnEraseBkgnd(CDC* pDC)
{
	//return CWnd::OnEraseBkgnd(pDC);
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::SetCurrTime( float time )
{
	if (time < m_timeRange.start)
		time = m_timeRange.start;
	if (time > m_timeRange.end)
		time = m_timeRange.end;
	
	//bool bChange = fabs(time-m_currentTime) >= (1.0f/m_ticksStep);
	bool bChange = fabs(time-m_currentTime) >= 0.001f;

	if (bChange)
	{
		int x1 = TimeToClient(m_currentTime);
		int x2 = TimeToClient(time);
		m_currentTime = time;
		//Invalidate();


		CRect rc(x1-3,m_rcClient.top,x1+4,m_rcClient.bottom);
		RedrawWindow( rc,NULL,RDW_INVALIDATE );
		CRect rc1(x2-3,m_rcClient.top,x2+4,m_rcClient.bottom);
		RedrawWindow( rc1,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ERASE );
		GetIEditor()->GetAnimation()->SetTime( m_currentTime );
	}
}

void CTrackViewKeys::SetStartMarker(float fTime)
{
	m_timeMarked.start=fTime;
	if (m_timeMarked.start<m_timeRange.start)
		m_timeMarked.start=m_timeRange.start;
	if (m_timeMarked.start>m_timeRange.end)
		m_timeMarked.start=m_timeRange.end;
	if (m_timeMarked.start>m_timeMarked.end)
		m_timeMarked.end=m_timeMarked.start;
	GetIEditor()->GetAnimation()->SetMarkers(m_timeMarked);
	Invalidate();
}
	
void CTrackViewKeys::SetEndMarker(float fTime)
{
	m_timeMarked.end=fTime;
	if (m_timeMarked.end<m_timeRange.start)
		m_timeMarked.end=m_timeRange.start;
	if (m_timeMarked.end>m_timeRange.end)
		m_timeMarked.end=m_timeRange.end;
	if (m_timeMarked.start>m_timeMarked.end)
		m_timeMarked.start=m_timeMarked.end;
	GetIEditor()->GetAnimation()->SetMarkers(m_timeMarked);
	Invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::SetMouseActionMode( ETVActionMode mode )
{
	m_mouseActionMode = mode;
	if (mode == TVMODE_ADDKEY)
	{
		SetMouseCursor( m_crsAddKey );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewKeys::CanCopyPasteKeys()
{
	IAnimTrack *pCopyFromTrack=NULL;
	// are all selected keys from the same source track ?
	if (!m_bAnySelected)
		return false;
	for (int i=0;i<GetCount();i++)
	{
		IAnimTrack *pCurrTrack=GetTrack(i);
		if (!pCurrTrack)
			continue;
		for (int nKey=0;nKey<pCurrTrack->GetNumKeys();nKey++)
		{
			if (pCurrTrack->GetKeyFlags(nKey) &	AKEY_SELECTED)
			{
				if (!pCopyFromTrack)
				{
					pCopyFromTrack=pCurrTrack;
				}else
				{
					if (pCopyFromTrack!=pCurrTrack)
						return false;
				}
			}
		}
	}
	if (!pCopyFromTrack)
		return false;
	// is a destination-track selected ?
	if (m_selected==-1)
		return false;
	IAnimTrack *pCurrTrack=GetTrack(m_selected);
	if (!pCurrTrack)
		return false;
	if (pCurrTrack->GetType()!=pCopyFromTrack->GetType())
		return false;
	return (pCopyFromTrack==pCurrTrack);
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewKeys::CopyPasteKeys()
{
	IAnimTrack *pCopyFromTrack=NULL;
	std::vector<int> vecKeysToCopy;
	if (!CanCopyPasteKeys())
		return false;
	if (!m_bAnySelected)
		return false;
	for (int i=0;i<GetCount();i++)
	{
		IAnimTrack *pCurrTrack=GetTrack(i);
		if (!pCurrTrack)
			continue;
		for (int nKey=0;nKey<pCurrTrack->GetNumKeys();nKey++)
		{
			if (pCurrTrack->GetKeyFlags(nKey) &	AKEY_SELECTED)
			{
				pCopyFromTrack=pCurrTrack;
				vecKeysToCopy.push_back(nKey);
			}
		}
		if (pCopyFromTrack)
			break;
	}
	if (!pCopyFromTrack)
		return false;
	IAnimTrack *pCurrTrack=GetTrack(m_selected);
	if (!pCurrTrack)
		return false;
	for (int i=0;i<(int)vecKeysToCopy.size();i++)
	{
		pCurrTrack->CopyKey(pCopyFromTrack, vecKeysToCopy[i]);
	}
	Invalidate();
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::SetKeyInfo( IAnimTrack *track,int key,bool openWindow )
{
	EAnimTrackType trackType = track->GetType();
	
	if (openWindow && m_wndTrack != 0 && !::IsWindow(m_wndTrack->m_hWnd))
	{
		m_wndTrack->Create( CTVTrackPropsDialog::IDD,GetParent() );
	}
	if (m_wndTrack && m_wndTrack->m_hWnd != 0 && ::IsWindow(m_wndTrack->m_hWnd))
	{
		int paramId;
		IAnimNode *node = 0;
		for (int i = 0; i < GetCount(); i++)
		{
			if (m_tracks[i].track == track)
			{
				node = m_tracks[i].node;
				paramId = m_tracks[i].paramId;
				break;
			}
		}
		if (node)
			m_wndTrack->SetKey( node,paramId,track,key );
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewKeys::FindSingleSelectedKey( IAnimTrack* &selTrack,int &selKey )
{
	selTrack = 0;
	selKey = 0;
	for (int i = 0; i < GetCount(); i++)
	{
		IAnimTrack *track = GetTrack(i);
		if (!track)
			continue;
		int numKeys = track->GetNumKeys();
		for (int j = 0; j < numKeys; j++)
		{
			if (track->GetKeyFlags(j)&AKEY_SELECTED)
			{
				// Check if already different key selected.
				if (selTrack != 0)
				{
					return false;
				}
				selTrack = track;
				selKey = j;
			}
		}
	}
	if (selTrack)
		return true;
	
	return false;
}

//////////////////////////////////////////////////////////////////////////
int CTrackViewKeys::GetItemRect( int item,CRect &rect )
{
	if (item < 0 || item >= GetCount())
		return -1;
	int x = 0;
	int y = item*m_itemHeight - m_scrollOffset.y;
	rect.SetRect( x,y,x+m_rcClient.Width(),y+m_itemHeight );
	return 0;
}

//////////////////////////////////////////////////////////////////////////
int CTrackViewKeys::ItemFromPoint( CPoint pnt )
{
	CRect rc;
	int num = GetCount();
	for (int i = 0; i < num; i++)
	{
		GetItemRect(i,rc);
		if (rc.PtInRect(pnt))
			return i;
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::SetHorizontalExtent( int min,int max )
{
	m_scrollMin = min;
	m_scrollMax = max;
	m_itemWidth = max - min;
	int nPage = m_rcClient.Width()/2;
	int sx = m_itemWidth - nPage + m_leftOffset;

	SCROLLINFO si;
	ZeroStruct(si);
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = m_scrollMin;
	si.nMax = m_scrollMax - nPage + m_leftOffset;
	si.nPage = m_rcClient.Width()/2;
	si.nPos = m_scrollOffset.x;
	//si.nPage = max(0,m_rcClient.Width() - m_leftOffset*2);
	//si.nPage = 1;
	//si.nPage = 1;
	SetScrollInfo( SB_HORZ,&si,TRUE );
};

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::UpdateAnimation()
{
	GetIEditor()->GetAnimation()->ForceAnimation();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::SetCurSel( int sel )
{
	if (sel != m_selected)
	{
		m_selected = sel;
		Invalidate();
	}
}

bool CTrackViewKeys::CopyKeys()
{
	bool bCopySelected=false;
	bool bIsSeveralTracks = false;
	bool bIsSeveralEntity = false;

	IAnimTrack *pCopyFromTrack=0;
	IAnimNode * pCurNode = 0;

	for (int i=0;i<GetCount();i++)
	{
		IAnimTrack *pCurrTrack=GetTrack(i);
		if (!pCurrTrack)
			continue;

		Item item = GetItem(i);

		for (int nKey=0;nKey<pCurrTrack->GetNumKeys();nKey++)
		{
			if (pCurrTrack->GetKeyFlags(nKey) &	AKEY_SELECTED)
			{
				if(pCopyFromTrack && pCopyFromTrack!=pCurrTrack)
					bIsSeveralTracks=true;

				pCopyFromTrack=pCurrTrack;
				bCopySelected=true;

				if(pCurNode && pCurNode!=item.node)
					bIsSeveralEntity=true;
				pCurNode=item.node;
			}
		}
	}

	if(bIsSeveralEntity)
		return false;

	if(!bCopySelected && !GetTrack(m_selected))
		return false;

	XmlNodeRef copyNode = CreateXmlNode("CopyKeysNode");

	if(bCopySelected)
	{
		pCopyFromTrack=0;
		XmlNodeRef childNode;
		for (int i=0;i<GetCount();i++)
		{
			IAnimTrack *pCurrTrack=GetTrack(i);
			if (!pCurrTrack)
				continue;

			Item item = GetItem(i);

			bCopySelected=false;

			for (int nKey=0;nKey<pCurrTrack->GetNumKeys();nKey++)
			{
				if (pCurrTrack->GetKeyFlags(nKey) &	AKEY_SELECTED)
				{
					if(pCopyFromTrack!=pCurrTrack)
					{
						childNode = copyNode->newChild("Track");
						childNode->setAttr("paramId", item.paramId);
					}

					pCopyFromTrack=pCurrTrack;
					bCopySelected=true;
				}
			}
			if(bCopySelected && pCopyFromTrack)
			{
				pCopyFromTrack->SerializeSelection(childNode, false, true);
			}
		}
	}
	else
	{
		IAnimTrack *pCurrTrack=GetTrack(m_selected);
		if (!pCurrTrack)
			return false;

		Item item=GetItem(m_selected);

		XmlNodeRef childNode = copyNode->newChild("Track");
		childNode->setAttr("paramId", item.paramId);
		if(pCurrTrack->GetNumKeys()>0)
			pCurrTrack->SerializeSelection(childNode, false, false);
	}

	CClipboard clip;
	clip.Put(copyNode, "Track view keys");

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::StartPasteKeys()
{
	if (m_pSequence)
	{
		CUndo undo("Paste Keys");
		CUndo::Record( new CUndoAnimSequenceObject(m_pSequence) );
	}

	m_mouseMode = MOUSE_MODE_PASTE_DRAGTIME;
	// If mouse over selected key, change cursor to left-right arrows.
	SetMouseCursor( m_crsLeftRight );
	SetCapture();
	m_mouseDownPos = m_mouseOverPos;
	GetIEditor()->BeginUndo();

	PasteKeys(0,0);
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewKeys::PasteKeys( IAnimNode * pAnimNode, float fTimeOffset )
{

	int nPasteToItem = -1;
	if (!pAnimNode)
	{
		// Find mouse-over item.
		int nPasteToItem = ItemFromPoint( m_mouseOverPos );
		if (nPasteToItem >= 0)
		{
			pAnimNode = GetItem(nPasteToItem).node;
		}
	}

	if (!pAnimNode)
		return false;

	CClipboard clip;
	if(clip.IsEmpty())
		return false;

	XmlNodeRef copyNode = clip.Get();
	if(strcmp(copyNode->getTag(), "CopyKeysNode"))
		return false;

	int nNumTracksToPaste = copyNode->getChildCount();

	if(nNumTracksToPaste==0)
		return false;


	bool bIsPasted=false;

	bool bSaveUndo = !CUndo::IsRecording();

	if (bSaveUndo)
		GetIEditor()->BeginUndo();

	UnselectAllKeys();
	
	{
		bool bNewTrackCreated = false;
		for (int i=0; i<copyNode->getChildCount(); i++)
		{
			XmlNodeRef trackNode = copyNode->getChild(i);
			int paramId;
			trackNode->getAttr("paramId", paramId);

			IAnimTrack *pTrack = pAnimNode->GetTrack(paramId);
			if (!pTrack)
			{
				// Make this track.
				pTrack = pAnimNode->CreateTrack(paramId);
				if (pTrack)
					bNewTrackCreated = true;
			}
			if(pTrack)
			{
				CUndo::Record( new CUndoTrackObject(pTrack) );
				pTrack->SerializeSelection(trackNode, true,true,fTimeOffset);
			}
		}
		if (bNewTrackCreated)
		{
			GetIEditor()->Notify( eNotify_OnUpdateTrackView );
		}
	}

	if (bSaveUndo)
		GetIEditor()->AcceptUndo("Paste keys");

	Invalidate();

	return true;
}





void CTrackViewKeys::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_DELETE)
	{
    DelSelectedKeys();
	}

	if(nChar == 'C')
	{
		if(GetKeyState(VK_CONTROL))
			if(CopyKeys())
				return;
	}

	if(nChar == 'V')
	{
		if(GetKeyState(VK_CONTROL))
		{
			StartPasteKeys();
			return;
		}
	}


	if(nChar == 'Z')
	{
		if(GetKeyState(VK_CONTROL))
		{
			GetIEditor()->Undo();
				return;
		}
	}

	CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}


//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::RecordTrackUndo( const Item &item )
{
	if (item.track != 0)
	{
		CUndo undo("Track Modify");
		CUndo::Record( new CUndoTrackObject(item.track) );
	}
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewKeys::ShowKeyTooltip( IAnimTrack *pTrack,int nKey,CPoint point )
{
	assert(pTrack);
	float time = pTrack->GetKeyTime(nKey);
	const char *desc = "";
	float duration = 0;
	pTrack->GetKeyInfo( nKey,desc,duration );

	CString tipText;
	tipText.Format( "%.3f, {%s}",time,desc );
	m_tooltip.UpdateTipText( tipText,this,1 );
	m_tooltip.Activate(TRUE);
}

//////////////////////////////////////////////////////////////////////////
void  CTrackViewKeys::InvalidateItem( int item )
{
	CRect rc;
	if (GetItemRect(item,rc) != -1)
	{
		InvalidateRect(rc);

	}
}
