////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   smartobject.h
//  Version:     v1.00
//  Created:     3/7/2006 by Dejan Pavlovski
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __smartobject_h__
#define __smartobject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Entity.h"
#include "SmartObjects/SmartObjectsEditorDialog.h"

/*!
 *	CSmartObject is an special entity visible in editor but invisible in game, that's registered with AI system as an smart object.
 *
 */
class CSmartObject : public CEntity
{
protected:
	IStatObj*	m_pStatObj;
	BBox		m_bbox;
	IMaterial*	m_pHelperMtl;

	CSOLibrary::CClassTemplateData const* m_pClassTemplate;

public:
	DECLARE_DYNCREATE( CSmartObject )

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	virtual bool Init( IEditor* ie, CBaseObject* prev, const CString& file );
	virtual void InitVariables() {}
	virtual void Done();
	virtual void Display( DisplayContext& disp );
	virtual bool HitTest( HitContext& hc );
	virtual void GetLocalBounds( BBox& box );
	virtual void SetScale( const Vec3& scale );
	virtual void SetHelperScale( float scale ) { m_helperScale = scale; }
	virtual float GetHelperScale() { return m_helperScale; }

	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams( IEditor *ie );

	//////////////////////////////////////////////////////////////////////////

protected:
	//! Dtor must be protected.
	CSmartObject();
	~CSmartObject();

	float GetRadius();

	void DeleteThis() { delete this; }

	const IStatObj* GetIStatObj();
	IMaterial* GetHelperMaterial();

public:
	void OnPropertyChange( IVariable *var );
};

/*!
 * Class Description of SmartObject.	
 */
class CSmartObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {8287BBFD-2581-47c1-99AE-6102F9893B7A}
		static const GUID guid = { 0x8287bbfd, 0x2581, 0x47c1, { 0x99, 0xae, 0x61, 0x02, 0xf9, 0x89, 0x3b, 0x7a } };
		return guid;
	};
	ObjectType GetObjectType() { return OBJTYPE_AIPOINT; }
	const char* ClassName() { return "SmartObject"; }
	const char* Category() { return "AI"; }
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CSmartObject); }
	int GameCreationOrder() { return 111; }
};

#endif // __smartobject_h__
