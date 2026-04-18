////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2006.
// -------------------------------------------------------------------------
//  File name:   MFXSoundEffect.cpp
//  Version:     v1.00
//  Created:     28/11/2006 by JohnN/AlexL
//  Compilers:   Visual Studio.NET
//  Description: Sound effect
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "MFXSoundEffect.h"

CMFXSoundEffect::CMFXSoundEffect()
{
}

CMFXSoundEffect::~CMFXSoundEffect()
{
}

void CMFXSoundEffect::Execute(SMFXRunTimeEffectParams &params)
{
  FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	if (!(params.playflags & MFX_PLAY_SOUND))
		return;

	_smart_ptr<ISound> snd = 0;
	ISoundSystem *ssystem = gEnv->pSoundSystem;
	if (!ssystem)
		return;

	const char* realSoundName = m_soundParams.name;
	if (params.playSoundFP)
	{
		// AlexL: 07/03/2007
		// temporary solution until we can 2D Sounds here. Seems disabled for now and we
		// have to create a whole new soundname, appending "_fp"
		// Ask Tomas when he's back from GDC
		static const char postFix[] = "_fp";
		static const size_t postFixLen = (sizeof(postFix)/sizeof(postFix[0]))-1;
		size_t len = m_soundParams.name.length();
		char* stackString = (char*) alloca(len+postFixLen+1);
		if (stackString)
		{
			memcpy(stackString, realSoundName, len);
			strcpy(stackString+len, postFix);
			realSoundName = stackString;
		}
	}

	if (params.soundProxyEntityId)
	{
		IEntity *pEnt = gEnv->pEntitySystem->GetEntity(params.soundProxyEntityId);
		if (pEnt)
		{
			IEntitySoundProxy *pSoundProxy = (IEntitySoundProxy*)pEnt->GetProxy(ENTITY_PROXY_SOUND);
			if (!pSoundProxy)
				pSoundProxy = (IEntitySoundProxy*)pEnt->CreateProxy(ENTITY_PROXY_SOUND);
			if (pSoundProxy)
			{
				ISound *pSound = ssystem->CreateSound(realSoundName, (params.soundNoObstruction || params.playSoundFP) ? 0 : FLAG_SOUND_DEFAULT_3D);
				
				if (pSound)
				{
					EntityId SkipEntIDs[2];
					SkipEntIDs[0] = params.src;
					SkipEntIDs[1] = params.trg;

					pSound->SetPhysicsToBeSkipObstruction(SkipEntIDs, 2);
					pSound->SetSemantic(params.soundSemantic);

					if (params.soundDistanceMult< 1.0f && !m_soundParams.bIgnoreDistMult)
						pSound->SetDistanceMultiplier(params.soundDistanceMult);

					for (int i=0; i<params.MAX_SOUND_PARAMS; ++i)
					{
						const char* soundParamName = params.soundParams[i].paramName;
						if (soundParamName && *soundParamName)
						{
							pSound->SetParam(soundParamName, params.soundParams[i].paramValue, false);
						}
					}
					pSoundProxy->PlaySound(pSound, params.soundProxyOffset, FORWARD_DIRECTION, 1.0f);
				}
			}
		}
	}
	else
	{
		snd = ssystem->CreateSound(realSoundName, (params.soundNoObstruction || params.playSoundFP) ? 0 : FLAG_SOUND_DEFAULT_3D);
		if (snd)
		{
			EntityId SkipEntIDs[2];
			SkipEntIDs[0] = params.src;
			SkipEntIDs[1] = params.trg;

			snd->SetPhysicsToBeSkipObstruction(SkipEntIDs, 2);
			snd->SetSemantic(params.soundSemantic);
			snd->SetPosition(params.pos);

			if (params.soundDistanceMult< 1.0f && !m_soundParams.bIgnoreDistMult)
				snd->SetDistanceMultiplier(params.soundDistanceMult);
			
			for (int i=0; i<params.MAX_SOUND_PARAMS; ++i)
			{
				const char* soundParamName = params.soundParams[i].paramName;
				if (soundParamName && *soundParamName)
				{
					snd->SetParam(soundParamName, params.soundParams[i].paramValue, false);
				}
			}
			snd->Play();
		}
	}
}

void CMFXSoundEffect::ReadXMLNode(XmlNodeRef &node)
{
	IMFXEffect::ReadXMLNode(node);
	m_soundParams.bIgnoreDistMult = false;
	XmlNodeRef nameNode = node->findChild("Name");
	if (nameNode)
	{
		m_soundParams.name = nameNode->getContent();
		node->getAttr("bIgnoreDistMult", m_soundParams.bIgnoreDistMult);
	}
}

IMFXEffectPtr CMFXSoundEffect::Clone()
{
	CMFXSoundEffect *clone = new CMFXSoundEffect();
	clone->m_soundParams = m_soundParams;
	clone->m_effectParams = m_effectParams;
	return clone;
}

void CMFXSoundEffect::GetResources(SMFXResourceList &rlist)
{
	SMFXSoundListNode *listNode = SMFXSoundListNode::Create();
	listNode->m_soundParams.name = m_soundParams.name.c_str();

  SMFXSoundListNode* next = rlist.m_soundList;

  if (!next)
    rlist.m_soundList = listNode;
  else
  { 
    while (next->pNext)
      next = next->pNext;

    next->pNext = listNode;
  }  
}