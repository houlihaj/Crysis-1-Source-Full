////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   AIReinforcementSpot.h
//  Version:     v1.00
//  Created:     15/2/2007 by Mikko.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AIReinforcementSpot_h__
#define __AIReinforcementSpot_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Entity.h"

// forward declaration.
struct IAIObject;

/*!
*	CAIReinforcementSpot is an special tag point,that is registered with AI, and tell him what to do when AI calling reinforcements.
*
*/
class CAIReinforcementSpot : public CEntity
{
public:
	DECLARE_DYNCREATE(CAIReinforcementSpot)

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CBaseObject.
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
	CAIReinforcementSpot();

	float GetRadius();

	void DeleteThis() { delete this; };

	static float m_helperScale;
};

/*!
* Class Description of ReinforcementSpot.	
*/
class CAIReinforcementSpotClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {81C0EF77-4C60-40c7-98B2-C66A0725ACA0}
		static const GUID guid = { 0x81c0ef77, 0x4c60, 0x40c7, { 0x98, 0xb2, 0xc6, 0x6a, 0x7, 0x25, 0xac, 0xa0 } };
		return guid;
	};
	ObjectType GetObjectType() { return OBJTYPE_AIPOINT; };
	const char* ClassName() { return "AIReinforcementSpot"; };
	const char* Category() { return "AI"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAIReinforcementSpot); };
	int GameCreationOrder() { return 111; };
};

#endif // __AIReinforcementSpot_h__