////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   EyeMovementFaceAnim.h
//  Version:     v1.00
//  Created:     20/6/2005 by Michael S.
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "EyeMovementFaceAnim.h"
#include "FacialInstance.h"
#include "../CharacterManager.h"	
#include "../CharacterInstance.h"
#include "FaceEffector.h"
#include "FaceEffectorLibrary.h"

#if defined(__GNUC__)
#include <float.h> // FLT_MIN
#endif

const char* CEyeMovementFaceAnim::szEffectorNames[] = {
	"Eye_left_upper_middle",
	"Eye_left_upper_right",
	"Eye_left_middle_right",
	"Eye_left_lower_right",
	"Eye_left_lower_middle",
	"Eye_left_lower_left",
	"Eye_left_middle_left",
	"Eye_left_upper_left",
	"Eye_right_upper_middle",
	"Eye_right_upper_right",
	"Eye_right_middle_right",
	"Eye_right_lower_right",
	"Eye_right_lower_middle",
	"Eye_right_lower_left",
	"Eye_right_middle_left",
	"Eye_right_upper_left"
};

const char* CEyeMovementFaceAnim::szEyeBoneNames[] = {
	"eye_left_bone",
	"eye_right_bone"
};

CEyeMovementFaceAnim::CEyeMovementFaceAnim(CFacialInstance* pInstance)
:	m_pInstance(pInstance),
	m_bInitialized(false)
{
	InitialiseChannels();
}

void CEyeMovementFaceAnim::Update(float fDeltaTimeSec, const QuatTS& rAnimLocationNext, CSkinInstance* pCharacter, CFacialEffectorsLibrary* pEffectorsLibrary, CFaceState* pFaceState)
{
	if (!m_bInitialized)
	{
		m_bInitialized = true;
		InitialiseBoneIDs(); // This needs to be done here because we need the parent to be attached, if this is an attachment. During
		                     // construction the sub-character is not yet attached.
	}

	QuatT additionalRotations[EyeCOUNT];
	CalculateEyeAdditionalRotation(pCharacter, pFaceState, pEffectorsLibrary, additionalRotations);

	for (EyeID eye = EyeID(0); eye < EyeCOUNT; eye = EyeID(eye + 1))
		UpdateEye(rAnimLocationNext, eye, additionalRotations[eye]);
}

void CEyeMovementFaceAnim::OnExpressionLibraryLoad()
{
	InitialiseChannels();
}

void CEyeMovementFaceAnim::InitialiseChannels()
{
	std::fill(m_channelEntries, m_channelEntries + EffectorCOUNT, CChannelEntry());
}

uint32 CEyeMovementFaceAnim::GetChannelForEffector(EffectorID effector)
{
	assert(effector >= 0 && effector < EffectorCOUNT);
	if (effector < 0 || effector >= EffectorCOUNT)
		return ~0;

	// Check whether the effector has already been loaded.
	if (!m_channelEntries[effector].loadingAttempted)
		CreateChannelForEffector(effector);

	return m_channelEntries[effector].id;
}

uint32 CEyeMovementFaceAnim::CreateChannelForEffector(EffectorID effector)
{
	assert(effector >= 0 && effector < EffectorCOUNT);
	if (effector < 0 || effector >= EffectorCOUNT)
		return ~0;

	// Look up the effector.
	const char* szEffectorName = szEffectorNames[effector];
	CFacialEffector* pEffector = static_cast<CFacialEffector*>(m_pInstance->FindEffector(szEffectorName));

	// Try to create a channel for the effector.
	uint32 id = ~0;
	if (pEffector)
	{
		SFacialEffectorChannel ch;
		ch.status = SFacialEffectorChannel::STATUS_ONE;
		ch.pEffector = (CFacialEffector*)pEffector;
		ch.fCurrWeight = 0;
		ch.fWeight = 0;
		ch.bIgnoreLifeTime = true;
		ch.fFadeTime = 0;
		ch.fLifeTime = 0;
		ch.nRepeatCount = 0;

		id = m_pInstance->GetAnimContext()->StartChannel(ch);
	}

	// Update the cache information.
	m_channelEntries[effector].id = id;
	m_channelEntries[effector].loadingAttempted = true;

	return id;
}

