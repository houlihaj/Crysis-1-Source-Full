////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   AreaBox.h
//  Version:     v1.00
//  Created:     22/10/2001 by Lennert.
//  Compilers:   Visual C++ 6.0
//  Description: AreaBox object definition.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __AreaBox_h__
#define __AreaBox_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Entity.h"
#include "PickEntitiesPanel.h"

//////////////////////////////////////////////////////////////////////////
// Base class for area objects.
//////////////////////////////////////////////////////////////////////////
class CAreaObjectBase : public CEntity, public IPickEntitesOwner
{
public:
	virtual void UpdateGameArea() {};
	//////////////////////////////////////////////////////////////////////////
	// Binded entities.
	//////////////////////////////////////////////////////////////////////////
	//! Bind entity to this shape.
	void AddEntity( CBaseObject *pEntity );
	void RemoveEntity( int index );
	CEntity* GetEntity( int index );
	int GetEntityCount() { return m_entities.size(); }

protected:
	virtual void PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx );

	//! List of binded entities.
	std::vector<GUID> m_entities;
};

/*!
 *	CAreaBox is a box volume in space where entites can be attached to.
 *
 */
class CAreaBox : public CAreaObjectBase
{
public:
	DECLARE_DYNCREATE(CAreaBox)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void Done();
	bool CreateGameObject();
	virtual void InitVariables();

	void Display( DisplayContext &dc );

	void InvalidateTM( int nWhyFlags );

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
	void SetBox(BBox box);
	BBox GetBox();

	virtual void UpdateGameArea();

protected:
	//! Dtor must be protected.
	CAreaBox();

	void DeleteThis() { delete this; };

	void OnAreaChange(IVariable *pVar);
	void OnSizeChange(IVariable *pVar);

	//! AreaId
	CVariable<int> m_areaId;
	//! EdgeWidth
	CVariable<float> m_edgeWidth;
	//! Local volume space bounding box.
	CVariable<float> mv_width;
	CVariable<float> mv_length;
	CVariable<float> mv_height;
	CVariable<int> mv_groupId;
	CVariable<int> mv_priority;
	
	//! make roof completely block fading
	CVariable<bool> mv_obstructRoof;
	//! make floor completely block fading
	CVariable<bool> mv_obstructFloor;

	CVariable<bool> mv_filled;

	//! Local volume space bounding box.
	BBox m_box;

	static int m_nRollupId;
	static CPickEntitiesPanel *m_pPanel;
};

/*!
 * Class Description of AreaBox.
 */
class CAreaBoxClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {0EEA0A78-428C-4ad4-9EC1-97525AEB1BCB}
		static const GUID guid = { 0xeea0a78, 0x428c, 0x4ad4, { 0x9e, 0xc1, 0x97, 0x52, 0x5a, 0xeb, 0x1b, 0xcb } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_VOLUME; };
	const char* ClassName() { return "AreaBox"; };
	const char* Category() { return "Area"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CAreaBox); };
	int GameCreationOrder() { return 52; };
};

#endif // __AreaBox_h__