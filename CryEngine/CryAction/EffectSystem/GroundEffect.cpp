#include "StdAfx.h"
#include "GroundEffect.h"
#include "IMaterialEffects.h"
#include "CryAction.h"

bool DebugLog()
{
  static ICVar* pVar = gEnv->pConsole->GetCVar("g_groundeffectsdebug");
  return (pVar && pVar->GetIVal()>0);
}

CGroundEffect::CGroundEffect(IEntity* pParent) :
 m_pEntity(pParent), 
 m_flags(eGEF_AlignToGround|eGEF_AlignToOcean), 
 m_height(10.0f), 
 m_isActive(false),
 m_isStopped(false),
 m_slot(-1),
 m_pParticleEffect(0),
 m_matId(0), 
 m_countScale(1.f),
 m_sizeScale(1.f),
 m_speedScale(1.f),
 m_maxHeightCountScale(1.f),
 m_maxHeightSizeScale(1.f),
 m_sizeGoal(1.f),
 m_speedGoal(1.f),
 m_interpSpeed(5.f)
{
}

CGroundEffect::~CGroundEffect()
{
	Deactivate();
}

void CGroundEffect::SetHeight(float height)
{
  m_height = height;         
  Deactivate();  
}

void CGroundEffect::SetHeightScale(float countScale, float sizeScale)
{ 
  m_maxHeightCountScale = countScale;
  m_maxHeightSizeScale = sizeScale;
}

void CGroundEffect::SetBaseScale(float sizeScale, float countScale, float speedScale)
{ 
  m_sizeGoal = sizeScale;  
  m_countScale = countScale;   
  m_speedGoal = speedScale;
}

void CGroundEffect::SetInterpolation(float speed)
{
  assert(speed>=0.f);
  m_interpSpeed = speed;
}


bool CGroundEffect::SetParticleEffect(const char* name)
{	
  if (name)
	  m_pParticleEffect = gEnv->p3DEngine->FindParticleEffect(name);
  else 
    m_pParticleEffect = 0;

	if (m_isActive)
	{
		// invalidate the settings to cause the new effect to be activate
		Deactivate();
	}

	m_isStopped = false;
	
	if (!m_pParticleEffect || !m_pEntity)
		return false;

  if (DebugLog())
    CryLog("[GroundEffect] <%s> set particle effect to (%s)", m_pEntity->GetName(), m_pParticleEffect->GetName());

	return true;
}

void CGroundEffect::SetInteraction(const char* name)
{
  // if an interaction name is set, the particle effect is looked up in MaterialEffects.
  // if not, one can still call SetParticleEffect by herself
  m_interaction = name;  
}

void CGroundEffect::SetSpawnParams(const SpawnParams& params)
{   
  if (m_slot < 0)
    return;
  
  SEntitySlotInfo info;  
  if (m_pEntity->GetSlotInfo(m_slot, info) && info.pParticleEmitter)
  {       
    info.pParticleEmitter->SetSpawnParams(params);
  }  
}


