#ifndef __VehicleComponent_h__
#define __VehicleComponent_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Objects/BaseObject.h"
#include "Util/Variable.h"

class CVehicleHelper;
class CVehiclePart;
class CVehiclePrototype;


#include "VehicleDialogComponent.h"


/*!
*	CVehicleComponent represents an editable vehicle Component
*
*/
class CVehicleComponent : public CBaseObject, public CVeedObject
{
public:
  DECLARE_DYNCREATE(CVehicleComponent)

  //////////////////////////////////////////////////////////////////////////
  // Overwrites from CBaseObject.
  //////////////////////////////////////////////////////////////////////////  
  void Done();
  void InitVariables() {};
  void Display( DisplayContext &disp );

  bool HitTest( HitContext &hc );

  void GetLocalBounds( BBox &box );
  void GetBoundBox( BBox &box );

  void AttachChild( CBaseObject* child, bool bKeepPos=true);    

  void Serialize( CObjectArchive &ar ){}
  /////////////////////////////////////////////////////////////////////////

  
  void AddComponent(CVehicleComponent* pComponent);    
  CString GetComponentClass();
  
  // Overrides from IVeedObject
  /////////////////////////////////////////////////////////////////////////
  void UpdateVarFromObject();
  void UpdateObjectFromVar();

  const char* GetElementName(){ return "Component"; }    
  int GetIconIndex();

  void SetVariable(IVariable* pVar);
  void UpdateScale(float scale);
  /////////////////////////////////////////////////////////////////////////

  void SetVehicle(CVehiclePrototype* pProt){ m_pVehicle = pProt; }

  void OnObjectEvent( CBaseObject *node, int event );
  
protected:
  CVehicleComponent();
  void DeleteThis() { delete this; };
  void PostClone( CBaseObject *pFromObject, CObjectCloneContext &ctx );

  void UpdateFromVar();
  void OnSetBehaviorClass(IVariable* pVar);
      
  CVehiclePrototype* m_pVehicle;
  CVehicleComponent* m_pParent;

  IVariable* m_pMinBound;
  IVariable* m_pMaxBound;
  bool m_partBounds;
  
  Vec3 m_min; // object cs
  Vec3 m_max; // object cs

};


/*!
* Class Description of VehicleComponent.	
*/
class CVehicleComponentClassDesc : public CObjectClassDesc
{
public:
  REFGUID ClassID()
  {
    // {680D0873-CC5F-48da-B88F-E8F95D666D81}
    static const GUID guid = { 0x680d0873, 0xcc5f, 0x48da, { 0xb8, 0x8f, 0xe8, 0xf9, 0x5d, 0x66, 0x6d, 0x81 } };
    return guid;
  }
  ObjectType GetObjectType() { return OBJTYPE_OTHER; };
  const char* ClassName() { return "VehicleComponent"; };
  const char* Category() { return ""; };
  CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVehicleComponent); };  
};



#endif // __VehicleComponent_h__