#ifndef __VehiclePart_h__
#define __VehiclePart_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include <list>

#include "Objects/Entity.h"
#include "Util/Variable.h"

class CVehicleHelper;
class CVehicleSeat;
class CVehiclePrototype;

#include "VehicleDialogComponent.h"

#define PARTCLASS_WHEEL "SubPartWheel"
#define PARTCLASS_MASS  "MassBox"
#define PARTCLASS_LIGHT "Light"
#define PARTCLASS_TREAD "Tread"

typedef std::list<CVehicleHelper*> THelperList;

/*!
*	CVehiclePart represents an editable vehicle part.
*
*/
class CVehiclePart : public CBaseObject, public CVeedObject
{
public:
  DECLARE_DYNCREATE(CVehiclePart)

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

  void AttachChild( CBaseObject* child, bool bKeepPos=true );    
  void RemoveChild( CBaseObject *node );

  void Serialize( CObjectArchive &ar ){}
  /////////////////////////////////////////////////////////////////////////

  void AddHelper(CVehicleHelper* pHelper, IVariable* pHelperVar = 0);    
  int GetNumHelpers() const { return m_helpers.size(); }  
  CVehicleHelper* GetHelper(CString name);  

  void AddPart(CVehiclePart* pPart);
  void SetMainPart(bool bMain){ m_isMain = bMain; }  
  bool IsMainPart() const { return m_isMain; }
  CString GetPartClass();

  //! returns whether this part is a leaf part (false means it can have children)
  bool IsLeaf();
  
  void SetVariable(IVariable* pVar);  
  IVariable* GetVariable(){ return m_pVar; }
  
  // Overrides from IVeedObject
  /////////////////////////////////////////////////////////////////////////
  void UpdateVarFromObject();
  void UpdateObjectFromVar();

  const char* GetElementName(){ return "Part"; }    
  int GetIconIndex();
  void UpdateScale(float scale);
  void OnTreeSelection();
  /////////////////////////////////////////////////////////////////////////

  void SetVehicle(CVehiclePrototype* pProt){ m_pVehicle = pProt; }

  void OnObjectEvent( CBaseObject *node, int event );
  
protected:
  CVehiclePart();
  void DeleteThis() { delete this; };

  void UpdateFromVar();

  void OnSetClass(IVariable* pVar);
  void OnSetPos(IVariable* pVar);

  void DrawRotationLimits(DisplayContext& dc, IVariable* pSpeed, IVariable* pLimits, IVariable* pHelper, int axis);
  
  CVehiclePrototype* m_pVehicle;
  CVehiclePart* m_pParent;

  THelperList m_helpers;
  bool m_isMain;
  IVariable* m_pVar;

  // pointer for saving per-frame lookups  
  IVariable* m_pYawSpeed;
  IVariable* m_pYawLimits;
  IVariable* m_pPitchSpeed;
  IVariable* m_pPitchLimits;
  IVariable* m_pHelper;
  IVariable* m_pPartClass;
};


/*!
* Class Description of VehiclePart.	
*/
class CVehiclePartClassDesc : public CObjectClassDesc
{
public:
  REFGUID ClassID()
  {
    // {C8ABFCCC-104E-473a-AB0F-E946E2D00F1C}
    static const GUID guid = { 0xc8abfccc, 0x104e, 0x473a, { 0xab, 0xf, 0xe9, 0x46, 0xe2, 0xd0, 0xf, 0x1c } };
    return guid;
  }
  ObjectType GetObjectType() { return OBJTYPE_OTHER; };
  const char* ClassName() { return "VehiclePart"; };
  const char* Category() { return ""; };
  CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVehiclePart); };  
};



#endif // __VehiclePart_h__