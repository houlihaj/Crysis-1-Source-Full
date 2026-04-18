//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:Skeleton.cpp
//  Implementation of Skeleton class (Inverse Kinematics)
//
//	History:
//	March 18, 2005: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <I3DEngine.h>
#include <IRenderAuxGeom.h>
#include "CharacterInstance.h"
#include "Model.h"
#include "ModelSkeleton.h"
#include "CharacterManager.h"



//*****************************************************************************
void CSkeletonPose::CCDInitIKBuffer(QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK)
{
	if (m_pSkeletonAnim->m_TrackViewExclusive)
		return;
	uint32	numJoints  = m_AbsolutePose.size();
	for( uint32 i=0; i<numJoints; i++ )
	{
		pRelativeQuatIK[i] = m_RelativePose[i];
		pAbsoluteQuatIK[i] = m_AbsolutePose[i];
	}
}

int32* CSkeletonPose::CCDInitIKChain(int32 sCCDJoint,int32 eCCDJoint)
{

	if (m_pSkeletonAnim->m_TrackViewExclusive)
		return 0;
	if (sCCDJoint<0)	return 0;
	if (eCCDJoint<0)	return 0;

	int32 m_arrChain[0x400];
	int32 pSJoint = sCCDJoint;	
	int32 pEJoint = eCCDJoint;	

	uint32 c;
	for (c=0; c<0x400; )
	{
		m_arrChain[c] = m_parrModelJoints[pEJoint].m_idx;
		pEJoint       = m_parrModelJoints[pEJoint].m_idxParent;
		c++;
		if (pEJoint==pSJoint) 
			break; 
	}
	m_arrChain[c]=m_parrModelJoints[pSJoint].m_idx;

	uint32 d=c+1;
	m_arrCCDChain.resize(d);
	for (uint32 i=0; i<d; i++)
	{
		m_arrCCDChain[i]=m_arrChain[c];
		c--;
	}

	return 0;
}


//---------------------------------------------------------------------
//---------------------------------------------------------------------
//---------------------------------------------------------------------
void CSkeletonPose::CCDLockHunterFoot(int32 idx,QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK) 
{

	int32 idx0,idx1,idx2,idx3;

	Quat Legs[3];
	Quat difquat;
	Vec3 OldIkPos,NewIkPos;
	Vec3 Distance,pDistance;

	int32 id0 = idx;
	int32 id1 = m_parrModelJoints[id0].m_idxParent;
	int32 id2 = m_parrModelJoints[id1].m_idxParent;
	Legs[0]=m_AbsolutePose[id0].q;
	Legs[1]=m_AbsolutePose[id1].q;
	Legs[2]=m_AbsolutePose[id2].q;
	int32 numCCDJoints = m_arrCCDChain.size();
	idx0=m_arrCCDChain[numCCDJoints-1];
	idx1=m_arrCCDChain[numCCDJoints-2];
	idx2=m_arrCCDChain[numCCDJoints-3];
	idx3=m_arrCCDChain[numCCDJoints-4];
	OldIkPos = pAbsoluteQuatIK[idx2].t;
	NewIkPos = m_AbsolutePose[id2].t;
	pAbsoluteQuatIK[idx0] = m_AbsolutePose[id0];
	pAbsoluteQuatIK[idx1] = m_AbsolutePose[id1];
	pAbsoluteQuatIK[idx2] = m_AbsolutePose[id2];
	pRelativeQuatIK[idx0] =	pAbsoluteQuatIK[idx1].GetInverted() * pAbsoluteQuatIK[idx0];
	pRelativeQuatIK[idx1] =	pAbsoluteQuatIK[idx2].GetInverted() * pAbsoluteQuatIK[idx1];
	pRelativeQuatIK[idx2] =	pAbsoluteQuatIK[idx3].GetInverted() * pAbsoluteQuatIK[idx2];

	Distance	= NewIkPos-OldIkPos;
	pDistance	=	Distance/f32(numCCDJoints-5);
	for (int32 i=numCCDJoints-4; i!=-1; i--)
	{
		int32 idx = m_arrCCDChain[i];
		pAbsoluteQuatIK[idx].t += Distance;
		Distance -= pDistance;
	}
	for (int32 c=0; c<numCCDJoints; c++)
	{
		int32 i = m_arrCCDChain[c];
		int32 p = m_parrModelJoints[i].m_idxParent;	assert(p>=0);
		pRelativeQuatIK[i] = pAbsoluteQuatIK[p].GetInverted() * pAbsoluteQuatIK[i];

		f32 length	=	pRelativeQuatIK[i].t.GetLength();
		Vec3 normal	=	pRelativeQuatIK[i].t/length;
		difquat.SetRotationV0V1(Vec3(1,0,0),normal); 
		pRelativeQuatIK[i].t = Vec3(1,0,0)*length;
		pRelativeQuatIK[i].q = pRelativeQuatIK[i].q*difquat;
		pAbsoluteQuatIK[i]		= pAbsoluteQuatIK[p] * pRelativeQuatIK[i];
		pAbsoluteQuatIK[i].q.Normalize();
	}
}


