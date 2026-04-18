////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   MeshObject.h
//  Version:     v1.00
//  Created:     8/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MeshObject_h__
#define __MeshObject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "BrushObject.h"

class CEdMesh;

/*!
 *	CTagPoint is an object that represent named 3d position in world.
 *
 */
class CMeshObject : public CBrushObject
{
public:
	DECLARE_DYNCREATE(CMeshObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void Done();
	void Display( DisplayContext &dc );
	bool CreateGameObject();

	void GetLocalBounds( BBox &box );

	bool HitTest( HitContext &hc );

	virtual void SetScale( const Vec3 &scale );
	virtual void SetSelected( bool bSelect );
	virtual IPhysicalEntity* GetCollisionEntity() const;

	//////////////////////////////////////////////////////////////////////////
	void BeginEditParams( IEditor *ie,int flags );
	void EndEditParams( IEditor *ie );
	void BeginEditMultiSelParams( bool bAllOfSameType );
	void EndEditMultiSelParams();

	//! Called when object is being created.
	int MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	void Serialize( CObjectArchive &ar );
	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode );

	virtual void Validate( CErrorReport *report );
	virtual bool IsSimilarObject( CBaseObject *pObject );
	virtual bool StartSubObjSelection( int elemType );
	virtual void EndSubObjectSelection();
	virtual void CalculateSubObjectSelectionReferenceFrame( ISubObjectSelectionReferenceFrameCalculator* pCalculator );
	virtual void ModifySubObjSelection( SSubObjSelectionModifyContext &modCtx );
	virtual void AcceptSubObjectModify();

	virtual CEdGeometry* GetGeometry();
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	void SetMeshGeometry( CEdMesh *pMesh );
	CEdMesh* GetMeshGeometry() const;

	IStatObj* GetIStatObj() const;
	int GetRenderFlags() const { return m_renderFlags; };
	IRenderNode* GetEngineNode() const { return m_pRenderNode; };
	float GetRatioLod() const { return mv_ratioLOD; };
	float GetRatioViewDist() const { return mv_ratioViewDist; };

	//////////////////////////////////////////////////////////////////////////
	// Material
	//////////////////////////////////////////////////////////////////////////
	virtual void SetMaterial( CMaterial *mtl );
	virtual CMaterial* GetRenderMaterial() const;
	virtual void SetMaterialLayersMask( uint32 nLayersMask );

	//////////////////////////////////////////////////////////////////////////
	bool IsRecieveLightmap() const { return mv_recvLightmap; }
	bool ApplyIndirLighting() const { return !mv_noIndirLight; }
	
	//////////////////////////////////////////////////////////////////////////

	void SaveToCGF( const CString &filename );
	void ReloadGeometry();

	bool IncludeForGI();

	float GetLightmapQuality() const { return mv_lightmapQuality;	}
	void SetLightmapQuality( const f32 fLightmapQuality ) { mv_lightmapQuality = fLightmapQuality;	}

protected:
	//! Dtor must be protected.
	CMeshObject();
	void FreeGameData();

	virtual void UpdateVisibility( bool visible );

	//! Convert ray givven in world coordinates to the ray in local brush coordinates.
	void WorldToLocalRay( Vec3 &raySrc,Vec3 &rayDir );

	bool ConvertFromObject( CBaseObject *object );
	void DeleteThis() { delete this; };
	void InvalidateTM( int nWhyFlags );
	void OnEvent( ObjectEvent event );

	void CreateBrushFromPrefab( const char *meshFilname );
	void UpdateEngineNode( bool bOnlyTransform=false );

	void OnFileChange( CString filename );

	//////////////////////////////////////////////////////////////////////////
	// Local callbacks.
	void OnGeometryChange( IVariable *var );
	void OnRenderVarChange( IVariable *var );
	void OnAIRadiusVarChange( IVariable *var );
	//////////////////////////////////////////////////////////////////////////
};

/*!
 * Class Description of CMeshObject.	
 */
class CMeshObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {577788BD-CF1F-4497-B327-6516CD36A3CF}
		static const GUID guid = { 0x577788bd, 0xcf1f, 0x4497, { 0xb3, 0x27, 0x65, 0x16, 0xcd, 0x36, 0xa3, 0xcf } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_BRUSH; };
	const char* ClassName() { return "MeshObject"; };
	const char* Category() { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CMeshObject); };
	const char* GetFileSpec() { return ""; };
	int GameCreationOrder() { return 150; };
};

#endif // __MeshObject_h__
