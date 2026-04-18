////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   brush.h
//  Version:     v1.00
//  Created:     9/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __brush_h__
#define __brush_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "BrushPlane.h"
#include "BrushPoly.h"
#include "BrushFace.h"
#include "Geometry\EdGeometry.h"

// forward declrations.
struct SBrushPlane;
struct SBrushPoly;

#define	DIST_EPSILON	0.01f

//////////////////////////////////////////////////////////////////////////
// Brush flags.
//////////////////////////////////////////////////////////////////////////
enum BrushFlags 
{
	BRF_HIDE = 0x01,
	BRF_FAILMODEL = 0x02,
	BRF_TEMP = 0x04,
	BRF_NODEL = 0x08,
	BRF_LIGHTVOLUME = 0x10,
	BRF_SHOW_AFTER = 0x20,
	BRF_MESH_VALID = 0x40, // Set if brush mesh is valid.
	BRF_MODIFIED = 0x80, // Set if brush mesh is modified.
	BRF_SUB_OBJ_SEL = 0x100, // Brush is in sub object selection mode.
	BRF_SELECTED    = 0x200, // Brush is selected object.					
};

//////////////////////////////////////////////////////////////////////////
/** Holds selected data from inside brush.
*/
struct SBrushSubSelection
{
	// Points contained in brush sub-selection.
	std::vector<Vec3*> points;

	//! Adds point to brush sub-selection.
	bool AddPoint( Vec3 *pnt );
	//! Clear all points in brush sub-selection.
	void Clear();
};

//////////////////////////////////////////////////////////////////////////
/** Solid Brush
*/
struct SBrush : public CEdGeometry
{
public:
	//////////////////////////////////////////////////////////////////////////
	// CEdGeometry implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEdGeometryType GetType() const { return GEOM_TYPE_BRUSH; };
	virtual void Serialize( CObjectArchive &ar ) {};

	virtual void GetBounds( AABB &box );
	virtual CEdGeometry* Clone();
	virtual IIndexedMesh* GetIndexedMesh();
	virtual void SetModified( bool bModified=true );
	virtual bool IsModified() const;
	virtual bool StartSubObjSelection( const Matrix34 &nodeWorldTM,int elemType,int nFlags );
	virtual void EndSubObjSelection();
	virtual void Display( DisplayContext &dc );
	virtual bool HitTest( HitContext &hit );
	virtual void ModifySelection( SSubObjSelectionModifyContext &modCtx );
	virtual void AcceptModifySelection();
	//////////////////////////////////////////////////////////////////////////

public:
	//! Ctor.
	SBrush();
	//! Dtor (virtual)
	virtual ~SBrush();

	// Copy ctor.
	SBrush( const SBrush& b );
	//! Assignment operator.
  SBrush& operator = (const SBrush& b);

	//! Create brush planes from bounding box.
  void Create( const Vec3 &mins,const Vec3& maxs,int numSides );
	void CreateBox( const Vec3 &mins,const Vec3& maxs );

	//! Create Solid brush out of planes.
	//! @return False if solid brush cannot be created.
  bool BuildSolid( bool bRemoveEmptyFaces=false );

	// Recalculate all texture coords of the brush.
	void RecalcTexCoords();
	
	//! Remove brush faces that are empty.
	void RemoveEmptyFaces();

  SBrush*	Clone( bool bBuildSolid );

	//! Calculates brush volume.
	//! @return Return volume of this brush.
  float GetVolume();
	
	//! Move brush by delta offset.
  void Move( Vec3& delta );

  void ClipPrefabModels(bool bClip);

	//! Transform all planes of this brush by specified matrix.
	//! @param bBuild if true after transform face of brush will be build out of planes.
	void Transform( Matrix34 &tm,bool bBuild=true );

	//! Snap all plane points of brush to the specified grid.
  void SnapToGrid( bool bOnlySelected=false );
  
	//! Check if ray intersect brush, and return nearest face hit by the ray.
	//! @return Nearest face that was hit by ray.
	//! @param rayOrigin Source point of ray (in Brush Space).
	//! @param rayDir Normilized ray direction vector (in Brush Space).
	//! @param dist Returns distance from ray origin to the hit point on the brush.
  SBrushFace* RayHit( Vec3 rayOrigin,Vec3 rayDir, float *dist );
  
