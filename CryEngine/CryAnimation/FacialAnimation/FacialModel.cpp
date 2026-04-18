////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FacialModel.cpp
//  Version:     v1.00
//  Created:     10/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "FacialModel.h"
#include "FaceAnimation.h"
#include "FaceEffectorLibrary.h"

#include <I3DEngine.h>
#include <CryHeaders.h>
#include "../CharacterInstance.h"
#include "../ModelMesh.h"
#include "../Model.h"

#undef PI
#define PI 3.14159265358979323f

//////////////////////////////////////////////////////////////////////////
CFacialModel::CFacialModel( CCharacterModel *pModel )
{
	m_pModel = pModel;
	InitMorphTargets();
	m_bAttachmentChanged = false;
}

//////////////////////////////////////////////////////////////////////////
CFaceState* CFacialModel::CreateState()
{
	CFaceState *pState = new CFaceState(this);
	if (m_pEffectorsLibrary)
		pState->SetNumWeights( m_pEffectorsLibrary->GetLastIndex() );
	return pState;
}

//////////////////////////////////////////////////////////////////////////
void CFacialModel::InitMorphTargets()
{
	CModelMesh *pModelMesh = m_pModel->GetModelMesh(0);
	int nMorphTargetsCount = (int)pModelMesh->m_morphTargets.size();
	m_effectors.reserve(nMorphTargetsCount);
	m_morphTargets.reserve( nMorphTargetsCount );
	for (int i = 0; i < nMorphTargetsCount; i++)
	{
		CMorphTarget *pMorphTarget = pModelMesh->m_morphTargets[i];
		if (!pMorphTarget)
			continue;

		CFacialMorphTarget *pMorphEffector = new CFacialMorphTarget(i);
		string morphname = pMorphTarget->m_name;
		if (!morphname.empty() && morphname[0] == '#') // skip # character at the begining of the morph target.
			morphname = morphname.substr(1);
		pMorphEffector->SetNameString(morphname);
		pMorphEffector->SetIndexInState(i);

		SEffectorInfo ef;
		ef.pEffector = pMorphEffector;
		ef.nMorphTargetId = i;
		m_effectors.push_back( ef );

		SMorphTargetInfo mrphTrg;
		morphname.MakeLower();
		mrphTrg.name = morphname;
		mrphTrg.nMorphTargetId = i;
		m_morphTargets.push_back( mrphTrg );
	}

	std::sort( m_morphTargets.begin(),m_morphTargets.end() );

	//m_libToFaceState
}

