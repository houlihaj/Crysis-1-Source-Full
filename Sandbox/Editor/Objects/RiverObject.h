////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   RiverObject.h
//  Version:     v1.00
//  Created:     25/07/2005 by Timur Davidenko.
//  Compilers:   Visual C++ 6.0
//  Description: RiverObject object definition.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __RiverObject_h__
#define __RiverObject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "RoadObject.h"

/*!
 *	CRiverObject used to create rivers.
 *
 */
class CRiverObject : public CRoadObject
{
public:
	DECLARE_DYNCREATE(CRiverObject)

	CRiverObject();

	virtual void Serialize( CObjectArchive& ar );
	virtual XmlNodeRef Export( const CString& levelPath, XmlNodeRef& xmlNode );

protected:
	virtual bool Init( IEditor* ie, CBaseObject* prev, const CString& file );
	virtual void InitVariables();
	virtual void UpdateSectors();
	virtual void UpdateSector( CRoadSector& sector );
	
	virtual void OnRiverParamChange( IVariable* var );
	virtual void OnRiverPhysicsParamChange( IVariable* var );

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

	uint64 m_waterVolumeID;
	Plane m_fogPlane;
};


/*!
 * Class Description of RiverObject.	
 */
class CRiverObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {55EEF85E-56C6-44e2-AF0B-33D9E746D1F9}
		static const GUID guid = { 0x55eef85e, 0x56c6, 0x44e2, { 0xaf, 0xb, 0x33, 0xd9, 0xe7, 0x46, 0xd1, 0xf9 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_ROAD; };
	const char* ClassName() { return "River"; };
	const char* Category() { return "Misc"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CRiverObject); };
	int GameCreationOrder() { return 50; };
};

#endif // __RiverObject_h__