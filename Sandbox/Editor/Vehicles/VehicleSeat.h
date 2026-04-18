#ifndef __VehicleSeat_h__
#define __VehicleSeat_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include <list>

#include "Objects/Entity.h"
#include "Util/Variable.h"


class CVehiclePrototype;
class CVehicleHelper;
class CVehiclePart;
class CVehicleWeapon;

#include "VehicleDialogComponent.h"

typedef enum {
  WEAPON_PRIMARY,
  WEAPON_SECONDARY
} eWeaponType;

/*!
*	CVehicleSeat represents an editable vehicle seat.
*
*/
class CVehicleSeat : public CBaseObject, public CVeedObject
{
public:
  DECLARE_DYNCREATE(CVehicleSeat)

  //////////////////////////////////////////////////////////////////////////
  // Overwrites from CBaseObject.
  //////////////////////////////////////////////////////////////////////////
  bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
  void Done();
  void InitVariables() {};
  void Display( DisplayContext &disp );

  bool HitTest( HitContext &hc );

  void GetLocalBounds( BBox &box );
  void GetBoundBox( BBox &box );  

  void AttachChild( CBaseObject* child, bool bKeepPos=true);  

  void Serialize( CObjectArchive &ar ){}
  /////////////////////////////////////////////////////////////////////////
  
    
  // Overrides from IVeedObject
  /////////////////////////////////////////////////////////////////////////
  IVariable* GetVariable(){ return m_pVar; }

  void UpdateVarFromObject();
  void UpdateObjectFromVar();

  const char* GetElementName(){ return "Seat"; }  
  virtual int GetIconIndex(){ return VEED_SEAT_ICON; }
  /////////////////////////////////////////////////////////////////////////

  void SetVehicle(CVehiclePrototype* pProt);
  void SetVariable(IVariable* pVar); 
  
  //! Sets/gets the optional part the seat belongs to
  void SetPart(CVehiclePart* pPart){ m_pPart = pPart; }  
  CVehiclePart* GetPart(){ return m_pPart; }

  //! Add Weapon to Seat
  void AddWeapon(int weaponType, CVehicleWeapon* pWeap, IVariable* pVar = 0);

  void OnObjectEvent( CBaseObject *node, int event );

  void OnSetPart(IVariable* pVar);
  
protected:
  CVehicleSeat();
  void DeleteThis() { delete this; };

  void UpdateFromVar();
  
  CVehiclePrototype* m_pVehicle;
  CVehiclePart* m_pPart;

  IVariable* m_pVar;  
};


/*!
* Class Description of VehicleSeat.	
*/
class CVehicleSeatClassDesc : public CObjectClassDesc
{
public:
  REFGUID ClassID()
  {
    // {7EA47EA4-E49D-43ae-A862-15A5B764F8DC}
    static const GUID guid = { 0x7ea47ea4, 0xe49d, 0x43ae, { 0xa8, 0x62, 0x15, 0xa5, 0xb7, 0x64, 0xf8, 0xdc } };
    return guid;
  }
  ObjectType GetObjectType() { return OBJTYPE_OTHER; };
  const char* ClassName() { return "VehicleSeat"; };
  const char* Category() { return ""; };
  CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVehicleSeat); };  
};



#endif // __VehicleSeat_h__