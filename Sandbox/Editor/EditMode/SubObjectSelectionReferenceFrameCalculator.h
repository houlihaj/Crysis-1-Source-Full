////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   SubObjectSelectionReferenceFrameCalculator.h
//  Version:     v1.00
//  Created:     9/3/2006 Michael Smith
//  Compilers:   Visual Studio.NET 2005
//  Description: Calculate the reference frame for sub-object selections.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __SUBOBJECTSELECTIONREFERENCEFRAMECALCULATOR_H__
#define __SUBOBJECTSELECTIONREFERENCEFRAMECALCULATOR_H__

#include "ISubObjectSelectioNReferenceFrameCalculator.h"
#include "Objects\SubObjSelection.h"

class SubObjectSelectionReferenceFrameCalculator : public ISubObjectSelectionReferenceFrameCalculator
{
public:
	SubObjectSelectionReferenceFrameCalculator(ESubObjElementType selectionType);

	virtual void AddBrush(const Matrix34& worldTM, SBrush* pBrush);
	virtual void SetExplicitFrame(bool bAnySelected, const Matrix34& refFrame);
	bool GetFrame(Matrix34& refFrame);

private:
	bool bAnySelected;
	Vec3 pos;
	Vec3 normal;
	int nNormals;
	ESubObjElementType selectionType;
	std::vector<Vec3> positions;
	Matrix34 refFrame;
	bool bUseExplicitFrame;
	bool bExplicitAnySelected;
};

#endif //__SUBOBJECTSELECTIONREFERENCEFRAMECALCULATOR_H__
