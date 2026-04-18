
#ifndef __VehiclePrototype_h__
#define __VehiclePrototype_h__

#if _MSC_VER > 1000
#pragma once
#endif


#include "Objects/Entity.h"
#include "VehicleDialogComponent.h"


struct IVehicleData;
class CVehicleEditorDialog;
class CVehiclePart;
class CVehicleComponent;

struct IVehiclePrototype
{  
  virtual IVehicleData* GetVehicleData() = 0;  
};

/*!
*	CVehiclePrototype represents an editable Vehicle.
*
*/
class CVehiclePrototype : public CBaseObject, public CVeedObject, public IVehiclePrototype
{
public:
  DECLARE_DYNCREATE(CVehiclePrototype)
  

  //////////////////////////////////////////////////////////////////////////
  // Overwrites from CBaseObject.
  //////////////////////////////////////////////////////////////////////////
  bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
  void InitVariables() {};
  void Display( DisplayContext &disp );  
  void Done();
  
  bool HitTest( HitContext &hc );

  void GetLocalBounds( BBox &box );
  void GetBoundBox( BBox &box );

  void AttachChild( CBaseObject* child, bool bKeepPos=true);     
  void RemoveChild( CBaseObject *node );

  void Serialize( CObjectArchive &ar );
  //////////////////////////////////////////////////////////////////////////

  // IVeedObject
  //////////////////////////////////////////////////////////////////////////
  void UpdateVarFromObject(){}
  void UpdateObjectFromVar(){}

  const char* GetElementName(){ return "Vehicle"; }
  IVariable* GetVariable();  

  virtual int GetIconIndex(){ return VEED_VEHICLE_ICON; }
  //////////////////////////////////////////////////////////////////////////

  void SetVehicleEntity(CEntity* pEnt);
  CEntity* GetCEntity(){ return m_vehicleEntity; }

  bool hasEntity(){ return m_vehicleEntity!=0; }
  bool ReloadEntity();
  
  IVehicleData* GetVehicleData();
  void ApplyClonedData();
  
  void AddPart(CVehiclePart* pPart);
  void AddComponent(CVehicleComponent* pComp);

  void OnObjectEvent( CBaseObject *node, int event ); 
 
protected:
  CVehiclePrototype();
  void DeleteThis();
    
  CEntity* m_vehicleEntity;
  
  // root of vehicle data tree
  IVehicleData* m_pVehicleData;
  IVariablePtr m_pClone;

  CString m_name;

  friend class CVehicleEditorDialog;
  
};


/*!
* Class Description of VehicleObject.	
*/
class CVehiclePrototypeClassDesc : public CObjectClassDesc
{
public:
  REFGUID ClassID()
  {
    // {559A1504-0AF2-47f2-A3A9-DFDF5720E232}
    static const GUID guid = { 0x559a1504, 0xaf2, 0x47f2, { 0xa3, 0xa9, 0xdf, 0xdf, 0x57, 0x20, 0xe2, 0x32 } };
    return guid;
  }
  ObjectType GetObjectType() { return OBJTYPE_OTHER; };
  const char* ClassName() { return "VehiclePrototype"; };
  const char* Category() { return ""; };
  CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVehiclePrototype); };
  const char* GetFileSpec() { return "Scripts/Entities/Vehicles/Implementations/Xml\\*.xml"; };
  //int GameCreationOrder() { return 150; }; 
};



#endif // __VehicleObject_h__