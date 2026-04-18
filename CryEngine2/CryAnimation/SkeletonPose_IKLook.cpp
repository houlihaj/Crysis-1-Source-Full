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

#include "FacialAnimation/FacialInstance.h"

//--------------------------------------------------------------------------------------
//--------                     set Look-IK parameters                           --------
//--------------------------------------------------------------------------------------
void CSkeletonPose::SetLookIK(uint32 ik, f32 FOR, const Vec3& vLookAtTarget,const f32 *customBlends, bool allowAdditionalTransforms) 
{
	uint32 IsValid = vLookAtTarget.IsValid();
	if (IsValid==0)
	{
		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,m_pInstance->GetFilePath(),	"CryAnimation: Look Target Invalid (%f %f %f)",vLookAtTarget.x,vLookAtTarget.y,vLookAtTarget.z );
		g_pISystem->debug_LogCallStack();
	}

	//	float fColor[4] = {1,1,0,1};
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"LookIK: %d   %12.8f    Target:(%12.8f %12.8f %12.8f) ",ik,FOR, LookAtTarget.x, LookAtTarget.y, LookAtTarget.z );	g_YLine+=16.0f;
	m_LookIK.m_UseLookIK=ik;	
	m_LookIK.m_AllowAdditionalTransforms=allowAdditionalTransforms;
	m_LookIK.m_LookIKFOR=FOR;
	if (IsValid && ik)
		m_LookIK.m_LookIKTarget=vLookAtTarget;



	f32 t0=1.0f-m_AimIK.m_AimIKBlend;
	f32 t1=m_AimIK.m_AimIKBlend;

	//default values
	m_LookIK.m_lookIKBlends[0] = 0.04f*t0;
	m_LookIK.m_lookIKBlends[1] = 0.06f*t0;
	m_LookIK.m_lookIKBlends[2] = 0.08f*t0;
	m_LookIK.m_lookIKBlends[3] = 0.15f*t0;
	m_LookIK.m_lookIKBlends[4] = 0.60f*t0+0.25f*t1;

	if (customBlends)
	{
		cryMemcpy(m_LookIK.m_lookIKBlends,customBlends,sizeof(f32)*LOOKIK_BLEND_RATIOS);
		m_LookIK.m_lookIKBlends[0] = m_LookIK.m_lookIKBlends[0]*t0;
		m_LookIK.m_lookIKBlends[1] = m_LookIK.m_lookIKBlends[1]*t0;
		m_LookIK.m_lookIKBlends[2] = m_LookIK.m_lookIKBlends[2]*t0;
		m_LookIK.m_lookIKBlends[3] = m_LookIK.m_lookIKBlends[3]*t0;
		m_LookIK.m_lookIKBlends[4] = m_LookIK.m_lookIKBlends[4]*t0+0.25f*t1;
	}
}



