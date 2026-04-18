////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   grid.h
//  Version:     v1.00
//  Created:     8/7/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __grid_h__
#define __grid_h__

#if _MSC_VER > 1000
#pragma once
#endif

/** Definition of grid used in 2D viewports.
*/
class CGrid
{
public:
	//! Resolution of grid, it must be multiply of 2.
	double size;
	//! Draw major lines every Nth grid line.
	int majorLine;
	//! True if grid enabled.
	bool bEnabled;
	//! Meters per grid unit.
	double scale;

	bool bUserDefined;
	bool bGetFromObject;
	Ang3 rotationAngles;
	Vec3 translation;

	//! If snap to angle.
	bool bAngleSnapEnabled;
	double angleSnap;

	//////////////////////////////////////////////////////////////////////////
	CGrid();

	//! Snap vector to this grid.
	Vec3 Snap( const Vec3 &vec ) const;
	Vec3 Snap( const Vec3 &vec,double fZoom ) const;

	//! Snap angle to current angle snapping value.
	double SnapAngle( double angle ) const;
	//! Snap angle to current angle snapping value.
	Ang3 SnapAngle( const Ang3& angle ) const;

	//! Enable or disable grid.
	void Enable( bool enable ) { bEnabled = enable; }
	//! Check if grid enabled.
	bool IsEnabled() const { return bEnabled; }

	//! Enables or disable angle snapping.
	void EnableAngleSnap( bool enable ) { bAngleSnapEnabled = enable; };

	//! Return if snapping of angle is enabled.
	bool IsAngleSnapEnabled() const { return bAngleSnapEnabled; };
	//! Returns ammount of snapping for angle in degrees.
	double GetAngleSnap() const { return angleSnap; };

	void Serialize( XmlNodeRef &xmlNode,bool bLoading );

	//! Get orientation matrix of gird.
	Matrix34 GetOrientationMatrix() const;
	//! Get translation of grid.
	Vec3 GetTranslation() const;
	//! Get transformation matrix of grid.
	Matrix34 GetMatrix() const;
};


#endif // __grid_h__
