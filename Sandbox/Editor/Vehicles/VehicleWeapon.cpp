////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   VehicleWeapon.cpp
//  Version:     v1.00
//  Created:     21/06/2005 by MichaelR
//  Compilers:   Visual C++ 6.0
//  Description: Vehicle Weapon Object implementation
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "VehicleWeapon.h"

#include "VehicleData.h"
#include "VehiclePrototype.h"
#include "VehiclePart.h"
#include "VehicleSeat.h"


IMPLEMENT_DYNCREATE(CVehicleWeapon,CBaseObject)

//////////////////////////////////////////////////////////////////////////
CVehicleWeapon::CVehicleWeapon()
{	
  m_pVar = 0;  
  m_pSeat = 0;
  m_pVehicle = 0;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleWeapon::UpdateVarFromObject()
{  
  if (!m_pVar || !m_pVehicle)
    return;
  
}

//////////////////////////////////////////////////////////////////////////
void CVehicleWeapon::Done()
{	
  VeedLog("[CVehicleWeapon:Done] <%s>", GetName());
  CBaseObject::Done();
}



//////////////////////////////////////////////////////////////////////////
void CVehicleWeapon::BeginEditParams( IEditor *ie,int flags )
{	
}


//////////////////////////////////////////////////////////////////////////
bool CVehicleWeapon::HitTest( HitContext &hc )
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleWeapon::Display( DisplayContext &dc )
{
	// todo: draw at mount helper, add rotation limits from parts

	DrawDefault( dc );
}


//////////////////////////////////////////////////////////////////////////
void CVehicleWeapon::GetBoundBox( BBox &box )
{  
  // Transform local bounding box into world space.
  GetLocalBounds( box );
  box.SetTransformedAABB( GetWorldTM(), box );  
}

//////////////////////////////////////////////////////////////////////////
void CVehicleWeapon::GetLocalBounds( BBox &box )
{ 
  // todo
  // return local bounds    
}