//--------------------------------------------------------------------------------------
//--------                  Look IK                                             --------
//--------------------------------------------------------------------------------------
void CSkeletonPose::LookIK( const QuatT& rPhysLocationNext, uint32 LIKFlag )
{

#define BINOUT (1.0f/0.6f)

	if( Console::GetInst().ca_UseLookIK==0 || m_timeStandingUp>=0)
		return;

	if ( (m_pSkeletonAnim->m_IsAnimPlaying&0x1)==0)
		return;
	if (m_bInstanceVisible==0)
		return;

	int32 Spine0Idx	=	m_pModelSkeleton->m_IdxArray[eIM_Spine0Idx];
	int32 Spine1Idx	=	m_pModelSkeleton->m_IdxArray[eIM_Spine1Idx];
	int32 Spine2Idx	=	m_pModelSkeleton->m_IdxArray[eIM_Spine2Idx];
	int32 Spine3Idx	=	m_pModelSkeleton->m_IdxArray[eIM_Spine3Idx];
	int32 NeckIdx		=	m_pModelSkeleton->m_IdxArray[eIM_NeckIdx];
	int32 HeadIdx		= m_pModelSkeleton->m_IdxArray[eIM_HeadIdx];
	int32 ridx			= m_pModelSkeleton->m_IdxArray[eIM_RightEyeIdx];
	int32 lidx			= m_pModelSkeleton->m_IdxArray[eIM_LeftEyeIdx];

	//	if (Spine1Idx<0)	return;
	//	if (Spine2Idx<0)	return;
	//	if (Spine3Idx<0)	return;
	if (NeckIdx<0)		return;
	if (HeadIdx<0)		return;
	if (ridx<0)				return;
	if (lidx<0)				return;

	uint32 numJoints	= m_AbsolutePose.size();
	QuatT* pRelativeQuatIK = (QuatT*)alloca( numJoints*sizeof(QuatT) );
	QuatT* pAbsoluteQuatIK = (QuatT*)alloca( numJoints*sizeof(QuatT) );



	//------------------------------------------------------------------------
	//---    use the attachment position of the eye for precise Look-IK    ---
	//------------------------------------------------------------------------
	IAttachmentManager* pIAttachmentManager = m_pInstance->GetIAttachmentManager();   

	int32 alidx = pIAttachmentManager->GetIndexByName("eye_left");
	int32 aridx = pIAttachmentManager->GetIndexByName("eye_right");

	QuatT ql=QuatT(IDENTITY);
	QuatT qr=QuatT(IDENTITY);
	if (alidx>0 && aridx>0 )
	{
		IAttachment* pIAttachmentL = pIAttachmentManager->GetInterfaceByIndex(alidx);
		IAttachment* pIAttachmentR = pIAttachmentManager->GetInterfaceByIndex(aridx);
		ql = QuatT(pIAttachmentL->GetAttRelativeDefault());
		qr = QuatT(pIAttachmentR->GetAttRelativeDefault());
	}

	//-------------------------------------------------------------------------

	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	g_pAuxGeom->SetRenderFlags( renderFlags );

	
	//const char* mname = m_pInstance->m_pModel->GetFilePathCStr();
	//if ( strcmp(mname,"objects/characters/human/us/officer/officer.chr")==0 )
//	{
//	float fColor[4] = {1,1,0,1};
//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"LookIK: %d   %12.8f    Target:(%12.8f %12.8f %12.8f) ",m_UseLookIK,m_LookIKFOR, LookIKTarget.x, LookIKTarget.y, LookIKTarget.z );	g_YLine+=16.0f;
//	} 
	


	Vec3 v1		=	m_AbsolutePose[NeckIdx  ].q.GetColumn1();
	Vec3 v2		=	m_AbsolutePose[HeadIdx  ].q.GetColumn1();

	Vec3 r0=(v1+v2).GetNormalized();
	Vec3 r1=r0;		r1.z=0; r1.Normalize(); 
	Vec3 PivotAxisX=rPhysLocationNext.q * (r0+4*r1).GetNormalized();

	IRenderAuxGeom * pAux = g_pISystem->GetIRenderer()->GetIRenderAuxGeom();
	//pAux->DrawLine(Vec3(ZERO), RGBA8(0,0,0,0), PivotAxisX*100.0f, RGBA8(0xff,0xff,0,0));


	Vec3 LocalLookAtTarget = m_LookIK.m_LookIKTargetSmooth-rPhysLocationNext.t;
	Vec3 LeftEyePos		= (m_AbsolutePose[lidx]*ql).t;
	Vec3 RightEyePos	= (m_AbsolutePose[ridx]*qr).t;
	Vec3 MiddleEyePos	= (LeftEyePos+RightEyePos)*0.5f;

	Vec3 LookDir = (LocalLookAtTarget-MiddleEyePos).GetNormalized();

	f32 dot	=	PivotAxisX|LookDir;
	extern f32 g_YLine;
	float fColor[4] = {1,1,0,1};
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"dot: %12.8f", dot );	g_YLine+=16.0f;

	f32 FOR = cosf(m_LookIK.m_LookIKFOR);
	if (dot<FOR || m_LookIK.m_LookIKFadeout) 
	{
		//avoid Linda-Blair effect
		m_LookIK.m_LookIKFadeout=1;
		m_LookIK.m_LookIKBlend-=BINOUT*fabsf(m_pInstance->m_fOriginalDeltaTime);
		if (m_LookIK.m_LookIKBlend<0.0f)	
		{
			m_LookIK.m_LookIKFadeout=0;
			m_LookIK.m_LookIKBlend=0.0f;
			return;
		}
	}
	else
	{
		if (LIKFlag)
		{
			m_LookIK.m_LookIKBlend+=BINOUT*fabsf(m_pInstance->m_fOriginalDeltaTime);
			if (m_LookIK.m_LookIKBlend>1.0f)	m_LookIK.m_LookIKBlend=1.0f;
			assert(m_LookIK.m_LookIKFadeout==0);
		}
		else
		{
			m_LookIK.m_LookIKBlend-=BINOUT*fabsf(m_pInstance->m_fOriginalDeltaTime);
			if (m_LookIK.m_LookIKBlend<0.0f)	m_LookIK.m_LookIKBlend=0.0f;
			assert(m_LookIK.m_LookIKFadeout==0);
		}
	}

