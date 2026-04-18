////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   AutoCubemapObject.h
//  Version:     v1.00
//  Created:     6/5/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Special tag point for comment.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AutoCubemapObject_h__
#define __AutoCubemapObject_h__
#pragma once

#include "BaseObject.h"

struct IAutoCubeMapRenderNode;

/*!
 *	CAutoCubemapObject is an object that represent text commentary added to named 3d position in world.
 *
 */
class CAutoCubemapObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CAutoCubemapObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	virtual void Done();
	virtual bool CreateGameObject();

	virtual void Display( DisplayContext &disp );
	virtual void SetScale( const Vec3 &scale );

	//! Called when object is being created.
	virtual int MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );
	virtual bool HitTest( HitContext &hc );

	virtual void GetLocalBounds( BBox &box );
	virtual void InvalidateTM( int nWhyFlags );
	virtual XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode );
	virtual void Serialize( CObjectArchive &ar );
	virtual void SetMinSpec( uint32 nSpec );
	//////////////////////////////////////////////////////////////////////////

protected:
	//! Dtor must be protected.
	CAutoCubemapObject();
	void DeleteThis() { delete this; };

	void UpdateRenderNode();
	void OnParamChange( IVariable *var );

protected:
	IAutoCubeMapRenderNode* m_pRenderNode;
	bool m_bIgnoreUpdateRenderNode;

	CVariable<bool> mv_alwaysUpdate;
	CVariable<float> mv_sizeX;
	CVariable<float> mv_sizeY;
	CVariable<float> mv_sizeZ;
};

/*!
 * Class Description of CAutoCubemapObject.	
 */
class CAutoCubemapObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {756674D9-2729-48de-9A9E-E6A23B413D9D}
		static const GUID guid = { 0x756674d9, 0x2729, 0x48de, { 0x9a, 0x9e, 0xe6, 0xa2, 0x3b, 0x41, 0x3d, 0x9d } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_TAGPOINT; };
	const char* ClassName() { return "AutoCubeMap"; };
	const char* Category() { return "Misc"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAutoCubemapObject); };
	virtual const char* GetTextureIcon() { return "Editor/ObjectIcons/Camera.bmp"; };
};

#endif // __AutoCubemapObject_h__