/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a base class for vehicle parts

-------------------------------------------------------------------------
History:
- 23:08:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"

#include "ICryAnimation.h"
#include "CryAction.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehicleComponent.h"
#include "VehicleSeat.h"
#include "VehicleSeatSerializer.h"

#include "PersistantDebug.h"


#define TURNSPEED_COEFF 0.002f

const char * CVehiclePartBase::m_geometryDestroyedSuffix = "_damaged";


CVehiclePartBase::CVehiclePartBase()
: m_orientation(Quat::CreateIdentity(), 0.75f, 3.0f)
, m_slot( -1 )
, m_typeId( eVPT_Base )
, m_physId( -1 )
, m_pSeat( 0 )
, m_state( eVGS_Last )
, m_isRotationBlocked(false)
, m_users(0)
, m_pParentPart(NULL)
, m_pVehicle(NULL)
, m_prevWorldQuat(IDENTITY)
{
  m_actionYaw = 0.0f;
  m_actionPitch = 0.0f;
  m_currentYaw = m_lastYaw = 0.f;
  m_currentPitch = m_lastPitch = 0.f;
  m_yawAccel = 0.f;
  m_pitchAccel = 0.f;
  m_rotationStopped = 0.f;
  m_relSpeed = 0.f;
  m_turnSoundId = InvalidSoundEventId;
  m_damageSoundId = InvalidSoundEventId;
  m_position.zero();  
  m_pitchSpeed = 0.0f;
  m_pitchMax = 0.0f;
  m_pitchMin = 0.0f;
  m_yawSpeed = 0.0f;
  m_yawMax = 0.0f;
  m_yawMin = 0.0f;
  m_hideCount = 0;
  m_useOption = -1;
  m_mass = 0.0f;
  m_density = -1.0f;
  m_isPhysicalized = true;
  m_disableCollision = false;  
  m_rotationChanged = false;  
  m_damageRatio = 0.0f;  
  m_wasMaterialCloned = false;
  m_hideMode = eVPH_NoFade;  
  m_detachBaseForce.zero();
  m_detachProbability = 0.f;
  m_hideTimeCount = 0.f;
  m_hideTimeMax = 0.f;  
  m_pComponentName = 0;
  m_rotWorldSpace = 0.f;  
	m_rotTestHelpers[0] = NULL;
	m_rotTestHelpers[1] = NULL;
	m_rotTestHelperNames[0] = "";
	m_rotTestHelperNames[1] = "";
	m_rotTestRadius = 0.0f;
	m_prevBaseTM.SetIdentity();
}

//------------------------------------------------------------------------
CVehiclePartBase::~CVehiclePartBase()
{   
}

