////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   VegetationTool.h
//  Version:     v1.00
//  Created:     11/1/2002 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: Places vegetation on terrain.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __VegetationTool_h__
#define __VegetationTool_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "EditTool.h"

class CVegetationObject;
struct CVegetationInstance;
class CVegetationMap;

//////////////////////////////////////////////////////////////////////////
class CVegetationTool : public CEditTool
{
	DECLARE_DYNCREATE(CVegetationTool)
public:
	CVegetationTool();

	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams();

	virtual void Display( DisplayContext &dc );

	// Ovverides from CEditTool
	bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	void OnManipulatorDrag( CViewport *view,ITransformManipulator *pManipulator,CPoint &point0,CPoint &point1,const Vec3 &value );

	// Key down.
	bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	bool OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	
	void SetBrushRadius( float r ) { m_brushRadius = r; };
	float GetBrushRadius() const { return m_brushRadius; };

	void SetMode( bool paintMode );

	void Distribute();
	void DistributeMask( const char *maskFile );
	void Clear();
	void ClearMask( const char *maskFile );

	void ScaleObjects();
	void DoRandomRotate();
	void DoClearRotate();

	void HideSelectedObjects( bool bHide );
	void RemoveSelectedObjects();

	void PaintBrush();
	void PlaceThing();
	void GetSelectedObjects( std::vector<CVegetationObject*> &objects );

	void ClearThingSelection();

	static void RegisterTool( CRegistrationContext &rc );

	bool IsNeedMoveTool();

	void MoveSelectedInstancesToCategory(CString category);

	int GetCountSelectedInstances(){return m_selectedThings.size();}

protected:
	virtual ~CVegetationTool();
	// Delete itself.
	void DeleteThis() { delete this; };

private:
	void SelectThing( CVegetationInstance *thing,bool bSelObject=true,bool bShowManipulator=false );
	CVegetationInstance* SelectThingAtPoint( CViewport *view,CPoint point,bool bNotSelect=false,bool bShowManipulator=false );
	bool IsThingSelected( CVegetationInstance *pObj );
	void SelectThingsInRect( CViewport *view,CRect rect,bool bShowManipulator=false );
	void MoveSelected( CViewport *view,const Vec3 &offset,bool bFollowTerrain );
	void ScaleSelected( float fScale );
	void RotateSelected( float fRotate );

	// Specific mouse events handlers.
	bool OnLButtonDown( CViewport *view,UINT nFlags,CPoint point );
	bool OnLButtonUp( CViewport *view,UINT nFlags,CPoint point );
	bool OnMouseMove( CViewport *view,UINT nFlags,CPoint point );

	static void Command_Activate();

	CPoint m_mouseDownPos;
	CPoint m_mousePos;
	CPoint m_prevMousePos;
	Vec3 m_pointerPos;
	static float m_brushRadius;

	bool m_bPlaceMode;
	bool m_bPaintingMode;

	enum OpMode {
		OPMODE_NONE,
		OPMODE_PAINT,
		OPMODE_SELECT,
		OPMODE_MOVE,
		OPMODE_SCALE,
		OPMODE_ROTATE
	};
	OpMode m_opMode;

	IEditor *m_ie;
	std::vector<CVegetationObject*> m_selectedObjects;

	CVegetationMap*	m_vegetationMap;

	//! Selected vegetation instances
	std::vector<CVegetationInstance*> m_selectedThings;

	int m_panelId;
	class CVegetationPanel *m_panel;
	int m_panelPreviewId;
	class CPanelPreview *m_panelPreview;
};


#endif // __VegetationTool_h__