/*	
	extern f32 g_YLine;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_LookIKBlend: %12.8f", m_LookIKBlend );	g_YLine+=16.0f;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Model: %s ",m_pInstance->GetModelFilePath());	g_YLine+=16.0f;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Target:(%12.8f %12.8f %12.8f) ",m_LookIKTarget.x, m_LookIKTarget.y, m_LookIKTarget.z );	g_YLine+=16.0f;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"LookIK_FOR: %12.8f    SmoothTarget:(%12.8f %12.8f %12.8f) ",m_LookIKFOR, m_LookIKTargetSmooth.x, m_LookIKTargetSmooth.y, m_LookIKTargetSmooth.z );	g_YLine+=16.0f;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_fOriginalDeltaTime: %12.8f",m_pInstance->m_fOriginalDeltaTime );	g_YLine+=16.0f;
*/

	if (Console::GetInst().ca_DrawLookIK)
	{
		IRenderAuxGeom * pAux = g_pISystem->GetIRenderer()->GetIRenderAuxGeom();
		ColorF color( 1.0f, 0.0f, 0.0f, m_LookIK.m_LookIKBlend );
		pAux->DrawLine(rPhysLocationNext*MiddleEyePos, color, m_LookIK.m_LookIKTargetSmooth, color);

		pAux->DrawSphere(m_LookIK.m_LookIKTargetSmooth, 0.3f, color);

		g_pISystem->GetIRenderer()->DrawLabel
			( 
			m_LookIK.m_LookIKTargetSmooth + Vec3(0,0,0.4f), 1.0f, "t:(%.1f %.1f %.1f)\nm:(%.1f %.1f %.1f)\nl:(%.1f %.1f %.1f)", 
			rPhysLocationNext.t.x,
			rPhysLocationNext.t.y,
			rPhysLocationNext.t.z,
			m_LookIK.m_LookIKTargetSmooth.x,
			m_LookIK.m_LookIKTargetSmooth.y,
			m_LookIK.m_LookIKTargetSmooth.z,
			MiddleEyePos.x,
			MiddleEyePos.y,
			MiddleEyePos.z
			);
	}

	//	m_LookIKFadeout=0;
	for (uint32 i=0; i<numJoints; i++)
		pRelativeQuatIK[i] = m_RelativePose[i];


	Vec3 CamPos	= LocalLookAtTarget * rPhysLocationNext.q;
	Vec3 LookAt	= (CamPos-MiddleEyePos).GetNormalized();


	//VDirIdentity is the matrix that is not changing the rotation of the head
	Vec3 headX=m_parrModelJoints[HeadIdx].m_DefaultAbsPose.q.GetColumn0();
	Vec3 headY=m_parrModelJoints[HeadIdx].m_DefaultAbsPose.q.GetColumn1();
	Vec3 headZ=m_parrModelJoints[HeadIdx].m_DefaultAbsPose.q.GetColumn2();
	Matrix33 VDirIdentity = Matrix33::CreateFromVectors( -headZ, headY, headX ); 

	Quat AlignmentQuat	 =	 !Quat(VDirIdentity) * m_parrModelJoints[HeadIdx].m_DefaultAbsPose.q;  

	Quat absHeadQuat = Quat::CreateRotationVDir(LookAt) * AlignmentQuat;
	Quat relHeadQuat = !m_AbsolutePose[NeckIdx].q * absHeadQuat ;
	relHeadQuat =  !pRelativeQuatIK[HeadIdx].q * relHeadQuat;

	//-----------------------------------------------------------------------
	//---           new Spline1 orientation      -------------------------------
	//-----------------------------------------------------------------------
	if (Spine1Idx>=0)
	{
		int32 spine0 = m_parrModelJoints[Spine1Idx].m_idxParent;
		Quat lerpSpine1	= Quat::CreateSlerp( Quat(IDENTITY), relHeadQuat, m_LookIK.m_lookIKBlendsSmooth[0]);
		pRelativeQuatIK[Spine1Idx].q *= lerpSpine1;
		m_AbsolutePose[Spine1Idx] = m_AbsolutePose[spine0] * pRelativeQuatIK[Spine1Idx];
	}

	//-----------------------------------------------------------------------
	//---           new Spline2 orientation      -------------------------------
	//-----------------------------------------------------------------------
	if (Spine2Idx>=0)
	{
		int32 spine1	= m_parrModelJoints[Spine2Idx].m_idxParent;
		Quat lerpSpine2	= Quat::CreateSlerp( Quat(IDENTITY), relHeadQuat, m_LookIK.m_lookIKBlendsSmooth[1]);
		pRelativeQuatIK[Spine2Idx].q *=  lerpSpine2;
		m_AbsolutePose[Spine2Idx] = m_AbsolutePose[spine1] * pRelativeQuatIK[Spine2Idx];
	}
	//-----------------------------------------------------------------------
	//---           new Spline3 orientation      ----------------------------
	//-----------------------------------------------------------------------
	if (Spine3Idx>=0)
	{
		int32 spine2	= m_parrModelJoints[Spine3Idx].m_idxParent;
		Quat lerpSpine3	= Quat::CreateSlerp( Quat(IDENTITY), relHeadQuat, m_LookIK.m_lookIKBlendsSmooth[2]);
		pRelativeQuatIK[Spine3Idx].q *=  lerpSpine3;
		m_AbsolutePose[Spine3Idx] = m_AbsolutePose[spine2] * pRelativeQuatIK[Spine3Idx];
	}
	//-----------------------------------------------------------------------
	//---           new Neck orientation      -------------------------------
	//-----------------------------------------------------------------------
	Quat lerpNeck		= Quat::CreateSlerp( Quat(IDENTITY), relHeadQuat, m_LookIK.m_lookIKBlendsSmooth[3]);
	int32 spine3 = m_parrModelJoints[NeckIdx].m_idxParent;
	pRelativeQuatIK[NeckIdx].q *= lerpNeck;
	m_AbsolutePose[NeckIdx] = m_AbsolutePose[spine3] * pRelativeQuatIK[NeckIdx];

	//-----------------------------------------------------------------------
	//---           new Head orientation      -------------------------------
	//-----------------------------------------------------------------------
	m_AbsolutePose[HeadIdx] = m_AbsolutePose[NeckIdx] * pRelativeQuatIK[HeadIdx];
	m_AbsolutePose[HeadIdx].q.Normalize();

	//-----------------------------------------------------------------------
	//---           new eyes orientation      -------------------------------
	//-----------------------------------------------------------------------
	m_AbsolutePose[lidx] =	m_AbsolutePose[HeadIdx]*pRelativeQuatIK[lidx];
	m_AbsolutePose[lidx].q.Normalize();
	m_AbsolutePose[ridx] =	m_AbsolutePose[HeadIdx]*pRelativeQuatIK[ridx];
	m_AbsolutePose[ridx].q.Normalize();

	LeftEyePos  = m_AbsolutePose[lidx].t;
	RightEyePos = m_AbsolutePose[ridx].t;

	MiddleEyePos = (LeftEyePos+RightEyePos)*0.5f;
	LookAt = (CamPos-MiddleEyePos).GetNormalized();

	absHeadQuat = Quat::CreateRotationVDir(LookAt) * AlignmentQuat;
	relHeadQuat = !m_AbsolutePose[NeckIdx].q * absHeadQuat ;
	relHeadQuat = !pRelativeQuatIK[HeadIdx].q * relHeadQuat;

	Quat lerpHead		= Quat::CreateSlerp( Quat(IDENTITY), relHeadQuat, m_LookIK.m_lookIKBlendsSmooth[4]);

	//-----------------------------------------------------------------------
	//---           new Head orientation      -------------------------------
	//-----------------------------------------------------------------------
	pRelativeQuatIK[HeadIdx].q *= lerpHead;
	m_AbsolutePose[HeadIdx] = m_AbsolutePose[NeckIdx] * pRelativeQuatIK[HeadIdx];
	m_AbsolutePose[HeadIdx].q.Normalize();

	if (m_LookIK.m_LookIKFadeout)
	{
		if (Spine1Idx>=0)
			pRelativeQuatIK[Spine1Idx].q	=	m_LookIK.m_oldSpine1;
		if (Spine2Idx>=0)
			pRelativeQuatIK[Spine2Idx].q	=	m_LookIK.m_oldSpine2;
		if (Spine3Idx>=0)
			pRelativeQuatIK[Spine3Idx].q	=	m_LookIK.m_oldSpine3;
		pRelativeQuatIK[NeckIdx].q			=	m_LookIK.m_oldNeck;
		pRelativeQuatIK[HeadIdx].q			=	m_LookIK.m_oldHead;

		if (Spine1Idx>=0)
			m_AbsolutePose[Spine1Idx] = m_AbsolutePose[Spine0Idx] * pRelativeQuatIK[Spine1Idx];
		if (Spine2Idx>=0)
			m_AbsolutePose[Spine2Idx] = m_AbsolutePose[Spine1Idx] * pRelativeQuatIK[Spine2Idx];
		if (Spine3Idx>=0)
			m_AbsolutePose[Spine3Idx] = m_AbsolutePose[Spine2Idx] * pRelativeQuatIK[Spine3Idx];

		m_AbsolutePose[NeckIdx] = m_AbsolutePose[spine3 ] * pRelativeQuatIK[NeckIdx];
		m_AbsolutePose[HeadIdx] = m_AbsolutePose[NeckIdx] * pRelativeQuatIK[HeadIdx];
	}

	//----------------------------------------------------------------------
	//---     calculate the look-direction for each eye individually     ---
	//----------------------------------------------------------------------
	m_AbsolutePose[lidx] =	m_AbsolutePose[HeadIdx]*pRelativeQuatIK[lidx];
	m_AbsolutePose[ridx] =	m_AbsolutePose[HeadIdx]*pRelativeQuatIK[ridx];
	Vec3 left	=	(m_AbsolutePose[lidx]*ql).t;
	Vec3 right=	(m_AbsolutePose[ridx]*qr).t;

	m_LookIK.m_oldEyeLeft = m_RelativePose[lidx].q * m_qAdditionalRotPos[lidx].q;
	Vec3 CamPosL = CamPos-left;
	LookAt = CamPosL.GetNormalized();
	QuatT absLeftEye34 = QuatT(Quat::CreateRotationVDir(LookAt),left) * m_qAdditionalRotPos[lidx];
	pRelativeQuatIK[lidx] = m_AbsolutePose[HeadIdx].GetInverted() * absLeftEye34;
	m_AbsolutePose[lidx] = absLeftEye34;

	m_LookIK.m_oldEyeRight = m_RelativePose[ridx].q * m_qAdditionalRotPos[lidx].q;
	Vec3 CamPosR = CamPos-right;
	LookAt = CamPosR.GetNormalized();
	QuatT absRightEye34 = QuatT(Quat::CreateRotationVDir(LookAt),right) * m_qAdditionalRotPos[ridx];
	pRelativeQuatIK[ridx] = m_AbsolutePose[HeadIdx].GetInverted() * absRightEye34;
	m_AbsolutePose[ridx] = absRightEye34;

	//-------------------------------------------------------------------------------
	//---     blend with the FK-pose and calculate the new absolute quaternions   ---
	//-------------------------------------------------------------------------------
	f32 IKBlend = clamp(m_LookIK.m_LookIKBlend,0.0f,1.0f);