//------------------------------------------------------------------------
bool CVehiclePartBase::Init(IVehicle* pVehicle, const SmartScriptTable &table, IVehiclePart* pParent, CVehicle::SPartInitInfo& initInfo)
{	
	m_pVehicle = (CVehicle*) pVehicle;
	
	char* name;
	if (table->GetValue("name", name))
	{
		m_name = name;
	}

	char* helperName;
	if (table->GetValue("helper", helperName))
	{
		m_helperPosName = helperName;
	}
  
  table->GetValue("position", m_position);
	bool hidden = false;
	table->GetValue("isHidden", hidden);  
	m_hideCount = (hidden == true) ? 1 : 0;
  table->GetValue("useOption", m_useOption);
	table->GetValue("mass", m_mass);
  table->GetValue("density", m_density);

	reinterpret_cast<CVehicle*>(pVehicle)->m_mass += m_mass;

	m_pParentPart = pParent;

  if (pParent)
    pParent->AddChildPart(this);

	if (table->GetValue("disablePhysics", m_isPhysicalized))
		m_isPhysicalized = !m_isPhysicalized;

	table->GetValue("disableCollision", m_disableCollision);
	
  char* component = "";
  if (table->GetValue("component", component) && component[0])
    initInfo.partComponentMap.push_back(CVehicle::SPartComponentPair(this, component));

  // read optional multiple components (part->component relationship is now 1:n)
  SmartScriptTable components;
  if (table->GetValue("Components", components))
  {
    IScriptTable::Iterator it = components->BeginIteration();
    while (components->MoveNext(it) && it.value.type == ANY_TSTRING)
    { 
      initInfo.partComponentMap.push_back(CVehicle::SPartComponentPair(this, it.value.str));
    }
    components->EndIteration(it);
  }
	
  char* filename = 0;
  if (table->GetValue("filename", filename))
  { 
    if (strstr(filename, ".cgf"))
      m_slot = GetEntity()->LoadGeometry(m_slot, filename);
    else
      m_slot = GetEntity()->LoadCharacter(m_slot, filename);
  }

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartBase::PostInit()
{
	if(!m_rotTestHelperNames[0].empty())
	{
		m_rotTestHelpers[0] = m_pVehicle->GetHelper(m_rotTestHelperNames[0]);
	}
	if(!m_rotTestHelperNames[1].empty())
	{
		m_rotTestHelpers[1] = m_pVehicle->GetHelper(m_rotTestHelperNames[1]);
	}
}

//------------------------------------------------------------------------
void CVehiclePartBase::InitComponent()
{
  if (m_pComponentName)
  {
    CVehicleComponent* pVehicleComponent = static_cast<CVehicleComponent*>(m_pVehicle->GetComponent(m_pComponentName));
    
    if (pVehicleComponent)
      pVehicleComponent->AddPart(this);

    m_pComponentName = 0;
  }
}

//------------------------------------------------------------------------
void CVehiclePartBase::InitHelpers(IVehicle* pVehicle, const SmartScriptTable &table)
{
	SmartScriptTable helpersTable;
	if (table->GetValue("Helpers", helpersTable))
	{
		IScriptTable::Iterator ite = helpersTable.GetPtr()->BeginIteration();
		while (helpersTable.GetPtr()->MoveNext(ite))
		{
			if (ite.value.type == ANY_TTABLE)
			{
				SmartScriptTable helperTable;
				ite.value.CopyTo(helperTable);

				Vec3 position;
				Vec3 direction;
				char* pName = NULL;

				if (helperTable->GetValue("name", pName))
				{
					if (!helperTable->GetValue("position", position))
						position.zero();

					if (!helperTable->GetValue("direction", direction))
						direction.zero();

					m_pVehicle->AddHelper(pName, position, direction, this);
				}
			}
		}

		helpersTable.GetPtr()->EndIteration(ite);
	}
}

//------------------------------------------------------------------------
bool CVehiclePartBase::InitRotation(IVehicle* pVehicle, const SmartScriptTable &rotationParams)
{
	if (rotationParams->GetValue("pitchSpeed", m_pitchSpeed))
	{	
    rotationParams->GetValue("pitchAccel", m_pitchAccel);

		SmartScriptTable pitchLimitsTable;
		if (rotationParams->GetValue("pitchLimits", pitchLimitsTable))
		{
			pitchLimitsTable->GetAt(1, m_pitchMin);
			pitchLimitsTable->GetAt(2, m_pitchMax);
			m_pitchMin = DEG2RAD(m_pitchMin);
			m_pitchMax = DEG2RAD(m_pitchMax);
		}
	}

	if (rotationParams->GetValue("yawSpeed", m_yawSpeed))
	{	
    rotationParams->GetValue("yawAccel", m_yawAccel);

		SmartScriptTable yawLimitsTable;
		if (rotationParams->GetValue("yawLimits", yawLimitsTable))
		{
			yawLimitsTable->GetAt(1, m_yawMin);
			yawLimitsTable->GetAt(2, m_yawMax);
			m_yawMin = DEG2RAD(m_yawMin);
			m_yawMax = DEG2RAD(m_yawMax);
		}
	}

  rotationParams->GetValue("worldSpace", m_rotWorldSpace);
	m_isRotationBlocked = false;

	SmartScriptTable rotationTestTable;
	if(rotationParams->GetValue("RotationTest", rotationTestTable))
	{
		char* helperName = NULL;
		if(rotationTestTable->GetValue("helper1", helperName))
		{
			if(IVehicleHelper* pHelper = m_pVehicle->GetHelper(helperName))
				m_rotTestHelpers[0] = pHelper;
			m_rotTestHelperNames[0] = helperName;
		}
		if(rotationTestTable->GetValue("helper2", helperName))
		{
			if(IVehicleHelper* pHelper = m_pVehicle->GetHelper(helperName))
				m_rotTestHelpers[1] = pHelper;
			m_rotTestHelperNames[1] = helperName;
		}

		rotationTestTable->GetValue("radius", m_rotTestRadius);
	}  
	return true;
}

//------------------------------------------------------------------------
bool CVehiclePartBase::InitRotationSounds(const SmartScriptTable &rotationParams)
{
  SmartScriptTable sound;
  if (rotationParams->GetValue("Sound", sound))
  {
    const char* eventName = 0;
    sound->GetValue("event", eventName);

    if (eventName && eventName[0])
    {
      char* pHelperName = 0;
      if (sound->GetValue("helper", pHelperName))
      {
        if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(pHelperName))
        {
          SVehicleSoundInfo info;
          info.name = eventName;
          info.pHelper = pHelper;
          m_turnSoundId = m_pVehicle->AddSoundEvent(info);  
          
          if (sound->GetValue("eventDamage", eventName) && eventName[0])
          {
            SVehicleSoundInfo dmgInfo;
            info.name = eventName;
            info.pHelper = pHelper;
            m_damageSoundId = m_pVehicle->AddSoundEvent(info);
          }
                    
          return true;
        }
      }
      return false;
    }           
  }

  return true;
}

//------------------------------------------------------------------------
EntityId CVehiclePartBase::SpawnEntity()
{
	char pPartName[128];
	_snprintf(pPartName, 128, "%s_part_%s", m_pVehicle->GetEntity()->GetName(), m_name.c_str());
  pPartName[sizeof(pPartName)-1] = '\0';

	SEntitySpawnParams params;
	params.sName = pPartName;
	params.nFlags = ENTITY_FLAG_CLIENT_ONLY;
	params.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("VehiclePart");

	IEntity* pPartEntity = gEnv->pEntitySystem->SpawnEntity(params, true);
	if (pPartEntity)
	{
		if (m_pParentPart)
			m_pParentPart->GetEntity()->AttachChild(pPartEntity);
		else
			m_pVehicle->GetEntity()->AttachChild(pPartEntity);

		pPartEntity->SetFlags(pPartEntity->GetFlags() | ENTITY_FLAG_CASTSHADOW );

		return pPartEntity->GetId();
	}
	
	assert(0);
	return 0;
}

