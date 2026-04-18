////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TerrainMoveTool.h
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Updated:     5/12/2005 by Sergiy Shaykin.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain modification tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __TerrainMoveTool_h__
#define __TerrainMoveTool_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "EditTool.h"


struct SMTBox
{
	bool isShow;
	bool isSelected;
	bool isCreated;
	Vec3 pos;

	SMTBox()
	{
		isShow=false;
		isSelected=false;
		isCreated=false;
		pos = Vec3(0,0,0);
	}
};


//////////////////////////////////////////////////////////////////////////
class CTerrainMoveTool : public CEditTool
{
	DECLARE_DYNCREATE(CTerrainMoveTool)
public:
	CTerrainMoveTool();
	virtual ~CTerrainMoveTool();

	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams();

	virtual void Display( DisplayContext &dc );

	// Ovverides from CEditTool
	bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	// Key down.
	bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	bool OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );

	void OnManipulatorDrag( CViewport *view,ITransformManipulator *pManipulator,CPoint &p0,CPoint &p1,const Vec3 &value );

	// Delete itself.
	void DeleteThis() { delete this; };

	void Move(bool isCopy = false, bool bOnlyVegetation = true, bool bOnlyTerrain = true);

	void CorrectPosition();

	void SetDym(Vec3 dym);
	Vec3 GetDym()	{	return m_dym; }

	// 0 - unselect all, 1 - select source, 2 - select target
	void Select(int nBox);

	void SetArchive( CXmlArchive *ar );

	bool IsNeedMoveTool() { return true; };

private:
	Vec3 m_pointerPos;
	CXmlArchive* m_archive;

	// !!!WARNING
	CRect m_srcRect;

	//static SMTBox m_source;
	//static SMTBox m_target;
	SMTBox m_source;
	SMTBox m_target;

	static Vec3 m_dym;

	int m_panelId;
	class CTerrainMoveToolPanel *m_panel;

	IEditor *m_ie;

	friend class CUndoTerrainMoveTool;
	friend class CTerrainMoveToolPanel;
};


#endif // __TerrainMoveTool_h__