//	IKBlend = IKBlend * IKBlend * ( 3 - 2 * IKBlend );
	IKBlend -= 0.5f;
	IKBlend = 0.5f+IKBlend/(0.5f+2.0f*IKBlend*IKBlend);
//	IKBlend = IKBlend * IKBlend * ( 3 - 2 * IKBlend );
//	IKBlend = IKBlend * IKBlend * ( 3 - 2 * IKBlend );

	//float fColor[4] = {0,1,0,1};
	//extern float g_YLine;
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_LookIKFadeout: %d",m_LookIKFadeout );	g_YLine+=16.0f;
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"IKBlend: %f dot:%f",IKBlend,dot );	g_YLine+=16.0f;
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"LookAt: %f  %f %f",LookAt.x,LookAt.y,LookAt.z );	g_YLine+=16.0f;

	for (uint32 i=0; i<numJoints; i++)
	{
		Quat quat0 = m_RelativePose[i].q;
		Quat quat1 = pRelativeQuatIK[i].q;
		m_RelativePose[i].q.SetNlerp(quat0,quat1,IKBlend);

		m_AbsolutePose[i] = m_RelativePose[i];
		int16 parent = m_parrModelJoints[i].m_idxParent;
		if (parent>=0)
			m_AbsolutePose[i]	= m_AbsolutePose[parent] * m_AbsolutePose[i];

		assert(m_AbsolutePose[i].IsValid());
	}

	if (Spine1Idx>=0)
		m_LookIK.m_oldSpine1	=	pRelativeQuatIK[Spine1Idx].q;
	if (Spine2Idx>=0)
		m_LookIK.m_oldSpine2	=	pRelativeQuatIK[Spine2Idx].q;;
	if (Spine3Idx>=0)
		m_LookIK.m_oldSpine3	=	pRelativeQuatIK[Spine3Idx].q;;
	m_LookIK.m_oldNeck		=	pRelativeQuatIK[NeckIdx].q;;
	m_LookIK.m_oldHead		=	pRelativeQuatIK[HeadIdx].q;;
}

