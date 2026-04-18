////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   IKeyTimeSet.h
//  Version:     v1.00
//  Created:     14/9/2006 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __IKEYTIMESET_H__
#define __IKEYTIMESET_H__

class IKeyTimeSet
{
public:
	virtual int GetKeyTimeCount() const = 0;
	virtual float GetKeyTime(int index) const = 0;
	virtual void MoveKeyTimes(int numChanges, int* indices, float scale, float offset, bool copyKeys) = 0;
	virtual bool GetKeyTimeSelected(int index) const = 0;
	virtual void SetKeyTimeSelected(int index, bool selected) = 0;
	virtual int GetKeyCount(int index) const = 0;
	virtual int GetKeyCountBound() const = 0;
	virtual void BeginEdittingKeyTimes() = 0;
	virtual void EndEdittingKeyTimes() = 0;
};

#endif //__IKEYTIMESET_H__
