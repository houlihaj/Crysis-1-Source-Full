////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   TagPoint.h
//  Version:     v1.00
//  Created:     10/10/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: TagPoint object definition.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __RopeObject_h__
#define __RopeObject_h__

#pragma once

#include "ShapeObject.h"

//////////////////////////////////////////////////////////////////////////
class CRopeObject : public CShapeObject
{
	DECLARE_DYNCREATE(CRopeObject)
public:
	CRopeObject();
	
	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	virtual bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	virtual void Done();
	virtual void SetScale( const Vec3 &vScale,int nWhyFlags=0 ) {};
	virtual void SetSelected( bool bSelect );
	virtual bool CreateGameObject();
	virtual void InitVariables();
	virtual void UpdateGameArea( bool bRemove=false );
	virtual void InvalidateTM( int nWhyFlags );
	virtual void SetMaterial( CMaterial *mtl );
	virtual void Display( DisplayContext &dc );
	virtual void OnEvent( ObjectEvent event );

	virtual void BeginEditParams( IEditor *ie,int flags );
	virtual void EndEditParams( IEditor *ie );
	virtual void BeginEditMultiSelParams( bool bAllOfSameType );
	virtual void EndEditMultiSelParams();

	virtual void Serialize( CObjectArchive &ar );
	virtual void PostLoad( CObjectArchive &ar );
	virtual XmlNodeRef Export( const CString &levelPath,XmlNodeRef &xmlNode );
	//////////////////////////////////////////////////////////////////////////

protected:
	virtual bool IsOnlyUpdateOnUnselect() const { return false; }
	virtual int GetMinPoints() const { return 2; };
	virtual int GetMaxPoints() const { return 200; };
	virtual float GetShapeZOffset() const { return 0.0f; }
	virtual void CalcBBox();

	IRopeRenderNode *GetRenderNode();

	//! Called when Road variable changes.
	void OnParamChange( IVariable *var );

protected:
	friend class CRopePanelUI;

	IRopeRenderNode::SRopeParams m_ropeParams;

	struct SEndPointLinks
	{
		int m_nPhysEntityId;
		Vec3 offset;
		Vec3 object_pos;
		Quat object_q;
	};
	SEndPointLinks m_linkedObjects[2];
	int m_endLinksDisplayUpdateCounter;
};

/*!
* Class Description of CVisAreaShapeObject.
*/
class CRopeObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {DC346747-0A92-439b-9281-2EAB88593FCE}
		static const GUID guid = { 0xdc346747, 0xa92, 0x439b, { 0x92, 0x81, 0x2e, 0xab, 0x88, 0x59, 0x3f, 0xce } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_OTHER; };
	const char* ClassName() { return "Rope"; };
	const char* Category() { return "Misc"; };
	virtual const char* GetTextureIcon() { return "Editor/ObjectIcons/rope.bmp"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CRopeObject); };
	int GameCreationOrder() { return 300; }; // after most entities are created.
};

#endif // __RopeObject_h__