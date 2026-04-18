////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek 2001-2007.
// -------------------------------------------------------------------------
//  File name:   CharAttachHelper.h
//  Version:     v1.00
//  Created:     09/06/2007 by Timur.
//  Description: Entity character attachment helper object.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CharAttachHelper_h__
#define __CharAttachHelper_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Entity.h"

//////////////////////////////////////////////////////////////////////////
class CRYEDIT_API CCharacterAttachHelperObject : public CEntity
{
public:
	DECLARE_DYNCREATE(CCharacterAttachHelperObject)

	CCharacterAttachHelperObject();

	//////////////////////////////////////////////////////////////////////////
	// CEntity
	//////////////////////////////////////////////////////////////////////////
	virtual void InitVariables() {};
	virtual void Display( DisplayContext &disp );
	
	virtual void SetHelperScale( float scale ) { m_charAttachHelperScale = scale; };
	virtual float GetHelperScale() { return m_charAttachHelperScale; };
	//////////////////////////////////////////////////////////////////////////
private:
	static float m_charAttachHelperScale;
};

/*!
* Class Description of Entity
*/
class CCharacterAttachHelperObjectClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
		// {E7F69317-3179-488b-ACC0-69994E0C0CF2}
		static const GUID guid = { 0xe7f69317, 0x3179, 0x488b, { 0xac, 0xc0, 0x69, 0x99, 0x4e, 0xc, 0xc, 0xf2 } };
		return guid;
	}
	virtual ObjectType GetObjectType() { return OBJTYPE_ENTITY; };
	virtual const char* ClassName() { return "CharAttachHelper"; };
	virtual const char* Category() { return "Misc"; };
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CCharacterAttachHelperObject); };
	virtual int GameCreationOrder() { return 200; };
	virtual const char* GetTextureIcon() { return "Editor/ObjectIcons/Magnet.bmp"; };
};

#endif // __CharAttachHelper_h__