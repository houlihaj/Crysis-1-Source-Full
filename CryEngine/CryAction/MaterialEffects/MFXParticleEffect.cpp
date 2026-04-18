#include "StdAfx.h"

#include "ParticleParams.h"
#ifndef _LIB
#include "ParticleParamsTypeInfo.cpp"
#endif
#include "MFXParticleEffect.h"
#include "MaterialEffectsCVars.h"
#include "IActorSystem.h"

CMFXParticleEffect::CMFXParticleEffect()
{
}

CMFXParticleEffect::~CMFXParticleEffect()
{
}

void CMFXParticleEffect::Execute(SMFXRunTimeEffectParams& params)
{
  FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	if (!(params.playflags & MFX_PLAY_PARTICLES))
		return;
		
  Vec3 pos = params.pos;
	Vec3 dir = ZERO;
	Vec3 inDir = params.dir[0];
	Vec3 reverso = inDir * -1.0f;
	switch (m_particleParams.directionType)
	{
	case MFX_PART_DIRECTION_NORMAL:
		dir = params.normal;
		break;
	case MFX_PART_DIRECTION_RICOCHET:
		dir = reverso.GetRotated(params.normal, gf_PI).normalize();
		break;
	default:
		dir = params.normal;
		break;
	}

  float distToPlayer = 0.f;
  IActor *pAct = gEnv->pGame->GetIGameFramework()->GetClientActor();
  if (pAct)
  {
    distToPlayer = (pAct->GetEntity()->GetWorldPos() - params.pos).GetLength();
  }
  
  SMFXParticleEntries::const_iterator end = m_particleParams.m_entries.end();
  for (SMFXParticleEntries::const_iterator it=m_particleParams.m_entries.begin(); it!=end; ++it)
  {
    // choose effect based on distance
    if (it->maxdist == 0.f || distToPlayer <= it->maxdist)
    { 
      IParticleEffect *pParticle = gEnv->p3DEngine->FindParticleEffect(it->name.c_str());

      if (pParticle)
      {
        SMFXParticleEffectParams& pa = params.particleParams;

        float pfx_minscale = (it->minscale != 0.f) ? it->minscale : (pa.minscale != 0.f) ? pa.minscale : CMaterialEffectsCVars::Get().mfx_pfx_minScale;
        float pfx_maxscale = (it->maxscale != 0.f) ? it->maxscale : (pa.maxscale != 0.f) ? pa.maxscale : CMaterialEffectsCVars::Get().mfx_pfx_maxScale; 
        float pfx_maxdist = (it->maxscaledist != 0.f) ? it->maxscaledist : (pa.maxscaledist != 0.f) ? pa.maxscaledist : CMaterialEffectsCVars::Get().mfx_pfx_maxDist; 
                
        float truscale = pfx_minscale + ((pfx_maxscale - pfx_minscale) * (distToPlayer!=0.f ? min(1.0f, distToPlayer/pfx_maxdist) : 1.f));  

        // Spawn the particle
        SpawnParams spawnparms;
        spawnparms.fSizeScale = truscale * it->scale * params.scale;
        
        IEntity *pEntTrg = gEnv->pEntitySystem->GetEntity(params.trg);
        if(pEntTrg && !strnicmp("train", pEntTrg->GetName(),6))
        {// attach the emitter to the train
          Matrix34 loc=IParticleEffect::ParticleLoc(pos, dir);
          int slot=pEntTrg->LoadParticleEmitter(-1, pParticle, &spawnparms);
          Matrix34 worldMatrix = pEntTrg->GetWorldTM();
          Matrix34 localMatrix = worldMatrix.GetInverted()*loc;
          pEntTrg->SetSlotLocalTM(slot, localMatrix);
          break;
        }
        IParticleEmitter *emit = pParticle->Spawn( true, IParticleEffect::ParticleLoc(pos, dir) );
        emit->SetSpawnParams(spawnparms);
      }
      
      break;
    }
  }
	
}

void CMFXParticleEffect::ReadXMLNode(XmlNodeRef& node)
{
	IMFXEffect::ReadXMLNode(node);
	  
  for (int i=0; i<node->getChildCount(); ++i)
  {
    XmlNodeRef child = node->getChild(i);
    if (!strcmp(child->getTag(), "Name"))
    {
      SMFXParticleEntry entry;            
      entry.name = child->getContent();

      if (child->haveAttr("userdata"))
        entry.userdata = child->getAttr("userdata");
      
      if (child->haveAttr("scale"))
        child->getAttr("scale", entry.scale);

      if (child->haveAttr("maxdist"))
        child->getAttr("maxdist", entry.maxdist);

      if (child->haveAttr("minscale"))
        child->getAttr("minscale", entry.minscale);

      if (child->haveAttr("maxscale"))
        child->getAttr("maxscale", entry.maxscale);

      if (child->haveAttr("maxscaledist"))
        child->getAttr("maxscaledist", entry.maxscaledist);
      
      m_particleParams.m_entries.push_back(entry);
    }
  }
  
  MFXParticleDirection dir = MFX_PART_DIRECTION_NORMAL;
	XmlNodeRef dirType = node->findChild("Direction");
	if (dirType)
	{
		const char *val = dirType->getContent();
		if (!strcmp(val, "Normal"))
		{
			dir = MFX_PART_DIRECTION_NORMAL;
		}
		else if (!strcmp(val, "Ricochet"))
		{
			dir = MFX_PART_DIRECTION_RICOCHET;
		}
	}
	m_particleParams.directionType = dir;
	
}

IMFXEffectPtr CMFXParticleEffect::Clone()
{
	CMFXParticleEffect *clone = new CMFXParticleEffect();
	clone->m_effectParams = m_effectParams;
	clone->m_particleParams = m_particleParams;
	return clone;
}

void CMFXParticleEffect::GetResources(SMFXResourceList& rlist)
{
	SMFXParticleListNode *listNode = SMFXParticleListNode::Create();
	
  if (!m_particleParams.m_entries.empty())
  {
    const SMFXParticleEntry& entry = m_particleParams.m_entries.back();    
    listNode->m_particleParams.name = entry.name.c_str();
    listNode->m_particleParams.userdata = entry.userdata.c_str();
    listNode->m_particleParams.scale = entry.scale;
  }  
  listNode->m_particleParams.directionType = m_particleParams.directionType;

  SMFXParticleListNode* next = rlist.m_particleList;
  
  if (!next)
    rlist.m_particleList = listNode;
  else
  { 
    while (next->pNext)
      next = next->pNext;

    next->pNext = listNode;
  }  
}

void CMFXParticleEffect::PreLoadAssets()
{
	IMFXEffect::PreLoadAssets();
	for (size_t i=0; i<m_particleParams.m_entries.size(); i++)
	{
		gEnv->p3DEngine->FindParticleEffect( m_particleParams.m_entries[i].name.c_str() );
	}
}
