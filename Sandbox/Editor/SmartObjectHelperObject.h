////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   SmartObjectHelperObject.h
//  Version:     v1.00
//  Created:     18/09/2005 by Dejan Pavlovski
//  Compilers:   Visual C++ 7.1
//  Description: Smart Object Helper Object
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SmartObjectHelperObject_h__
#define __SmartObjectHelperObject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Objects/BaseObject.h"


class CEntity;
class CSmartObjectClass;


/*!
 *  CSmartObjectHelperObject is a simple helper object for specifying a position and orientation
 */
class CSmartObjectHelperObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CSmartObjectHelperObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor* ie, CBaseObject* prev, const CString& file );
	void Done();

	void Display( DisplayContext& dc );
	void BeginEditParams( IEditor* ie, int flags );
	void EndEditParams( IEditor* ie );

	//! Called when object is being created.
	int MouseCreateCallback( CViewport* view, EMouseEvent event, CPoint& point, int flags );

	void GetBoundSphere( Vec3& pos, float& radius );
	void GetBoundBox( BBox& box );
	void GetLocalBounds( BBox& box );
	bool HitTest( HitContext& hc );
	
	//////////////////////////////////////////////////////////////////////////
	void UpdateVarFromObject();
	void UpdateObjectFromVar()
	{
	}

//	const char* GetElementName() { return "Helper"; }

	IVariable* GetVariable() { return m_pVar; }
	void SetVariable( IVariable* pVar ) { m_pVar = pVar; }
	//////////////////////////////////////////////////////////////////////////

//	void IsFromGeometry( bool b ) { m_fromGeometry = b; }
//	bool IsFromGeometry() { return m_fromGeometry; }

	CSmartObjectClass* GetSmartObjectClass() const { return m_pSmartObjectClass; }
	void SetSmartObjectClass( CSmartObjectClass* pClass ) { m_pSmartObjectClass = pClass; }

	CEntity* GetSmartObjectEntity() const { return m_pEntity; }
	void SetSmartObjectEntity( CEntity* pEntity ) { m_pEntity = pEntity; }

protected:
	//! Dtor must be protected.
	CSmartObjectHelperObject();
	~CSmartObjectHelperObject();

	void DeleteThis() { delete this; };

	float m_innerRadius;
	float m_outerRadius;

	IVariable* m_pVar;

	IEditor* m_ie;

//	bool m_fromGeometry;

	CSmartObjectClass* m_pSmartObjectClass;

	CEntity* m_pEntity;
};

/*!
 * Class Description of SmartObjectHelper.	
 */
class CSmartObjectHelperClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {9c9af214-be12-4213-bf12-83e734ccbd13}
		static const GUID guid = { 0x9c9af214, 0xbe12, 0x4213, { 0xbf, 0x12, 0x83, 0xe7, 0x34, 0xcc, 0xbd, 0x13 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_OTHER; };
	const char* ClassName() { return "SmartObjectHelper"; };
	const char* Category() { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS( CSmartObjectHelperObject ); };
};

#endif // __SmartObjectHelperObject_h__