//////////////////////////////////////////////////////////////////////////
void CFacialModel::AssignLibrary( IFacialEffectorsLibrary *pLibrary )
{
	assert( pLibrary );
	if (pLibrary == m_pEffectorsLibrary)
		return;

	m_pEffectorsLibrary = (CFacialEffectorsLibrary*)pLibrary;

	int i;
	// Adds missing morph targets to the Facial Library.
	for (i = 0; i < (int)m_effectors.size(); i++)
	{
		CFacialEffector *pModelEffector = m_effectors[i].pEffector;

		CFacialEffector *pEffector = (CFacialEffector*)m_pEffectorsLibrary->Find( pModelEffector->GetName() );
		if (!pEffector)
		{
			// Library does not contain this effector, so add it into the library.
			if (m_effectors[i].nMorphTargetId >= 0)
			{
				IFacialEffector* pNewEffector = m_pEffectorsLibrary->CreateEffector( EFE_TYPE_MORPH_TARGET,pModelEffector->GetName() );
				IFacialEffector* pRoot = m_pEffectorsLibrary->GetRoot();
				if (pRoot && pNewEffector)
					pRoot->AddSubEffector(pNewEffector);
			}
		}
	}

	// Creates a mapping table between Face state index to the index in MorphTargetsInfo array.
	// When face state is applied to the model, this mapping table is used to find what morph target is associated with state index.
	int nIndices = m_pEffectorsLibrary->GetLastIndex();
	m_faceStateIndexToMorphTargetId.resize(nIndices);
	for (int i = 0; i < nIndices; i++)
	{
		// Initialize all face state mappings.
		m_faceStateIndexToMorphTargetId[i] = -1;
	}
	SMorphTargetInfo tempMT;
	for (CFacialEffectorsLibrary::NameToEffectorMap::iterator it = m_pEffectorsLibrary->m_nameToEffectorMap.begin(); it != m_pEffectorsLibrary->m_nameToEffectorMap.end(); ++it)
	{
		CFacialEffector *pEffector = it->second;
		int nStateIndex = pEffector->GetIndexInState();
		if (nStateIndex >= 0 && nStateIndex < nIndices)
		{
			// Find morph target for this state index.
			// Morph targets must be sorted by name for binary_find to work correctly.
			tempMT.name = pEffector->GetName();
			tempMT.name.MakeLower();
			MorphTargetsInfo::iterator it = stl::binary_find( m_morphTargets.begin(),m_morphTargets.end(),tempMT );
			if (it != m_morphTargets.end())
			{
				m_faceStateIndexToMorphTargetId[nStateIndex] = (int)std::distance(m_morphTargets.begin(),it);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IFacialEffectorsLibrary* CFacialModel::GetLibrary()
{
	return m_pEffectorsLibrary;
}

//////////////////////////////////////////////////////////////////////////
int CFacialModel::GetMorphTargetCount() const
{
	return m_morphTargets.size();
}

//////////////////////////////////////////////////////////////////////////
const char* CFacialModel::GetMorphTargetName(int morphTargetIndex) const
{
	return (morphTargetIndex >= 0 && morphTargetIndex < int(m_morphTargets.size()) ? m_morphTargets[morphTargetIndex].name.c_str() : 0);
}

//////////////////////////////////////////////////////////////////////////
void CFacialModel::ApplyFaceStateToCharacter( CFaceState* pFaceState,CSkinInstance *pCharacter, CCharInstance *pMasterCharacter, int numForcedRotations, CFacialAnimForcedRotationEntry* forcedRotations, bool blendBoneRotations )
{
	if (!m_pEffectorsLibrary)
		return;
	
	// Update Face State with the new number of indices.
	if (pFaceState->GetNumWeights() != m_pEffectorsLibrary->GetLastIndex())
	{
		pFaceState->SetNumWeights( m_pEffectorsLibrary->GetLastIndex() );
	}

	// Apply forced rotations if any. This is currently used only in the facial editor preview mode to
	// support the code interface to force the neck and eye orientations.
	{
		// Make sure we use the master skeleton, in case we are an attachment.
		for (int rotIndex = 0; pMasterCharacter && pCharacter && forcedRotations && rotIndex < numForcedRotations; ++rotIndex)
		{
			CSkeletonPose* pSkeletonPose = &pMasterCharacter->m_SkeletonPose;
			int32 id=forcedRotations[rotIndex].jointId;
			pSkeletonPose->m_qAdditionalRotPos[id].q = pSkeletonPose->m_qAdditionalRotPos[id].q * forcedRotations[rotIndex].rotation;
		}
	}

	CSkeletonPose* pSkeletonPose = &pMasterCharacter->m_SkeletonPose;
	assert(pSkeletonPose->GetJointCount() <= MAX_JOINT_AMOUNT);
	QuatT additionalRotPos[MAX_JOINT_AMOUNT];
	for (int boneIndex = 0, boneCount = pSkeletonPose->GetJointCount(); boneIndex < boneCount; ++boneIndex)
		additionalRotPos[boneIndex].SetIdentity();

	int nNumIndices = pFaceState->GetNumWeights();
	const CModelJoint* pModelJoint = pSkeletonPose->GetModelJointPointer(0);
	for (int i = 0; i < nNumIndices; i++)
	{
		float fWeight = pFaceState->GetWeight(i);
		if (fabs(fWeight) < EFFECTOR_MIN_WEIGHT_EPSILON)
			continue;

		if (i < (int)m_faceStateIndexToMorphTargetId.size() && m_faceStateIndexToMorphTargetId[i] >= 0)
		{
			// This is a morph target.
			int nMorphIndex = m_faceStateIndexToMorphTargetId[i];
			if (nMorphIndex >= 0 && nMorphIndex < (int)m_morphTargets.size())
			{
				int nMorphTargetId = m_morphTargets[nMorphIndex].nMorphTargetId;

				// Add morph target to character.
				CryModEffMorph morph;
				morph.m_fTime = 0;
				morph.m_nFlags = CryCharMorphParams::FLAGS_FREEZE | CryCharMorphParams::FLAGS_NO_BLENDOUT | CryCharMorphParams::FLAGS_INSTANTANEOUS;
				morph.m_nMorphTargetId = nMorphTargetId;
				morph.m_Params.m_fAmplitude = fWeight;
				morph.m_Params.m_fBlendIn = 0;
				morph.m_Params.m_fBlendOut = 0;
				morph.m_Params.m_fBalance = pFaceState->GetBalance(i);
				pCharacter->m_Morphing.m_arrMorphEffectors.push_back(morph);
			}
		}
		else
		{
			CFacialEffector *pEffector = m_pEffectorsLibrary->GetEffectorFromIndex(i);
			if (pEffector)
			{
				// Not a morph target.
				switch (pEffector->GetType())
				{
				case EFE_TYPE_ATTACHMENT:
					ApplyAttachmentEffector( pMasterCharacter,pEffector,pFaceState->GetWeight(i) );
					break;
				case EFE_TYPE_BONE:
					ApplyBoneEffector( pMasterCharacter,pEffector,pFaceState->GetWeight(i),additionalRotPos );
					break;
				}
			}
		}
	}

	uint32 numJoints=pSkeletonPose->GetJointCount();
	for (uint32 i=0; i<numJoints; i++)
	{
		SmoothCD(pSkeletonPose->m_FaceAnimPosSmooth[i], pSkeletonPose->m_FaceAnimPosSmoothRate[i], pMasterCharacter->m_fOriginalDeltaTime, additionalRotPos[i].t, 0.05f);
		pSkeletonPose->m_qAdditionalRotPos[i].q=Quat::CreateSlerp(pSkeletonPose->m_qAdditionalRotPos[i].q,additionalRotPos[i].q,1.0f-pSkeletonPose->m_LookIK.m_LookIKBlend);
	}

	/*
	if (blendBoneRotations)
	{
		// Blend the additional rotations - this is necessary when we are changing sequences or look ik settings to avoid snapping.
		const float interpolationRate = 5.0f;
		float interpolateFraction = min(1.0f, max(0.0f, pMasterCharacter->m_fOriginalDeltaTime * interpolationRate));
		for (int boneIndex = 0, boneCount = pSkeletonPose->GetJointCount(); boneIndex < boneCount; ++boneIndex)
		{
			pSkeletonPose->m_qAdditionalRotPos[boneIndex].q.SetSlerp(pSkeletonPose->m_qAdditionalRotPos[boneIndex].q, additionalRotPos[boneIndex].q, interpolateFraction);
			pSkeletonPose->m_qAdditionalRotPos[boneIndex].t.SetLerp(pSkeletonPose->m_qAdditionalRotPos[boneIndex].t, additionalRotPos[boneIndex].t, interpolateFraction);
		}
	}
	else
	{
		// Usually we don't want to blend, we want the exact settings as they are in the fsq to preserve the subtleties of the animation.
		for (int boneIndex = 0, boneCount = pSkeletonPose->GetJointCount(); boneIndex < boneCount; ++boneIndex)
			pSkeletonPose->m_qAdditionalRotPos[boneIndex] = additionalRotPos[boneIndex];
	}*/

	//IVO: make sure that all facial movements are relative to the parent (in this case the head)
	for (uint32 i=1; i<numJoints; i++)
	{
		int32 p=pSkeletonPose->m_parrModelJoints[i].m_idxParent;
		pSkeletonPose->m_qAdditionalRotPos[i].t = !pModelJoint[p].m_DefaultAbsPose.q * pSkeletonPose->m_FaceAnimPosSmooth[i];
	}

	// TEMPORARY HACK TO MAKE HEADS WORK
	pSkeletonPose->m_bHackForceAbsoluteUpdate = true;
}

//////////////////////////////////////////////////////////////////////////
void CFacialModel::ApplyAttachmentEffector( CSkinInstance *pCharacter,CFacialEffector *pEffector,float fWeight )
{
	const char *sAttachmentName = pEffector->GetParamString( EFE_PARAM_BONE_NAME );
	
	IAttachment *pAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByName( sAttachmentName );
	if (pAttachment)
	{
		Ang3 vRot = Ang3(pEffector->GetParamVec3( EFE_PARAM_BONE_ROT_AXIS ));
		vRot = vRot * fWeight;

		CAttachment *pCAttachment = (CAttachment*)pAttachment;
		Quat q;
		q.SetRotationXYZ( vRot );
		pCAttachment->m_additionalRotation = pCAttachment->m_additionalRotation * q;
		m_bAttachmentChanged = true;
	}
	else
	{
		// No such attachment, look into the parent.
		CAttachment* pMasterAttachment = pCharacter->m_pSkinAttachment;
		if (pMasterAttachment) 
		{
			uint32 type = pMasterAttachment->GetType();
			if (pMasterAttachment->GetType() == CA_SKIN)
			{
				// Apply on parent bones.
				CCharInstance* pCharacter = pMasterAttachment->m_pAttachmentManager->m_pSkelInstance;
				IAttachment *pAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByName( sAttachmentName );
				if (pAttachment)
				{
					Ang3 vRot = Ang3(pEffector->GetParamVec3( EFE_PARAM_BONE_ROT_AXIS ));
					vRot = vRot * fWeight;

					CAttachment *pCAttachment = (CAttachment*)pAttachment;
					Quat q;
					q.SetRotationXYZ( vRot );
					pCAttachment->m_additionalRotation = pCAttachment->m_additionalRotation * q;
					m_bAttachmentChanged = true;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFacialModel::ApplyBoneEffector( CCharInstance* pCharacter,CFacialEffector *pEffector,float fWeight,QuatT* additionalRotPos )
{
	const char *sBoneName = pEffector->GetParamString( EFE_PARAM_BONE_NAME );

	CAttachment* pMasterAttachment = pCharacter->m_pSkinAttachment;
	if (pMasterAttachment) 
	{
		uint32 type = pMasterAttachment->GetType();
		if (pMasterAttachment->GetType() == CA_SKIN)
		{
			// Apply on parent bones.
			CCharInstance* pCharacter = pMasterAttachment->m_pAttachmentManager->m_pSkelInstance;
			int nBoneId = pCharacter->m_SkeletonPose.GetJointIDByName(sBoneName);
			if (nBoneId >= 0)
			{
				Vec3 vPos = pEffector->GetParamVec3( EFE_PARAM_BONE_POS_AXIS );
				Ang3 vRot = Ang3(pEffector->GetParamVec3( EFE_PARAM_BONE_ROT_AXIS ));
				vRot = vRot * fWeight;
				vPos = vPos * fWeight;


				Quat q;
				q.SetRotationXYZ( vRot );
				additionalRotPos[nBoneId].q = additionalRotPos[nBoneId].q * q;
				additionalRotPos[nBoneId].t += vPos;
			}
			return;
		}
	}

	int nBoneId = pCharacter->m_SkeletonPose.GetJointIDByName(sBoneName);
	if (nBoneId >= 0)
	{
		CSkeletonPose* pSkeletonPose = 	&pCharacter->m_SkeletonPose;
			Vec3 vPos = pEffector->GetParamVec3( EFE_PARAM_BONE_POS_AXIS );
			Ang3 vRot = Ang3(pEffector->GetParamVec3( EFE_PARAM_BONE_ROT_AXIS ));
			vRot = vRot * fWeight;
			vPos = vPos * fWeight;

			Quat q;
			q.SetRotationXYZ( vRot );
			if (pSkeletonPose->m_LookIK.m_AllowAdditionalTransforms)
			{
				additionalRotPos[nBoneId].q = additionalRotPos[nBoneId].q * q;
				additionalRotPos[nBoneId].t += vPos;
			}
	}
	else
	{
		// No such attachment.
	}
}

size_t CFacialModel::SizeOfThis()
{
	size_t nSize(sizeof(CFacialModel));
	nSize += sizeofVector(m_effectors);
	nSize += sizeofVector(m_faceStateIndexToMorphTargetId);
	nSize += sizeofVector(m_morphTargets);
	
	return nSize;
}