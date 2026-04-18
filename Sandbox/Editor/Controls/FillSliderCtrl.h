////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CFillSliderCtrl.h
//  Version:     v1.00
//  Created:     24/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Slider like control, with bar filled from the left.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CFillSliderCtrl_h__
#define __CFillSliderCtrl_h__
#pragma once

#include "SliderCtrlEx.h"

// This notification (Sent with WM_COMMAND) sent when slider changes position.
#define WMU_FS_CHANGED          (WM_USER+100)
#define WMU_FS_LBUTTONDOWN      (WM_USER+101)
#define WMU_FS_LBUTTONUP        (WM_USER+102)

/////////////////////////////////////////////////////////////////////////////
// CFillSliderCtrl window
class CFillSliderCtrl : public CSliderCtrlEx
{
	DECLARE_DYNCREATE(CFillSliderCtrl);
	// Construction
public:
	typedef Functor1<CFillSliderCtrl*>	UpdateCallback;

	CFillSliderCtrl();
	~CFillSliderCtrl();

	void SetFilledLook( bool bFilled );

	// Operations
public:
	//! Set current value.
	virtual void SetValue( float val );

	// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnPaint();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);

	void NotifyUpdate( bool tracking );
	void ChangeValue( int sliderPos,bool bTracking );

protected:
	bool m_bFilled;
	CPoint m_mousePos;
};

#endif //__CFillSliderCtrl_h__