// [MichaelS] Added this function so that eyes can be updated after absolute transforms are calculated.
// This is so that eyes continue to look at the target even if additional rotations are applied to the
// neck joint (for example). This is needed for facial animations when look-ik is enabled.
void CSkeletonPose::LookIKEyes( const QuatT& rPhysLocationNext )
{
	int32 HeadIdx		= (m_pModelSkeleton ? m_pModelSkeleton->m_IdxArray[eIM_HeadIdx] : 0);
	int32 ridx			= (m_pModelSkeleton ? m_pModelSkeleton->m_IdxArray[eIM_RightEyeIdx] : 0);
	int32 lidx			= (m_pModelSkeleton ? m_pModelSkeleton->m_IdxArray[eIM_LeftEyeIdx] : 0);
	int eyeBones[2] = {lidx, ridx};

	uint32 numJoints = m_AbsolutePose.size();

	// [MichaelS] - This line seems to be needed - there is some special processing for eye attachments
	// (which connect to these eye bones) that relies on RelativeQuat being set. Previously it was the
	// case that eye bones were always identity unless look ik was on, but now eye bones can be rotated
	// by facial animations, so we need to make sure this is set properly.
	for (int eye = 0; eye < 2; ++eye)
	{
		int idx = eyeBones[eye];
		if (idx >= 0 && idx < int(numJoints))
			m_RelativePose[idx].q = m_RelativePose[idx].q * m_qAdditionalRotPos[idx].q;
	}

	if( Console::GetInst().ca_UseLookIK==0)
		return;

	Vec3 LocalLookAtTarget = m_LookIK.m_LookIKTargetSmooth - (m_pInstance ? rPhysLocationNext.t : ZERO);
	Vec3 CamPos	= LocalLookAtTarget * (m_pInstance ? rPhysLocationNext.q : IDENTITY);

	//////////////////////////////////////////////////////////////////////////	
 
	//CFacialInstance *pInstance=(CFacialInstance *)(m_pInstance->GetFacialInstance());
	//if (pInstance)
	//	pInstance->ApplyProceduralFaceBehaviour(CamPos);	

	//////////////////////////////////////////////////////////////////////////

	if (m_LookIK.m_UseLookIK==0)
		return;

	QuatT* pRelativeQuatIK = (QuatT*)alloca( numJoints*sizeof(QuatT) );
	QuatT* pAbsoluteQuatIK = (QuatT*)alloca( numJoints*sizeof(QuatT) );

	f32 IKBlend = clamp(m_LookIK.m_LookIKBlend,0.0f,1.0f);
//	IKBlend = IKBlend * IKBlend * ( 3 - 2 * IKBlend );
	IKBlend -= 0.5f;
	IKBlend = 0.5f+IKBlend/(0.5f+2.0f*IKBlend*IKBlend);
//	IKBlend = IKBlend * IKBlend * ( 3 - 2 * IKBlend );
//	IKBlend = IKBlend * IKBlend * ( 3 - 2 * IKBlend );

	IAttachmentManager* pIAttachmentManager = (m_pInstance ? m_pInstance->GetIAttachmentManager() : 0);

	int32 alidx = (pIAttachmentManager ? pIAttachmentManager->GetIndexByName("eye_left") : 0);
	int32 aridx = (pIAttachmentManager ? pIAttachmentManager->GetIndexByName("eye_right") : 0);

	QuatT ql=QuatT(IDENTITY);
	QuatT qr=QuatT(IDENTITY);
	if (alidx>=0 || aridx>=0 )
	{
		IAttachment* pIAttachmentL = (pIAttachmentManager ? pIAttachmentManager->GetInterfaceByIndex(alidx) : 0);
		IAttachment* pIAttachmentR = (pIAttachmentManager ? pIAttachmentManager->GetInterfaceByIndex(aridx) : 0);
		ql = (pIAttachmentL ? QuatT(pIAttachmentL->GetAttRelativeDefault()) : IDENTITY);
		qr = (pIAttachmentR ? QuatT(pIAttachmentR->GetAttRelativeDefault()) : IDENTITY);
	}

	Quat* quats[2] = {&m_LookIK.m_oldEyeLeft, &m_LookIK.m_oldEyeRight};
	QuatT* attachmentPositions[2] = {&ql, &qr};
	for (int eye = 0; eye < 2; ++eye)
	{
		int idx = eyeBones[eye];

		Vec3 eyePos = (idx >= 0 && idx < int(numJoints)? (m_AbsolutePose[idx] * (*attachmentPositions[eye])).t : ZERO);
		Vec3 CamPosDir = CamPos-eyePos;
		Vec3 LookAt = (CamPosDir.IsZero(0.1f) ? Vec3(1.0f, 0.0f, 0.0f) : CamPosDir.GetNormalized());
		QuatT absEye34 = (idx >= 0 && idx < int(numJoints) ? QuatT(Quat::CreateRotationVDir(LookAt),eyePos) * m_qAdditionalRotPos[idx] : IDENTITY);


		if ( idx >= 0 && idx < int(numJoints) )
			pRelativeQuatIK[idx] = (HeadIdx >= 0 && HeadIdx < int(numJoints) ? m_AbsolutePose[HeadIdx].GetInverted() * absEye34 : IDENTITY);
		//m_arrJoints[idx].m_AbsolutePose = absEye34;

		Quat& quat0 = *quats[eye];
		Quat quat1 = (idx >= 0 && idx < int(numJoints) ? pRelativeQuatIK[idx].q : IDENTITY);
		if ((m_LookIK.m_UseLookIK || m_LookIK.m_LookIKBlend) && idx >= 0 && idx < int(numJoints))
			m_RelativePose[idx].q.SetNlerp(quat0,quat1,IKBlend);
		else if (idx >= 0 && idx < int(numJoints))
			// [MichaelS] - This line seems to be needed - there is some special processing for eye attachments
			// (which connect to these eye bones) that relies on RelativeQuat being set. Previously it was the
			// case that eye bones were always identity unless look ik was on, but now eye bones can be rotated
			// by facial animations, so we need to make sure this is set properly.
			m_RelativePose[idx].q = m_RelativePose[idx].q * m_qAdditionalRotPos[idx].q;

		if (idx >= 0 && idx < int(numJoints))
		{
			m_AbsolutePose[idx] = m_RelativePose[idx];
			int16 parent = m_parrModelJoints[idx].m_idxParent;
			if (parent >= 0 && parent < int(numJoints))
				m_AbsolutePose[idx]	= m_AbsolutePose[parent] * m_AbsolutePose[idx];
		}
	}
}




