/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2006.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a seat action to head/spot lights

-------------------------------------------------------------------------
History:
- 01:03:2006: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehicleSeat.h"
#include "VehiclePartLight.h"
#include "VehicleSeatActionLights.h"


static const char* activationSounds[] = 
{
  "sounds/vehicles:vehicle_accessories:light",
  "sounds/vehicles:vehicle_accessories:flashlight",
  "sounds/vehicles:vehicle_accessories:spotlight",
};

//------------------------------------------------------------------------
bool CVehicleSeatActionLights::Init(IVehicle* pVehicle, TVehicleSeatId seatId, const SmartScriptTable &table)
{
  m_pVehicle = pVehicle;
	m_seatId = seatId;
  m_pSound = 0;
	m_stopUseTime = -1.0f;

	SmartScriptTable lightsTable;
	if (table->GetValue("Lights", lightsTable))
	{
    char* activation = "";
    lightsTable->GetValue("activation", activation);

    if (!strcmp(activation, "brake"))
      m_activation = eLA_Brake;
    else 
      m_activation = eLA_Toggle;

    if (eLA_Toggle == m_activation && gEnv->pSoundSystem)
    { 
      int sound = 1;
      lightsTable->GetValue("sound", sound);
      
      if (sound > 0 && sound <= sizeof(activationSounds)/sizeof(activationSounds[0]))
      {
        m_pSound = gEnv->pSoundSystem->CreateSound(activationSounds[sound-1], FLAG_SOUND_DEFAULT_3D);
      }      
    }

		SmartScriptTable lightsPartsArray;
		if (lightsTable->GetValue("LightParts", lightsPartsArray))
		{
			m_lightParts.reserve(lightsPartsArray->Count());

			IScriptTable::Iterator partsIte = lightsPartsArray->BeginIteration();

			while (lightsPartsArray->MoveNext(partsIte))
			{
        char* pPartName = 0;
				if (partsIte.value.CopyTo(pPartName))
				{	  
				  if (IVehiclePart* pPart = pVehicle->GetPart(pPartName))
				  {
					  if (CVehiclePartLight* pPartLight = CAST_VEHICLEOBJECT(CVehiclePartLight, pPart))
					  { 
              SLightPart light;
              light.pPart = pPartLight;
              
						  m_lightParts.push_back(light);
					  }
				  }				
          
				}
			}

			lightsPartsArray->EndIteration(partsIte);
		}
	}

	m_enabled = false;

	return !m_lightParts.empty();
}

//------------------------------------------------------------------------
void CVehicleSeatActionLights::Reset()
{
  m_enabled = false;
}


//------------------------------------------------------------------------
void CVehicleSeatActionLights::Serialize(TSerialize ser, unsigned aspects)
{
  if (ser.GetSerializationTarget() != eST_Network)
  {
		// if waiting for lights to go off, save them in the off state
		if(ser.IsWriting() && m_enabled && m_stopUseTime > 0.0f)
		{
			ser.Value("enabled", false);
		}
		else
		{
			bool enabled = m_enabled;
			ser.Value("enabled", enabled);
			m_stopUseTime = -1.0f;

			if (ser.IsReading() && enabled!=m_enabled)
			{
				ToggleLights(enabled);  
			}
		}
  }
	else
  {
    if (aspects&CVehicle::ASPECT_SEAT_ACTION)
	  {
		  bool enabled=m_enabled;
		  ser.Value("enabled", enabled, 'bool');

      if (ser.IsReading() && enabled!=m_enabled)
      {
        ToggleLights(enabled);  
      }
	  }
  }
}

//------------------------------------------------------------------------
void CVehicleSeatActionLights::PostSerialize()
{
	if (m_pVehicle->GetEntity()->IsHidden())
		ToggleLights(false);
}

//------------------------------------------------------------------------
void CVehicleSeatActionLights::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{ 
  if (actionId == eVAI_ToggleLights && m_activation == eLA_Toggle)
  {
    if (eAAM_OnPress == activationMode)
    { 
      ToggleLights(value==0.f ? false : !m_enabled);

      CVehicleSeat *pSeat=static_cast<CVehicleSeat *>(m_pVehicle->GetSeatById(m_seatId));
      if (pSeat)
        pSeat->ChangedNetworkState(CVehicle::ASPECT_SEAT_ACTION);	
    }
  }
}

//---------------------------------------------------------------------------
void CVehicleSeatActionLights::ToggleLights(bool enable)
{
  for (TVehiclePartLightVector::iterator ite=m_lightParts.begin(), end=m_lightParts.end(); ite!=end; ++ite)	  
    ite->pPart->ToggleLight(enable);    	
  
  if (m_pSound)
  {
    IEntitySoundProxy* pSoundProxy = (IEntitySoundProxy*)m_pVehicle->GetEntity()->CreateProxy(ENTITY_PROXY_SOUND);
    if (pSoundProxy)
    {
      pSoundProxy->PlaySound(m_pSound);
    }
  }

  m_enabled = enable;
}

//---------------------------------------------------------------------------
void CVehicleSeatActionLights::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
  switch (event)
  {
  case eVE_Brake:
    {
      if (eLA_Brake == m_activation)
      {
        bool toggle = true;

        if (params.bParam)
        {
          // only enable brake lights if engine is powered, and driver is still inside
          if (!m_pVehicle->GetMovement()->IsPowered())
            toggle = false;
          else
          {
            IVehicleSeat* pSeat = m_pVehicle->GetSeatById(1);
            if (pSeat && (!pSeat->GetPassenger() || pSeat->GetCurrentTransition()==IVehicleSeat::eVT_Exiting))
              toggle = false;
          }   
        }
        
        if (toggle)
          ToggleLights(params.bParam);        
      }
    }
    break;
  case eVE_EngineStopped:
    {
      if (eLA_Brake == m_activation)
        ToggleLights(false);
    }
    break;
	}  
}


//---------------------------------------------------------------------------
void CVehicleSeatActionLights::StopUsing()
{ 
	if(m_enabled && !VehicleCVars().v_lights_enable_always && VehicleCVars().v_lights_disable_time >= 0.0f)
	{
		m_stopUseTime = gEnv->pTimer->GetCurrTime();
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_Visible);
	}
}

void CVehicleSeatActionLights::StartUsing(EntityId passengerId)
{
	m_stopUseTime = -1.0f;
}

void CVehicleSeatActionLights::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->AddContainer(m_lightParts);
}

void CVehicleSeatActionLights::Update(const float deltaTime)
{
	if(m_enabled && m_stopUseTime > 0.0f && gEnv->pTimer->GetCurrTime() - m_stopUseTime > VehicleCVars().v_lights_disable_time)
	{
		ToggleLights(false);
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
		m_stopUseTime = -1.0f;

		CVehicleSeat *pSeat=static_cast<CVehicleSeat *>(m_pVehicle->GetSeatById(m_seatId));
		if (pSeat)
			pSeat->ChangedNetworkState(CVehicle::ASPECT_SEAT_ACTION);	
	}
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionLights);
