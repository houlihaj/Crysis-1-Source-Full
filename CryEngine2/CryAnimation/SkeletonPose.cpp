//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:Skeleton.cpp
//  Implementation of Skeleton class (Forward Kinematics)
//
//	History:
//	January 12, 2005: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <I3DEngine.h>
#include <IRenderAuxGeom.h>

#include "CharacterManager.h"
#include "CharacterInstance.h"
#include "Model.h"
#include "ModelSkeleton.h"



//////////////////////////////////////////////////////////////////////////
// initialize the moving skeleton 
//////////////////////////////////////////////////////////////////////////

void CSkeletonPose::InitSkeletonPose(CCharInstance* pInstance, CSkeletonAnim* pSkeletonAnim)
{

	m_pInstance					=	pInstance;
	m_pSkeletonAnim			=	pSkeletonAnim;
	m_pModelSkeleton		=	pInstance->m_pModel->m_pModelSkeleton;
	m_parrModelJoints		=	&pInstance->m_pModel->m_pModelSkeleton->m_arrModelJoints[0];


	//usually not needed for skin-attachments
	uint32 numJoints		= m_pInstance->m_pModel->m_pModelSkeleton->m_arrModelJoints.size();
	m_RelativePose.resize(numJoints);
	m_AbsolutePose.resize(numJoints);
	m_arrControllerInfo.resize (numJoints);
	m_qAdditionalRotPos.resize(numJoints);
	m_JointMask.resize(numJoints);
	m_FaceAnimPosSmooth.resize(numJoints);
	m_FaceAnimPosSmoothRate.resize(numJoints);
	
	for (uint32 i=0; i<numJoints; i++)
		m_qAdditionalRotPos[i].q.SetIdentity();

	//------------------------------------------------------------


	m_nForceSkeletonUpdate=0;
	m_bAllAbsoluteJointsValid = false;
	m_bHackForceAbsoluteUpdate = false;
	m_LFootGroundAlign=0;
	m_RFootGroundAlign=0;
	m_enableFootGroundAlignment = (Console::GetInst().ca_GroundAlignment != 0);

	m_pPostProcessCallback0 = 0;
	m_pPostProcessCallbackData0 = 0;
	m_pPostProcessCallback1 = 0;
	m_pPostProcessCallbackData1 = 0;

	m_pPostPhysicsCallback = 0;
	m_pPostPhysicsCallbackData = 0;


	//---------------------------------------------------------------------
	//---                   physics                                    ----
	//---------------------------------------------------------------------
	m_pCharPhysics = 0;
	m_bHasPhysics = 0;
	m_timeStandingUp = -1.0f;
	m_ppBonePhysics = 0;
	m_timeLying = -1.0f;
	m_timeNoColl = -1.0f;
	m_timeStandingUp = -1.0f;
	m_fScale = 0.01f;
	m_bPhysicsAwake = 0;
	m_bPhysicsWasAwake = 1;
	m_nAuxPhys = 0;
	m_fPhysBlendTime = 1E6f;
	m_fPhysBlendMaxTime = 1.0f;
	m_frPhysBlendMaxTime = 1.0f;
	m_bPhysicsRelinquished = 0;
	m_iFnPSet = 0;
	m_vOffset(0,0,0);



	m_NewTempRenderMat.SetIdentity();
	m_vRenderOffset=Vec3(ZERO);
	m_AABB.min=Vec3(-2,-2,-2);	
	m_AABB.max=Vec3(+2,+2,+2);	
	m_Superimposed=0;

	SetDefaultPose();
}

//////////////////////////////////////////////////////////////////////////
// reset the bone to default position/orientation
// initializes the bones and IK limb pose
//////////////////////////////////////////////////////////////////////////
void CSkeletonPose::SetDefaultPose()
{
	SetDefaultPosePerInstance();	

	IAttachmentManager* pIAttachmentManager = m_pInstance->GetIAttachmentManager();
	uint32 numAttachments = pIAttachmentManager->GetAttachmentCount();
	for (uint32 i=0; i<numAttachments; i++)
	{
		CAttachment* pAttachment = (CAttachment*)pIAttachmentManager->GetInterfaceByIndex(i);
		IAttachmentObject* pIAttachmentObject = pAttachment->m_pIAttachmentObject;
		if (pIAttachmentObject)
		{
			if (pAttachment->GetType()==CA_SKIN)
				continue;
			if (pIAttachmentObject->GetAttachmentType() != IAttachmentObject::eAttachment_Character)
				continue;

			CCharInstance* pCharacterInstance = (CCharInstance*)pAttachment->m_pIAttachmentObject->GetICharacterInstance();
			pCharacterInstance->m_SkeletonPose.SetDefaultPosePerInstance();	
		}
	}

}


