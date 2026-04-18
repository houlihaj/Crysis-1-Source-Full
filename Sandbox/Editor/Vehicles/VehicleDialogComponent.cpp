////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   VehicleEditorDialog.cpp
//  Version:     v1.00
//  Created:     28-07-2005 by MichaelR.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "VehicleDialogComponent.h"

#include "Objects/BaseObject.h"
#include "VehicleData.h"
#include "VehiclePrototype.h"
#include "VehiclePart.h"
#include "VehicleHelperObject.h"
#include "VehicleSeat.h"
#include "VehicleWeapon.h"
#include "VehicleComp.h"

IVeedObject* IVeedObject::GetVeedObject(CBaseObject* pObj)
{
  assert(pObj);
  if (!pObj)
    return NULL;

  if (pObj->IsKindOf(RUNTIME_CLASS(CVehiclePart)))
    return (CVehiclePart*)pObj;

  else if (pObj->IsKindOf(RUNTIME_CLASS(CVehicleHelper)))
    return (CVehicleHelper*)pObj;

  else if (pObj->IsKindOf(RUNTIME_CLASS(CVehicleSeat)))
    return (CVehicleSeat*)pObj;

  else if (pObj->IsKindOf(RUNTIME_CLASS(CVehicleWeapon)))
    return (CVehicleWeapon*)pObj;

  else if (pObj->IsKindOf(RUNTIME_CLASS(CVehicleComponent)))
    return (CVehicleComponent*)pObj;

  else if (pObj->IsKindOf(RUNTIME_CLASS(CVehiclePrototype)))
    return (CVehiclePrototype*)pObj;

  return 0;
}

void VeedLog(const char* s,... )
{
  static ICVar* pVar = gEnv->pConsole->GetCVar("v_debugdraw");
  
  if(pVar && pVar->GetIVal() == DEBUGDRAW_VEED)
  {
    char buffer[1024];
    va_list arg;
    va_start(arg, s);   
    vsprintf_s(buffer, s, arg);    
    va_end(arg);    

    Log(buffer);
  }
}