////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   AxisHelper.h
//  Version:     v1.00
//  Created:     29/10/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AxisHelper_h__
#define __AxisHelper_h__
#pragma once

struct DisplayContext;
struct HitContext;

//////////////////////////////////////////////////////////////////////////
// Helper Axis object.
//////////////////////////////////////////////////////////////////////////
class CAxisHelper
{
public:
	enum EHelperMode
	{
		MOVE_MODE    = 1,
		SCALE_MODE   = 2,
		ROTATE_MODE  = 4,
	};

	CAxisHelper();

	void SetMode( int nModeFlags );
	void DrawAxis( const Matrix34 &worldTM, DisplayContext &dc );
	bool HitTest( const Matrix34 &worldTM, HitContext &hc );

	void SetHighlightAxis( int axis ) { m_highlightAxis = axis; };
	int  GetHighlightAxis() const { return m_highlightAxis; };

private:
	void Prepare( const Matrix34 &worldTM,CViewport *view );
	float GetDistance2D( CViewport *view,CPoint p,Vec3 &wp );

	int m_nModeFlags;
	int m_highlightAxis;
	int m_highlightMode;
	int m_currentMode;

	float m_fScreenScale;
	bool m_bNeedX;
	bool m_bNeedY;
	bool m_bNeedZ;
	float m_size;
	Matrix34 m_matrix;
};

#endif // __AxisHelper_h__

