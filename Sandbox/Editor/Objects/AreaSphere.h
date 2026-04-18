////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   AreaSphere.h
//  Version:     v1.00
//  Created:     25/10/2001 by Lennert.
//  Compilers:   Visual C++ 6.0
//  Description: AreaSphere object definition.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AreaSphere_h__
#define __AreaSphere_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Entity.h"
#include "PickEntitiesPanel.h"
#include "AreaBox.h"

/*!
 *	CAreaSphere is a sphere volume in space where entites can be attached to.
 *
 */
class CAreaSphere : public CAreaObjectBase
{
public:
	DECLARE_DYNCREATE(CAreaSphere)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void Done();
	bool CreateGameObject();
	virtual void InitVariables();

	void Display( DisplayContext &dc );

	void SetAngles( const Ang3& angles );
	void SetScale( const Vec3 &scale );

	void GetLocalBounds( BBox &box );

	bool HitTest( HitContext &hc );

	void BeginEditParams( IEditor *ie,int flags );
	void EndEditParams( IEditor *ie );

	virtual void PostLoad( CObjectArchive &ar );

	void Serialize( CObjectArchive &ar );
	XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode );

	void SetAreaId(int nAreaId);
	int GetAreaId();
	void SetRadius(float fRadius);
	float GetRadius();

	void UpdateGameArea();

protected:
	//! Dtor must be protected.
	CAreaSphere();

	void DeleteThis() { delete this; };

	void OnAreaChange(IVariable *pVar);
	void OnSizeChange(IVariable *pVar);

	//! AreaId
	CVariable<int> m_areaId;
	//! EdgeWidth
	CVariable<float> m_edgeWidth;
	//! Local volume space radius.
	CVariable<float> m_radius;
	CVariable<int> mv_groupId;
	CVariable<int> mv_priority;
	CVariable<bool> mv_filled;

	static int m_nRollupId;
	static CPickEntitiesPanel *m_pPanel;
};

/*!
 * Class Description of AreaBox.
 */
class CAreaSphereClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {640A1105-A421-4ed3-9E71-7783F091AA27}
		static const GUID guid = { 0x640a1105, 0xa421, 0x4ed3, { 0x9e, 0x71, 0x77, 0x83, 0xf0, 0x91, 0xaa, 0x27 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_VOLUME; };
	const char* ClassName() { return "AreaSphere"; };
	const char* Category() { return "Area"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAreaSphere); };
	int GameCreationOrder() { return 51; };
};

#endif // __AreaSphere_h__