	//! Select brush sides to SubSelection.
  void SelectSide( Vec3 Origin, Vec3 Dir,bool shear,SBrushSubSelection &subSelection );
	
	//! Select brush face to SubSelection.
	//! Called by select side.
	void SelectFace( SBrushFace *f,bool shear,SBrushSubSelection &subSelection );

	//! Create polygon for one of brush faces.
	SBrushPoly*	CreateFacePoly( SBrushFace *f );

	//! Detect on which side of plane this brush lies.
	//!	@return Side cane be SIDE_FRONT or SIDE_BACK.
  int OnSide (SBrushPlane *plane);

	//! Split brush by face and return front and back brushes resulting from split..
	//! @param front Returns splitted front part of brush.
	//! @param back Returns splitted back part of brush.
  void SplitByFace( SBrushFace *f, SBrush* &front, SBrush* &back );
	
	//! Split brush by plane and return front and back brushes resulting from split..
	//! @param front Returns splitted front part of brush.
	//! @param back Returns splitted back part of brush.
  void SplitByPlane (SBrushPlane *plane, SBrush* &front, SBrush* &back);

	//! Fit texture coordinates to match brush size.
  void FitTexture(int nX, int nY);
	
	//! Check if 3D point is inside volume of the brush.
	bool	IsInside(const Vec3 &vPoint);

	//! Check if volumes of 2 brushes intersect.
  bool IsIntersect(SBrush *b);

	//! Intersect 2 brushes, and returns list of all brush parts resulting from intersections.
	bool Intersect(SBrush *b, std::vector<SBrush*>& List);

  void Draw();
  bool DrawModel(bool b);
  void AddToList(CRenderObject *obj);
  bool AddToListModel(bool b);

	//! Return ammount of memory allocated for this brush.
	int GetMemorySize();

	//! Remove faces from brush.
	void ClearFaces();

	//! Serialize brush parameters to xml node.
	void Serialize( XmlNodeRef &xmlNode,bool bLoading );

	//! Set transformation matrix.
	void SetMatrix( const Matrix34 &tm );
	//! Get tranformation matrix.
	const Matrix34& GetMatrix() const { return m_matrix; };

	//! Render brush.
	void Render( CViewport *view,const Vec3 &objectSpaceCamSrc );

	// Obsolete
	void MakeSelFace( SBrushFace *f );

	void Invalidate();

	// Record undo of this brush.
	void RecordUndo( const char *sUndoDescription=0 );

	//
	void GenerateIndexMesh(IIndexedMesh *pMesh);

	//! Build render element for each face.
	bool UpdateMesh();

	//////////////////////////////////////////////////////////////////////////
	// Operations on SubObj Selection.
	//////////////////////////////////////////////////////////////////////////	
	IStatObj* GetIStatObj();
	// Clear all face/edge/vertex selection, return true if anything was selected.
	bool ClearSelection();
	//void OnSelectionChange();

	void SetSelected( bool bSelected ) { m_flags = (bSelected) ? (m_flags|BRF_SELECTED) : (m_flags&(~BRF_SELECTED)); }

	//////////////////////////////////////////////////////////////////////////
	void Selection_SnapToGrid();
	void Selection_DeleteVertex();
	void Selection_SplitFace();
	void Selection_DeleteFace();
	void Selection_SetMatId( int nMatId );
	void Selection_SelectMatId( int nMatId );
	//////////////////////////////////////////////////////////////////////////

protected:
	/*
	struct SUniqueVertex
	{
		SBrushVert
	};
	void GetUniqueVertices( std:vector<SUniqueVertex> &verts );
	*/

	//! Caluclate planes of brush faces.
	void MakeFacePlanes();

public:
	BBox m_bounds;
	std::vector<SBrushFace*> m_Faces;

	//! This brush transformation matrix.
	Matrix34 m_matrix;

protected:
	IStatObj *m_pStatObj;
	int m_flags;
	int m_selectionType;
};

#endif // __brush_h__