//------------------------------------------------------------------------
IEntity* CVehiclePartBase::GetEntity()
{
	if (m_pParentPart)
		return m_pParentPart->GetEntity();
	else
		return m_pVehicle->GetEntity();
}

//------------------------------------------------------------------------
void CVehiclePartBase::Release()
{
  delete this;
}

//------------------------------------------------------------------------
void CVehiclePartBase::Reset()
{ 
	m_actionYaw = m_currentYaw = m_lastYaw = 0.0f;
	m_actionPitch = m_currentPitch = m_lastPitch = 0.0f;
  m_rotationStopped = 0.f;

	m_damageRatio = 0.0f;	
  m_relSpeed = 0.f;

  ResetLocalTM(true);
	m_prevBaseTM = GetLocalBaseTM();

  ChangeState(eVGS_Default);   
}

//------------------------------------------------------------------------
void CVehiclePartBase::OnEvent(const SVehiclePartEvent& event)
{
  switch (event.type)
  {  
  case eVPE_Damaged:
	  { 
      float old = m_damageRatio;
		  m_damageRatio = event.fparam;
  		
      if (m_damageRatio >= 1.0f && old < 1.f)
      {
        EVehiclePartState state = GetStateForDamageRatio(m_damageRatio);
        ChangeState(state, eVPSF_Physicalize);
      }
      else if (m_damageRatio == 0.f)
      {
        ChangeState(eVGS_Default);
      }      
	  }  
    break;  
  case eVPE_Repair:
    {
      float old = m_damageRatio;
      m_damageRatio = max(0.f, event.fparam);
      
      if (eVPT_Base == m_typeId && m_damageRatio <= COMPONENT_DAMAGE_LEVEL_RATIO && m_hideCount==0)
        Hide(false);    

      if (m_damageRatio <= COMPONENT_DAMAGE_LEVEL_RATIO && old > COMPONENT_DAMAGE_LEVEL_RATIO)
        ChangeState(eVGS_Default, eVPSF_Physicalize);      
    }
    break;
  case eVPE_StartUsing:
    {
		  m_users++;

		  if (m_users == 1)
		  {
			  m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
			  m_relSpeed = 0.f;
        m_prevWorldQuat = Quat(GetWorldTM());
				m_prevBaseTM = GetLocalBaseTM();
		  }
    }  
    break;  
  case eVPE_StopUsing:
    {
		  m_users = max(--m_users, 0);

		  if (m_users <= 0)
		  {
			  m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
			  m_pVehicle->StopSound(m_turnSoundId);    
			  m_relSpeed = 0.f;
		  }
    }
    break;
  case eVPE_GotDirty:
	  break;
  case eVPE_Hide:
	  {
		  if (event.bparam)
			  m_hideMode = eVPH_FadeOut;
		  else
			  m_hideMode = eVPH_FadeIn;

		  m_hideTimeCount = event.fparam;
		  m_hideTimeMax = event.fparam;

		  m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
	  }
    break;
  case eVPE_BlockRotation:
		m_isRotationBlocked = event.bparam;      	  
    break;
  }
}

//------------------------------------------------------------------------
void CVehiclePartBase::Hide(bool hide)
{
  if (m_slot == -1)
    return;

  IEntity* pEntity = GetEntity();
  if (!pEntity)
    return;

  if (hide)
	{
		++m_hideCount;

		if(m_hideCount > 0)
			pEntity->SetSlotFlags(m_slot, pEntity->GetSlotFlags(m_slot)&~ENTITY_SLOT_RENDER);
	}
  else
	{
		--m_hideCount;

		if(m_hideCount <= 0)
		{
			m_hideCount = 0;
			pEntity->SetSlotFlags(m_slot, pEntity->GetSlotFlags(m_slot)|ENTITY_SLOT_RENDER);
		}
	}
}

//------------------------------------------------------------------------
IVehiclePart::EVehiclePartState CVehiclePartBase::GetMaxState()
{
  EVehiclePartState state = m_state;
  
  for (TVehicleChildParts::iterator it=m_children.begin(), end=m_children.end(); it!=end; ++it)
  {
    EVehiclePartState maxState = (*it)->GetMaxState();
    
    if (maxState > state && maxState != eVGS_Last)
      state = maxState;    
  }

  return state;
}

//------------------------------------------------------------------------
IVehiclePart::EVehiclePartState CVehiclePartBase::GetStateForDamageRatio(float ratio)
{
  if (ratio >= 1.f)
    return m_pVehicle->IsDestroyed() ? eVGS_Destroyed : eVGS_Damaged1;

  return eVGS_Default;
}


//------------------------------------------------------------------------
bool CVehiclePartBase::ChangeState(EVehiclePartState state, int flags)
{ 
	if (state == m_state && !(flags&eVPSF_Force))
		return false;

  // once destroyed, we only allow to return to default state
  if (m_state == eVGS_Destroyed && state != eVGS_Default && !(flags&eVPSF_Force))
    return false;

  if (eVPT_Base == m_typeId && m_hideCount==0)
  {
    if (state == eVGS_Destroyed)
      Hide(true);
    else if (state == eVGS_Default)
      Hide(false);
  }

	return true;
}

