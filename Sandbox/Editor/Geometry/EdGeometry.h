////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   EdGeometry.h
//  Version:     v1.00
//  Created:     26/11/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __EdGeometry_h__
#define __EdGeometry_h__
#pragma once

struct IIndexedMesh;
struct DisplayContext;
struct HitContext;
struct SSubObjSelectionModifyContext;
class CObjectArchive;

// Basic supported geometry types.
enum EEdGeometryType
{
	GEOM_TYPE_MESH = 0, // Mesh geometry.
	GEOM_TYPE_BRUSH,    // Solid brush geometry.
	GEOM_TYPE_PATCH,    // Bezier patch surface geometry.
	GEOM_TYPE_NURB,			// Nurbs surface geometry.
};

//////////////////////////////////////////////////////////////////////////
// Description:
//    CEdGeometry is a base class for all supported editable geometries.
//////////////////////////////////////////////////////////////////////////
class CRYEDIT_API CEdGeometry : public CRefCountBase
{
	DECLARE_DYNAMIC(CEdGeometry)
public:
	CEdGeometry() {};

	// Query the type of the geometry mesh.
	virtual EEdGeometryType GetType() const = 0;

	// Serialize geometry.
	virtual void Serialize( CObjectArchive &ar ) = 0;

	// Return geometry axis aligned bounding box.
	virtual void GetBounds( AABB &box ) = 0;

	// Clones Geometry, returns exact copy of the original geometry.
	virtual CEdGeometry* Clone() = 0;

	// Access to the indexed mesh.
	// Return false if geometry can not be represented by an indexed mesh.
	virtual IIndexedMesh* GetIndexedMesh() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Advanced geometry interface for SubObject selection and modification.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetModified( bool bModified=true ) = 0;
	virtual bool IsModified() const = 0;
	virtual bool StartSubObjSelection( const Matrix34 &nodeWorldTM,int elemType,int nFlags ) = 0;
	virtual void EndSubObjSelection() = 0;

	// Display geometry for sub object selection.
	virtual void Display( DisplayContext &dc ) = 0;

	// Sub geometry hit testing and selection.
	virtual bool HitTest( HitContext &hit ) = 0;

	//////////////////////////////////////////////////////////////////////////	
	virtual void ModifySelection( SSubObjSelectionModifyContext &modCtx ) = 0;
	// Called when selection modification is accepted.
	virtual void AcceptModifySelection() = 0;

protected:
	~CEdGeometry() {};
};

#endif //__EdGeometry_h__