//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------
void CSkeletonPose::CCDRotationSolver(const Vec3& vIKPos,f32 fThreshold,f32 StepSize, uint32 iTry,const Vec3& dir,QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK,Quat* pQuats)
{

	if (m_pSkeletonAnim->m_TrackViewExclusive)
		return;

	if (m_bFullSkeletonUpdate==0)
		return;

	if (m_pSkeletonAnim->m_IsAnimPlaying==0)
		return;

	Vec3 PositionCCD=m_pInstance->m_PhysEntityLocation.GetInverted()*vIKPos;

	uint32 m_UpdateSteps=0;
	Quat qrel0,qrel1,qrel2;

	uint32	stop=1; 
	int32 numCCDJoints2 = m_arrCCDChain.size();
	int32 sCCDJoint	=	m_arrCCDChain[0];
	int32 eCCDJoint	=	m_arrCCDChain[numCCDJoints2-1];

	Vec3	FKPosition	=	pAbsoluteQuatIK[eCCDJoint].t;					// Position of end effector
	f32		fDistance		= (FKPosition-PositionCCD).GetLength();	// What do we have to do to reach that?
	if (fDistance>50.0f)
	{
		const char* mname = m_pInstance->GetFilePath();
		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,mname,	"CryAnimation: CCD-IK request out of range: IK-Pos: %f %f %f   Dist:%30.20f",vIKPos.x,vIKPos.y,vIKPos.z, fDistance);
	}

	int32	iJointIterator	= numCCDJoints2-2;
	int32 useConstraints	= (int32)(dir|dir);

	// Cyclic Coordinate Descent
	do
	{
		Vec3	vecEnd				=	pAbsoluteQuatIK[eCCDJoint].t;			// Position of end effector
		int32 linkidx				= m_arrCCDChain[iJointIterator];
		Vec3	vecLink				=	pAbsoluteQuatIK[linkidx].t;	// Position of current node
		f32		fError				= (vecEnd-PositionCCD).GetLength();								// What do we have to do to reach that?
		Vec3	vecLinkTarget	= (PositionCCD-vecLink).GetNormalized();					// Vector current link -> target
		Vec3	vecLinkEnd		= (vecEnd-vecLink).GetNormalized();								// Vector current link -> current effector position

		if (useConstraints)
			CCDDirConstraint(dir,pRelativeQuatIK,pAbsoluteQuatIK);
		else
			CCDQuatConstraint(pQuats,pRelativeQuatIK,pAbsoluteQuatIK);

		qrel0.SetRotationV0V1(vecLinkEnd,vecLinkTarget); 
		assert((fabs_tpl(1-(qrel0|qrel0)))<0.001);   //check if unit-quaternion
		if( qrel0.w < 0 ) { qrel0=-qrel0;	} 

		f32 t	=	0.05f*(qrel0.w+1.0f)*StepSize;

		qrel2.w		= qrel0.w  *t + (1.0f-t);
		qrel2.v.x = qrel0.v.x*t;
		qrel2.v.y = qrel0.v.y*t;
		qrel2.v.z = qrel0.v.z*t;
		qrel2.Normalize();

		//calculate new relative IK-orientation 
		//	SJoint* pParent = m_arrJoints[linkidx].m_pqqqqParent;	assert(pParent);
		int32 parent = m_parrModelJoints[linkidx].m_idxParent;	assert(parent>=0);
		pRelativeQuatIK[linkidx].q = !pAbsoluteQuatIK[parent].q * qrel2 * pAbsoluteQuatIK[linkidx].q;
		pRelativeQuatIK[linkidx].q.Normalize();

		//calculate new absolute IK-orientation 
		int32 c = iJointIterator;
		while( c<numCCDJoints2 )
		{
			int32 idx = m_arrCCDChain[c];
			int32 par = m_parrModelJoints[idx].m_idxParent;	assert(par>=0);
			pAbsoluteQuatIK[idx]	= pAbsoluteQuatIK[par]*pRelativeQuatIK[idx];
			c++;
		}

		iJointIterator--;	//Next link
		if( iJointIterator == 0 )				
			iJointIterator = numCCDJoints2-2;

		m_UpdateSteps++;
		if (m_UpdateSteps>=iTry) stop=0; 
		if (fError < fThreshold) stop=0; 

	} while(stop);

}