//------------------------------------------------------------------------
IStatObj* CVehiclePartBase::GetSubGeometry(CVehiclePartBase* pPart, EVehiclePartState state, Matrix34& position, bool removeFromParent)
{
	if (CVehiclePartBase* pParentPartBase = (CVehiclePartBase*) m_pParentPart)
		return pParentPartBase->GetSubGeometry(pPart, state, position, removeFromParent);
	else
		return NULL;
}


//------------------------------------------------------------------------
void CVehiclePartBase::GetGeometryName(EVehiclePartState state, string& name)
{
  name = GetName(); 

  switch (state)
  {  
  case eVGS_Destroyed:
    name += m_geometryDestroyedSuffix; 
    break;
  case eVGS_Damaged1:
    name += "_damaged_1";
    break;
  }  
}


//------------------------------------------------------------------------
const Matrix34& CVehiclePartBase::GetLocalTM(bool relativeToParentPart)
{
  const Matrix34& tm = GetEntity()->GetSlotLocalTM(m_slot, relativeToParentPart);

	return VALIDATE_MAT(tm);  
}

//------------------------------------------------------------------------
void CVehiclePartBase::SetLocalTM(const Matrix34& localTM)
{
	GetEntity()->SetSlotLocalTM(m_slot, localTM);
}

//------------------------------------------------------------------------
void CVehiclePartBase::ResetLocalTM(bool recursive)
{
  if (recursive)
  {
    for (TVehicleChildParts::iterator it=m_children.begin(),end=m_children.end(); it!=end; ++it)  
      (*it)->ResetLocalTM(true);
  }    
}

//------------------------------------------------------------------------
const Matrix34& CVehiclePartBase::GetWorldTM()
{
	return VALIDATE_MAT(GetEntity()->GetSlotWorldTM(m_slot));
}


//------------------------------------------------------------------------
const Matrix34& CVehiclePartBase::LocalToVehicleTM(const Matrix34& localTM)
{
  FUNCTION_PROFILER( gEnv->pSystem, PROFILE_GAME );

  static Matrix34 tm;  
  tm = VALIDATE_MAT(localTM);

  IVehiclePart* pParent = GetParent();
  while (pParent)
  {
    tm = pParent->GetLocalTM(true) * tm;
    pParent = pParent->GetParent();
  }      

  return VALIDATE_MAT(tm);
}

//------------------------------------------------------------------------
const AABB& CVehiclePartBase::GetLocalBounds()
{
	if (m_slot == -1)
	{
		m_bounds.Reset();
	}
	else
	{
		
		if (IStatObj* pStatObj = GetEntity()->GetStatObj(m_slot))
		{
			m_bounds = pStatObj->GetAABB();
			m_bounds.SetTransformedAABB(GetEntity()->GetSlotLocalTM(m_slot, false), m_bounds);
		}
		else if (ICharacterInstance* pCharInstance = GetEntity()->GetCharacter(m_slot))
		{
		  m_bounds = pCharInstance->GetAABB();
      m_bounds.SetTransformedAABB(GetEntity()->GetSlotLocalTM(m_slot, false), m_bounds);
		}
    else
    {
      GetEntity()->GetLocalBounds(m_bounds);
    }		
	}  
	
  return VALIDATE_AABB(m_bounds);
}

//------------------------------------------------------------------------
inline bool ComputeRotation(float &actual, float goal, float speed, float frameTime, float slowMult=1.f)
{
  if (actual == goal)
    return false;

  if (speed > 0.f)
  {
    bool slowDown = sgn(goal)*sgn(actual) == -1 || abs(goal) < abs(actual);

    float delta(goal - actual);
    actual += sgn(delta) * min(abs(delta), frameTime*speed*(slowDown ? slowMult : 1.f));

    return slowDown;
  }
  else
  {
    actual = goal;
  }  

  return false;
}

//------------------------------------------------------------------------
void CVehiclePartBase::Update(float frameTime)
{   
  FUNCTION_PROFILER( GetISystem(), PROFILE_GAME );

	if (m_pitchSpeed != 0.0f || m_yawSpeed != 0.0f)
	{
		float updatedAngle = UpdateRotation(frameTime);
		
    if (gEnv->bClient)
      UpdateRotationSound(frameTime, updatedAngle);

		m_lastPitch = m_currentPitch;
		m_lastYaw = m_currentYaw;
		m_prevBaseTM = GetLocalBaseTM();
	}
  
	if (m_hideMode != eVPH_NoFade)
	{
		if (m_hideMode == eVPH_FadeIn)
		{
			m_hideTimeCount += frameTime;

			if (m_hideTimeCount >= m_hideTimeMax)
				m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
		}
		else if (m_hideMode == eVPH_FadeOut)
		{
			m_hideTimeCount -= frameTime;

			if (m_hideTimeCount <= 0.0f)
				m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
		}
		else
			assert(0);

		if (IMaterial* pMaterialMain = GetMaterial())
		{
			float opacity = min(1.0f, max(0.0f, (m_hideTimeCount / m_hideTimeMax)));

			if (IRenderShaderResources* pShaderRes = pMaterialMain->GetShaderItem().m_pShaderResources)
			{
				pShaderRes->GetOpacity() = opacity;
			}

			for (int i = 0; i < pMaterialMain->GetSubMtlCount(); i++)
			{
				if (IMaterial* pMaterial = pMaterialMain->GetSubMtl(i))
				{
					if (IRenderShaderResources* pShaderRes = pMaterial->GetShaderItem().m_pShaderResources)
						pShaderRes->GetOpacity() = opacity;
				}
			}
		}
	}
}