//--------------------------------------------------------------------------------------
//--------                procedural recoil animation                           --------
//--------------------------------------------------------------------------------------

void CSkeletonPose::ApplyRecoilAnimation(f32 fDuration, f32 fKinematicImpact, uint32 nArms ) 
{
	m_Recoil.m_fAnimTime	= 0.0f;
	m_Recoil.m_fDuration	= fDuration;
	m_Recoil.m_fStrengh		= fKinematicImpact; //recoil in cm
	m_Recoil.m_nArms			= nArms;   //1-left arm  2-right arm   3-both
};


//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
f32 RecoilEffect( f32 t ) 
{
	if (t<0.0f)	t=0.0f;
	if (t>1.0f)	t=1.0f;

	f32 sq2		= sqrtf(2.0f);
	f32 scale	=	sq2+gf_PI;

	f32 x=t*scale-sq2;
	if (x<0.0f)
		return (-(x*x)+2.0f)*0.5f;
	else
		return (cosf(x)+1.0f)*0.5f;
}


void CSkeletonPose::PlayRecoilAnimation( const QuatT& rPhysEntityLocation ) 
{
	//return;

	if (m_Recoil.m_fAnimTime>=(m_Recoil.m_fDuration*2))
		return;

//	m_Recoil.m_nArms=2;

	f32 tn = m_Recoil.m_fAnimTime/m_Recoil.m_fDuration;
	f32 fImpact = RecoilEffect(tn);

	m_Recoil.m_fAnimTime += m_pInstance->m_fOriginalDeltaTime;

	//float fColor[4] = {0,1,0,1};
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_Recoil.m_fAnimTime: %f   fImpact: %f",m_Recoil.m_fAnimTime,fImpact);	g_YLine+=16.0f;
	//g_YLine+=16.0f;

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------

	int32 WeaponBoneIdx		= m_pModelSkeleton->m_IdxArray[eIM_WeaponBoneIdx];
	if (WeaponBoneIdx<0)	
		CryError("CryAnimation: invalid bone-index - WeaponBoneIdx");

	QuatT WeaponWorld			= rPhysEntityLocation*m_AbsolutePose[WeaponBoneIdx];

	Vec3 vWWeaponX			= WeaponWorld.GetColumn0();
	Vec3 vWWeaponY			= WeaponWorld.GetColumn1();
	Vec3 vWWeaponZ			= WeaponWorld.GetColumn2();
	Vec3 vWRecoilTrans	= (-vWWeaponY*fImpact*m_Recoil.m_fStrengh)+(vWWeaponZ*fImpact*m_Recoil.m_fStrengh*0.4f); 

	Vec3 vLWeaponX			= m_AbsolutePose[WeaponBoneIdx].q.GetColumn0();
	Vec3 vLWeaponY			= m_AbsolutePose[WeaponBoneIdx].q.GetColumn1();
	Vec3 vLWeaponZ			= m_AbsolutePose[WeaponBoneIdx].q.GetColumn2();
	Vec3 vLRecoilTrans	= (-vLWeaponY*fImpact*m_Recoil.m_fStrengh)+(vLWeaponZ*fImpact*m_Recoil.m_fStrengh*0.4f); 

	//g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
	//g_pAuxGeom->DrawLine(WeaponWorld.t,RGBA8(0x3f,0x3f,0x3f,0x00), WeaponWorld.t+vWWeaponX,RGBA8(0xff,0x00,0x00,0x00) );
	//g_pAuxGeom->DrawLine(WeaponWorld.t,RGBA8(0x3f,0x3f,0x3f,0x00), WeaponWorld.t+vWWeaponY,RGBA8(0x00,0xff,0x00,0x00) );
	//g_pAuxGeom->DrawLine(WeaponWorld.t,RGBA8(0x3f,0x3f,0x3f,0x00), WeaponWorld.t+vWWeaponZ,RGBA8(0x00,0x00,0xff,0x00) );


	int32 l0 = m_pModelSkeleton->m_IdxArray[eIM_LClavicleIdx];	
	int32 l1 = m_pModelSkeleton->m_IdxArray[eIM_LUpperArmIdx];	
	int32 l2 = m_pModelSkeleton->m_IdxArray[eIM_LForeArmIdx];
	int32 l3 = m_pModelSkeleton->m_IdxArray[eIM_LHandIdx];
	if (m_Recoil.m_nArms&1)
	{
		int32 LHand2Idx				= m_pModelSkeleton->m_IdxArray[eIM_LHandIdx];
		Vec3 vRealLHandPos		= rPhysEntityLocation*m_AbsolutePose[LHand2Idx].t;
		Quat qRealLHandRot		= m_AbsolutePose[LHand2Idx].q;
		Vec3 LocalGoalL=rPhysEntityLocation.GetInverted()*(vRealLHandPos+vWRecoilTrans*0.7f);
		TwoBoneIK(l1,l2,l3,LocalGoalL);
		m_AbsolutePose[LHand2Idx].q	=	qRealLHandRot;
		int32 pl=m_parrModelJoints[LHand2Idx].m_idxParent;
		m_RelativePose[LHand2Idx].q = !m_AbsolutePose[pl].q * m_AbsolutePose[LHand2Idx].q;
	}

	int32 r0 = m_pModelSkeleton->m_IdxArray[eIM_RClavicleIdx]; 
	int32 r1 = m_pModelSkeleton->m_IdxArray[eIM_RUpperArmIdx];
	int32 r2 = m_pModelSkeleton->m_IdxArray[eIM_RForeArmIdx];	
	int32 r3 = m_pModelSkeleton->m_IdxArray[eIM_RHandIdx];
	if (m_Recoil.m_nArms&2)
	{
		int32 RHand2Idx				= m_pModelSkeleton->m_IdxArray[eIM_RHandIdx];
		Vec3 vRealRHandPos		= rPhysEntityLocation*m_AbsolutePose[RHand2Idx].t;
		Quat qRealRHandRot		= m_AbsolutePose[RHand2Idx].q;
		Vec3 LocalGoalR=rPhysEntityLocation.GetInverted()*(vRealRHandPos+vWRecoilTrans);
		TwoBoneIK(r1,r2,r3,LocalGoalR);
		m_AbsolutePose[RHand2Idx].q	=	qRealRHandRot;
		int32 pr=m_parrModelJoints[RHand2Idx].m_idxParent;
		m_RelativePose[RHand2Idx].q = !m_AbsolutePose[pr].q * m_AbsolutePose[RHand2Idx].q;
	}


	int32 bp = m_pModelSkeleton->m_IdxArray[eIM_BipIdx]; 

	int32 tl = m_pModelSkeleton->m_IdxArray[eIM_LThighIdx];	
	int32 kl = m_pModelSkeleton->m_IdxArray[eIM_LCalfIdx];

	int32 tr = m_pModelSkeleton->m_IdxArray[eIM_RThighIdx];
	int32 kr = m_pModelSkeleton->m_IdxArray[eIM_RCalfIdx]; 

	int32 p0 = m_pModelSkeleton->m_IdxArray[eIM_PelvisIdx];	
	int32 s0 = m_pModelSkeleton->m_IdxArray[eIM_Spine0Idx];	
	int32 s1 = m_pModelSkeleton->m_IdxArray[eIM_Spine1Idx];	
	int32 s2 = m_pModelSkeleton->m_IdxArray[eIM_Spine2Idx]; 
	int32 s3 = m_pModelSkeleton->m_IdxArray[eIM_Spine3Idx];	
	int32 n0 = m_pModelSkeleton->m_IdxArray[eIM_NeckIdx]; 




	Vec3 vLRecoilTrans1	= -vLWeaponY*RecoilEffect(tn-0.20f)*m_Recoil.m_fStrengh; 
	Vec3 vLRecoilTrans2	= -vLWeaponY*RecoilEffect(tn-0.30f)*m_Recoil.m_fStrengh; 
	Vec3 vLRecoilTrans3	= -vLWeaponY*RecoilEffect(tn-0.40f)*m_Recoil.m_fStrengh; 
	Vec3 vLRecoilTrans4	= -vLWeaponY*RecoilEffect(tn-0.50f)*m_Recoil.m_fStrengh; 
	Vec3 vLRecoilTrans5	= -vLWeaponY*RecoilEffect(tn-0.60f)*m_Recoil.m_fStrengh; 

	m_AbsolutePose[tl].t += vLRecoilTrans5*0.05f;
	m_AbsolutePose[tr].t += vLRecoilTrans5*0.05f;

	m_AbsolutePose[p0].t += vLRecoilTrans5*0.15f;
	m_AbsolutePose[s0].t += vLRecoilTrans5*0.20f;
	m_AbsolutePose[s1].t += vLRecoilTrans4*0.25f;
	m_AbsolutePose[s2].t += vLRecoilTrans3*0.30f;
	m_AbsolutePose[s3].t += vLRecoilTrans2*0.35f;
	m_AbsolutePose[n0].t += vLRecoilTrans1*0.40f;

	if (m_Recoil.m_nArms&1)
	{
		m_AbsolutePose[l0].t += vLRecoilTrans1*0.45f; //clavicle
		m_AbsolutePose[l1].t += vLRecoilTrans*0.50f;
	}

	if (m_Recoil.m_nArms&2)
	{
		m_AbsolutePose[r0].t += vLRecoilTrans1*0.50f; //clavicle
		m_AbsolutePose[r1].t += vLRecoilTrans*0.60f;
	}



	m_RelativePose[kl] = m_AbsolutePose[tl].GetInverted() * m_AbsolutePose[kl];
	m_RelativePose[tl] = m_AbsolutePose[p0].GetInverted() * m_AbsolutePose[tl];

	m_RelativePose[kr] = m_AbsolutePose[tr].GetInverted() * m_AbsolutePose[kr];
	m_RelativePose[tr] = m_AbsolutePose[p0].GetInverted() * m_AbsolutePose[tr];

	m_RelativePose[p0] = m_AbsolutePose[bp].GetInverted() * m_AbsolutePose[p0];

	m_RelativePose[s0] = m_AbsolutePose[p0].GetInverted() * m_AbsolutePose[s0];
	m_RelativePose[s1] = m_AbsolutePose[s0].GetInverted() * m_AbsolutePose[s1];
	m_RelativePose[s2] = m_AbsolutePose[s1].GetInverted() * m_AbsolutePose[s2];
	m_RelativePose[s3] = m_AbsolutePose[s2].GetInverted() * m_AbsolutePose[s3];
	m_RelativePose[n0] = m_AbsolutePose[s3].GetInverted() * m_AbsolutePose[n0];

	m_RelativePose[r0] = m_AbsolutePose[n0].GetInverted() * m_AbsolutePose[r0];
	m_RelativePose[r1] = m_AbsolutePose[r0].GetInverted() * m_AbsolutePose[r1];
	m_RelativePose[r2] = m_AbsolutePose[r1].GetInverted() * m_AbsolutePose[r2];

	m_RelativePose[l0] = m_AbsolutePose[n0].GetInverted() * m_AbsolutePose[l0];
	m_RelativePose[l1] = m_AbsolutePose[l0].GetInverted() * m_AbsolutePose[l1];
	m_RelativePose[l2] = m_AbsolutePose[l1].GetInverted() * m_AbsolutePose[l2];

	uint32 numJoints = m_AbsolutePose.size();
	for (uint32 i=1; i<numJoints; i++)
		m_AbsolutePose[i]	= m_AbsolutePose[m_parrModelJoints[i].m_idxParent] * m_RelativePose[i];
};


















