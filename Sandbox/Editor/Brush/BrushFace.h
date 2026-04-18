////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   brushface.h
//  Version:     v1.00
//  Created:     9/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History: Based on Andrey's Indoor editor.
//
////////////////////////////////////////////////////////////////////////////

#ifndef __brushface_h__
#define __brushface_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "BrushPlane.h"

// forward declarations.
class CREPrefabGeom;
class CRendElement;


struct SBrush;
struct SBrushPlane;
struct SBrushPoly;
struct SBrushVert;

//////////////////////////////////////////////////////////////////////////
// Texture generation info for a brush face.
//////////////////////////////////////////////////////////////////////////
struct SBrushTexInfo
{
  float shift[2];
  float scale[2];
	float rotate;

	SBrushTexInfo()
  {
		shift[0] = shift[1] = 0;
    scale[0] = scale[1] = 1.0f;
		rotate = 0;
  }
};

/** Prefab geometry used instead of texture.
*/
struct SPrefabItem
{
  SPrefabItem()
  {
    m_Geom = NULL;
    m_Model = NULL;
  }
  CREPrefabGeom *m_Geom;
  Matrix34 m_Matrix;
  //CComModel *m_Model;
	IStatObj *m_Model;
};

/** Single Face of brush.
*/
struct SBrushFace
{
  SBrushFace()
  {
		m_nMatID = 0;
    m_Poly = 0;
    m_bSelected = false;
		m_fArea = 0;
		ZeroStruct(m_PlanePts);
		m_vCenter(0,0,0);
  }
  ~SBrushFace();
  SBrushFace& operator = (const SBrushFace& f);

	//check if the face intersect the brush
	bool	IsIntersecting(SBrush *Vol);
	//check if the face intersect a plane
	bool	IsIntersecting(const SBrushPlane &Plane);

	float CalcArea();
	void	CalcCenter();

  void ClipPrefab(SPrefabItem *pi);
  void DeletePrefabs();
  void CalcTexCoords( SBrushVert &v );
	void CalcTextureBasis( Vec3 &u, Vec3 &v );
  void BuildPrefabs();
	//! Build Render element for this face using specified world matrix.
  void BuildRE( const Matrix34 &worldTM );
  void BuildPrefabs_r(SBrushPoly *p, Vec3& Area, Vec3& Scale);
  void FitTexture( float fTileU, float fTileV );

  bool ClipLine(Vec3& p1, Vec3& p2); 

  void Draw();

	//! Make plane of brush face.
	void MakePlane();

	// Select points on plane from selected points in polygon.
	void SelectPointsFromPoly();

private:
	void ReleaseRE();

public:
	unsigned int m_nMatID : 16;
	unsigned int m_bSelected : 1;
	unsigned int m_nSelectedPoints : 3;

	SBrushPlane	m_Plane;
	SBrushPoly*	m_Poly;
  Vec3				m_PlanePts[3];
	SBrushTexInfo	m_TexInfo;

	//! Color of face.
  ///COLORREF		m_color;

	//! List of prefabs.
	//SPrefabItem *m_Prefabs;

	//! Stores face center.
	Vec3	m_vCenter;
	//! Area of face.
	float	m_fArea;
};

#endif // __brushface_h__