//------------------------------------------------------------------------
float CVehiclePartBase::UpdateRotation(float frameTime)
{
  FUNCTION_PROFILER( gEnv->pSystem, PROFILE_GAME );
  
  if (gEnv->bClient && m_pVehicle->GetGameObject()->IsProbablyDistant() && !m_pVehicle->GetGameObject()->IsProbablyVisible())
    return 0.f;

  const float threshold = 0.02f;	
  m_rotationChanged = false;
  
  IVehiclePart* pParent = GetParent();
  IActor* pActor = m_pSeat ? m_pSeat->GetPassengerActor() : 0;
  
  bool remote = m_pSeat && m_pSeat->GetCurrentTransition() == IVehicleSeat::eVT_RemoteUsage;
  bool worldSpace = m_rotWorldSpace > 0.f && VehicleCVars().v_independentMountedGuns != 0;

	bool checkRotation = (m_rotTestHelpers[0] && m_rotTestHelpers[1] && pActor );
  
  if (worldSpace && pParent && pActor && pActor->IsClient() && !remote)
  { 
    // we want to keep the old worldspace rotation
    // therefore we're updatink the local transform from it
        
    Matrix34 localTM = pParent->GetWorldTM().GetInverted() * Matrix34(m_prevWorldQuat);
    localTM.OrthonormalizeFast(); // precision issue

    const Matrix34& baseTM = GetLocalBaseTM();
  
    if (!baseTM.IsEquivalent(localTM))
    {
      Ang3 anglesCurr(baseTM);
      Ang3 angles(localTM);

      if (m_pitchSpeed != 0.f)
      { 
        if (m_pitchMax != 0.f || m_pitchMin != 0.f)
          Limit(angles.x, m_pitchMin, m_pitchMax);

        angles.y = anglesCurr.y;
        angles.z = anglesCurr.z;
      }    
      else if (m_yawSpeed != 0.f)
      { 
        angles.x = anglesCurr.x;
        angles.y = anglesCurr.y;      

        if (m_yawMin != 0.f || m_yawMax != 0.f)
          Limit(angles.z, m_yawMin, m_yawMax);
      }

      localTM.SetRotationXYZ(angles);
      localTM.SetTranslation(baseTM.GetTranslation());
      SetLocalBaseTM(localTM);

      m_pSeat->ChangedNetworkState(CVehicle::ASPECT_PART_MATRIX);
    }

    if (VehicleCVars().v_debugdraw == eVDB_Parts)
    {
      float color[] = {1,1,1,1};
      Ang3 a(localTM), aBase(baseTM);    
      gEnv->pRenderer->Draw2dLabel(200,200,1.4f,color,false,"localAng: %.1f (real: %.1f)", RAD2DEG(a.z), RAD2DEG(aBase.z));        
    }    
  }
 	
	float updatedAngle = m_orientation.Update(frameTime);

	float convertedPitchMax = m_pitchMax;
	float convertedPitchMin = m_pitchMin;

  Matrix34 tm = GetLocalBaseTM();

  if (!(pActor && pActor->IsPlayer()))
  {
    // needed for AI (ask Tetsuji)
    Matrix33 initTM = Matrix33(GetLocalInitialTM());
    Matrix33 initialLocalLocalMatInvert( initTM );
    initialLocalLocalMatInvert.Invert();

    while (pParent)
    {
      initTM = Matrix33(pParent->GetLocalInitialTM()) * initTM;
      pParent = pParent->GetParent();
    }
    
    Matrix33 initialLocalMat( initTM );
    Matrix33 rotMatZ,rotMatXMin,rotMatXMax;

    float yaw;

    // this pitch should be converted in the parts space;
    rotMatXMax.SetRotationX(convertedPitchMax);	
    rotMatXMin.SetRotationX(convertedPitchMin);	
    Vec3 defaultFireDir(0.0f,1.0f,0.0f);
    Vec3 defaultFireDirRot;

    defaultFireDirRot = rotMatXMax *initialLocalMat * initialLocalLocalMatInvert * defaultFireDir;
    yaw = cry_atan2f(defaultFireDirRot.y,defaultFireDirRot.x);
    rotMatZ.SetRotationZ(-yaw);		
    defaultFireDirRot = rotMatZ * defaultFireDirRot;
    convertedPitchMax	=cry_atan2f(defaultFireDirRot.z,defaultFireDirRot.x);

    defaultFireDirRot = rotMatXMin *initialLocalMat * initialLocalLocalMatInvert * defaultFireDir;
    yaw = cry_atan2f(defaultFireDirRot.y,defaultFireDirRot.x);
    rotMatZ.SetRotationZ(-yaw);		
    defaultFireDirRot = rotMatZ * defaultFireDirRot;
    convertedPitchMin	=cry_atan2f(defaultFireDirRot.z,defaultFireDirRot.x);

    if ( convertedPitchMin > convertedPitchMax )
      std::swap( convertedPitchMin, convertedPitchMax );
  }
	
	{
		bool slowedYaw = ComputeRotation(m_currentYaw, m_actionYaw, m_yawAccel, frameTime, 1.8f);
		bool slowedPitch = ComputeRotation(m_currentPitch, m_actionPitch, m_pitchAccel, frameTime, 1.8f);

		if (IsDebugParts())
		{
			float color[] = {1,1,1,1};
			gEnv->pRenderer->Draw2dLabel(300, 250, 1.5f, color, false, "z: %.1f %s", m_currentYaw, (slowedYaw ? "(slowdown)" : ""));
			gEnv->pRenderer->Draw2dLabel(300, 280, 1.5f, color, false, "x: %.1f %s", m_currentPitch, (slowedPitch ? "(slowdown)" : ""));
		}
	}

  Ang3 deltaAngles(ZERO);

	if (m_pitchSpeed != 0.0f && abs(m_currentPitch) > threshold)
	{      
		deltaAngles.x = TURNSPEED_COEFF * max(min(m_currentPitch, m_pitchSpeed), -m_pitchSpeed) * GetDamageSpeedMul();
		m_rotationChanged = true;
	}

	if (m_yawSpeed != 0.0f && abs(m_currentYaw) > threshold)
	{ 
		deltaAngles.z = TURNSPEED_COEFF * max(min(m_currentYaw, m_yawSpeed), -m_yawSpeed) * GetDamageSpeedMul();
		m_rotationChanged = true;
	}

	if (m_isRotationBlocked)
	{
		deltaAngles.x = m_pitchSpeed * TURNSPEED_COEFF * 2.0;
		deltaAngles.z = 0.0f;
		m_rotationChanged = true;
	}

	// don't set m_orientation unless we're the client or an AI (for remote players it will be set in Serialize).
	// Also, allow rotation for remotely used seats (driver controlled guns).
	if (m_rotationChanged && ((m_pSeat && m_pSeat->GetCurrentTransition() == IVehicleSeat::eVT_RemoteUsage) || (pActor && (pActor->IsClient() || !pActor->IsPlayer()))))
	{ 
		m_orientation.Set(Quat(Matrix33(tm) * Matrix33::CreateRotationXYZ(deltaAngles)));
		Ang3 angles = Ang3::GetAnglesXYZ(m_orientation.Get());

		bool clamped = false;

		// clamp to limits
		if (m_pitchMax != 0.0f || m_pitchMin != 0.0f)
		{ 
			if (angles.x > convertedPitchMax || angles.x < convertedPitchMin)
			{
				angles.x = max(min(angles.x, convertedPitchMax), convertedPitchMin);
				m_currentPitch = 0.f;
				clamped = true;
			}
		}

		if (m_yawMax != 0.0f || m_yawMin != 0.0f)
		{
			if (angles.z > m_yawMax || angles.z < m_yawMin)
			{
				angles.z = max(min(angles.z, m_yawMax), m_yawMin);
				m_currentYaw = 0.f;
				clamped = true;
			}
		}

		if (clamped)
			m_orientation.Set(Quat::CreateRotationXYZ(angles));
	}

	m_actionPitch = 0.0f;
	m_actionYaw = 0.0f;

	bool updated = updatedAngle > 0.f || m_rotationChanged || m_orientation.IsInterpolating();

	Vec3 oldHelper2Pos = ZERO;
	if(checkRotation)
	{
		oldHelper2Pos = m_rotTestHelpers[1]->GetWorldTM().GetTranslation();
	}

	if (updated)
	{ 
		Matrix34 newTM(m_orientation.Get().GetNormalized());
		newTM.SetTranslation(tm.GetTranslation());

		SetLocalBaseTM(newTM);
	}

	const Matrix34& worldTM = GetWorldTM();
	m_prevWorldQuat = Quat(worldTM);
	assert(m_prevWorldQuat.IsValid());

	if(updated && checkRotation)
	{
		// force recalculation based on the new matrix set above.
		m_rotTestHelpers[0]->Invalidate();
		m_rotTestHelpers[1]->Invalidate();

		// need to test the new rotations before applying them. Sweep a sphere between the two helpers and check for collisions...
		static IPhysicalEntity* pSkipEntities[10];
		int numSkip = m_pVehicle->GetSkipEntities(pSkipEntities, 10);
		primitives::sphere sphere;
		sphere.center = m_rotTestHelpers[0]->GetWorldTM().GetTranslation();
		sphere.r = m_rotTestRadius;

		geom_contact* pContact = NULL;
		Vec3 dir = m_rotTestHelpers[1]->GetWorldTM().GetTranslation() - sphere.center;
		float hit = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, dir, ent_static|ent_terrain|ent_rigid|ent_sleeping_rigid, &pContact, 0, (geom_colltype_player<<rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEntities, numSkip);
		if(hit > 0.001f && pContact)
		{
			// ignore contacts with breakable things, as moving the barrel into them should break them.
			bool ignore = false;
			if(pContact)
			{
				ISurfaceType* pST = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager()->GetSurfaceType(pContact->id[1]);
				if(pST && pST->GetBreakability() > 0)
					ignore = true;
			}

			if(!ignore)
			{
				// there was a collision. check whether the barrel is moving towards the collision point or not... if not, ignore the collision.
				if(VehicleCVars().v_debugdraw > 0)
				{
					CPersistantDebug* pPD = CCryAction::GetCryAction()->GetPersistantDebug();
					pPD->Begin("VehicleCannon", false);

					ColorF col(1.0f, 0.0f, 0.0f, 1.0f);
					if(pContact && hit > 0.0f)
					{
						pPD->AddSphere(pContact->pt, 0.1f, col, 30.0f);
					}
				}

				Vec3 endPos = m_rotTestHelpers[1]->GetWorldTM().GetTranslation();
				Vec3 moveDir = (endPos - oldHelper2Pos).GetNormalizedSafe();
				Vec3 hitDir = (pContact->pt - oldHelper2Pos).GetNormalizedSafe();

				//CryLog("Collision dot=%.2f", moveDir.Dot(hitDir));
				if(moveDir.Dot(hitDir) > 0.0f)
				{
					// reset as though the rotation never happened.
					SetLocalBaseTM(m_prevBaseTM);
					updatedAngle = 0.0f;
					m_orientation.Set(Quat(m_prevBaseTM));
					m_prevWorldQuat = Quat(m_prevBaseTM);
				}
			}
		}
	}

	return updatedAngle;
}