void CEyeMovementFaceAnim::UpdateEye(const QuatTS& rAnimLocationNext, EyeID eye, const QuatT& additionalRotation)
{
	if (m_eyeBoneIDs[eye] < 0)
		return;

	// Set the weights of all the channels to 0.
	for (DirectionID direction = static_cast<DirectionID>(0); direction < DirectionCOUNT; direction = static_cast<DirectionID>(direction + 1))
	{
		uint32 id = GetChannelForEffector(EffectorFromEyeAndDirection(eye, direction));
		if (id != -1)
			m_pInstance->GetAnimContext()->SetChannelWeight(id, 0);
	}

	// Find the direction the eye is looking in.
	float angle;
	float strength;
	FindLookAngleAndStrength(eye, angle, strength, additionalRotation);

	// Find the two closest directions to the angle.
	float octantAngle = (angle * DirectionCOUNT / gf_PI2);
	octantAngle = 2.0f - octantAngle;
	if (octantAngle < 0.0f)
		octantAngle += DirectionCOUNT;
	//octantAngle = DirectionCOUNT - octantAngle;
	DirectionID directionInterpolateLow = DirectionID(int(floorf(octantAngle)));
	if (directionInterpolateLow >= DirectionCOUNT)
		directionInterpolateLow = DirectionID(directionInterpolateLow - DirectionCOUNT);
	DirectionID directionInterpolateHigh = DirectionID(int(ceilf(octantAngle + FLT_MIN)));
	if (directionInterpolateHigh >= DirectionCOUNT)
		directionInterpolateHigh = DirectionID(directionInterpolateHigh - DirectionCOUNT);
	float interpolationFraction = octantAngle - floorf(octantAngle);

	// Set the weight of the two channels.
	{
		uint32 id = GetChannelForEffector(EffectorFromEyeAndDirection(eye, directionInterpolateLow));
		if (id != -1)
			m_pInstance->GetAnimContext()->SetChannelWeight(id, sin((1.0f - interpolationFraction) * 3.14159f / 2) * strength);
	}
	{
		uint32 id = GetChannelForEffector(EffectorFromEyeAndDirection(eye, directionInterpolateHigh));
		if (id != -1)
			m_pInstance->GetAnimContext()->SetChannelWeight(id, sin(interpolationFraction * 3.14159f / 2) * strength);
	}

	if (Console::GetInst().ca_DebugFacialEyes)
	{
		string text;
		text.Format("%.3f, %.3f, %.3f", octantAngle, strength, interpolationFraction * strength);
		DisplayDebugInfoForEye(rAnimLocationNext, eye, text);
	}
}

void CEyeMovementFaceAnim::InitialiseBoneIDs()
{
	CSkinInstance* pCharacterInstance = (m_pInstance ? m_pInstance->GetCharacter() : 0);
	CAttachment* pMasterAttachment = (pCharacterInstance ? pCharacterInstance->m_pSkinAttachment : 0);
	CAttachmentManager* pMasterAttachmentManager = (pMasterAttachment ? pMasterAttachment->m_pAttachmentManager : 0);

	CCharInstance* pMasterCharacterInstance = (CCharInstance*)(pMasterAttachmentManager ? pMasterAttachmentManager->m_pSkinInstance : pCharacterInstance);
	CSkeletonAnim* pSkeleton = (pMasterCharacterInstance ? &pMasterCharacterInstance->m_SkeletonAnim : 0);
	CSkeletonPose* pSkeletonPose = (pMasterCharacterInstance ? &pMasterCharacterInstance->m_SkeletonPose : 0);

	for (EyeID eye = EyeID(0); eye < EyeCOUNT; eye = EyeID(eye + 1))
		m_eyeBoneIDs[eye] = (pSkeletonPose ? pSkeletonPose->GetJointIDByName(szEyeBoneNames[eye]) : -1);
}


void CEyeMovementFaceAnim::FindLookAngleAndStrength(EyeID eye, float& angle, float& strength, const QuatT& additionalRotation)
{
	CSkinInstance* pCharacterInstance = (m_pInstance ? m_pInstance->GetCharacter() : 0);
	CAttachment* pMasterAttachment = (pCharacterInstance ? pCharacterInstance->m_pSkinAttachment : 0);
	CAttachmentManager* pMasterAttachmentManager = (pMasterAttachment ? pMasterAttachment->m_pAttachmentManager : 0);

	CCharInstance* pMasterCharacterInstance = (CCharInstance*)(pMasterAttachmentManager ? pMasterAttachmentManager->m_pSkinInstance : pCharacterInstance);
	CSkeletonAnim* pSkeleton = (pMasterCharacterInstance ? &pMasterCharacterInstance->m_SkeletonAnim : 0);
	CSkeletonPose* pSkeletonPose = (pMasterCharacterInstance ? &pMasterCharacterInstance->m_SkeletonPose : 0);

//	SJoint* pJoint = (pSkeletonPose ? pSkeletonPose->GetJoint(m_eyeBoneIDs[eye]) : 0);

	// Find the transform of the bone and the default transform.
	QuatT eyeBoneTransform = (pSkeletonPose ? pSkeletonPose->GetRelJointByID(m_eyeBoneIDs[eye]) : IDENTITY);
	eyeBoneTransform = eyeBoneTransform * additionalRotation;
	QuatT eyeBoneTransformDefault = QuatT(IDENTITY);//m_pInstance->GetCharacter()->GetISkeleton()->GetDefaultRelJQuatByID(m_eyeBoneIDs[eye]);

	// Project the forward vector onto a plane based on the default pose.
	Vec3 forward = eyeBoneTransform.GetColumn1();
	const Vec3 rightVector = eyeBoneTransformDefault.GetColumn2();
	const Vec3 upVector = eyeBoneTransformDefault.GetColumn0();
	const Vec3 forwardVector = eyeBoneTransformDefault.GetColumn1();
	float rightComponent = -(forward | rightVector);
	float upComponent = forward | upVector;
	float forwardComponent = forward | forwardVector;

	// Check whether we are basically facing forward.
	if (fabsf(rightComponent) < 0.001f && fabsf(upComponent) < 0.001f)
	{
		angle = 0.0f;
		strength = 0.0f;
	}
	else
	{
		angle = atan2(upComponent, rightComponent);
		strength = (acos_tpl(forwardComponent) / 25.0f) * 180.0f / 3.14159f;
		if (strength > 1.0f)
			strength = 1.0f;
	}
}

