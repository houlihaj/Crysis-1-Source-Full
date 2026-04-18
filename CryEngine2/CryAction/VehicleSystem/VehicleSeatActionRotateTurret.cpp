/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a seat action to rotate a turret

-------------------------------------------------------------------------
History:
- 14:12:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"
#include "GameObjects/GameObject.h"
#include "IGameObject.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehicleSeat.h"
#include "VehiclePartBase.h"
#include "VehicleSeatActionRotateTurret.h"
#include "VehicleSeatActionWeapons.h"

#include "IRenderAuxGeom.h"



//------------------------------------------------------------------------
bool CVehicleSeatActionRotateTurret::Init(IVehicle* pVehicle, TVehicleSeatId seatId, const SmartScriptTable &table)
{
  m_pUserEntity = NULL;
  m_hasReceivedAction = false;
  m_aimGoal.zero();
  m_aimGoalPriority = 0;
  m_actionPitch = 0.f;
  m_actionYaw = 0.f;  
  m_pPitchPart = NULL;
  m_pYawPart = NULL;

  m_pVehicle = pVehicle;
  m_seatId = seatId;

	SmartScriptTable rotateTurretParams;
	if (!table->GetValue("RotateTurret", rotateTurretParams))
		return false;

	char* pPitchPartName = NULL;
	if (rotateTurretParams->GetValue("pitchPart", pPitchPartName))
		m_pPitchPart = static_cast<CVehiclePartBase*>(m_pVehicle->GetPart(pPitchPartName));

	char* pYawPartName = NULL;
	if (rotateTurretParams->GetValue("yawPart", pYawPartName))
		m_pYawPart = static_cast<CVehiclePartBase*>(m_pVehicle->GetPart(pYawPartName));

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::Reset()
{
	m_aimGoal.zero();
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::StartUsing(EntityId passengerId)
{
	m_pUserEntity = gEnv->pEntitySystem->GetEntity(passengerId);
  m_aimGoal.zero();
  
  CVehicleSeat* pSeat = (CVehicleSeat*)m_pVehicle->GetSeatById(m_seatId);

  IVehiclePart::SVehiclePartEvent partEvent;
  partEvent.type = IVehiclePart::eVPE_StartUsing;

	if(pSeat->GetPassengerActor() && pSeat->GetPassengerActor()->IsPlayer())
	{
		// players should update input (this seat action) first, then yaw then pitch.

		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

		if (m_pYawPart)  
		{
			m_pYawPart->OnEvent(partEvent);
			m_pYawPart->SetSeat(pSeat);
		}

		if (m_pPitchPart)  
		{
			m_pPitchPart->OnEvent(partEvent);
			m_pPitchPart->SetSeat(pSeat);
		}
	}
	else
	{
		// leaving this as it used to be for AI
		if (m_pPitchPart)  
		{
			m_pPitchPart->OnEvent(partEvent);
			m_pPitchPart->SetSeat(pSeat);
		}

		if (m_pYawPart)  
		{
			m_pYawPart->OnEvent(partEvent);
			m_pYawPart->SetSeat(pSeat);
		}

		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::StopUsing()
{
	m_pUserEntity = NULL;
	m_aimGoal.zero();

  IVehiclePart::SVehiclePartEvent partEvent;
  partEvent.type = IVehiclePart::eVPE_StopUsing;

  if (m_pPitchPart)  
    m_pPitchPart->OnEvent(partEvent);

  if (m_pYawPart)  
    m_pYawPart->OnEvent(partEvent);     

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
  if (!m_aimGoal.IsZero())
  {
    // if seat action is used remotely, rotation is set through aim goal, thus return here
    CVehicleSeat* pSeat = (CVehicleSeat*)m_pVehicle->GetSeatById(m_seatId);
    if (pSeat && pSeat->GetCurrentTransition() == CVehicleSeat::eVT_RemoteUsage)
      return;
  }
  
	if (actionId == eVAI_RotatePitch)
	{
		m_actionPitch = value * -1.0f;
		m_hasReceivedAction = true;
	}
	else if (actionId == eVAI_RotateYaw)
	{
		m_actionYaw = value * -1.0f;
		m_hasReceivedAction = true;
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::Serialize(TSerialize ser, unsigned aspects)
{
	CVehicleSeat *pSeat = (CVehicleSeat*)m_pVehicle->GetSeatById(m_seatId);
  
  // MR: for network, only turret parts are serialized
  // for savegame, all parts are serialized (by CVehicle)
  if (ser.GetSerializationTarget() == eST_Network)
  {
    if (m_pYawPart)
    {
      m_pYawPart->SetSeat(pSeat);		
      m_pYawPart->Serialize(ser, aspects);
    }
    if (m_pPitchPart)
    {
      m_pPitchPart->SetSeat(pSeat);
      m_pPitchPart->Serialize(ser, aspects);
    }
  }

	if(ser.IsReading())
	{
		// force update
		m_hasReceivedAction = true;
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::Update(float frameTime)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

  if (gEnv->bClient && m_pVehicle->GetGameObject()->IsProbablyDistant() && !m_pVehicle->GetGameObject()->IsProbablyVisible())
    return;

	CVehicleSeat *pSeat=static_cast<CVehicleSeat *>(m_pVehicle->GetSeatById(m_seatId));

	if (m_hasReceivedAction)
	{
		if (m_pPitchPart)
			m_pPitchPart->m_actionPitch = m_actionPitch;

		if (m_pYawPart)
			m_pYawPart->m_actionYaw = m_actionYaw;
		
		m_hasReceivedAction = false;
		
		if (pSeat)
			pSeat->ChangedNetworkState(CVehicle::ASPECT_PART_MATRIX);
	}
	else if (!m_aimGoal.IsZero())
	{
		if (m_pPitchPart)
		{
      const Matrix34& vehicleWorldTM = m_pVehicle->GetEntity()->GetWorldTM();
      Matrix34 pitchPartVehicleTM = m_pPitchPart->GetLocalTM(false);
      const Matrix34& yawPartVehicleTM = m_pYawPart->GetLocalTM(false);
      
      Vec3 pitchPartWorldPos = vehicleWorldTM * pitchPartVehicleTM.GetTranslation();
      Vec3 firingPos = pitchPartWorldPos;
      Vec3 vFiringDir = m_aimGoal - firingPos;
			
      if (CVehicleSeatActionWeapons* pWeaponAction = pSeat->GetSeatActionWeapons())
			{
				EntityId weaponId = pWeaponAction->GetWeaponEntityId(pWeaponAction->GetCurrentWeapon());
				pWeaponAction->GetFiringPos(weaponId, 0, firingPos);
				pWeaponAction->GetActualWeaponDir(weaponId, 0, vFiringDir, Vec3(ZERO), Vec3(ZERO) );
			}

			// Just in case, fix the angle error between parts angle and weapon angle.

			Vec3 vDir = m_aimGoal - firingPos;//m_pPitchPart->GetWorldTM().GetTranslation();

			Vec3 vDirNormal	= (m_aimGoal - firingPos).GetNormalizedSafe();
			Vec3 vDirNear	= (m_aimGoal - pitchPartWorldPos).GetNormalizedSafe();

			if ( vDirNormal.Dot(vDirNear) < cosf( DEG2RAD(30.0f) ) )
				vDir = m_aimGoal - pitchPartWorldPos;// this is more stable, but not precise.

			vDir.NormalizeSafe();

			Vec3 v1 = pitchPartWorldPos;      
			Vec3 v2 = vehicleWorldTM * yawPartVehicleTM.GetTranslation(); 
/*
			ColorB white(255, 255, 255);
			IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();
			pRenderAux->DrawLine(v1, white, m_aimGoal, white);
			pRenderAux->DrawLine(v2, white, m_aimGoal, white);
			pRenderAux->DrawLine(firingPos, white, m_aimGoal, white);
*/
			//setups of matrixes - globals

			Matrix33 rotWorld(Matrix33(m_pPitchPart->GetEntity()->GetWorldTM()));
			Matrix33 rotWorldInvert(rotWorld);
			rotWorldInvert.Invert();

			//setups of matrixes - locals

			Matrix33 rotMatX,rotMatZ;

			IVehiclePart* pParent = m_pPitchPart->GetParent();
			Matrix33 initTM = Matrix33(m_pPitchPart->GetLocalInitialTM());
			Matrix33 initialLocalLocalMat( initTM );			// initial local
			Matrix33 initialLocalLocalMatInvert( initTM );		// initial local
			initialLocalLocalMatInvert.Invert();
			while (pParent)
			{
				initTM = Matrix33(pParent->GetLocalInitialTM()) * initTM;
				pParent = pParent->GetParent();
			}

			Matrix33 initialLocalMat( initTM );					// initial local->vehicle
			Matrix33 initialLocalMatInvert( initialLocalMat );
			initialLocalMatInvert.Invert();

			Matrix33 localMat(pitchPartVehicleTM); // local->vehicle
			Matrix33 localMatYaw(m_pYawPart->GetLocalInitialTM());

			// get a default direction in vehicle space
			Vec3 defaultFireDir(0.0f,1.0f,0.0f);
			Vec3 defaultFireDirRot;
			Vec3 defaultFireDirRot2;

			// CHECK!!!: defaultFireDir should be always (0.0f,1.0f,0.0f);
			// but it seems to need painful assumption that if default yaw is -180, it means a model has (0,-1,0);
			defaultFireDir		= localMatYaw * defaultFireDir;

			defaultFireDir		= initialLocalMat * defaultFireDir;			// default turret direction in vehicle
			defaultFireDirRot	= initialLocalMatInvert * defaultFireDir;
			defaultFireDirRot	= localMat * defaultFireDirRot;

			float yaw = cry_atan2f(defaultFireDirRot.y,defaultFireDirRot.x);
			rotMatZ.SetRotationZ(-yaw);		
			defaultFireDirRot = rotMatZ * defaultFireDirRot;
			float pitch2 =cry_atan2f(defaultFireDirRot.z,defaultFireDirRot.x);

			// ideal direction in vehicle space
			Vec3 vecDir = rotWorldInvert * vDir;
			vecDir.NormalizeSafe();

			//Convert the direction to angles
			yaw = cry_atan2f(vecDir.y,vecDir.x);
			rotMatZ.SetRotationZ(-yaw);		
			vecDir = rotMatZ * vecDir;
			float pitch	=cry_atan2f(vecDir.z,vecDir.x);

			//get a difference between current angle and idal angle.
			pitch -= pitch2;

			// this pitch should be converted in the parts space;
			rotMatX.SetRotationX(pitch);	
			Vec3 defaultFireDir2(0.0f,1.0f,0.0f);

			defaultFireDir2 = rotMatX *initialLocalMat * initialLocalLocalMatInvert * defaultFireDir2;
			yaw = cry_atan2f(defaultFireDir2.y,defaultFireDir2.x);
			rotMatZ.SetRotationZ(-yaw);		
			defaultFireDir2 = rotMatZ * defaultFireDir2;
			pitch	=cry_atan2f(defaultFireDir2.z,defaultFireDir2.x);

			// clamp and finalize

			pitch = fmod(pitch,DEG2RAD(360.0f));
			if (pitch > DEG2RAD(180.0f))  pitch-=DEG2RAD(360.0f);
			if (pitch < DEG2RAD(-180.0f)) pitch+=DEG2RAD(360.0f);
			pitch *= -1.0f;

			Limit( pitch, DEG2RAD(-15.0f), DEG2RAD(15.0f) );
			m_pPitchPart->m_actionPitch = pitch * 100.0f * -1.0f;
			
			if (m_pYawPart)
			{
				rotWorld = Matrix33(m_pYawPart->GetEntity()->GetWorldTM());
				rotWorldInvert = rotWorld;
				rotWorldInvert.Invert();

				//setups of matrixes - locals

				pParent = m_pYawPart->GetParent();
				initTM = Matrix33(m_pYawPart->GetLocalInitialTM());
				initialLocalLocalMat = initTM;			// initial local
				initialLocalLocalMatInvert = initTM;		// initial local
				initialLocalLocalMatInvert.Invert();
				while (pParent)
				{
					initTM = Matrix33(pParent->GetLocalInitialTM()) * initTM;
					pParent = pParent->GetParent();
				}

				Matrix33 initialLocalMat( initTM );					// initial local->vehicle
				Matrix33 initialLocalMatInvert( initialLocalMat );
				initialLocalMatInvert.Invert();

				localMat = Matrix33(yawPartVehicleTM);	// local->vehicle
				localMatYaw = Matrix33(m_pYawPart->GetLocalInitialTM());

				// get a default direction in vehicle space
				defaultFireDir.Set(0.0f,1.0f,0.0f);
				
				// CHECK!!!: defaultFireDir should be always (0.0f,1.0f,0.0f);
				// but it seems to need painful assumption that if default yaw is -180, it means a model has (0,-1,0);
				defaultFireDir		= localMatYaw * defaultFireDir;

				defaultFireDir		= initialLocalMat * defaultFireDir;			// default turret direction in vehicle
				defaultFireDirRot	= initialLocalMatInvert * defaultFireDir;
				defaultFireDirRot	= localMat * defaultFireDirRot;

				float yaw2 = cry_atan2f(defaultFireDirRot.y,defaultFireDirRot.x);

				// ideal direction in vehicle space
				vecDir = rotWorldInvert * vDir;
				vecDir.NormalizeSafe();

				//Convert the direction to angles
				yaw = cry_atan2f(vecDir.y,vecDir.x);

				//if there is yawlimit
				if ( m_pYawPart->m_yawMin != 0.f && m_pYawPart->m_yawMax != 0.f)
				{
					if ( ( yaw >DEG2RAD(90.0f) || yaw <DEG2RAD(-90.0f) ) && ( yaw2 <DEG2RAD(90.0f) && yaw2 >DEG2RAD(-90.0f) ) )
						yaw =DEG2RAD(15.0f);
					else if ( ( yaw2 >DEG2RAD(90.0f) || yaw2 <DEG2RAD(-90.0f) ) && ( yaw <DEG2RAD(90.0f) && yaw >DEG2RAD(-90.0f) ) )
						yaw =DEG2RAD(-15.0f);
					else
						yaw -=yaw2;
				}
				else
				{
					yaw -=yaw2;
				}

				// this yaw should be converted in the parts space;
				rotMatZ.SetRotationZ(yaw);	
				Vec3 defaultFireDir2(1.0f,0.0f,0.0f);

				defaultFireDir2 = rotMatZ *initialLocalMat * initialLocalLocalMatInvert * defaultFireDir2;
				yaw = cry_atan2f(defaultFireDir2.y,defaultFireDir2.x);

				yaw =fmod(yaw,DEG2RAD(360.0f));
				if (yaw>DEG2RAD(180.0f))  yaw-=DEG2RAD(360.0f);
				if (yaw<DEG2RAD(-180.0f)) yaw+=DEG2RAD(360.0f);
				yaw *= -1.0f;

				Limit( yaw, DEG2RAD(-15.0f), DEG2RAD(15.0f) );
				m_pYawPart->m_actionYaw = yaw * 100.0f * -1.0f;
			}
		}
	}

	m_aimGoalPriority = 0;
}

//------------------------------------------------------------------------
void CVehicleSeatActionRotateTurret::SetAimGoal(Vec3 aimPos, int priority)
{
	if (m_aimGoalPriority <= priority)
	{
		m_aimGoal = aimPos;
		m_aimGoalPriority = priority;    
	}
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionRotateTurret);