//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------

void CSkeletonPose::CCDTranslationSolver(const Vec3& pos,QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK)
{

	if (m_pSkeletonAnim->m_TrackViewExclusive)
		return;

	if (m_bFullSkeletonUpdate==0)
		return;

	if (m_pSkeletonAnim->m_IsAnimPlaying==0)
		return;

	Vec3 PositionCCD	=	m_pInstance->m_PhysEntityLocation.GetInverted()*pos;

	int32 numCCDJoints	= m_arrCCDChain.size();
	int32 eCCDJoint			=	m_arrCCDChain[numCCDJoints-1];

	Vec3 absEndEffector =	pAbsoluteQuatIK[eCCDJoint].t;			// Position of end effector
	Vec3 Distance		= PositionCCD-absEndEffector; //f32(numCCDJoints);
	Vec3 pDistance	=	Distance/f32(numCCDJoints-2);



	int32 idx = m_arrCCDChain[numCCDJoints-1];
	pAbsoluteQuatIK[idx].t += Distance;
	for (int32 i=numCCDJoints-2; i!=-1; i--)
	{
		idx = m_arrCCDChain[i];
		pAbsoluteQuatIK[idx].t += Distance;
		Distance -= pDistance;
	}

	Quat difquat;
	for (int32 c=0; c<numCCDJoints; c++)
	{
		int32 i=m_arrCCDChain[c];
		int32 p = m_parrModelJoints[i].m_idxParent;	assert(p>=0);
		pRelativeQuatIK[i] = pAbsoluteQuatIK[p].GetInverted() * pAbsoluteQuatIK[i];

		Vec3 normal = Vec3(1,0,0);
		f32 length	=	pRelativeQuatIK[i].t.GetLength();
		if (length)
			normal	=	pRelativeQuatIK[i].t/length;

		difquat.SetRotationV0V1(Vec3(1,0,0),normal); 
		pRelativeQuatIK[i].t = Vec3(1,0,0)*length;
		pRelativeQuatIK[i].q = pRelativeQuatIK[i].q*difquat;
		pAbsoluteQuatIK[i]		= pAbsoluteQuatIK[p] * pRelativeQuatIK[i];
		pAbsoluteQuatIK[i].q.Normalize();
	}
}