//------------------------------------------------------------------------
bool CVehiclePartBase::ClampToRotationLimits(Ang3& angles)
{
  return false;
}

//------------------------------------------------------------------------
void CVehiclePartBase::UpdateRotationSound(float deltaTime, float updatedAngle)
{
  // update rotation sound, if available
  if (m_turnSoundId == InvalidSoundEventId)
    return;

  if (m_pVehicle->GetGameObject()->IsProbablyDistant())
    return;
  
  const static float minSpeed = 0.0002f;
  const static float soundDamageRatio = 0.2f;
  
  float speed = max(abs(m_currentYaw), abs(m_currentPitch));

  if (speed < 0.01f && updatedAngle > 0.f) // from interpolation
    speed = updatedAngle / TURNSPEED_COEFF;
  
  speed *= GetDamageSpeedMul();

  bool bDamage = m_damageRatio > soundDamageRatio && m_pVehicle->IsPlayerPassenger();

  if (speed != m_relSpeed)
  {      
    if (speed < 1e-4 && m_relSpeed < 1e-4)
      m_relSpeed = 0.f;
    else
      Interpolate(m_relSpeed, speed, 8.f, deltaTime);
    
    float speedParam = min(1.f, m_relSpeed/max(m_yawSpeed, m_pitchSpeed));      
    m_pVehicle->SetSoundParam(m_turnSoundId, "speed", speedParam, true);        
    
    if (bDamage)
      m_pVehicle->SetSoundParam(m_damageSoundId, "speed", speedParam, true);   
  }   
  else
  { 
    if (speed < minSpeed)
    {
      m_rotationStopped += deltaTime;

      if (m_rotationStopped > 2.f)
      {            
        m_pVehicle->StopSound(m_turnSoundId);
        m_pVehicle->StopSound(m_damageSoundId);
        m_rotationStopped = 0.f;
      }
    }
  }

  float inout = 1.f;
  if (m_pVehicle->IsPlayerPassenger())
  { 
    if (IVehicleSeat* pSeat = m_pVehicle->GetSeatForPassenger(CCryAction::GetCryAction()->GetClientActor()->GetEntityId()))
    {
      if (IVehicleView* pView = pSeat->GetView(pSeat->GetCurrentView()))
      {
        if (!pView->IsThirdPerson())
          inout = pSeat->GetSoundParams().inout;
      }
    }
  }        

  if (ISound* pSound = m_pVehicle->GetSound(m_turnSoundId, false))
    pSound->SetParam("in_out", inout, false); 

  if (bDamage) 
  {
    if (ISound* pSound = m_pVehicle->GetSound(m_damageSoundId, false))
    {
      pSound->SetParam("in_out", inout, false);     
      pSound->SetParam("damage", min(1.f, m_damageRatio), false);
    }      
  } 
}

