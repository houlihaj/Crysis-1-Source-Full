////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   trackviewspline.h
//  Version:     v1.00
//  Created:     7/5/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __trackviewspline_h__
#define __trackviewspline_h__

#if _MSC_VER > 1000
#pragma once
#endif

//forward declaration
struct IAnimTrack;

// CTrackViewSpline

class CTrackViewSpline : public CWnd
{
	DECLARE_DYNAMIC(CTrackViewSpline)

public:
	CTrackViewSpline();
	virtual ~CTrackViewSpline();

	void SetTrack( IAnimTrack *track );
	
	void SetTimeScale( float timeScale );
	float GetTimeScale() { return m_timeScale; }

	void SetTimeRange( float start,float end );

	void SetCurrentTime( float currTime );
	float GetCurrentTime() const { return m_currTime; };

protected:
	DECLARE_MESSAGE_MAP()

	float						m_currTime;				//
	float						m_timeScale;			//

public:
	afx_msg void OnPaint();

	IAnimTrack *		m_track;					//
	Range						m_timeRange;			//
	float						m_ticksStep;			//
};


#endif // __trackviewspline_h__