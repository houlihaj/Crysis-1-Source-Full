////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   ISubObjectSelectionReferenceFrameCalculator.h
//  Version:     v1.00
//  Created:     9/3/2006 Michael Smith
//  Compilers:   Visual Studio.NET 2005
//  Description: Calculate the reference frame for sub-object selections.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __ISUBOBJECTSELECTIONREFERENCEFRAMECALCULATOR_H__
#define __ISUBOBJECTSELECTIONREFERENCEFRAMECALCULATOR_H__

struct SBrush;

class ISubObjectSelectionReferenceFrameCalculator
{
public:
	virtual void AddBrush(const Matrix34& worldTM, SBrush* pBrush) = 0;
	virtual void SetExplicitFrame(bool bAnySelected, const Matrix34& refFrame) = 0;
};

#endif //__ISUBOBJECTSELECTIONREFERENCEFRAMECALCULATOR_H__