void CEyeMovementFaceAnim::DisplayDebugInfoForEye(const QuatTS& rAnimLocationNext, EyeID eye, const string& text)
{
	CSkinInstance* pCharacterInstance = (m_pInstance ? m_pInstance->GetCharacter() : 0);
	CAttachment* pMasterAttachment = (pCharacterInstance ? pCharacterInstance->m_pSkinAttachment : 0);
	CAttachmentManager* pMasterAttachmentManager = (pMasterAttachment ? pMasterAttachment->m_pAttachmentManager : 0);

	CCharInstance* pMasterCharacterInstance = (CCharInstance*)(pMasterAttachmentManager ? pMasterAttachmentManager->m_pSkinInstance : pCharacterInstance);
	CSkeletonAnim* pSkeleton = (pMasterCharacterInstance ? &pMasterCharacterInstance->m_SkeletonAnim : 0);
	CSkeletonPose* pSkeletonPose = (pMasterCharacterInstance ? &pMasterCharacterInstance->m_SkeletonPose : 0);

	QuatT eyeBoneTransform = (pSkeletonPose ? pSkeletonPose->GetAbsJointByID(m_eyeBoneIDs[eye]) : QuatT(IDENTITY));
	Vec3 pos = rAnimLocationNext.t;
	float color[4] = { 1,1,1,1 };
	pos += (pSkeletonPose ? pSkeletonPose->GetAbsJointByID(m_eyeBoneIDs[eye]).t : Vec3(ZERO));
	g_pIRenderer->DrawLabelEx( pos,2.0f,color,true,true,"%s",text.c_str() );
}

void CEyeMovementFaceAnim::CalculateEyeAdditionalRotation(CSkinInstance* pCharacter, CFaceState* pFaceState, CFacialEffectorsLibrary* pEffectorsLibrary, QuatT additionalRotation[EyeCOUNT])
{
	for (EyeID eye = EyeID(0); eye < EyeCOUNT; eye = EyeID(eye + 1))
		additionalRotation[eye] = IDENTITY;

	int nNumIndices = (pFaceState ? pFaceState->GetNumWeights() : 0);
	for (int i = 0; i < nNumIndices; i++)
	{
		float fWeight = (pFaceState ? pFaceState->GetWeight(i) : 0);
		if (fabs(fWeight) < EFFECTOR_MIN_WEIGHT_EPSILON)
			continue;

		CFacialEffector *pEffector = (pEffectorsLibrary ? pEffectorsLibrary->GetEffectorFromIndex(i) : 0);
		if (pEffector)
		{
			switch (pEffector->GetType())
			{
			case EFE_TYPE_BONE:
				{
					CFacialEffectorBone* pBoneEffector = static_cast<CFacialEffectorBone*>(pEffector);
					for (EyeID eye = EyeID(0); eye < EyeCOUNT; eye = EyeID(eye + 1))
					{
						if (pBoneEffector && stricmp(pBoneEffector->GetParamString(EFE_PARAM_BONE_NAME), szEyeBoneNames[eye]) == 0)
						{
							QuatT rotation;
							Ang3 angle = Ang3( pBoneEffector->GetParamVec3(EFE_PARAM_BONE_ROT_AXIS) );
							rotation.SetRotationXYZ(angle*pFaceState->GetWeight(i));
							additionalRotation[eye] = rotation * additionalRotation[eye];
						}
					}
				}
				break;
			}
		}
	}
}
