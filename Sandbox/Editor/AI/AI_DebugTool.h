////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   AI_DebugTool.h
//  Version:     v1.00
//  Created:     18/12/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Definition of AI_DebugTool, tool used to pick objects.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AI_DebugTool_h__
#define __AI_DebugTool_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "EditTool.h"
#include "Objects\ObjectManager.h"

struct IEntity;

//////////////////////////////////////////////////////////////////////////
class CAI_DebugTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CAI_DebugTool)

	enum EOpMode {
		eMode_GoTo,
	};

	CAI_DebugTool();

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CEditTool
	//////////////////////////////////////////////////////////////////////////
	bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams();
	virtual void Display( DisplayContext &dc );
	virtual bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	virtual bool OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags ) { return false; };
	//////////////////////////////////////////////////////////////////////////

protected:
	virtual ~CAI_DebugTool();
	// Delete itself.
	void DeleteThis() { delete this; };

	IEntity *GetNearestAI( Vec3 pos );
	void GotoToPosition( IEntity *pEntityAI,Vec3 pos,bool bSendSignal );

private:
	Vec3 m_curPos;
	Vec3 m_mouseDownPos;

	IEntity *m_pSelectedAI;

	EOpMode m_currentOpMode;

	float m_fAISpeed;
};


#endif // __AI_DebugTool_h__