//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
void CSkeletonPose::CCDDirConstraint(const Vec3& dir,QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK)
{

	int32 numCCDJoints	= m_arrCCDChain.size();
	int32 eCCDJoint			=	m_arrCCDChain[numCCDJoints-1];

	int32 id0 = eCCDJoint;
	int32 id1 = m_parrModelJoints[id0].m_idxParent;
	int32 id2 = m_parrModelJoints[id1].m_idxParent;;
	int32 id3 = m_parrModelJoints[id2].m_idxParent;;
	int32 id4 = m_parrModelJoints[id3].m_idxParent;;
	int32 id5 = m_parrModelJoints[id4].m_idxParent;;
	int32 id6 = m_parrModelJoints[id5].m_idxParent;;
	pRelativeQuatIK[id0].q.SetIdentity();
	pRelativeQuatIK[id1].q.SetIdentity();
	pRelativeQuatIK[id2].q.SetIdentity();
	pRelativeQuatIK[id3].q.SetIdentity();
	pRelativeQuatIK[id4].q.SetIdentity();

	//calculate new absolute IK-orientation 
	int c=numCCDJoints-4;
	while( c<numCCDJoints )
	{
		int32 idx=m_arrCCDChain[c];
		int32 par = m_parrModelJoints[idx].m_idxParent;	assert(par>=0);
		pAbsoluteQuatIK[idx]	= pAbsoluteQuatIK[par]*pRelativeQuatIK[idx];
		c++;
	}

	Quat nquat,disp;
	Vec3 t0 = pAbsoluteQuatIK[id4].t;
	Vec3 t1 = pAbsoluteQuatIK[id3].t;
	Vec3 lv = (t1-t0).GetNormalized();
	nquat.SetRotationV0V1(lv,dir);

	Quat q5 =  pRelativeQuatIK[id5].q;
	Quat q4 =  pRelativeQuatIK[id4].q;
	Quat q3 = !pAbsoluteQuatIK[id4].q*nquat*pAbsoluteQuatIK[id3].q;
	Quat q2 =  pRelativeQuatIK[id2].q;
	Quat q1 =  pRelativeQuatIK[id1].q;

	Quat final=(q5*0.20f)%(q4*0.20f)%(q3*0.20f)%(q2*0.20f)%(q1*0.20f);
	final.Normalize();

	pRelativeQuatIK[id5].q=final;
	pRelativeQuatIK[id4].q=final;
	pRelativeQuatIK[id3].q=final;
	pRelativeQuatIK[id2].q=final;
	pRelativeQuatIK[id1].q=final;

	c=numCCDJoints-6;
	while( c<numCCDJoints )
	{
		int32 idx=m_arrCCDChain[c];
		int32 par = m_parrModelJoints[idx].m_idxParent;	assert(par>=0);
		pAbsoluteQuatIK[idx]	= pAbsoluteQuatIK[par]*pRelativeQuatIK[idx];
		c++;
	}

}


//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
void CSkeletonPose::CCDQuatConstraint(const Quat* pQuats,QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK)
{
	if (pQuats==0)
		return;

	int32 numCCDJoints	= m_arrCCDChain.size();
	int32 eCCDJoint			=	m_arrCCDChain[numCCDJoints-1];

	int32 id0 = eCCDJoint;
	int32 id1 = m_parrModelJoints[id0].m_idxParent;
	int32 id2 = m_parrModelJoints[id1].m_idxParent;
	int32 id3 = m_parrModelJoints[id2].m_idxParent;
	int32 id4 = m_parrModelJoints[id3].m_idxParent;
	int32 id5 = m_parrModelJoints[id4].m_idxParent;

	pRelativeQuatIK[id5].q=pQuats[5];
	pRelativeQuatIK[id4].q=pQuats[4];
	pRelativeQuatIK[id3].q=pQuats[3];
	pRelativeQuatIK[id2].q=pQuats[2];
	pRelativeQuatIK[id1].q=pQuats[1];
	pRelativeQuatIK[id0].q=pQuats[0];

	int32 c=numCCDJoints-6;
	while( c<numCCDJoints )
	{
		int32 idx=m_arrCCDChain[c];
		int32 par = m_parrModelJoints[idx].m_idxParent;	assert(par>=0);
		pAbsoluteQuatIK[idx]	= pAbsoluteQuatIK[par]*pRelativeQuatIK[idx];
		c++;
	}

}



