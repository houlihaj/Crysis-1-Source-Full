////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   BitmapToolTip.cpp
//  Version:     v1.00
//  Created:     5/6/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Tooltip that displays bitmap.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "BitmapToolTip.h"

/////////////////////////////////////////////////////////////////////////////
// CBitmapToolTip
CBitmapToolTip::CBitmapToolTip()
{
	m_nTimer = 0;
	m_hToolWnd = 0;
	m_toolRect.SetRectEmpty();
}

CBitmapToolTip::~CBitmapToolTip()
{
	if (m_previewBitmap.m_hObject)
		m_previewBitmap.DeleteObject();
}


BEGIN_MESSAGE_MAP(CBitmapToolTip, CWnd)
	ON_WM_CREATE()
	ON_WM_SHOWWINDOW()
	ON_WM_ERASEBKGND()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEWHEEL()
	ON_WM_SIZE()
END_MESSAGE_MAP()


//////////////////////////////////////////////////////////////////////////
bool CBitmapToolTip::LoadImage( const CString &imageFilename )
{
	if (!m_hWnd)
		return false;

	bool m_bShowAlphaMask = false;//m_previewMask.GetCheck() == BST_CHECKED;

	if (m_filename == imageFilename)
		return true;

	CImage image;
	if (CImageUtil::LoadImage( imageFilename,image ))
	{
		m_filename = imageFilename;
		CString imginfo;
		imginfo.Format( "%dx%d %s",image.GetWidth(),image.GetHeight(),(const char*)image.GetFormatDescription() );
		m_staticText.SetWindowText( imginfo );

		int w = image.GetWidth();
		int h = image.GetHeight();
		if (w > 256 || w == 0)
			w = 256;
		if (h > 256 || h == 0)
			h = 256;

		CRect rc;
		GetWindowRect(rc);

		if (rc.Width() != w || rc.Height() != h)
		{
			SetWindowPos( NULL,0,0,w+2,h+2+14,SWP_NOMOVE );
		}
		ShowWindow( SW_SHOW );

		CRect bmpRc;
		m_staticBitmap.GetClientRect(bmpRc);

		w = bmpRc.Width();
		h = bmpRc.Height();

		CImage scaledImage;
		scaledImage.Allocate( w,h );
		CImageUtil::ScaleToFit( image,scaledImage );

		if (m_bShowAlphaMask)
		{
			for (int h = 0; h < scaledImage.GetHeight(); h++)
				for (int w = 0; w < scaledImage.GetWidth(); w++)
				{
					int a = scaledImage.ValueAt(w,h) >> 24;
					scaledImage.ValueAt(w,h) = RGB(a,a,a);
				}
		}
		else
			scaledImage.SwapRedAndBlue();

		if (m_previewBitmap.m_hObject)
			m_previewBitmap.DeleteObject();
		m_previewBitmap.CreateBitmap( scaledImage.GetWidth(),scaledImage.GetHeight(), 1, 32, scaledImage.GetData() );
		m_staticBitmap.SetBitmap( m_previewBitmap );
		return true;
	}
	else
	{
		m_staticBitmap.SetBitmap( NULL );
	}
	RedrawWindow();
	return false;
}

