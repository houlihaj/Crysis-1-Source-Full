////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TerrainMiniMapTool.h
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain modification tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __TerrainMiniMapTool_h__
#define __TerrainMiniMapTool_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "EditTool.h"
#include "Mission.h"

//////////////////////////////////////////////////////////////////////////
class CTerrainMiniMapTool : public CEditTool, public IEditorNotifyListener
{
	DECLARE_DYNCREATE(CTerrainMiniMapTool)
public:
	CTerrainMiniMapTool();
	virtual ~CTerrainMiniMapTool();

	//////////////////////////////////////////////////////////////////////////
	// CEditTool
	//////////////////////////////////////////////////////////////////////////
	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams();
	virtual void Display( DisplayContext &dc );
	virtual bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	virtual bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	virtual bool OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	virtual void DeleteThis() { delete this; };
	//////////////////////////////////////////////////////////////////////////

	// IEditorNotifyListener
	virtual void OnEditorNotifyEvent( EEditorNotifyEvent event );

	void Generate();

private:
	friend class CTerrainMiniMapPanel;

//	Vec2 m_min,m_max; // Min/max.
	Vec2 m_vCenter;
	Vec2 m_vExtends;
	CPoint m_mouseDown;
	bool m_bDragging;

	SMinimapInfo m_minimap;
	SMinimapInfo m_minimapOriginal;

	int m_moveSide;

	std::map<std::string,float> m_ConstClearList;
	bool b_stateScreenShot;
};


#endif // __TerrainMiniMapTool_h__