//------------------------------------------------------------------
//----                      final bone update                  -----
//------------------------------------------------------------------
void CSkeletonPose::CCDUpdateSkeleton(QuatT* pRelativeQuatIK,QuatT* pAbsoluteQuatIK)
{
	if (m_pSkeletonAnim->m_TrackViewExclusive)
		return;

	if (m_bFullSkeletonUpdate==0)
		return;

	if (m_pSkeletonAnim->m_IsAnimPlaying==0)
		return;

	uint32 numJoints = m_AbsolutePose.size();
	for(uint32 i=0; i<numJoints; i++)
	{
		pAbsoluteQuatIK[i] = pRelativeQuatIK[i];
		int32 p = m_parrModelJoints[i].m_idxParent;
		if(p>=0)
			pAbsoluteQuatIK[i] = pAbsoluteQuatIK[p] * pRelativeQuatIK[i];

		pAbsoluteQuatIK[i].q.Normalize();
	}

	for(uint32 i=0; i<numJoints; i++)
	{
		m_RelativePose[i] = pRelativeQuatIK[i];
		m_AbsolutePose[i] =	pAbsoluteQuatIK[i];
	}

	//--------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------
	int32 numCCDJoints	= m_arrCCDChain.size();
	int32 eCCDJoint			=	m_arrCCDChain[numCCDJoints-1];

	static Ang3 angle=Ang3(ZERO);
	angle.x += 0.1f;
	angle.y += 0.01f;
	angle.z += 0.001f;
	AABB sAABB1 = AABB(Vec3(-0.4f,-0.4f,-0.4f),Vec3(+0.4f,+0.4f,+0.4f));
	AABB sAABB2 = AABB(Vec3(-0.3f,-0.3f,-0.3f),Vec3(+0.3f,+0.3f,+0.3f));
	OBB obb1 =	OBB::CreateOBBfromAABB( Matrix33::CreateRotationXYZ(angle), sAABB1 );
	OBB obb2 =	OBB::CreateOBBfromAABB( Matrix33::CreateRotationXYZ(angle), sAABB2 );
	//Vec3 r=m_arrJoints[sCCDJoint].pAbsoluteQuatIK.t;
	//Vec3 e=m_arrJoints[eCCDJoint].pAbsoluteQuatIK.t;
	//g_pAuxGeom->DrawOBB(obb1,r,1,RGBA8(0x1f,0xff,0x1f,0xff),eBBD_Extremes_Color_Encoded);
	//g_pAuxGeom->DrawOBB(obb1,e,1,RGBA8(0xff,0x3f,0x3f,0xff),eBBD_Extremes_Color_Encoded);

	Vec3	absEndEffector 	=	m_pInstance->m_PhysEntityLocation * pAbsoluteQuatIK[eCCDJoint].t;			// Position of end effector
	//g_pAuxGeom->DrawOBB(obb2,absEndEffector,1,RGBA8(0xff,0xff,0x3f,0xff),eBBD_Extremes_Color_Encoded);
}




