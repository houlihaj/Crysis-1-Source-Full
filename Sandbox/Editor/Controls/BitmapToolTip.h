////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   BitmapToolTip.h
//  Version:     v1.00
//  Created:     5/6/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Tooltip that displays bitmap.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __BitmapToolTip_h__
#define __BitmapToolTip_h__

#if _MSC_VER > 1000
#pragma once
#endif

//////////////////////////////////////////////////////////////////////////
class CBitmapToolTip : public CWnd
{
	// Construction
public:
	CBitmapToolTip();

	BOOL Create( const RECT& rect );

	// Attributes
public:

	// Operations
public:
	bool LoadImage( const CString &imageFilename );
	void SetTool( CWnd *pWnd,const CRect &rect );
	
public:
	virtual ~CBitmapToolTip();

	// Generated message map functions
protected:
	virtual void PreSubclassWindow();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();

	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);

	DECLARE_MESSAGE_MAP()

private:
	CStatic m_staticBitmap;
	CStatic m_staticText;
	CString m_filename;
	CBitmap m_previewBitmap;
	CImage m_image;
	int m_nTimer;
	HWND m_hToolWnd;
	CRect m_toolRect;
};


#endif //__BitmapToolTip_h__