void CSkeletonPose::SetDefaultPosePerInstance()
{
	m_pSkeletonAnim->StopAnimationsAllLayers();

	m_bInstanceVisible=0;
	if(gEnv->pConsole->GetCVar("es_logDrawnActors")->GetIVal() != 0)
		CryLogAlways("SetDefaultPosePerInstance setting m_bFullSkeletonUpdate to 0");
	m_bFullSkeletonUpdate=0;

	m_Recoil.Init();
	m_LookIK.Init();
	m_AimIK.Init();
	m_FootAnchor.Init();
	m_arrPostProcess.resize(0);

	uint32 numJoints = m_AbsolutePose.size();

	if (numJoints)
	{
		for (uint32 i=0; i<numJoints; i++)
		{
			m_RelativePose[i]	= m_parrModelJoints[i].m_DefaultRelPose;
			m_AbsolutePose[i]	= m_parrModelJoints[i].m_DefaultAbsPose;
			m_qAdditionalRotPos[i].SetIdentity();
			m_JointMask[i]	= 0xFFFF; //activate all bones in all layers
			m_FaceAnimPosSmooth[i]	= Vec3(ZERO);
			m_FaceAnimPosSmoothRate[i]	= Vec3(ZERO);
		}

		int32 leye = m_pModelSkeleton->m_IdxArray[eIM_LeftEyeIdx];
		int32 reye = m_pModelSkeleton->m_IdxArray[eIM_RightEyeIdx];
		if (leye>0)
			m_RelativePose[leye].q.SetIdentity();
		if (reye>0)
			m_RelativePose[reye].q.SetIdentity();

		for (uint32 i=1; i<numJoints; i++)
		{
			m_AbsolutePose[i] = m_RelativePose[i];
			int32 p = m_parrModelJoints[i].m_idxParent;
			if (p>=0)
				m_AbsolutePose[i]	= m_AbsolutePose[p] * m_RelativePose[i];
			m_AbsolutePose[i].q.Normalize();
		}
	}

	if ( m_pCharPhysics != NULL )
		if (m_pInstance->m_pModel->m_ObjectType==CGA)
			m_pInstance->UpdatePhysicsCGA( 1, QuatTS(IDENTITY) );
		else if (!m_bPhysicsRelinquished)
			SynchronizeWithPhysicalEntity( m_pCharPhysics, Vec3(ZERO), Quat(IDENTITY), QuatT(IDENTITY), 0 );
}



void CSkeletonPose::InitPhysicsSkeleton()
{

	assert(!m_arrPhysicsJoints.size());
	uint32 numJoints	= m_pInstance->m_pModel->m_pModelSkeleton->m_arrModelJoints.size();

	m_arrPhysicsJoints.resize(numJoints);
	for (uint32 i=0; i<numJoints; i++)
	{
		m_arrPhysicsJoints[i].m_DefaultRelativeQuat	=	m_parrModelJoints[i].m_DefaultRelPose;
		m_arrPhysicsJoints[i].m_qRelPhysParent[0] = m_parrModelJoints[i].m_qDefaultRelPhysParent[0];
		m_arrPhysicsJoints[i].m_qRelPhysParent[1] = m_parrModelJoints[i].m_qDefaultRelPhysParent[1];
		
#if (EXTREME_TEST==1)
			uint32 valid1 = m_parrModelJoints[i].m_DefaultRelPose.IsValid();
			if (valid1==0)
			{
				assert(0);
				CryError("CryAnimation: m_DefaultRelQuat nr. %d is invalid in InitPhysicsSkeleton: %s",i, m_pInstance->GetFilePath());
			}
			uint32 valid2 = m_parrModelJoints[i].m_qDefaultRelPhysParent[0].IsValid();
			if (valid2==0)
			{
				assert(0);
				CryError("CryAnimation: m_qRelPhysParent[0] nr. %d is invalid in InitPhysicsSkeleton: %s",i, m_pInstance->GetFilePath());
			}
			uint32 valid3 = m_parrModelJoints[i].m_qDefaultRelPhysParent[1].IsValid();
			if (valid3==0)
			{
				assert(0);
				CryError("CryAnimation: m_qRelPhysParent[1] nr. %d is invalid in InitPhysicsSkeleton: %s",i, m_pInstance->GetFilePath());
			}
#endif

	}
}

void CSkeletonPose::InitCGASkeleton()
{
	uint32 numJoints	= m_pInstance->m_pModel->m_pModelSkeleton->m_arrModelJoints.size();

	m_arrCGAJoints.resize(numJoints);

	for (uint32 i=0; i<numJoints; i++)
	{
		m_arrCGAJoints[i].m_CGAObjectInstance		= m_parrModelJoints[i].m_CGAObject;
	}
}








int16 CSkeletonPose::GetJointIDByName (const char* szJointName) const
{
	return static_cast<int16>(m_pInstance->m_pModel->m_pModelSkeleton->m_JointMap.GetValue(szJointName));
}

int16 CSkeletonPose::GetParentIDByID (int32 nChildID) const
{
	int32 numJoints = m_AbsolutePose.size();
	if(nChildID>=0 && nChildID<numJoints)
		return m_parrModelJoints[nChildID].m_idxParent;
	assert(0);
	return -1;
}

