////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   JoystickUtils.h
//  Version:     v1.00
//  Created:     28/6/2007 by Michael S.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __JOYSTICKUTILS_H__
#define __JOYSTICKUTILS_H__

class IJoystick;
class IJoystickChannel;

namespace JoystickUtils
{
	float Evaluate(IJoystickChannel* pChannel, float time);
	void SetKey(IJoystickChannel* pChannel, float time, float value, bool createIfMissing=true);
	void Serialize(IJoystickChannel* pChannel, XmlNodeRef node, bool bLoading);
	bool HasKey(IJoystickChannel* pChannel, float time);
	void RemoveKey(IJoystickChannel* pChannel, float time);
	void RemoveKeysInRange(IJoystickChannel* pChannel, float startTime, float endTime);
	void PlaceKey(IJoystickChannel* pChannel, float time);
}

#endif //__JOYSTICKUTILS_H__
