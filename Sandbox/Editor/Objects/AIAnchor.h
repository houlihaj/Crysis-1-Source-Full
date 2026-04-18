////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   aianchor.h
//  Version:     v1.00
//  Created:     9/9/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __aianchor_h__
#define __aianchor_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Entity.h"

// forward declaration.
struct IAIObject;

/*!
 *	CAIAnchor is an special tag point,that registered with AI, and tell him what to do when AI is idle.
 *
 */
class CAIAnchor : public CEntity
{
public:
	DECLARE_DYNCREATE(CAIAnchor)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	virtual bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	virtual void InitVariables() {};
	virtual void Done();
	virtual void Display( DisplayContext &disp );
	virtual bool HitTest( HitContext &hc );
	virtual void GetLocalBounds( BBox &box );
	virtual void SetScale( const Vec3 &scale );
	virtual void SetHelperScale( float scale ) { m_helperScale = scale; };
	virtual float GetHelperScale() { return m_helperScale; };
	//////////////////////////////////////////////////////////////////////////

protected:
	//! Dtor must be protected.
	CAIAnchor();

	float GetRadius();

	void DeleteThis() { delete this; };

	static float m_helperScale;
};

/*!
 * Class Description of TagPoint.	
 */
class CAIAnchorClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {10E57056-78C7-489e-B230-89B673196DE4}
		static const GUID guid = { 0x10e57056, 0x78c7, 0x489e, { 0xb2, 0x30, 0x89, 0xb6, 0x73, 0x19, 0x6d, 0xe4 } };
		return guid;
	};
	ObjectType GetObjectType() { return OBJTYPE_AIPOINT; };
	const char* ClassName() { return "AIAnchor"; };
	const char* Category() { return "AI"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAIAnchor); };
	int GameCreationOrder() { return 111; };
};

#endif // __aianchor_h__