uint32 CSkeletonPose::GetJointCRC32 (int32 nJointID) const
{
	int32 numJoints = m_AbsolutePose.size();
	if(nJointID>=0 && nJointID<numJoints)
		return m_parrModelJoints[nJointID].m_nJointCRC32;
	assert(0);
	return -1;
}

// Return name of bone from bone table, return zero id nId is out of range
const char* CSkeletonPose::GetJointNameByID(int32 nJointID) const
{
	int32 numJoints = m_AbsolutePose.size();
	if(nJointID>=0 && nJointID<numJoints)
		return m_parrModelJoints[nJointID].GetJointName();
	assert("GetJointNameByID - Index out of range!");
	return ""; // invalid bone id
}




IStatObj* CSkeletonPose::GetStatObjOnJoint(int32 nId)
{
	if (nId < 0 || nId >= (int)m_AbsolutePose.size())
	{
		assert(0);
		return NULL;
	}
	if (m_arrCGAJoints.size())
	{
		const CCGAJoint& joint = m_arrCGAJoints[nId];
		return joint.m_CGAObjectInstance;
	}

	return 0;
}

void CSkeletonPose::SetStatObjOnJoint(int32 nId, IStatObj* pStatObj)
{
	if (nId < 0 || nId >= (int)m_AbsolutePose.size())
	{
		assert(0);
		return;
	}

	assert(m_arrCGAJoints.size());
	// do not handle physicalization in here, use IEntity->SetStatObj instead
	CCGAJoint& joint = m_arrCGAJoints[nId];  
	joint.m_CGAObjectInstance = pStatObj;  

	if(joint.m_pRNTmpData)
		GetISystem()->GetI3DEngine()->FreeRNTmpData(&joint.m_pRNTmpData);
	assert(!joint.m_pRNTmpData);
}


void CSkeletonPose::SetPhysEntOnJoint(int32 nId, IPhysicalEntity *pPhysEnt)
{
	if (nId < 0 || nId >= (int)m_AbsolutePose.size())
	{
		assert(0);
		return;
	}
	if (!m_ppBonePhysics)
		memset(m_ppBonePhysics = new IPhysicalEntity*[m_AbsolutePose.size()], 0, m_AbsolutePose.size()*sizeof(IPhysicalEntity*));
	if (m_ppBonePhysics[nId])
		if (m_ppBonePhysics[nId]->Release()<=0)
			gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_ppBonePhysics[nId]);
	m_ppBonePhysics[nId] = pPhysEnt;
	if (pPhysEnt)
		pPhysEnt->AddRef();
}

int CSkeletonPose::GetPhysIdOnJoint(int32 nId)
{
	if (nId < 0 || nId >= (int)m_AbsolutePose.size())
	{
		assert(0);
		return -1;
	}

	if (m_arrCGAJoints.size())
	{
		CCGAJoint& joint = m_arrCGAJoints[nId];

		if (joint.m_qqqhasPhysics>=0)
			return joint.m_qqqhasPhysics;
	}
	
	return -1;
}

void CSkeletonPose::DestroyPhysics()
{
	int i;
	if (m_pCharPhysics)
		g_pIPhysicalWorld->DestroyPhysicalEntity(m_pCharPhysics);
	m_pCharPhysics = 0;
	if (m_ppBonePhysics)
	{
		for(i=0;i<(int)m_AbsolutePose.size();i++)	if (m_ppBonePhysics[i])
			m_ppBonePhysics[i]->Release(), g_pIPhysicalWorld->DestroyPhysicalEntity(m_ppBonePhysics[i]);
		delete[] m_ppBonePhysics;
		m_ppBonePhysics = 0;
	}
	
	for(i=0;i<m_nAuxPhys;i++) {
		g_pIPhysicalWorld->DestroyPhysicalEntity(m_auxPhys[i].pPhysEnt);
		delete[] m_auxPhys[i].pauxBoneInfo;
		delete[] m_auxPhys[i].pVtx;
	}
	m_nAuxPhys = 0;
	m_bPhysicsRelinquished=m_bAliveRagdoll = 0;	m_timeStandingUp = -1;
}

void CSkeletonPose::SetAuxParams(pe_params* pf)
{
	for(int32 i=0; i<m_nAuxPhys; i++)
		m_auxPhys[i].pPhysEnt->SetParams(pf);
}

void CSkeletonPose::UpdateJointControllers(CAnimation &anim)
{
	const CModelJoint *pModelJoint = GetModelJointPointer(0);
	uint32 numJoints=m_AbsolutePose.size();

	for (int i=0; i<1; i++)
	{
		SLocoGroup &lmg = i ? anim.m_LMG1 : anim.m_LMG0;

		for (int l=0; l<lmg.m_numAnims; l++)
		{
			lmg.m_jointControllers[l].resize(numJoints);

			for (uint32 j=0; j<numJoints; j++)
			{
				lmg.m_jointControllers[l][j] =
					g_AnimationManager.m_arrGlobalAnimations[lmg.m_nGlobalID[l]].GetControllerByJointID(pModelJoint[j].m_nJointCRC32);
			}
		}
	}
}







