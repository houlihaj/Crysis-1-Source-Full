////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   SkeletonEffectManager.cpp
//  Version:     v1.00
//  Created:     3/5/2007 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SkeletonEffectManager.h"
#include "ICryAnimation.h"

CSkeletonEffectManager::CSkeletonEffectManager()
{
}

CSkeletonEffectManager::~CSkeletonEffectManager()
{
	KillAllEffects();
}

void CSkeletonEffectManager::Update(ISkeletonAnim* pSkeleton,ISkeletonPose* pSkeletonPose, const Matrix34& entityTM)
{
	for (int i = 0; i < m_effects.size();)
	{
		EffectEntry& entry = m_effects[i];

		// If the animation has stopped, kill the effect.
		bool effectStillPlaying = (entry.pEmitter ? entry.pEmitter->IsAlive() : false);
		bool animStillPlaying = IsPlayingAnimation(pSkeleton, entry.animID);
		if (animStillPlaying && effectStillPlaying)
		{
			// Update the effect position.
			Matrix34 tm;
			GetEffectTM(pSkeletonPose, tm, entry.boneID, entry.offset, entry.dir, entityTM);
			if (entry.pEmitter)
				entry.pEmitter->SetMatrix(tm);

			 ++i;
		}
		else
		{
			if (Console::GetInst().ca_DebugSkeletonEffects > 0)
			{
				CryLogAlways("CSkeletonEffectManager::Update(this=%p): Killing effect \"%s\" because %s.", this,
					(m_effects[i].pEffect ? m_effects[i].pEffect->GetName() : "<EFFECT NULL>"), (effectStillPlaying ? "animation has ended" : "effect has ended"));
			}
			if (m_effects[i].pEmitter)
				m_effects[i].pEmitter->Activate(false);
			m_effects.erase(m_effects.begin() + i);
		}
	}
}

void CSkeletonEffectManager::KillAllEffects()
{
	if (Console::GetInst().ca_DebugSkeletonEffects)
	{
		for (int effectIndex = 0, effectCount = m_effects.size(); effectIndex < effectCount; ++effectIndex)
		{
			IParticleEffect* pEffect = m_effects[effectIndex].pEffect;
			CryLogAlways("CSkeletonEffectManager::KillAllEffects(this=%p): Killing effect \"%s\" because animated character is in simplified movement.", this, (pEffect ? pEffect->GetName() : "<EFFECT NULL>"));
		}
	}

	for (int i = 0, count = m_effects.size(); i < count; ++i)
	{
		if (m_effects[i].pEmitter)
			m_effects[i].pEmitter->Activate(false);
	}

	m_effects.clear();
}

void CSkeletonEffectManager::SpawnEffect(ISkeletonPose* pSkeletonPose, int animID, const char* animName, const char* effectName, const char* boneName, const Vec3& offset, const Vec3& dir, const Matrix34& entityTM)
{
	// Check whether we are already playing this effect, and if so dont restart it.
	bool alreadyPlayingEffect = IsPlayingEffect(effectName);

	if (alreadyPlayingEffect)
	{
		if (Console::GetInst().ca_DebugSkeletonEffects)
			CryLogAlways("CSkeletonEffectManager::SpawnEffect(this=%p): Refusing to start effect \"%s\" because effect is already running.", this, (effectName ? effectName : "<MISSING EFFECT NAME>"));
	}
	else
	{
		IParticleEffect* pEffect = gEnv->p3DEngine->FindParticleEffect(effectName);
		if (!pEffect)
			AnimWarning("Anim events cannot find effect \"%s\", requested by animation \"%s\".", (effectName ? effectName : "<MISSING EFFECT NAME>"), (animName ? animName : "<MISSING ANIM NAME>"));
		int boneID = (boneName && boneName[0] && pSkeletonPose ? pSkeletonPose->GetJointIDByName(boneName) : -1);
		boneID = (boneID == -1 ? 0 : boneID);
		Matrix34 tm;
		GetEffectTM(pSkeletonPose, tm, boneID, offset, dir, entityTM);
		IParticleEmitter* pEmitter = (pEffect ? pEffect->Spawn(false, tm) : 0);
		if (pEffect && pEmitter)
		{
			if (Console::GetInst().ca_DebugSkeletonEffects)
				CryLogAlways("CSkeletonEffectManager::SpawnEffect(this=%p): starting effect \"%s\", requested by animation \"%s\".", this, (effectName ? effectName : "<MISSING EFFECT NAME>"), (animName ? animName : "<MISSING ANIM NAME>"));
			m_effects.push_back(EffectEntry(pEffect, pEmitter, boneID, offset, dir, animID));
		}
	}
}

CSkeletonEffectManager::EffectEntry::EffectEntry(_smart_ptr<IParticleEffect> pEffect, _smart_ptr<IParticleEmitter> pEmitter, int boneID, const Vec3& offset, const Vec3& dir, int animID)
:	pEffect(pEffect), pEmitter(pEmitter), boneID(boneID), offset(offset), dir(dir), animID(animID)
{
}

CSkeletonEffectManager::EffectEntry::~EffectEntry()
{
}

void CSkeletonEffectManager::GetEffectTM(ISkeletonPose* pSkeletonPose, Matrix34& tm, int boneID, const Vec3& offset, const Vec3& dir, const Matrix34& entityTM)
{
	if (dir.len2()>0)
		tm = Matrix33::CreateRotationXYZ(Ang3(dir * 3.14159f / 180.0f));
	else
		tm.SetIdentity();
	tm.AddTranslation(offset);

	if (pSkeletonPose)
		tm = Matrix34(pSkeletonPose->GetAbsJointByID(boneID)) * tm;
	tm = entityTM * tm;
}

bool CSkeletonEffectManager::IsPlayingAnimation(ISkeletonAnim* pSkeletonAnim, int animID)
{
	enum {NUM_LAYERS = 4};

	// Check whether the animation has stopped.
	bool animPlaying = false;
	for (int layer = 0; layer < NUM_LAYERS; ++layer)
	{
		for (int animIndex = 0, animCount = (pSkeletonAnim ? pSkeletonAnim->GetNumAnimsInFIFO(layer) : 0); animIndex < animCount; ++animIndex)
		{
			CAnimation& anim = pSkeletonAnim->GetAnimFromFIFO(animIndex, animIndex);
			int32 id = (anim.m_LMG0.m_nLMGID >= 0) ? anim.m_LMG0.m_nLMGID : anim.m_LMG0.m_nAnimID[0];
			animPlaying = animPlaying || (id == animID);
		}
	}

	return animPlaying;
}

bool CSkeletonEffectManager::IsPlayingEffect(const char* effectName)
{
	bool isPlaying = false;
	for (int effectIndex = 0, effectCount = m_effects.size(); effectIndex < effectCount; ++effectIndex)
	{
		IParticleEffect* pEffect = m_effects[effectIndex].pEffect;
		if (pEffect && stricmp(pEffect->GetName(), effectName) == 0)
			isPlaying = true;
	}
	return isPlaying;
}