//////////////////////////////////////////////////////////////////////////
BOOL CBitmapToolTip::Create( const RECT& rect )
{
	CString myClassName = AfxRegisterWndClass( CS_VREDRAW|CS_HREDRAW,::LoadCursor(NULL,IDC_ARROW),(HBRUSH)::GetStockObject(NULL_BRUSH),NULL );

	BOOL bRes = CreateEx( WS_EX_TOOLWINDOW|WS_EX_TOPMOST,myClassName,0,WS_VISIBLE|WS_POPUP|WS_BORDER,rect,GetDesktopWindow(),0 );
	if (bRes)
	{
		//ModifyStyleEx( 0,WS_EX_TOOLWINDOW|WS_EX_TOPMOST );
		CRect rcIn;
		GetClientRect(rcIn);
		m_staticBitmap.Create( "",WS_CHILD|WS_VISIBLE|SS_BITMAP,rcIn,this,0 );
		m_staticText.Create( "",WS_CHILD|WS_VISIBLE|SS_LEFT,CRect(0,0,0,0),this,1 );
		m_staticBitmap.ModifyStyleEx( 0,WS_EX_STATICEDGE );
		m_staticText.ModifyStyleEx( 0,WS_EX_STATICEDGE );
		m_staticText.SetFont( CFont::FromHandle( (HFONT)gSettings.gui.hSystemFont) );
	}
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
int CBitmapToolTip::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (__super::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}


//////////////////////////////////////////////////////////////////////////
void CBitmapToolTip::PreSubclassWindow()
{
	__super::PreSubclassWindow();
}

//////////////////////////////////////////////////////////////////////////
BOOL CBitmapToolTip::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
}

void CBitmapToolTip::OnTimer(UINT_PTR nIDEvent) 
{
	/*
	if (IsWindowVisible())
	{
		if (m_bHaveAnythingToRender)
			Invalidate();
	}
	*/
	if (::IsWindow(m_hToolWnd) && CWnd::FromHandle(m_hToolWnd))
	{
		CWnd *pWnd = CWnd::FromHandle(m_hToolWnd);
		CRect toolRc(m_toolRect);
		pWnd->ClientToScreen(&toolRc);
		CPoint cursorPos;
		::GetCursorPos(&cursorPos);
		if (!toolRc.PtInRect(cursorPos))
		{
			ShowWindow(SW_HIDE);
		}
	}

	CWnd::OnTimer(nIDEvent);
}

//////////////////////////////////////////////////////////////////////////
void CBitmapToolTip::OnDestroy() 
{
	m_filename = "";
	CWnd::OnDestroy();

	if (m_nTimer)
		KillTimer( m_nTimer );
}

//////////////////////////////////////////////////////////////////////////
void CBitmapToolTip::OnShowWindow(BOOL bShow, UINT nStatus)
{
	if (!bShow)
	{
		m_filename = "";
		if (m_nTimer)
			KillTimer( m_nTimer );
	}
	else
	{
		m_nTimer = SetTimer( 1,500,NULL );
	}
}

//////////////////////////////////////////////////////////////////////////
void CBitmapToolTip::OnLButtonDown(UINT nFlags, CPoint point) 
{
}

void CBitmapToolTip::OnLButtonUp(UINT nFlags, CPoint point) 
{
}


void CBitmapToolTip::OnMButtonDown(UINT nFlags, CPoint point) 
{	
}

void CBitmapToolTip::OnMButtonUp(UINT nFlags, CPoint point) 
{	
}

void CBitmapToolTip::OnMouseMove(UINT nFlags, CPoint point) 
{
}

void CBitmapToolTip::OnRButtonDown(UINT nFlags, CPoint point) 
{
}

void CBitmapToolTip::OnRButtonUp(UINT nFlags, CPoint point) 
{
}

BOOL CBitmapToolTip::OnMouseWheel(UINT nFlags, short zDelta, CPoint point) 
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
void CBitmapToolTip::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize( nType,cx,cy );
	if (m_staticBitmap.m_hWnd)
	{
		CRect rcClient;
		GetClientRect(rcClient);
		CRect rc(rcClient);
		rc.bottom -= 14;
		m_staticBitmap.MoveWindow(rc,FALSE);
		rc = rcClient;
		rc.top = rc.bottom-14;
		m_staticText.MoveWindow(rc,FALSE);
	}
}

//////////////////////////////////////////////////////////////////////////
void CBitmapToolTip::SetTool( CWnd *pWnd,const CRect &rect )
{
	assert(pWnd);
	m_hToolWnd = pWnd->GetSafeHwnd();
	m_toolRect = rect;
}