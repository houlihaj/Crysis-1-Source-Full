////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2005.
// -------------------------------------------------------------------------
//  File name:   VehicleWeapon.h
//  Version:     v1.00
//  Created:     22/09/2005 by MichaelR
//  Compilers:   Visual C++ 6.0
//  Description: Vehicle Weapon
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __VehicleWeapon_h__
#define __VehicleWeapon_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Objects/BaseObject.h"
#include "VehicleDialogComponent.h"

class CVehiclePart;
class CVehicleSeat;
class CVehiclePrototype;

/*!
 *	CVehicleWeapon represents a mounted vehicle weapon and is supposed 
 *  to be used as children of a VehicleSeat.
 */
class CVehicleWeapon : public CBaseObject, public CVeedObject
{
public:
	DECLARE_DYNCREATE(CVehicleWeapon)
  ~CVehicleWeapon(){}

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////	
	void Done();

	void Display( DisplayContext &dc );
	void BeginEditParams( IEditor *ie,int flags );
		
  void GetBoundBox( BBox &box );
  void GetLocalBounds( BBox &box );
	bool HitTest( HitContext &hc );
  void Serialize( CObjectArchive &ar ){}
	//////////////////////////////////////////////////////////////////////////

  // Ovverides from IVeedObject.
  //////////////////////////////////////////////////////////////////////////
  void UpdateVarFromObject();
  void UpdateObjectFromVar(){}

  const char* GetElementName(){ return "Weapon"; }
  virtual int GetIconIndex(){ return VEED_WEAPON_ICON; }
    
  void SetVariable(IVariable* pVar){ m_pVar = pVar; }  
  //////////////////////////////////////////////////////////////////////////
    
  void SetVehicle(CVehiclePrototype* pProt){ m_pVehicle = pProt;}

protected:	
  CVehicleWeapon();
	void DeleteThis() { delete this; };
  
  CVehicleSeat* m_pSeat;
  CVehiclePrototype* m_pVehicle;
};

/*!
 * Class Description of VehicleWeapon.	
 */
class CVehicleWeaponClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
    // {93296AA8-14EC-4738-BD67-19FF83532C4F}
    static const GUID guid = { 0x93296aa8, 0x14ec, 0x4738, { 0xbd, 0x67, 0x19, 0xff, 0x83, 0x53, 0x2c, 0x4f } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_OTHER; };
	const char* ClassName() { return "VehicleWeapon"; };
	const char* Category() { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVehicleWeapon); };
};

#endif // __VehicleWeapon_h__