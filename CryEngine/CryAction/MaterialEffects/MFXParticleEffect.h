#include "IMFXEffect.h"
#include "IMaterialEffects.h"
#pragma once

struct SMFXParticleEntry 
{
  string name;
  string userdata;
  float scale; // base scale
  float maxdist; // max distance for spawning this effect
  float minscale; // min scale (distance == 0)
  float maxscale; // max scale (distance == maxscaledist)
  float maxscaledist; 

  SMFXParticleEntry()
  {
    scale = 1.f;
    maxdist = 0.f;
    minscale = maxscale = maxscaledist = 0.f;    
  }  
};

typedef std::vector<SMFXParticleEntry> SMFXParticleEntries;

struct SMFXParticleParams 
{  
  SMFXParticleEntries m_entries;
	MFXParticleDirection directionType;
};

class CMFXParticleEffect :
	public IMFXEffect
{
public:
	virtual void Execute(SMFXRunTimeEffectParams& params);
	virtual void ReadXMLNode(XmlNodeRef& node);
	virtual IMFXEffectPtr Clone();
	virtual void GetResources(SMFXResourceList& rlist);
	virtual void PreLoadAssets();
	CMFXParticleEffect();
	virtual ~CMFXParticleEffect();
	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->AddContainer(m_particleParams.m_entries);
		for (size_t i=0; i<m_particleParams.m_entries.size(); i++)
		{
			s->Add(m_particleParams.m_entries[i].name);
			s->Add(m_particleParams.m_entries[i].userdata);
		}
		IMFXEffect::GetMemoryStatistics(s);
	}
protected:
	SMFXParticleParams m_particleParams;
};
