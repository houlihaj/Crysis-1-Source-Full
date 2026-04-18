////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   SequenceObject.h
//  Version:     v1.00
//  Created:     05/02/2005 by Sergiy Shaykin.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SequenceObject_h__
#define __SequenceObject_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "BaseObject.h"

class CSequenceObject : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CSequenceObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void Display( DisplayContext &dc );

	void GetBoundBox( BBox &box );
	void GetLocalBounds( BBox &box );
	void SetName( const CString &name );
	bool CreateGameObject();
	void Done();
	void Serialize( CObjectArchive &ar );
	virtual void PostLoad( CObjectArchive &ar );

	void SetModified();

	IAnimSequence* GetSequence() {return m_pSequence;}

protected:
	//! Dtor must be protected.
	CSequenceObject();

	void DeleteThis() { delete this; };

	//////////////////////////////////////////////////////////////////////////
	// Local callbacks.

private:
	BBox m_bbox;
	struct IAnimSequence * m_pSequence;
};

/*!
 * Class Description of CSequenceObject.	
 */
class CSequenceObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {3951b7f0-51df-4765-8b6f-13493818b9c7}
		static const GUID guid = { 0x3951b7f0, 0x51df, 0x4765, { 0x8b, 0x6f, 0x13, 0x49, 0x38, 0x18, 0xb9, 0xc7 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_OTHER; };
	const char* ClassName() { return "SequenceObject"; };
	const char* Category() { return "Misc"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CSequenceObject); };
	const char* GetFileSpec() { return ""; };
	int GameCreationOrder() { return 950; };
	const char* GetTextureIcon() { return "Editor/ObjectIcons/sequence.bmp"; };
};




#endif // __SequenceObject_h__