//------------------------------------------------------------------------
IMaterial* CVehiclePartBase::GetMaterial()
{
	if (IEntityRenderProxy *pRenderProxy = (IEntityRenderProxy*) GetEntity()->GetProxy(ENTITY_PROXY_RENDER))
	{
		return pRenderProxy->GetRenderMaterial(m_slot);
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehiclePartBase::SetMaterial(IMaterial* pMaterial)
{
	// moved to CVehiclePartAnimated
}

//------------------------------------------------------------------------
void CVehiclePartBase::ParsePhysicsParams(SEntityPhysicalizeParams& physicalizeParams, const SmartScriptTable &table)
{
	table->GetValue("mass", physicalizeParams.mass);
	table->GetValue("density", physicalizeParams.density);
}

//------------------------------------------------------------------------
void CVehiclePartBase::CheckColltypeHeavy(int partid)
{
  if (partid < 0 || !GetEntity()->GetPhysics())
    return;

  if (!m_disableCollision)
  {
    pe_params_part paramsPart;
    paramsPart.partid = partid;
    
    if (m_pVehicle->GetMass() >= VEHICLE_MASS_COLLTYPE_HEAVY)
    {
      paramsPart.flagsAND = ~geom_colltype_debris; // light debris from breakable objs
      paramsPart.flagsColliderAND = geom_colltype3;
      paramsPart.flagsColliderOR = geom_colltype3; // for heavy vehicles
    }

    paramsPart.flagsColliderOR |= geom_colltype6; // vehicle-only colltype
    
    GetEntity()->GetPhysics()->SetParams(&paramsPart);
  }  
}

//------------------------------------------------------------------------
void CVehiclePartBase::SetFlags(int partid, int flags, bool add)
{
  if (partid < 0 || !GetEntity()->GetPhysics())
    return;

  pe_params_part paramsPart;
  paramsPart.partid = partid;
  
  if (add)
  {
    paramsPart.flagsOR = flags;     
  }
  else
  {
    paramsPart.flagsAND = ~flags;      
  }

  GetEntity()->GetPhysics()->SetParams(&paramsPart);  
}

//------------------------------------------------------------------------
void CVehiclePartBase::SetFlagsCollider(int partid, int flagsCollider)
{
  if (partid < 0 || !GetEntity()->GetPhysics())
    return;

  pe_params_part paramsPart;
  paramsPart.partid = partid;

  paramsPart.flagsColliderAND = 0;
  paramsPart.flagsColliderOR = flagsCollider;    

  GetEntity()->GetPhysics()->SetParams(&paramsPart);  
}

//------------------------------------------------------------------------
IVehiclePart* CVehiclePartBase::GetParent(bool root/*=false*/)
{
  IVehiclePart* pParent = m_pParentPart;

  if (root && pParent)
  {
    while (IVehiclePart* pRoot = pParent->GetParent(false))
      pParent = pRoot;
  }

  return pParent;
}

//------------------------------------------------------------------------
float CVehiclePartBase::GetDamageSpeedMul()
{
  // slowdown by max 50%, starting from 75% damage    
  return 1.0f - 2.f*max(min(1.f,m_damageRatio)-0.75f, 0.f);
}

//------------------------------------------------------------------------
void CVehiclePartBase::Serialize(TSerialize ser, unsigned aspects)
{	
	bool bSaveGame = ser.GetSerializationTarget() != eST_Network;

	if (bSaveGame)		  
	{	
		ser.BeginGroup("VehiclePart");

		ser.Value("m_damageRatio", m_damageRatio);    
		ser.EnumValue("m_hideMode", m_hideMode, eVPH_NoFade, eVPH_FadeOut);
		ser.Value("m_hideTimeCount", m_hideTimeCount);

    if (m_pitchSpeed != 0.f || m_yawSpeed != 0.f)
    {
      ser.Value("m_currentPitch", m_currentPitch);
      ser.Value("m_currentYaw", m_currentYaw);
      ser.Value("m_relSpeed", m_relSpeed);		
    }       
	} 

  if (bSaveGame || aspects&CVehicle::ASPECT_COMPONENT_DAMAGE)
  {
    EVehiclePartState state = m_state;
    ser.EnumValue("m_state", state, eVGS_Default, eVGS_Last);	

    if (ser.IsReading())
    { 
      ChangeState(state, eVPSF_Physicalize);
    }
  }

  if (!bSaveGame)
	{
		if (aspects & CVehicle::ASPECT_PART_MATRIX)
		{
			Quat q;
			if (ser.IsWriting())
				q=Quat(GetLocalBaseTM());

			ser.Value("rotation", q, 'ori1');

			if (ser.IsReading())
				m_orientation.SetGoal(q);
		}
	}

	if (bSaveGame)
		ser.EndGroup();
}

//------------------------------------------------------------------------
void CVehiclePartBase::CloneMaterial()
{
	if (!m_wasMaterialCloned)
		return;

	if (IMaterial* pMaterialOriginal = GetMaterial())
	{
		if (IMaterial *pMaterialCloned = pMaterialOriginal->GetMaterialManager()->CloneMultiMaterial(pMaterialOriginal))
		{
			SetMaterial(pMaterialCloned);
			m_wasMaterialCloned = true;
		}
	}
}


//------------------------------------------------------------------------
int CVehiclePartBase::GetCGASlot(int jointId)
{ 
  assert(m_slot >= 0);

  if (m_slot < 0) 
    return -1; 

  return PARTID_CGA * (m_slot+1) + jointId;
}

//------------------------------------------------------------------------
bool CVehiclePartBase::GetRotationLimits(int axis, float& min, float& max)
{
  switch (axis)
  {
  case 0:
    {
      if (m_pitchSpeed > 0.f)
      {
        min = m_pitchMin;
        max = m_pitchMax;
        return true;
      }      
    }
    break;    
  case 2:
    {
      if (m_yawSpeed > 0.f)
      {
        min = m_yawMin;
        max = m_yawMax;
        return true;
      }      
    }
    break;    
  }

  return false;
}

void CVehiclePartBase::GetBaseMemoryStatistics(ICrySizer * s)
{
	s->Add(m_name);	
	s->Add(m_helperPosName);	
}


DEFINE_VEHICLEOBJECT(CVehiclePartBase);
