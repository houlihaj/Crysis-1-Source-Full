////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   AnimUtils.h
//  Version:     v1.00
//  Created:     9/11/2006 by Michael S.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __ANIMUTILS_H__
#define __ANIMUTILS_H__

struct ICharacterInstance;

namespace AnimUtils
{
	void StartAnim(ICharacterInstance* pCharacter, const string& animName);
	void SetAnimTime(ICharacterInstance* pCharacter, float time);
	void StopAnims(ICharacterInstance* pCharacter);
}

#endif //__ANIMUTILS_H__
