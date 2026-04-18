////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   SolidBrushObject.h
//  Version:     v1.00
//  Created:     16/9/2004 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SolidBrushObject_h__
#define __SolidBrushObject_h__
#pragma once

#include "Objects\BaseObject.h"
#include "Brush.h"

struct SBrush;
class CEdMesh;

enum ESolidBrushCreateType
{
	BRUSH_CREATE_TYPE_BOX,
	BRUSH_CREATE_TYPE_CONE,
	BRUSH_CREATE_TYPE_SPHERE,
	BRUSH_CREATE_TYPE_SHAPE,
};

/*!
 *	CTagPoint is an object that represent named 3d position in world.
 *
 */
class CSolidBrushObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CSolidBrushObject)

	static void RegisterCommands( CRegistrationContext &rc );
	
	static int m_nCreateNumSides;
	static ESolidBrushCreateType m_nCreateType;

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void Done();
	void Display( DisplayContext &dc );
	bool CreateGameObject();

	void GetBoundBox( BBox &box );
	void GetLocalBounds( BBox &box );

	bool HitTest( HitContext &hc );

	virtual void SetSelected( bool bSelect );
	virtual IPhysicalEntity* GetCollisionEntity() const;

	//////////////////////////////////////////////////////////////////////////
	void BeginEditParams( IEditor *ie,int flags );
	void EndEditParams( IEditor *ie );
	void BeginEditMultiSelParams( bool bAllOfSameType );
	void EndEditMultiSelParams();

	//! Called when object is being created.
	virtual	IMouseCreateCallback* GetMouseCreateCallback();

	void Serialize( CObjectArchive &ar );
	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode );

	virtual void SetMaterial( CMaterial *mtl );
	virtual void SetMaterialLayersMask( uint32 nLayersMask );
	virtual void SetMinSpec( uint32 nSpec );

	virtual void Validate( CErrorReport *report );
	virtual bool IsSimilarObject( CBaseObject *pObject );
	virtual void OnEvent( ObjectEvent event );

	virtual bool StartSubObjSelection( int elemType );
	virtual void EndSubObjectSelection();
	virtual void CalculateSubObjectSelectionReferenceFrame( ISubObjectSelectionReferenceFrameCalculator* pCalculator );
	virtual void ModifySubObjSelection( SSubObjSelectionModifyContext &modCtx );
	virtual void AcceptSubObjectModify();
	virtual CEdGeometry* GetGeometry();
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//! Assign brush to object.
	void SetBrush( SBrush *brush );
	//! Retrieve brush assigned to object.
	SBrush* GetBrush();

	// Call this if you change brush externally.
	void InvalidateBrush( bool bBuildSolid=true );

	// Create a brush from bounding box, returns true if successfully created a brush.
	virtual bool CreateBrush( const AABB &bbox,ESolidBrushCreateType createType,int numSides );

	void SelectBrushSide( const Vec3 &raySrc,const Vec3 &rayDir,bool shear );
	void MoveSelectedPoints( const Vec3 &worldOffset );

	//////////////////////////////////////////////////////////////////////////
	// Used by brush exporter.
	//////////////////////////////////////////////////////////////////////////
	Matrix34 GetBrushMatrix() const;
	void SetRenderFlags( int nRndFlags );
	int GetRenderFlags() const { return m_renderFlags; };
	IRenderNode* GetEngineNode() const { return m_pRenderNode; };
	
	void SetViewDistRatio( int nRatio );
	int GetViewDistRatio() const { return m_viewDistRatio; };

	void SetLMQuality( float value ) { m_lightmapQuality = value; }
	float GetLMQuality() const { return m_lightmapQuality; };

	//////////////////////////////////////////////////////////////////////////
	void ResetTransform();
	// Snap all brush points to the grid.
	void SnapPointsToGrid();
	// Save brush to the CGF object.
	void SaveToCgf( const CString filename );
	//////////////////////////////////////////////////////////////////////////

	void PivotToVertices();
	void PivotToCenter();

protected:
	//! Dtor must be protected.
	CSolidBrushObject();

	virtual void UpdateVisibility( bool visible );

	//! Convert ray givven in world coordinates to the ray in local brush coordinates.
	void WorldToLocalRay( Vec3 &raySrc,Vec3 &rayDir );

	bool ConvertFromObject( CBaseObject *object );
	void DeleteThis() { delete this; };
	void InvalidateTM( int nWhyFlags );

	void UpdateEngineNode( bool bOnlyTransform=false );

	void OnFileChange( CString filename );
	

	//////////////////////////////////////////////////////////////////////////
	// Local callbacks.
	void OnRenderVarChange( IVariable *var );
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Commands.
	//////////////////////////////////////////////////////////////////////////
	static void Command_ResetTransform();
	static void Command_SnapPointsToGrid();

protected:
	int m_brushFlags;
	BBox m_bbox;
	Matrix34 m_invertTM;
	
	//! Solid brush.
	_smart_ptr<SBrush> m_brush;
	
	// Selected points of this brush.
	SBrushSubSelection m_subSelection;
	//std::vector<Vec3*> m_selectedPoints;

	//! Engine node.
	//! Node that registered in engine and used to render brush prefab
	IRenderNode* m_pRenderNode;

	//////////////////////////////////////////////////////////////////////////
	// Brush rendering parameters.
	//////////////////////////////////////////////////////////////////////////
	int m_renderFlags;
	int m_viewDistRatio;
	float m_lightmapQuality;

	//////////////////////////////////////////////////////////////////////////
	bool m_bIgnoreNodeUpdate;
};

/*!
 * Class Description of CSolidBrushObject.	
 */
class CSolidBrushObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {E58B34C2-5ED2-4538-8896-4747FA2B2809}
		static const GUID guid = { 0xe58b34c2, 0x5ed2, 0x4538, { 0x88, 0x96, 0x47, 0x47, 0xfa, 0x2b, 0x28, 0x9 } };

		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_SOLID; };
	const char* ClassName() { return "Solid"; };
	const char* Category() { return "Solid"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CSolidBrushObject); };
	const char* GetFileSpec() { return ""; };
	int GameCreationOrder() { return 150; };
};

#endif //__SolidBrushObject_h__