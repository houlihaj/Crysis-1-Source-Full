////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   ITransformManipulator.h
//  Version:     v1.00
//  Created:     9/12/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ITransformManipulator_h__
#define __ITransformManipulator_h__
#pragma once

//////////////////////////////////////////////////////////////////////////
// ITransformManipulator implementation.
//////////////////////////////////////////////////////////////////////////
struct ITransformManipulator
{
	virtual Matrix34 GetTransformation( RefCoordSys coordSys ) const = 0;
	virtual void SetTransformation( RefCoordSys coordSys,const Matrix34 &tm ) = 0;
	virtual bool HitTestManipulator( HitContext &hc ) = 0;
	virtual bool MouseCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags ) = 0;
};

#endif // __ITransformManipulator_h__
