////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   watershapeobject.h
//  Version:     v1.00
//  Created:     10/12/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __watershapeobject_h__
#define __watershapeobject_h__

#pragma once

#include "ShapeObject.h"


struct IWaterVolumeRenderNode;


class CWaterShapeObject : public CShapeObject
{
	DECLARE_DYNCREATE(CWaterShapeObject)
public:
	CWaterShapeObject();

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////

	virtual void SetName( const CString& name );
	virtual void SetMaterial( CMaterial* mtl );
	virtual void Serialize( CObjectArchive& ar );
	virtual XmlNodeRef Export( const CString& levelPath, XmlNodeRef& xmlNode );
	virtual void SetMinSpec( uint32 nSpec );
	virtual void SetMaterialLayersMask( uint32 nLayersMask );
	//void Display( DisplayContext& dc );
	virtual void Validate( CErrorReport *report )
	{
		CBaseObject::Validate(report);
	}

protected:
	virtual bool Init( IEditor* ie, CBaseObject* prev, const CString& file );
	virtual void InitVariables();
	virtual bool CreateGameObject();
	virtual void Done();
	virtual void UpdateGameArea( bool remove = false );

	virtual void OnParamChange( IVariable* var );
	virtual void OnWaterParamChange( IVariable* var );
	virtual void OnWaterPhysicsParamChange( IVariable* var );

	Vec3 GetPremulFogColor() const;
	void Physicalize();

protected:
	CVariable<float> mv_waterVolumeDepth;	
	CVariable<float> mv_waterStreamSpeed;	
	CVariable<float> mv_waterFogDensity;
	CVariable<Vec3> mv_waterFogColor;
	CVariable<float> mv_waterFogColorMultiplier;
	CVariable<float> mv_waterSurfaceUScale;
	CVariable<float> mv_waterSurfaceVScale;

	IWaterVolumeRenderNode* m_pWVRN;

	uint64 m_waterVolumeID;
	Plane m_fogPlane;
};

/*!
 * Class Description of ShapeObject.	
 */
class CWaterShapeObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {3CC1CF42-917A-4c4d-80D2-2A81E6A32BDB}
		static const GUID guid = { 0x3cc1cf42, 0x917a, 0x4c4d, { 0x80, 0xd2, 0x2a, 0x81, 0xe6, 0xa3, 0x2b, 0xdb } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_VOLUME; };
	const char* ClassName() { return "WaterVolume"; };
	const char* Category() { return "Area"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CWaterShapeObject); };
	int GameCreationOrder() { return 16; };
};

#endif // __watershapeobject_h__
