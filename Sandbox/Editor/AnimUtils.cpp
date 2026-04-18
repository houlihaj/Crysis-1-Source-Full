////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   AnimUtils.cpp
//  Version:     v1.00
//  Created:     9/11/2006 by Michael S.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "AnimUtils.h"
#include "ICryAnimation.h"

void AnimUtils::StartAnim(ICharacterInstance* pCharacter, const string& animName)
{
	CryCharAnimationParams Params(0);

	ISkeletonAnim* pISkeletonAnim = (pCharacter ? pCharacter->GetISkeletonAnim() : 0);
	if (pISkeletonAnim)
		pISkeletonAnim->StopAnimationsAllLayers();

	Params.m_nFlags |= (CA_MANUAL_UPDATE | CA_REPEAT_LAST_KEY);

	if (pISkeletonAnim)
		pISkeletonAnim->StartAnimation(animName, 0, 0, 0, Params);
}

void AnimUtils::SetAnimTime(ICharacterInstance* pCharacter, float time)
{
	ISkeletonAnim* pISkeletonAnim = (pCharacter ? pCharacter->GetISkeletonAnim() : 0);
	float timeToSet = max(0.0f, time);
	if (pISkeletonAnim)
		pISkeletonAnim->SetLayerTime(0, timeToSet);
}

void AnimUtils::StopAnims(ICharacterInstance* pCharacter)
{
	ISkeletonAnim* pISkeletonAnim = (pCharacter ? pCharacter->GetISkeletonAnim() : 0);
	if (pISkeletonAnim)
		pISkeletonAnim->StopAnimationsAllLayers();
}
