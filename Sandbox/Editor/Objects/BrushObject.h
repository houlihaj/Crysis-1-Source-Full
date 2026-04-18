////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   brushobject.h
//  Version:     v1.00
//  Created:     8/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __brushobject_h__
#define __brushobject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "BaseObject.h"

class CEdMesh;

/*!
 *	CTagPoint is an object that represent named 3d position in world.
 *
 */
class CBrushObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CBrushObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void Done();
	void Display( DisplayContext &dc );
	bool CreateGameObject();

	void GetLocalBounds( BBox &box );

	bool HitTest( HitContext &hc );
	int HitTestAxis( HitContext &hc );

	virtual void SetScale( const Vec3 &scale );
	virtual void SetSelected( bool bSelect );
	virtual void SetMinSpec( uint32 nSpec );
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
	
	void SaveToCGF( const CString &filename );
	void ReloadGeometry();

	bool IncludeForGI();

	float GetLightmapQuality() const { return mv_lightmapQuality;	}
	void SetLightmapQuality( const f32 fLightmapQuality ) { mv_lightmapQuality = fLightmapQuality;	}

protected:
	//! Dtor must be protected.
	CBrushObject();
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

	BBox m_bbox;
	Matrix34 m_invertTM;

	//! Engine node.
	//! Node that registered in engine and used to render brush prefab
	IRenderNode* m_pRenderNode;
	_smart_ptr<CEdMesh> m_pGeometry;
	bool m_bNotSharedGeom;

	//////////////////////////////////////////////////////////////////////////
	// Brush parameters.
	//////////////////////////////////////////////////////////////////////////
	CVariable<CString> mv_geometryFile;

	//////////////////////////////////////////////////////////////////////////
	// Brush rendering parameters.
	//////////////////////////////////////////////////////////////////////////
	CVariable<bool> mv_outdoor;
//	CVariable<bool> mv_castShadows;
	//CVariable<bool> mv_selfShadowing;
	CVariable<bool> mv_castShadowMaps;
	CVariable<bool> mv_castScatterMaps;
	CVariable<bool> mv_castLightmap;
	CVariable<bool> mv_recvLightmap;
	CVariable<bool> mv_registerByBBox;
	CVariableEnum<int> mv_hideable;
//	CVariable<bool> mv_hideable;
//	CVariable<bool> mv_hideableSecondary;
	CVariable<int> mv_ratioLOD;
	CVariable<int> mv_ratioViewDist;
	CVariable<bool> mv_excludeFromTriangulation;
  CVariable<float> mv_aiRadius;
	CVariable<float> mv_lightmapQuality;
	CVariable<bool> mv_noDecals;
	CVariable<bool> mv_noIndirLight;
  CVariable<bool> mv_recvWind;

	//////////////////////////////////////////////////////////////////////////
	// Rendering flags.
	int m_renderFlags;

	bool m_bIgnoreNodeUpdate;
};

/*!
 * Class Description of CBrushObject.	
 */
class CBrushObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {032B8809-71DB-44d7-AAA1-69F75C999463}
		static const GUID guid = { 0x32b8809, 0x71db, 0x44d7, { 0xaa, 0xa1, 0x69, 0xf7, 0x5c, 0x99, 0x94, 0x63 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_BRUSH; };
	const char* ClassName() { return "Brush"; };
	const char* Category() { return "Brush"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CBrushObject); };
	const char* GetFileSpec() { return "Objects\\*.cgf"; };
	int GameCreationOrder() { return 150; };
};

#endif // __brushobject_h__
