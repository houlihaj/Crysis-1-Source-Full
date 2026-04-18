////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   VehicleHelper.h
//  Version:     v1.00
//  Created:     21/06/2005 by MichaelR
//  Compilers:   Visual C++ 6.0
//  Description: Vehicle Helper Object
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __VehicleHelper_h__
#define __VehicleHelper_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include "Objects/BaseObject.h"
#include "VehicleDialogComponent.h"

class CVehiclePart;
class CVehiclePrototype;

/*!
 *	CVehicleHelper is a simple helper object for specifying a position and orientation
 *  in a vehicle part coordinate frame
 *
 */
class CVehicleHelper : public CBaseObject, public CVeedObject
{
public:
	DECLARE_DYNCREATE(CVehicleHelper)
  ~CVehicleHelper();
	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init( IEditor *ie,CBaseObject *prev,const CString &file );
	void Done();

	void Display( DisplayContext &dc );
	void BeginEditParams( IEditor *ie,int flags );
	void EndEditParams( IEditor *ie );

	//! Called when object is being created.
	int MouseCreateCallback( CViewport *view,EMouseEvent event,CPoint &point,int flags );

	void GetBoundSphere( Vec3 &pos,float &radius );
  void GetBoundBox( BBox &box );
  void GetLocalBounds( BBox &box );
	bool HitTest( HitContext &hc );

  void Serialize( CObjectArchive &ar ){}
	//////////////////////////////////////////////////////////////////////////

  // Ovverides from IVeedObject.
  //////////////////////////////////////////////////////////////////////////
  void UpdateVarFromObject();
  void UpdateObjectFromVar();

  const char* GetElementName(){ return "Helper"; }
  virtual int GetIconIndex(){ return VEED_HELPER_ICON; }
  
  IVariable* GetVariable(){ return m_pVar; }
  void SetVariable(IVariable* pVar){ m_pVar = pVar; }  
  void UpdateScale(float scale);
  //////////////////////////////////////////////////////////////////////////
  
  void IsFromGeometry(bool b){ m_fromGeometry = b; }
  bool IsFromGeometry(){ return m_fromGeometry; }

  CVehiclePart* GetPart(){ return m_pPart; }
  void SetPart(CVehiclePart* pPart){ m_pPart = pPart; }

  void SetVehicle(CVehiclePrototype* pProt){ m_pVehicle = pProt;}

protected:
	//! Dtor must be protected.
	CVehicleHelper();
	void DeleteThis() { delete this; };
  void PostClone( CBaseObject *pFromObject,CObjectCloneContext &ctx );

	float m_innerRadius;
	float m_outerRadius;

  IVariable* m_pVar;

	IEditor *m_ie;	

  bool m_fromGeometry;

  CVehiclePart* m_pPart;
  CVehiclePrototype* m_pVehicle;
};

/*!
 * Class Description of VehicleHelper.	
 */
class CVehicleHelperClassDesc : public CObjectClassDesc
{
public:
	REFGUID ClassID()
	{
    // {13A9F32C-57B9-49f8-A877-AFDB4A3271A9}
    static const GUID guid = { 0x13a9f32c, 0x57b9, 0x49f8, { 0xa8, 0x77, 0xaf, 0xdb, 0x4a, 0x32, 0x71, 0xa9 } };
		return guid;
	}
	ObjectType GetObjectType() { return OBJTYPE_OTHER; };
	const char* ClassName() { return "VehicleHelper"; };
	const char* Category() { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CVehicleHelper); };
};

#endif // __VehicleHelper_h__