void CGroundEffect::Update()
{
	if (m_isStopped)
		return;
  
  Vec3 entityPos(m_pEntity->GetWorldPos());
  bool prevActive = m_isActive;
  bool newEffect = false;
  
  ray_hit rayhit;  
  int flags = rwi_colltype_any | rwi_ignore_noncolliding | rwi_stop_at_pierceable;
  int objTypes = ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid;

  float refHeight = 0.f;

  if (m_flags & eGEF_AlignToGround)
    refHeight = gEnv->p3DEngine->GetTerrainElevation(entityPos.x, entityPos.y);

  if (m_flags & eGEF_AlignToOcean)  
  {
    objTypes |= ent_water;
    refHeight = max(refHeight, gEnv->p3DEngine->GetWaterLevel(&entityPos));        
  }    
  
  if (m_interpSpeed>0.f)
  {
    float dt = gEnv->pTimer->GetFrameTime();
    Interpolate(m_sizeScale, m_sizeGoal, m_interpSpeed, dt);
    Interpolate(m_speedScale, m_speedGoal, m_interpSpeed, dt);
  }
  else
  {
    m_sizeScale = m_sizeGoal;
    m_speedScale = m_speedGoal;
  }
  
  Vec3 rayPos(entityPos.x, entityPos.y, entityPos.z + max(0.f, refHeight-entityPos.z+0.01f));
  int hits = gEnv->pPhysicalWorld->RayWorldIntersection(rayPos, Vec3(0,0,-m_height), objTypes, flags, &rayhit, 1, m_pEntity->GetPhysics());
  
  if (!hits)
  {
    m_isActive = false;
  }
  else
  { 
    m_isActive = true;
    m_ratio = rayhit.dist/m_height;

    // if surface changed, lookup new effect  
    if (rayhit.surface_idx != m_matId && !m_interaction.empty())
    {      
      IMaterialEffects* pMaterialEffects = CCryAction::GetCryAction()->GetIMaterialEffects();
	    TMFXEffectId effectId = pMaterialEffects->GetEffectId(m_interaction.c_str(), rayhit.surface_idx);
      const char* effect = 0;
      
      if (effectId != InvalidEffectId)
      {
        SMFXResourceListPtr pList = pMaterialEffects->GetResources(effectId);
        if (pList && pList->m_particleList)        
          effect = pList->m_particleList->m_particleParams.name;      
      }
      
      if (DebugLog())
      {
        const char* matName = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(rayhit.surface_idx)->GetName();
        CryLog("[GroundEffect] <%s> GetEffectString for matId %i (%s) returned <%s>", m_pEntity->GetName(), rayhit.surface_idx, matName, effect?effect:"null");
      }

      newEffect = SetParticleEffect(effect);
      m_matId = rayhit.surface_idx;
    }
  }	
 
	// status has changed 
	if (prevActive!=m_isActive || newEffect)
	{
		if (m_isActive && m_pParticleEffect != 0)
		{
			bool prime = ((m_flags & eGEF_PrimeEffect) != 0);
			m_slot = m_pEntity->LoadParticleEmitter(-1, m_pParticleEffect, 0, prime);
      
      if (DebugLog())
        CryLog("[GroundEffect] <%s> effect %s loaded to slot %i", m_pEntity->GetName(), m_pParticleEffect->GetName(), m_slot);
		}
		else
		{ 
      if (DebugLog())
        CryLog("[GroundEffect] <%s> slot %i freed", m_pEntity->GetName(), m_slot);

      Deactivate();
		}
	}

	if (m_isActive && m_flags & eGEF_StickOnGround)
	{
		Matrix34 effectMtx(Matrix33(IDENTITY),Vec3(entityPos.x,entityPos.y, rayhit.pt.z ));
		m_pEntity->SetSlotLocalTM(m_slot,m_pEntity->GetWorldTM().GetInverted() * effectMtx);
	}

  if (m_isActive && m_pParticleEffect)
  {
    // update scale
    SpawnParams sp;  
    sp.fSizeScale = m_sizeScale;
    sp.fCountScale = m_countScale;
    sp.fSpeedScale = m_speedScale;

    sp.fSizeScale  *= (1.f-m_maxHeightSizeScale)*(1.f-m_ratio) + m_maxHeightSizeScale;   
    sp.fCountScale *= (1.f-m_maxHeightCountScale)*(1.f-m_ratio) + m_maxHeightCountScale;

    SetSpawnParams(sp);
  }

  if (DebugLog())
  {
    Vec3 pos = (m_slot != -1) ? m_pEntity->GetSlotWorldTM(m_slot).GetTranslation() : entityPos;
    const char* effect = m_pParticleEffect ? m_pParticleEffect->GetName() : "";

    gEnv->pRenderer->DrawLabel(pos+Vec3(0,0,0.5f), 1.25f, "GO [interaction '%s'], effect '%s', active: %i", m_interaction.c_str(), effect, m_isActive);
    gEnv->pRenderer->DrawLabel(pos+Vec3(0,0,0.0f), 1.25f, "height %.1f/%.1f, base size/count scale %.2f/%.2f", rayhit.dist, m_height, m_sizeScale, m_countScale);
  }

}

void CGroundEffect::Deactivate()
{
  if (m_slot != -1)
  {
    if (m_pEntity)
    {
      SEntitySlotInfo info;
      if (m_pEntity->GetSlotInfo(m_slot, info) && info.pParticleEmitter)
        info.pParticleEmitter->Activate(false);

      m_pEntity->FreeSlot(m_slot);
    }

    m_slot = -1;
  }

  m_isActive = false;
}

void CGroundEffect::Stop(bool stop)
{
	if (stop)	
		Deactivate();
	
	m_isStopped = stop;
}