////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   bbox.h
//  Version:     v1.00
//  Created:     2/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __bbox_h__
#define __bbox_h__

#if _MSC_VER > 1000
#pragma once
#endif

/*!
 *  BBox is a bounding box structure.
 */
struct BBox : public AABB
{
	BBox() {}
	BBox( const Vec3 &vMin,const Vec3 &vMax ) { min = vMin; max = vMax; }

	bool IsEmpty() const { return min.IsEquivalent(max,VEC_EPSILON); }

	bool IsIntersectRay( const Vec3 &origin,const Vec3 &dir,Vec3 &pntContact );

	//! Check if ray intersect edge of bounding box.
	//! @param epsilonDist if distance between ray and egde is less then this epsilon then edge was intersected.
	//! @param dist Distance between ray and edge.
	//! @param intPnt intersection point.
	bool RayEdgeIntersection( const Vec3 &raySrc,const Vec3 &rayDir,float epsilonDist,float &dist,Vec3 &intPnt );
};

#endif // __bbox_h__
