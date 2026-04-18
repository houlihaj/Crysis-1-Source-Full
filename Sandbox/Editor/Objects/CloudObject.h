////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   CloudObject.h
//  Version:     v1.00
//  Created:     05/02/2005 by Sergiy Shaykin.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __cloudobject_h__
#define __cloudobject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "BaseObject.h"

class CEdMesh;

/*!
 *	CTagPoint is an object that represent named 3d position in world.
 *
 */
class CCloudObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CCloudObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void Display( DisplayContext &dc );

	void GetLocalBounds( BBox &box );

	bool HitTest( HitContext &hc );


	//! Called when object is being created.
	int MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	//////////////////////////////////////////////////////////////////////////
	//float GetRatioLod() const { return mv_ratioLOD; };
	//float GetRatioViewDist() const { return mv_ratioViewDist; };

	//////////////////////////////////////////////////////////////////////////
	//bool IsRecieveLightmap() const { return mv_recvLightmap; };


	//////////////////////////////////////////////////////////////////////////
	// Cloud parameters.
	//////////////////////////////////////////////////////////////////////////
	CVariable<int> mv_spriteRow;
	CVariable<float> m_width;
	CVariable<float> m_height;
	CVariable<float> m_length;
	CVariable<float> m_angleVariations;
	//CVariable<bool> mv_useSize;
	CVariable<float> mv_sizeSprites;
	CVariable<float> mv_randomSizeValue;

protected:
	//! Dtor must be protected.
	CCloudObject();

	//! Convert ray givven in world coordinates to the ray in local Cloud coordinates.
	void DeleteThis() { delete this; };

	void OnFileChange( CString filename );

	//////////////////////////////////////////////////////////////////////////
	// Local callbacks.

	void OnSizeChange(IVariable *pVar);
	//////////////////////////////////////////////////////////////////////////

	BBox m_bbox;
	Matrix34 m_invertTM;

	//! Engine node.
	//! Node that registered in engine and used to render Cloud prefab
	bool m_bNotSharedGeom;

	bool m_bIgnoreNodeUpdate;
};

/*!
 * Class Description of CCloudObject.	
 */
class CCloudObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {ea981e84-16e5-4e78-8058-ece7dc66b1b7}
		static const GUID guid = { 0xea981e84, 0x16e5, 0x4e78, { 0x80, 0x58, 0xec, 0xe7, 0xdc, 0x66, 0xb1, 0xb7 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_CLOUD; };
	const char* ClassName() { return "CloudVolume"; };
	const char* Category() { return "Misc"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CCloudObject); };
	const char* GetFileSpec() { return ""; };
	int GameCreationOrder() { return 150; };
};

#endif // __Cloudobject_h__