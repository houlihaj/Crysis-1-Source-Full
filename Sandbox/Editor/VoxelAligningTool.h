////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   VoxelAligningTool.h
//  Version:     v1.00
//  Created:     25/08/2005 by Sergiy Shaykin.
//  Description: Definition of VoxelAligningTool, edit tool for cloning of objects..
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __VoxelAligningTool_h__
#define __VoxelAligningTool_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "EditTool.h"

class CBaseObject;

/*!
 *	CVoxelAligningTool, When created duplicate current selection, and manages cloned selection.
 *	
 */

class CVoxelAligningTool : public CEditTool
{
public:
	DECLARE_DYNCREATE(CVoxelAligningTool)

	CVoxelAligningTool();

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CEditTool
	bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	
	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams();

	virtual void Display( DisplayContext &dc );
	virtual bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	virtual bool OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags ) { return false; };
	//////////////////////////////////////////////////////////////////////////

protected:
	virtual ~CVoxelAligningTool();
	// Delete itself.
	void DeleteThis() { delete this; };

private:
	bool m_bIsDragging;
	CBaseObject * m_curObj;
	CPoint m_difPoint;
	Quat m_q;
};


#endif // __VoxelAligningTool_h__