//--------------------------------------------------------------------------------------
//--------                  Look IK for the Hunter                              --------
//--------------------------------------------------------------------------------------
void CSkeletonPose::LookIKHunter( uint32 LIKFlag, const QuatT& rPhysLocationNext, const QuatTS& rAnimLocationNext  )
{

	if (m_pSkeletonAnim->m_TrackViewExclusive)
		return;

	int32 IM_HunterRootIdx				= m_pModelSkeleton->m_IdxArray[eIM_HunterRootIdx];
	if (IM_HunterRootIdx < 0) return;

	int32 IM_HunterFrontLegL01Idx = m_pModelSkeleton->m_IdxArray[eIM_HunterFrontLegL01Idx];
	if (IM_HunterFrontLegL01Idx < 0) return;
	int32 IM_HunterFrontLegL12Idx = m_pModelSkeleton->m_IdxArray[eIM_HunterFrontLegL12Idx];
	if (IM_HunterFrontLegL12Idx < 0) return;

	int32 IM_HunterFrontLegR01Idx = m_pModelSkeleton->m_IdxArray[eIM_HunterFrontLegR01Idx];
	if (IM_HunterFrontLegR01Idx < 0) return;
	int32 IM_HunterFrontLegR12Idx = m_pModelSkeleton->m_IdxArray[eIM_HunterFrontLegR12Idx];
	if (IM_HunterFrontLegR12Idx < 0) return;

	int32 IM_HunterBackLegL01Idx	= m_pModelSkeleton->m_IdxArray[eIM_HunterBackLegL01Idx];
	if (IM_HunterBackLegL01Idx < 0)  return;
	int32 IM_HunterBackLegL13Idx	= m_pModelSkeleton->m_IdxArray[eIM_HunterBackLegL13Idx];
	if (IM_HunterBackLegL13Idx < 0)  return;

	int32 IM_HunterBackLegR01Idx	= m_pModelSkeleton->m_IdxArray[eIM_HunterBackLegR01Idx];
	if (IM_HunterBackLegR01Idx < 0)  return;
	int32 IM_HunterBackLegR13Idx	= m_pModelSkeleton->m_IdxArray[eIM_HunterBackLegR13Idx];
	if (IM_HunterBackLegR13Idx < 0)  return;

	QuatT ACOffset = QuatT( rPhysLocationNext.GetInverted()*rAnimLocationNext);

	uint32 numJoints = m_AbsolutePose.size();
	QuatT* pRelativeQuatIK = (QuatT*)alloca( numJoints*sizeof(QuatT) );
	QuatT* pAbsoluteQuatIK = (QuatT*)alloca( numJoints*sizeof(QuatT) );

	Vec3 LocalLookAtTarget = m_LookIK.m_LookIKTargetSmooth-rPhysLocationNext.t;
	Vec3 CamPos	= LocalLookAtTarget * rPhysLocationNext.q;

	Vec3 RootPos			= m_AbsolutePose[IM_HunterRootIdx].t;
	Vec3 LookDir			= (CamPos-RootPos).GetNormalized();

	f32 time=0.50f*fabsf(m_pInstance->m_fDeltaTime);


	// Craig's VS2-change - make sure that we lock the look-ik into acceptable limits,
	// instead of discarding it to avoid the LBeffect
	Vec3 tempLookDir = LookDir*ACOffset.q; 
	Vec3 goodDir = Vec3(0,1,0);
	static const float MAX_X = tanf(DEG2RAD(30.0f));
	static const float MAX_Z = tanf(DEG2RAD(15.0f));
	if (fabsf(tempLookDir.y) < 0.001f)
	{
		goodDir.x = CLAMP( tempLookDir.x, -MAX_X, MAX_X );
		goodDir.z = CLAMP( tempLookDir.z, -MAX_Z, MAX_Z );
	}
	else
	{
		goodDir.x = CLAMP( tempLookDir.x / fabsf(tempLookDir.y), -MAX_X, MAX_X );
		goodDir.z = CLAMP( tempLookDir.z / fabsf(tempLookDir.y), -MAX_Z, MAX_Z );
	}

	LookDir = ACOffset.q*goodDir.GetNormalized();

	Vec3 LookDirPlane = Vec3(LookDir.x,LookDir.y,0).GetNormalized();
	f32 dot	=	Vec3(0,1,0)|LookDir;


	if (m_LookIK.m_LookIKFadeout) 
	{
		//avoid Linda-Blair effect
		m_LookIK.m_LookIKFadeout=1;
		m_LookIK.m_LookIKBlend-=time;
		if (m_LookIK.m_LookIKBlend<0.0f)	
		{
			m_LookIK.m_LookIKFadeout=0;
			m_LookIK.m_LookIKBlend=0.0f;
			//	return;
		}
	}
	else
	{
		if (LIKFlag)
		{
			m_LookIK.m_LookIKBlend+=time;
			if (m_LookIK.m_LookIKBlend>1.0f)	m_LookIK.m_LookIKBlend=1.0f;
		}
		else
		{
			m_LookIK.m_LookIKBlend-=time;
			if (m_LookIK.m_LookIKBlend<0.0f)	m_LookIK.m_LookIKBlend=0.0f;
		}
	}



	//------------------------------------------------------------------------------
	//------------------------------------------------------------------------------
	//------------------------------------------------------------------------------
	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	g_pAuxGeom->SetRenderFlags( renderFlags );

	f32 IKBlend = clamp(m_LookIK.m_LookIKBlend,0.0f,1.0f);
	//	IKBlend = IKBlend * IKBlend * ( 3 - 2 * IKBlend );
	IKBlend -= 0.5f;
	IKBlend = 0.5f+IKBlend/(0.5f+2.0f*IKBlend*IKBlend);
	//	IKBlend = IKBlend * IKBlend * ( 3 - 2 * IKBlend );
	//	IKBlend = IKBlend * IKBlend * ( 3 - 2 * IKBlend );
	f32  lr	=	min(1.0f,fabsf((Vec3(0,1,0)|LookDirPlane)-1.0f)*IKBlend);

	Vec3 FrontLegL	= rPhysLocationNext*m_AbsolutePose[IM_HunterFrontLegL12Idx].t;
	Vec3 FrontLegR	= rPhysLocationNext*m_AbsolutePose[IM_HunterFrontLegR12Idx].t;
	Vec3 BackLegL		= rPhysLocationNext*m_AbsolutePose[IM_HunterBackLegL13Idx].t;
	Vec3 BackLegR		= rPhysLocationNext*m_AbsolutePose[IM_HunterBackLegR13Idx].t;

	Vec3 MotionLookDir = m_AbsolutePose[IM_HunterRootIdx].q.GetColumn0();
	//LookDir.SetSlerp(MotionLookDir,LookDir,IKBlend);



	Quat LookAtQuat = Quat::CreateRotationV0V1( MotionLookDir,LookDir );

	CCDInitIKBuffer(pRelativeQuatIK,pAbsoluteQuatIK);

	Quat quat0 = pRelativeQuatIK[IM_HunterRootIdx].q;

	pAbsoluteQuatIK[IM_HunterRootIdx].q = LookAtQuat*pAbsoluteQuatIK[IM_HunterRootIdx].q;
	pAbsoluteQuatIK[IM_HunterRootIdx].t.z += 4.0f*lr+0.5f;
	int32 parent = m_parrModelJoints[IM_HunterRootIdx].m_idxParent;	assert(parent>=0);
	pAbsoluteQuatIK[parent].q.Normalize();
	pRelativeQuatIK[IM_HunterRootIdx] = pAbsoluteQuatIK[parent].GetInverted() * pAbsoluteQuatIK[IM_HunterRootIdx];

	Quat quat1 = pRelativeQuatIK[IM_HunterRootIdx].q;
	pRelativeQuatIK[IM_HunterRootIdx].q.SetNlerp(quat0,quat1,IKBlend);

	for(uint32 i=0; i<numJoints; i++)
	{
		pAbsoluteQuatIK[i] = pRelativeQuatIK[i];
		int32 p = m_parrModelJoints[i].m_idxParent;
		if(p>=0)
			pAbsoluteQuatIK[i] = pAbsoluteQuatIK[p] * pRelativeQuatIK[i];

		pAbsoluteQuatIK[i].q.Normalize();
	}


	/*
	extern float g_YLine;
	float fColor[4] = {1,1,1,1};
	g_YLine+=26.0f;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"angle: %f  IKBlend:%f", lr,IKBlend );	g_YLine+=16.0f;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_TrackViewExclusive: %d",m_TrackViewExclusive );	g_YLine+=16.0f;
	g_YLine+=10;
	*/


	//---------------------------------------------------------------------------------

	CCDInitIKChain(IM_HunterFrontLegL01Idx,IM_HunterFrontLegL12Idx);
	CCDRotationSolver(FrontLegL,0.02f,0.20f,200,Vec3(0,0,-1),pRelativeQuatIK,&pAbsoluteQuatIK[0]);
	CCDTranslationSolver(FrontLegL,pRelativeQuatIK,&pAbsoluteQuatIK[0]);
	CCDLockHunterFoot(IM_HunterFrontLegL12Idx,pRelativeQuatIK,&pAbsoluteQuatIK[0]); 

	CCDInitIKChain(IM_HunterFrontLegR01Idx,IM_HunterFrontLegR12Idx);
	CCDRotationSolver(FrontLegR,0.02f,0.20f,200,Vec3(0,0,-1),pRelativeQuatIK,&pAbsoluteQuatIK[0]);
	CCDTranslationSolver(FrontLegR,pRelativeQuatIK,&pAbsoluteQuatIK[0]);
	CCDLockHunterFoot(IM_HunterFrontLegR12Idx,pRelativeQuatIK,&pAbsoluteQuatIK[0]); 


	CCDInitIKChain(IM_HunterBackLegL01Idx,IM_HunterBackLegL13Idx);
	CCDRotationSolver(BackLegL,0.02f,0.20f,100,Vec3(0,0,-1),pRelativeQuatIK,&pAbsoluteQuatIK[0]);
	CCDTranslationSolver(BackLegL,pRelativeQuatIK,&pAbsoluteQuatIK[0]);
	CCDLockHunterFoot(IM_HunterBackLegL13Idx,pRelativeQuatIK,&pAbsoluteQuatIK[0]); 

	CCDInitIKChain(IM_HunterBackLegR01Idx,IM_HunterBackLegR13Idx);
	CCDRotationSolver(BackLegR,0.02f,0.20f,100,Vec3(0,0,-1),pRelativeQuatIK,&pAbsoluteQuatIK[0]);
	CCDTranslationSolver(BackLegR,pRelativeQuatIK,&pAbsoluteQuatIK[0]);
	CCDLockHunterFoot(IM_HunterBackLegR13Idx,pRelativeQuatIK,&pAbsoluteQuatIK[0]); 

	CCDUpdateSkeleton(pRelativeQuatIK,&pAbsoluteQuatIK[0]);


}
