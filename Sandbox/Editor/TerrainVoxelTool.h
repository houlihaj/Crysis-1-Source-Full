////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TerrainVoxelTool.h
//  Version:     v1.00
//  Created:     17/11/2004 by Sergiy Shaykin.
//  Compilers:   Visual C++ 6.0
//  Description: Terrain voxel painter tool.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __TerrainVoxelTool_h__
#define __TerrainVoxelTool_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "EditTool.h"

struct IVoxelObject;

// {85b7f7b4-958f-422b-bd65-038c34eca89d}
DEFINE_GUID( TERRAIN_VOXEL_TOOL_GUID, 0x85b7f7b4, 0x958f, 0x422b, 0xbd, 0x65, 0x03, 0x8c, 0x34, 0xec, 0xa8, 0x9d);

enum VoxelBrushType
{
	eVoxelBrushSubtract = 1,
	eVoxelBrushCreate,
	eVoxelBrushBlur,
	eVoxelBrushMaterial,
	eVoxelBrushCopyTerrain,
	eVoxelBrushTypeLast
};


enum VoxelBrushShape
{
	eVoxelBrushShapeSphere = 1,
	eVoxelBrushShapeBox,
	eVoxelBrushShapeLast
};



struct CTerrainVoxelBrush
{
	// Type of this brush.
	VoxelBrushType type;
	// Shape of this brush.
	VoxelBrushShape shape;
	//! Outside Radius of brush.
	float radius;
	//! Collision depth offset.
	float depth;

	CTerrainVoxelBrush() {
		type = eVoxelBrushCreate;
		shape = eVoxelBrushShapeSphere;
		radius  = 4;
		depth = 0;
	}
};



struct VoxelPaintParams
{
	int m_voxelType;
	CString m_SurfType;
	bool m_isUpdatePhysics;
	bool m_isSimplifyMesh;
	bool m_isComputeShadows;
	COLORREF m_Color;

	VoxelPaintParams()
	{
		m_voxelType = 1;
		m_SurfType = "Default";
		m_isUpdatePhysics = true;
		m_isSimplifyMesh = true;
		m_isComputeShadows = true;
		m_Color = 0x808080;
	}
};



//////////////////////////////////////////////////////////////////////////
class CTerrainVoxelTool : public CEditTool
{
	DECLARE_DYNCREATE(CTerrainVoxelTool)
public:
	CTerrainVoxelTool();
	virtual ~CTerrainVoxelTool();

	//! Register this tool to editor system.
	static void RegisterTool( CRegistrationContext &rc );

	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams();

	virtual void Display( DisplayContext &dc );

	// Ovverides from CEditTool
	bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	// Key down.
	bool OnKeyDown( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	bool OnKeyUp( CViewport *view,uint nChar,uint nRepCnt,uint nFlags );
	bool OnSetCursor( CViewport *vp );

	void OnManipulatorDrag( CViewport *view,ITransformManipulator *pManipulator,CPoint &p0,CPoint &p1,const Vec3 &value );
	
	// Delete itself.
	void DeleteThis() { delete this; };

	bool IsNeedMoveTool() { return true; };

	void SetActiveBrushType( VoxelBrushType type );
	void SetActiveBrushShape( VoxelBrushShape shape  );
	void SetActiveVoxelObjectType( int type );
	VoxelBrushType GetActiveBrushType() const { return m_currentBrushType; }
	VoxelBrushShape GetActiveBrushShape() const { return m_currentBrushShape; }
	void SetBrush( const CTerrainVoxelBrush &brush ) { m_brush[m_currentBrushType] = brush; };
	void GetBrush( CTerrainVoxelBrush &brush ) const { brush = m_brush[m_currentBrushType]; };

	// Align plane methods
	void SetupPosition(bool bIsSetup);
	void PlaneAlign(bool bIsAlign);
	Matrix34 * GetAlignPlaneTM(){return &m_AlignPlaneTM;}
	void SetPlaneSize(float fPlaneSize){ m_PlaneSize=fPlaneSize; }
	float GetPlaneSize(){return m_PlaneSize;}
	void UpdateTransformManipulator();
	void InitPosition();

	void Paint();

	static VoxelPaintParams m_vpParams;

private:
	void UpdateUI();
	//////////////////////////////////////////////////////////////////////////
	// Commands.
	static void Command_Activate();
	static void Command_Flatten();
	static void Command_Smooth();
	static void Command_RiseLower();
	//////////////////////////////////////////////////////////////////////////
	
	Vec3 m_pointerPos;
	IEditor *m_ie;

	int m_panelId;
	class CTerrainVoxelPanel *m_panel;

	static VoxelBrushType m_currentBrushType;
	static VoxelBrushType m_prevBrushType;
	static VoxelBrushShape m_currentBrushShape;
	static CTerrainVoxelBrush m_brush[eVoxelBrushTypeLast];
	CTerrainVoxelBrush *m_pBrush;

	int voxelObjectType;

	bool m_bSmoothOverride;
	bool m_bQueryHeightMode;
	bool m_bPaintingMode;
	bool m_bLowerTerrain;

	CPoint m_MButtonDownPos;
	float m_prevRadius;
	float m_prevRadiusInside;
	float m_prevHeight;

	
	bool m_bIsSetupPosition;
	bool m_IsPlaneAlign;

	static bool m_bIsPositionSetuped;
	static Matrix34 m_AlignPlaneTM;
	static float m_PlaneSize;
	
	HCURSOR m_hPaintCursor;

	std::vector <IVoxelObject*> m_voxObjs;

};


#endif // __TerrainVoxelTool_h__
