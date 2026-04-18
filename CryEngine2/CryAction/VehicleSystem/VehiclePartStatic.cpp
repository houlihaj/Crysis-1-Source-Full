/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a part for vehicles which uses static objects

-------------------------------------------------------------------------
History:
- 23:08:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"

#include "ICryAnimation.h"
#include "IVehicleSystem.h"

#include "CryAction.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartStatic.h"

//------------------------------------------------------------------------
bool CVehiclePartStatic::Init(IVehicle* pVehicle, const SmartScriptTable &table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo)
{
	if (!CVehiclePartBase::Init(pVehicle, table, parent, initInfo))
		return false;

  SmartScriptTable staticTable;
  if (!table->GetValue("Static", staticTable))
    return false;

	I3DEngine* p3DEngine = gEnv->p3DEngine;
	SmartScriptTable stateLevels;

	char* pFilename = NULL;
	char* pFilenameDestroyed = NULL;
	char* pGeometry = NULL;

	if (!staticTable->GetValue("filename", pFilename))
		return false;

	staticTable->GetValue("geometry", pGeometry);
	staticTable->GetValue("filenameDestroyed", pFilenameDestroyed);

	m_filename = pFilename;
	m_filenameDestroyed = pFilenameDestroyed;
	m_geometry = pGeometry;

  InitGeometry();
	InitHelpers(pVehicle, table);

	SmartScriptTable rotationParams;
	if (staticTable->GetValue("Rotation", rotationParams))
		InitRotation(pVehicle, rotationParams);

  m_state = eVGS_Default;

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartStatic::Release()
{
	if (m_slot != -1)
		m_pVehicle->GetEntity()->FreeSlot(m_slot);

	CVehiclePartBase::Release();
}

//------------------------------------------------------------------------
void CVehiclePartStatic::Reset()
{
	CVehiclePartBase::Reset();
}

//------------------------------------------------------------------------
void CVehiclePartStatic::InitGeometry()
{	
	if (m_isPhysicalized && m_slot > -1)
		GetEntity()->UnphysicalizeSlot(m_slot);

	m_slot = GetEntity()->LoadGeometry(m_slot, m_filename, m_geometry);

	if (IStatObj* pStatObj = GetEntity()->GetStatObj(m_slot))
	{
		if (m_hideCount == 0)
			GetEntity()->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot)|ENTITY_SLOT_RENDER);
		else
			GetEntity()->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot)&~(ENTITY_SLOT_RENDER|ENTITY_SLOT_RENDER_NEAREST));
	}

	if (!m_helperPosName.empty())
	{
		if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(m_helperPosName))
		{
			GetEntity()->SetSlotLocalTM(m_slot, pHelper->GetVehicleTM());
		}
	}

	if (m_pParentPart)
	{
		CVehiclePartBase* pAttachToPart = (CVehiclePartBase*) m_pParentPart;

		IEntity* pEntity = pAttachToPart->GetEntity();
		if (pEntity == GetEntity())
		{
			const Matrix34& pParentTM = GetEntity()->GetSlotLocalTM(pAttachToPart->GetSlot(), false);
			const Matrix34& pChildTM = GetEntity()->GetSlotLocalTM(m_slot, false);

			GetEntity()->SetSlotLocalTM(m_slot, pParentTM.GetInverted() * pChildTM);
			GetEntity()->SetParentSlot(pAttachToPart->GetSlot(), m_slot);
		}		
	}
}

//------------------------------------------------------------------------
void CVehiclePartStatic::Physicalize()
{
	if (m_isPhysicalized)
	{
    SEntityPhysicalizeParams params;
    params.mass = m_mass;
    params.density = m_density;
    params.nSlot = m_slot;		
		m_physId = GetEntity()->PhysicalizeSlot(m_slot, params);
    
    CheckColltypeHeavy(m_physId);
	}
  else if (m_slot != -1)
  {
    GetEntity()->UnphysicalizeSlot(m_slot);
  }
  	
	GetEntity()->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot)|ENTITY_SLOT_IGNORE_PHYSICS);
}

//------------------------------------------------------------------------
void CVehiclePartStatic::Update(const float frameTime)
{
	CVehiclePartBase::Update(frameTime);

  if (IsDebugParts())
  {
    //VehicleUtils::DrawTM(GetWorldTM());
  }
}

//------------------------------------------------------------------------
void CVehiclePartStatic::SetLocalTM(const Matrix34& localTM)
{
  CVehiclePartBase::SetLocalTM(localTM);

  // invalidate, entitysystem doesn't immediately update children otherwise
  GetEntity()->InvalidateTM();
}


DEFINE_VEHICLEOBJECT(CVehiclePartStatic);
