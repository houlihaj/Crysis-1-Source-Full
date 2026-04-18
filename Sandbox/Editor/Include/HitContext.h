////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   HitContext.h
//  Version:     v1.00
//  Created:     15/12/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __HitContext_h__
#define __HitContext_h__
#pragma once

class CGizmo;
class CBaseObject;
class CViewport;

//////////////////////////////////////////////////////////////////////////
// Flags used in HitContext for nSubObjFlags member.
//////////////////////////////////////////////////////////////////////////
enum ESubObjHitFlags
{
	SO_HIT_SELECT          = BIT(1),  // When set all hit elements will be selected.
	SO_HIT_TEST_SELECTED   = BIT(2),  // Only test selected elements for hit.
	SO_HIT_POINT           = BIT(3),   // Only hit test point2d, not rectangle (Will only test/select 1 closest element).
	SO_HIT_SELECT_ADD      = BIT(4), // Adds hit elements to previously selected ones.
	SO_HIT_SELECT_REMOVE   = BIT(5), // Remove hit elements from previously selected ones.
	SO_HIT_SELECTION_CHANGED = BIT(6), // Output flag, set if selection was changed.
	SO_HIT_HIGHLIGHT_ONLY  = BIT(7),  // Hit testing to highlight sub object-element.

	SO_HIT_ELEM_VERTEX  = BIT(10), // Check hit with vertices.
	SO_HIT_ELEM_EDGE    = BIT(11), // Check hit with edges.
	SO_HIT_ELEM_FACE    = BIT(12), // Check hit with faces.
	SO_HIT_ELEM_POLYGON = BIT(13)  // Check hit with polygons.
};
#define SO_HIT_ELEM_ALL (SO_HIT_ELEM_VERTEX|SO_HIT_ELEM_EDGE|SO_HIT_ELEM_FACE|SO_HIT_ELEM_POLYGON)

//! Collision structure passed to HitTest function.
struct HitContext
{
	CViewport *view;			// Viewport that originates hit testing.
	CPoint point2d;				// 2D point on view that is used for hit testig.
	CRect rect;						// 2d Selection rectangle (Only when HitTestRect)
	AABB *bounds;         // Optional limiting bounding box for hit testing.
	CCamera *camera;      // Optional camera for culling perspective viewports.

	bool b2DViewport;		// Testing performed in 2D viewport.
	bool bIgnoreAxis;			// True if axis collision must be ignored.
	bool bOnlyGizmo;			// Hit test only gizmo objects
	bool bUseSelectionHelpers; // Test objects using advanced selection helpers.

	// Input parameters.
	Vec3 raySrc;								// Ray origin.
	Vec3 rayDir;								// Ray direction.
	float distanceTollerance;		// Relaxation parameter for hit testing.
	int nSubObjFlags;           // Sub object hit testing flags, @see ESubObjHitFlags

	//////////////////////////////////////////////////////////////////////////
	// Output parameters.
	//////////////////////////////////////////////////////////////////////////
	bool weakHit;								//!< True if this hit should have less priority then non weak hits.
	//													//!< (exp: Ray hit entity bounding box but not entity geometry.)
	int axis;										//! Constrain axis if hit AxisGizmo.
	int manipulatorMode;        // if hit axis gizmo, 1 - move mode, 2 - rotate mode, 3 - scale mode.
	// Output parameters.
	//! Distance to the object from src.
	float dist;
	CBaseObject* object;        // Object that have been hit.
	CGizmo* gizmo;              // Gizmo object that have been hit.

	//Ctor
	HitContext()
	{
		rect.top = rect.left = rect.bottom = rect.right = 0;
		b2DViewport = false;
		view = 0;
		camera = 0;
		point2d.x = 0; point2d.y = 0;
		axis = 0;
		distanceTollerance = 0;
		raySrc(0,0,0);
		rayDir(0,0,0);
		dist = 0;
		object = 0;
		weakHit = false;
		manipulatorMode = 0;
		nSubObjFlags = 0;
		bounds = 0;
		bIgnoreAxis = false;
		bOnlyGizmo = false;
		bUseSelectionHelpers = false;
		//		geometryHit = false;
	}
};

#endif // __HitContext_h__
