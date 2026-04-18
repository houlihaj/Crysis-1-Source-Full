////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2007.
// -------------------------------------------------------------------------
//  File name:   SkeletonEffectManager.h
//  Version:     v1.00
//  Created:     3/5/2007 by MichaelS.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __SkeletonEffectManager_h__
#define __SkeletonEffectManager_h__

struct ISkeletonAnim;
struct ISkeletonPose;

class CSkeletonEffectManager
{
public:
	CSkeletonEffectManager();
	~CSkeletonEffectManager();
	
	void Update(ISkeletonAnim* pSkeletonAnim,ISkeletonPose* pSkeletonPose, const Matrix34& entityTM);
	void SpawnEffect(ISkeletonPose* pSkeletonPose, int animID, const char* animName, const char* effectName, const char* boneName, const Vec3& offset, const Vec3& dir, const Matrix34& entityTM);
	void KillAllEffects();

private:
	struct EffectEntry
	{
		EffectEntry(_smart_ptr<IParticleEffect> pEffect, _smart_ptr<IParticleEmitter> pEmitter, int boneID, const Vec3& offset, const Vec3& dir, int animID);
		~EffectEntry();

		_smart_ptr<IParticleEffect> pEffect;
		_smart_ptr<IParticleEmitter> pEmitter;
		int boneID;
		Vec3 offset;
		Vec3 dir;
		int animID;
	};

	void GetEffectTM(ISkeletonPose* pSkeletonPose, Matrix34& tm, int boneID, const Vec3& offset, const Vec3& dir, const Matrix34& entityTM);
	bool IsPlayingAnimation(ISkeletonAnim* pSkeletonAnim, int animID);
	bool IsPlayingEffect(const char* effectName);

	DynArray<EffectEntry> m_effects;
};

#endif //__SkeletonEffectManager_h__
