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
#include "CharacterInstance.h"
#include "Model.h"
#include "ModelSkeleton.h"
#include "CharacterManager.h"
#include <float.h>
#include "FacialAnimation/FacialInstance.h"


void CSkeletonPose::SkeletonPostProcess2( const QuatT& rPhysLocationNext, const QuatTS &__rAnimLocationNext, IAttachment* pIAttachment, uint32 OnRender )
{
	return;
}


void CSkeletonPose::SkeletonPostProcess( const QuatT& rPhysLocationNext, const QuatTS &rAnimLocationNext, IAttachment* pIAttachment, float fZoomAdjustedDistanceFromCamera, uint32 OnRender )
{
	DEFINE_PROFILER_FUNCTION();

#ifdef _DEBUG
	SCOPED_ENABLE_FLOAT_EXCEPTIONS;
#endif


#ifdef VTUNE_PROFILE 
	g_pISystem->VTuneResume();
#endif

	
/*
	const char* mname = m_pInstance->GetFilePath();
	if ( strcmp(mname,"objects/characters/human/asian/rifleman_light/rifleman_light_standard.cdf")==0 )
	{
		uint32 Inst=(uint32)m_pInstance;
		float fC1[4] = {1,0,0,1};
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.6f, fC1, false,"Post Update: Instance: %x OnRender: %x  Model: %s ",Inst,OnRender,mname );	
		g_YLine+=16.0f;
	} 
	*/


/*	uint32 numJoints2 = m_AbsolutePose.size();
	if (numJoints2==85)
	{
		float fColor[4] = {0,1,0,1};
		uint32 nInstance=(uint32)(m_pInstance);
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"Anim: (%f %f %f %f)    (%f %f %f)  OnRender: %x %x    m_nFallPlay: %x  instance: %x",rAnimLocationNext.q.w,rAnimLocationNext.q.v.x,rAnimLocationNext.q.v.y,rAnimLocationNext.q.v.z,  rAnimLocationNext.t.x,rAnimLocationNext.t.y,rAnimLocationNext.t.z, OnRender,m_bFullSkeletonUpdate,m_nFallPlay,nInstance );	
		g_YLine+=16.0f;
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"Phys: (%f %f %f %f)    (%f %f %f)  OnRender: %x",rPhysLocationNext.q.w,rPhysLocationNext.q.v.x,rPhysLocationNext.q.v.y,rPhysLocationNext.q.v.z,  rPhysLocationNext.t.x,rPhysLocationNext.t.y,rPhysLocationNext.t.z, OnRender );	
		g_YLine+=16.0f;
	}*/



	if (pIAttachment)
	{
		//It is an attachment, so it will need an update
		assert( pIAttachment->GetType()==CA_BONE || pIAttachment->GetType()==CA_FACE); 
		CCharInstance* pInstanceMaster=((CAttachment*)pIAttachment)->m_pAttachmentManager->m_pSkelInstance;
		m_bFullSkeletonUpdate = pInstanceMaster->m_SkeletonPose.m_bFullSkeletonUpdate;
		m_pSkeletonAnim->m_IsAnimPlaying=0;
		m_pSkeletonAnim->ProcessAnimations(rPhysLocationNext,rAnimLocationNext,OnRender);
	}
	

/*
	const char* mname = m_pInstance->GetModelFilePath();
	if ( strcmp(mname,"objects/weapons/attachments/tactical_attachment/tactical_attachment_fp.chr")==0 )
	{
		float fColor[4] = {1,1,0,1};
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"post update: %d  Attachment: %x  Model: %s ",OnRender, pIAttachment, mname );	g_YLine+=16.0f;
	}*/ 

	int nCurrentFrameID = g_pCharacterManager->m_nUpdateCounter; 
	if (m_pSkeletonAnim->GetCharEditMode()==0)
	{
		if ( m_pInstance->m_LastUpdateFrameID_Post==nCurrentFrameID )
		{
			//const char* name = m_pInstance->m_pModel->GetModelFilePath();
			//g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,name,	"several post-updates: FrameID: %x Old:%x New: %x",nCurrentFrameID, m_pInstance->m_LastUpdateFrameID_PostType, OnRender);
			//	assert(0);
			return;
		}
	}




	SmoothCD(m_LookIK.m_LookIKTargetSmooth, m_LookIK.m_LookIKTargetSmoothRate, m_pInstance->m_fOriginalDeltaTime, m_LookIK.m_LookIKTarget, 0.10f);
	SmoothCD(m_LookIK.m_lookIKBlendsSmooth[0], m_LookIK.m_lookIKBlendsSmoothRate[0], m_pInstance->m_fOriginalDeltaTime, m_LookIK.m_lookIKBlends[0], 0.10f);
	SmoothCD(m_LookIK.m_lookIKBlendsSmooth[1], m_LookIK.m_lookIKBlendsSmoothRate[1], m_pInstance->m_fOriginalDeltaTime, m_LookIK.m_lookIKBlends[1], 0.10f);
	SmoothCD(m_LookIK.m_lookIKBlendsSmooth[2], m_LookIK.m_lookIKBlendsSmoothRate[2], m_pInstance->m_fOriginalDeltaTime, m_LookIK.m_lookIKBlends[2], 0.10f);
	SmoothCD(m_LookIK.m_lookIKBlendsSmooth[3], m_LookIK.m_lookIKBlendsSmoothRate[3], m_pInstance->m_fOriginalDeltaTime, m_LookIK.m_lookIKBlends[3], 0.10f);
	SmoothCD(m_LookIK.m_lookIKBlendsSmooth[4], m_LookIK.m_lookIKBlendsSmoothRate[4], m_pInstance->m_fOriginalDeltaTime, m_LookIK.m_lookIKBlends[4], 0.10f);
	if (m_pModelSkeleton->IsHuman())
	{

		Vec3 vAimIKTarget = m_AimIK.m_AimIKTarget;
		int32 neckidx	=	m_pModelSkeleton->m_IdxArray[eIM_NeckIdx];
		Vec3 NeckBoneWorld	= rPhysLocationNext*m_AbsolutePose[neckidx].t;
		Vec3 vDistance=vAimIKTarget-NeckBoneWorld;
		const f32 fMinDist=1.5f; 
		if (vDistance.GetLength()<fMinDist)
			vAimIKTarget = NeckBoneWorld+vDistance.GetNormalizedSafe(Vec3(0,1,0))*fMinDist; 

		SmoothCD(m_AimIK.m_AimIKTargetSmooth, m_AimIK.m_AimIKTargetRate, m_pInstance->m_fOriginalDeltaTime, vAimIKTarget, m_AimIK.m_AimIKTargetSmoothTime);

		if (m_AimIK.m_UseAimIK)
		{
			if( Console::GetInst().ca_DrawAimPoses)
			{
				g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
				static Ang3 angle1(0,0,0); 
				angle1 += Ang3(0.01f,0.02f,0.08f);
				AABB aabb = AABB(Vec3(-0.09f,-0.09f,-0.09f),Vec3(+0.09f,+0.09f,+0.09f));
				OBB obb1=OBB::CreateOBBfromAABB( Matrix33::CreateRotationXYZ(angle1),aabb );
				g_pAuxGeom->DrawOBB(obb1,m_AimIK.m_AimIKTargetSmooth,1,RGBA8(0x00,0x00,0xff,0xff), eBBD_Extremes_Color_Encoded);
			}
		}

		f32 fFrameTime = m_pInstance->m_fOriginalDeltaTime;
		if (fFrameTime<0.0f) fFrameTime=0.0f;
		if (m_AimIK.m_AimIKFadeout || m_AimIK.m_UseAimIK==0) 
			m_AimIK.m_AimIKBlend-=m_AimIK.m_fAimIKFadeOutTime*fFrameTime;
		else
			m_AimIK.m_AimIKBlend+=m_AimIK.m_fAimIKFadeInTime*fFrameTime;

		Limit(m_AimIK.m_AimIKBlend, 0.0f, 1.0f);
	}


	uint32 nObjectType =	m_pInstance->m_pModel->m_ObjectType;
	uint32 numJoints = m_AbsolutePose.size();
	m_pInstance->m_PhysEntityLocation = rPhysLocationNext;
	m_pInstance->m_fOriginalDeltaTime	= g_pITimer->GetFrameTime();
	m_pInstance->m_fDeltaTime					=	m_pInstance->m_fOriginalDeltaTime*m_pInstance->m_fAnimSpeedScale;
	uint32 ObjectType =	m_pInstance->m_pModel->m_ObjectType;

	if (m_pInstance->m_bFacialAnimationEnabled)
		m_pInstance->m_Morphing.UpdateMorphEffectors(m_pInstance->m_pFacialInstance, m_pInstance->m_fDeltaTime,rAnimLocationNext, fZoomAdjustedDistanceFromCamera);
	else
		m_pInstance->m_Morphing.UpdateMorphEffectors(0, m_pInstance->m_fDeltaTime,rAnimLocationNext, fZoomAdjustedDistanceFromCamera);

#ifdef _DEBUG
	if ( (m_pSkeletonAnim->m_IsAnimPlaying&1) && m_pSkeletonAnim->m_AnimationDrivenMotion && nObjectType==CHR)
	{
		Quat q = m_RelativePose[0].q;
		Vec3 t = m_RelativePose[0].t;
		assert(q.IsEquivalent(IDENTITY,0.001f));
		assert(t.IsEquivalent(ZERO,0.001f));
	}
#endif


	if (m_pSkeletonAnim->m_IsAnimPlaying || m_bFullSkeletonUpdate)
	{
		
		if (m_pInstance->m_LastUpdateFrameID_Pre && m_pSkeletonAnim->m_IsAnimPlaying)
		{
			int32 nDifference = nCurrentFrameID-m_pInstance->m_LastUpdateFrameID_Pre;
			if ( nDifference) 
			{
				//const char* name = m_pInstance->m_pModel->GetModelFilePath();
				//g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,name,	"post-update not in synch with pre-update:  PreUpdate FrameID: %x   PostUpdate FrameID: %x",m_pInstance->m_LastUpdateFrameID_Pre, nCurrentFrameID);
				//return;
			}
		}


/*		if (numJoints==92)
		{
			float fColor[4] = {0,1,0,1};
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"AABB: min(%f %f %f)   max(%f %f %f)",m_AABB.min.x,m_AABB.min.y,m_AABB.min.z,  m_AABB.max.x,m_AABB.max.y,m_AABB.max.z );	
			g_YLine+=16.0f;
		}*/


		KinematicPostProcess( rPhysLocationNext, rAnimLocationNext);
	}

	//------------------------------------------------------------------------------

	if (m_Superimposed)
	{
		extern std::vector< std::vector<DebugJoint> > g_arrSkeletons;
		extern int32 g_nAnimID;
		extern int32 g_nGlobalAnimID;
		DrawThinSkeletons( QuatT(IDENTITY,Vec3(2,0,0)), g_arrSkeletons, g_nAnimID, g_nGlobalAnimID );
	}



	//-----------------------------------------------------------------------------------
	//--------                          update physics                          ---------
	//-----------------------------------------------------------------------------------
	if (1)
	{

		pe_params_flags pf,pf1;
		IPhysicalEntity* pCharPhys = GetCharacterPhysics();

		m_fPhysBlendTime += m_pInstance->m_fDeltaTime;
		if (ObjectType==CGA || pCharPhys && pCharPhys->GetType()==PE_ARTICULATED && pCharPhys->GetParams(&pf1) && pf1.flags & aef_recorded_physics)
		{
			if (m_bFullSkeletonUpdate && m_pSkeletonAnim->m_IsAnimPlaying || m_ppBonePhysics)
				//	if (m_InstanceVisible)
				m_pInstance->UpdatePhysicsCGA( 1, rAnimLocationNext );

			if (m_bFullSkeletonUpdate)
			{
				QuatT Offset = QuatT(rPhysLocationNext.GetInverted() * rAnimLocationNext);
				int32 numAttachments = m_pInstance->m_AttachmentManager.GetAttachmentCount()-1;
				for(int32 i=numAttachments-1; i>=0; i--) 
					m_pInstance->m_AttachmentManager.UpdatePhysicalizedAttachment(i,pCharPhys, Offset );
			}

#if (EXTREME_TEST==1)
			uint32 Valid = IsSkeletonValid();
			if (Valid==0)
			{
				assert(0);
				CryError("CryAnimation: invalid skeleton after ProcessPhysics on CGA: %s",m_pInstance->GetFilePath());
			}
#endif

		}	
		else 
			if (ObjectType==CHR)
			{
				if (m_bFullSkeletonUpdate)
				{
					DEFINE_PROFILER_SECTION("CharacterInstance::Update.ProcessPhysics");

					if ( g_pISystem->GetIPhysicalWorld()->GetPhysVars()->lastTimeStep>0)
					{
						ProcessPhysics( rPhysLocationNext,m_pInstance->m_fOriginalDeltaTime,m_pSkeletonAnim->m_IsAnimPlaying,0);
						pf.flagsAND = ~pef_disabled;
						m_pInstance->m_bWasVisible = 1;
					}	else
						m_pInstance->m_bWasVisible = 0;
				} 
				else 
					if (m_pCharPhysics || m_nAuxPhys>1)
						pf.flagsOR = pef_disabled;

				m_pInstance->m_bWasVisible &= m_bFullSkeletonUpdate > 0;
				SetAuxParams(&pf);

#if (EXTREME_TEST==1)
				for (uint32 i=0; i<numJoints; i++)
				{
					uint32 valid = m_RelativePose[i].t.IsValid();
					if (valid==0)
						CryError("CryAnimation: relative joint nr. %d is invalid after ProcessPhysics on CHR:  %s",i, m_pInstance->GetFilePath());
				}


				uint32 Valid = IsSkeletonValid();
				if (Valid==0)
				{
					assert(0);
					CryError("CryAnimation: invalid skeleton after ProcessPhysics on CHR: %s",m_pInstance->GetFilePath());
				}

#endif
			}

	}



	//---------------------------------------------------------------------------
	//---------------------------------------------------------------------------
	//---------------------------------------------------------------------------

	if (m_bFullSkeletonUpdate)
	{
		SetSkinningSkeletonMaster(rAnimLocationNext.s);
		m_pInstance->UpdateAttachedObjects(rPhysLocationNext,0,fZoomAdjustedDistanceFromCamera,OnRender);
		UpdateBBox();
	}
	else
	{
		m_pInstance->UpdateAttachedObjectsFast( rPhysLocationNext,fZoomAdjustedDistanceFromCamera,OnRender );
	}

	if (Console::GetInst().ca_DrawBodyMoveDir && m_pSkeletonAnim->m_AnimationDrivenMotion)
	{
		SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
		g_pAuxGeom->SetRenderFlags( renderFlags );

		QuatT BodyLocation = QuatT(rAnimLocationNext); 	
		BodyLocation.t += Vec3(0,0,0.31f);
		DrawArrow(BodyLocation,2.0f,RGBA8(0x00,0xff,0x00,0x00) );


		Vec3 vMoveDirLocal=m_pSkeletonAnim->GetCurrentVelocity(); 
		f32 fLength = m_pSkeletonAnim->GetCurrentVelocity().GetLength();

		QuatT TravelArrowLocation=QuatT(rAnimLocationNext);
		f32 yaw = -atan2f(vMoveDirLocal.x,vMoveDirLocal.y);
		TravelArrowLocation.t += Vec3(0.0f,0.0f,0.3f);
		TravelArrowLocation.q *= Quat::CreateRotationZ(yaw);
		DrawArrow(TravelArrowLocation,fLength,RGBA8(0xff,0xff,0x00,0x00) );

	}

	m_pInstance->m_LastUpdateFrameID_PostType = OnRender;
	if (OnRender!=1)
		m_pInstance->m_LastUpdateFrameID_Post=nCurrentFrameID;

	if (m_pSkeletonAnim->m_AnimationDrivenMotion && nObjectType==CHR)
		m_RelativePose[0].SetIdentity();

	if ( (m_nForceSkeletonUpdate&0x8000)==0)
	{
		m_nForceSkeletonUpdate--;
		if (m_nForceSkeletonUpdate<0)	m_nForceSkeletonUpdate=0;
	}

	//callback into the game with the latest skeleton pose
	if (m_pPostPhysicsCallback)		
		(*m_pPostPhysicsCallback)(m_pInstance,m_pPostPhysicsCallbackData);

#ifdef VTUNE_PROFILE 
	g_pISystem->VTunePause();
#endif

}



#if defined(_CPU_SSE) && !defined(_DEBUG) && !defined(PS3)
#include "fvec.h"
#endif

void CSkeletonPose::KinematicPostProcess(const QuatT &rPhysLocationNext, const QuatTS &rAnimLocationNext)
{

	if ( Console::GetInst().ca_DrawPositionPost & 1) 
	{
		//draw the entity in blue
		Vec3 wpos = rPhysLocationNext.t+Vec3(0.0f,0.0f,0.0f);
		g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
		static Ang3 angle(0,0,0);		angle += Ang3(0.01f,+0.03f,-0.07f);
		AABB aabb = AABB(Vec3(-0.05f,-0.05f,-0.05f),Vec3(+0.05f,+0.05f,+0.05f));
		OBB obb=OBB::CreateOBBfromAABB( Matrix33::CreateRotationXYZ(angle), aabb );
		g_pAuxGeom->DrawOBB(obb,wpos,1,RGBA8(0x00,0x00,0xff,0x00), eBBD_Extremes_Color_Encoded);

		Vec3 axisX = rPhysLocationNext.q.GetColumn0();
		Vec3 axisY = rPhysLocationNext.q.GetColumn1();
		Vec3 axisZ = rPhysLocationNext.q.GetColumn2();
		g_pAuxGeom->DrawLine(wpos,RGBA8(0x7f,0x00,0x00,0x00), wpos+axisX,RGBA8(0x7f,0x00,0x00,0x00) );
		g_pAuxGeom->DrawLine(wpos,RGBA8(0x00,0x7f,0x00,0x00), wpos+axisY,RGBA8(0x00,0x7f,0x00,0x00) );
		g_pAuxGeom->DrawLine(wpos,RGBA8(0x00,0x00,0x7f,0x00), wpos+axisZ,RGBA8(0x00,0x00,0x7f,0x00) );

		Vec3 line = rPhysLocationNext.q.GetColumn2()*5;

		g_pAuxGeom->DrawLine(wpos,RGBA8(0x00,0x00,0xff,0x00), wpos+line,RGBA8(0x00,0x00,0xff,0x00) );
	}

	if ( Console::GetInst().ca_DrawPositionPost & 2) 
	{
		//draw the animated character in red
		Vec3 wpos = rAnimLocationNext.t+Vec3(0.0f,0.0f,0.0f);
		g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
		static Ang3 angle(0,0,0); 
		angle += Ang3(0.01f,-0.02f,-0.03f);
		AABB aabb = AABB(Vec3(-0.05f,-0.05f,-0.05f),Vec3(+0.05f,+0.05f,+0.05f));
		OBB obb=OBB::CreateOBBfromAABB( Matrix33::CreateRotationXYZ(angle), aabb );
		g_pAuxGeom->DrawOBB(obb,wpos,1,RGBA8(0xff,0x00,0x00,0x00), eBBD_Extremes_Color_Encoded);

		Vec3 axisX = rAnimLocationNext.q.GetColumn0();
		Vec3 axisY = rAnimLocationNext.q.GetColumn1();
		Vec3 axisZ = rAnimLocationNext.q.GetColumn2();
		g_pAuxGeom->DrawLine(wpos,RGBA8(0xff,0x00,0x00,0x00), wpos+axisX,RGBA8(0xff,0x00,0x00,0x00) );
		g_pAuxGeom->DrawLine(wpos,RGBA8(0x00,0xff,0x00,0x00), wpos+axisY,RGBA8(0x00,0xff,0x00,0x00) );
		g_pAuxGeom->DrawLine(wpos,RGBA8(0x00,0x00,0xff,0x00), wpos+axisZ,RGBA8(0x00,0x00,0xff,0x00) );

		Vec3 line = rAnimLocationNext.q.GetColumn2()*5;
		g_pAuxGeom->DrawLine(wpos,RGBA8(0xff,0x00,0x00,0x00), wpos+line,RGBA8(0xff,0x00,0x00,0x00) );
	}

	if (Console::GetInst().ca_DebugAnimUpdates)
	{
		//just for debugging
		string strModelPath = m_pInstance->m_pModel->GetModelFilePath();
		g_pCharacterManager->m_arrSkeletonUpdates.push_back(strModelPath);
		g_pCharacterManager->m_arrAnimPlaying.push_back(m_pSkeletonAnim->m_IsAnimPlaying);
		g_pCharacterManager->m_arrForceSkeletonUpdates.push_back(m_nForceSkeletonUpdate);
		g_pCharacterManager->m_arrVisible.push_back(m_bInstanceVisible);
	}

	//------------------------------------------------------------------------------------------------

	uint32 numJoints = m_AbsolutePose.size();
	uint32 ObjectType =	m_pInstance->m_pModel->m_ObjectType;

	if (m_bFullSkeletonUpdate) 
	{

		int32 leye = m_pModelSkeleton->m_IdxArray[eIM_LeftEyeIdx];
		int32 reye = m_pModelSkeleton->m_IdxArray[eIM_RightEyeIdx];
		if (leye>0)
		{
			//m_RelativePose[leye].q.SetRotationY(gf_PI/2);
			m_RelativePose[leye].q.SetIdentity();
			if (m_arrPhysicsJoints.size())
				m_RelativePose[leye].t=m_arrPhysicsJoints[leye].m_DefaultRelativeQuat.t;
		}
		if (reye>0)
		{
			//m_RelativePose[reye].q.SetRotationY(gf_PI/2);
			m_RelativePose[reye].q.SetIdentity();
			if (m_arrPhysicsJoints.size())
				m_RelativePose[reye].t=m_arrPhysicsJoints[reye].m_DefaultRelativeQuat.t;
		}


		//	m_parrJoints[0].m_RelativePose.SetIdentity();
		//m_parrJoints[0].m_RelativePose = m_RelativePelvis;

		if ((m_pSkeletonAnim->m_IsAnimPlaying&1) && m_pSkeletonAnim->m_AnimationDrivenMotion && ObjectType==CHR)
		{
			QuatT delta = rPhysLocationNext.GetInverted()*QuatT(rAnimLocationNext);
			if (delta.t.x> 40.0f) delta.t.x= 40.0f;
			if (delta.t.y> 40.0f) delta.t.y= 40.0f;
			if (delta.t.z> 40.0f) delta.t.z= 40.0f;
			if (delta.t.x<-40.0f) delta.t.x=-40.0f;
			if (delta.t.y<-40.0f) delta.t.y=-40.0f;
			if (delta.t.z<-40.0f) delta.t.z=-40.0f;
			m_RelativePose[0] = delta;
			m_vRenderOffset = -delta.t;
		}
		else
		{
			/*	if (m_AnimationDrivenMotion==0 && ObjectType==CGA)
			{
			for (uint32 i=0; i<numJoints; i++)
			{
			QuatT delta = rPhysLocationNext.GetInverted()*QuatT(rAnimLocationNext);
			int32 p = m_parrModelJoints[i].m_idxParent;
			if (p<0)
			{
			m_parrJoints[i].m_RelativePose = delta*m_parrModelJoints[i].m_DefaultRelQuat;
			}
			}
			}*/
		}


#if (EXTREME_TEST==1)
		QuatT delta = rPhysLocationNext.GetInverted()*QuatT(rAnimLocationNext);
		for (uint32 i=0; i<numJoints; i++)
		{
			QuatT rp = m_RelativePose[i];
			f32 t=m_RelativePose[i].t.GetLength();
			uint32 valid = m_RelativePose[i].t.IsValid();
			if (valid==0)
			{
				assert(0);
				CryError("CryAnimation: relative joint nr. %d is invalid in kinematic post process:  %s",i, m_pInstance->GetFilePath());
			}
		}
#endif

		if (ObjectType==CHR)
		{
			//we assume that all CHRs are single root objects
			m_AbsolutePose[0] = m_RelativePose[0];
			for (uint32 i=1; i<numJoints; i++)
				m_AbsolutePose[i]	= m_AbsolutePose[m_parrModelJoints[i].m_idxParent] * m_RelativePose[i];
		}
		else
		{
			for (uint32 i=0; i<numJoints; i++)
			{
				int32 p = m_parrModelJoints[i].m_idxParent;
				if (p>=0)
					m_AbsolutePose[i]	= m_AbsolutePose[p] * m_RelativePose[i];
				else
					m_AbsolutePose[i] = m_RelativePose[i];
			}
		}
		

#if (EXTREME_TEST==1)
		uint32 Valid3 = IsSkeletonValid();
		if (Valid3==0)
		{
			assert(0);
			CryError("CryAnimation: invalid skeleton after AbsolutePose: %s",m_pInstance->GetFilePath());
		}
#endif

		//------------------------------------------------------------------------------
		//--------                        Aim-IK                                 -------
		//------------------------------------------------------------------------------
		if (m_AimIK.m_UseAimIK || m_AimIK.m_AimIKBlend)
		{
			AimIK(rPhysLocationNext,rAnimLocationNext);
#if (EXTREME_TEST==1)
			uint32 Valid = IsSkeletonValid();
			if (Valid==0)
				g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,m_pInstance->GetFilePath(),	"CryAnimation: Invalid Skeleton after AimIK" );
#endif
		}

		//------------------------------------------------------------------------------
		//--------                      Recoil-IK                                -------
		//------------------------------------------------------------------------------

		if (m_pModelSkeleton->IsHuman() && (m_pSkeletonAnim->m_IsAnimPlaying&1) && m_bInstanceVisible)
		{
			PlayRecoilAnimation(rPhysLocationNext);
#if (EXTREME_TEST==1)
			uint32 Valid2 = IsSkeletonValid();
			if (Valid2==0)
				g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,m_pInstance->GetFilePath(),	"CryAnimation: Invalid Skeleton after Recoil-IK" );
#endif
		}


		//------------------------------------------------------------------------------
		//--------                        Look-IK                                -------
		//------------------------------------------------------------------------------

		if (m_LookIK.m_UseLookIK || m_LookIK.m_LookIKBlend)
		{
			LookIK(rPhysLocationNext, m_LookIK.m_UseLookIK);
#if (EXTREME_TEST==1)
			uint32 Valid = IsSkeletonValid();
			if (Valid==0)
				g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,m_pInstance->GetFilePath(),	"CryAnimation: Invalid Skeleton after LookIK" );
#endif
		}
		else
		{
			int32 ridx			= m_pModelSkeleton->m_IdxArray[eIM_RightEyeIdx];
			int32 lidx			= m_pModelSkeleton->m_IdxArray[eIM_LeftEyeIdx];
			if (ridx>0 && lidx>0)
			{
				Vec3 LEyePos	= m_AbsolutePose[lidx].t;
				Vec3 REyePos	= m_AbsolutePose[ridx].t;
				Vec3 MEyePos	= rAnimLocationNext*((LEyePos+REyePos)*0.5f);
				Vec3 LEyeDir	= m_AbsolutePose[lidx].q.GetColumn1();
				Vec3 REyeDir	= m_AbsolutePose[ridx].q.GetColumn1();
				Vec3 MEyeDir	= rAnimLocationNext.q * (LEyeDir+REyeDir).GetNormalizedSafe();
				//g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
				//g_pAuxGeom->DrawLine(MEyePos,RGBA8(0x00,0x00,0x00,0x00), MEyePos+MEyeDir,RGBA8(0xff,0xff,0xff,0x00) );
				m_LookIK.m_LookIKTarget = MEyePos+MEyeDir*100.0f;	

				if ( (m_pSkeletonAnim->m_IsAnimPlaying&0x1) && m_bInstanceVisible)
				{
						Vec3 vLocalLookAtTarget = m_LookIK.m_LookIKTargetSmooth-rPhysLocationNext.t;
						CFacialInstance *pFacialInstance=(CFacialInstance *)(m_pInstance->GetFacialInstance());
						if (pFacialInstance)
								pFacialInstance->ApplyProceduralFaceBehaviour( vLocalLookAtTarget*rPhysLocationNext.q );	
				} 

			}
		} 


		if( Console::GetInst().ca_UseLookIK )
			LookIKHunter(m_LookIK.m_UseLookIK, rPhysLocationNext, rAnimLocationNext);

#if (EXTREME_TEST==1)
		uint32 Valid8 = IsSkeletonValid();
		if (Valid8==0)
		{
			assert(0);
			CryError("CryAnimation: invalid skeleton after LookIKHunter(): %s",m_pInstance->GetFilePath());
		}
#endif

		//-------------------------------------------------------------
		//update the new joint values we added in a post process
		//-------------------------------------------------------------

		//callback into the game with the latest skeleton pose
		if (m_pPostProcessCallback0)		
			(*m_pPostProcessCallback0)(m_pInstance,m_pPostProcessCallbackData0);

#if (EXTREME_TEST==1)
		uint32 Valid = IsSkeletonValid();
		if (Valid==0)
		{
			assert(0);
			CryError("CryAnimation: invalid skeleton after PostProcessCallback0: %s",m_pInstance->GetFilePath());
		}
#endif

		if (Console::GetInst().ca_GroundAlignment && m_enableFootGroundAlignment && (m_pSkeletonAnim->m_IsAnimPlaying&0xffff) && m_bInstanceVisible)
		{
			if (m_pPostProcessCallback1)		
				(*m_pPostProcessCallback1)(m_pInstance,m_pPostProcessCallbackData1);
		} 

#if (EXTREME_TEST==1)
		uint32 Valid9 = IsSkeletonValid();
		if (Valid9==0)
		{
			assert(0);
			CryError("CryAnimation: invalid skeleton after PostProcessCallback1: %s",m_pInstance->GetFilePath());
		}
#endif


	}





	if (m_bFullSkeletonUpdate)
	{
		uint32 numEntries = m_arrPostProcess.size();
		if (numEntries)
		{
			for (uint32 i=0; i<numEntries; i++)
			{
				int32 idx = m_arrPostProcess[i].m_JointIdx;
				QuatT qt  = m_arrPostProcess[i].m_RelativePose;
				if (!m_RelativePose[idx].IsEquivalent(qt, 0.0001f))
				{
					m_RelativePose[idx] = qt;
					m_bAllAbsoluteJointsValid = false;
					m_pSkeletonAnim->m_IsAnimPlaying |= 0x80000000;
				}
			}
		} 
	}

	m_arrPostProcess.resize(0);

	// Propogate relative to parent joint xforms into the absolute transforms.
	UpdateAbsoluteJointTransforms();

	// [MichaelS] Update the eye bones after post-processing in case additional joint rotations were applied.
	LookIKEyes(rPhysLocationNext);
#if (EXTREME_TEST==1)
	uint32 Valid = IsSkeletonValid();
	if (Valid==0)
	{
		assert(0);
		CryError("CryAnimation: invalid skeleton after LookIKEyes: %s",m_pInstance->GetFilePath());
	}
#endif


	/*
	int32 WeaponBoneIdx		= m_pModelSkeleton->m_IdxArray[eIM_WeaponBoneIdx];
	int32 Spine2Idx				= m_pModelSkeleton->m_IdxArray[eIM_Spine2Idx];

	QuatT absWeaponBone		= m_arrJoints[WeaponBoneIdx].m_AbsolutePose;
	QuatT absSpine2				= m_arrJoints[Spine2Idx].m_AbsolutePose;
	QuatT defSpine2				= m_parrModelJoints[Spine2Idx].m_DefaultAbsPose;

	float fColor[4] = {0,1,0,1};

	QuatT relative1 					= defSpine2*(absSpine2.GetInverted()*absWeaponBone);
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"relative rot1: %f (%f %f %f)",relative1.q.w, relative1.q.v.x,relative1.q.v.y,relative1.q.v.z );	
	g_YLine+=10.0f;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"relative pos1: (%f %f %f)", relative1.t.x,relative1.t.y,relative1.t.z );	
	g_YLine+=16.0f;
	*/



}


//////////////////////////////////////////////////////////////////////////
void CSkeletonPose::UpdateAbsoluteJointTransforms()
{
	if (m_bAllAbsoluteJointsValid && !m_bHackForceAbsoluteUpdate)
		return;

	m_bAllAbsoluteJointsValid = true;
	uint32 numJoints = m_AbsolutePose.size();
	if (numJoints==0)
		return;
	
		m_AbsolutePose[0] = m_RelativePose[0] * m_qAdditionalRotPos[0].q;

	for (uint32 i=1; i<numJoints; i++)
	{
		int32 p=m_parrModelJoints[i].m_idxParent;
		
		if (p>=0)
			m_AbsolutePose[i]	= m_AbsolutePose[p] * (m_RelativePose[i] * m_qAdditionalRotPos[i]);
		else
			m_AbsolutePose[i] = m_RelativePose[i] * m_qAdditionalRotPos[i];		
	}
}


#if defined(_CPU_SSE) && !defined(_DEBUG)
// simultaneous multiplication of 4 QuatT
__forceinline void FastQuatTMultiplicationTest(QuatTS& res0, QuatTS& res1,QuatTS& res2,QuatTS& res3,
																							 const QuatT& q0, const QuatT& q1,const QuatT& q2,const QuatT& q3,
																							 const QuatT& p0, const QuatT& p1, const QuatT& p2, const QuatT& p3
																							 )
{

	__m128 qx = _mm_set_ps(q0.q.v.x, q1.q.v.x, q2.q.v.x, q3.q.v.x);
	__m128 qy = _mm_set_ps(q0.q.v.y, q1.q.v.y, q2.q.v.y, q3.q.v.y);
	__m128 qz = _mm_set_ps(q0.q.v.z, q1.q.v.z, q2.q.v.z, q3.q.v.z);
	__m128 qw = _mm_set_ps(q0.q.w, q1.q.w, q2.q.w, q3.q.w);

	__m128 px = _mm_set_ps(p0.q.v.x, p1.q.v.x, p2.q.v.x, p3.q.v.x);
	__m128 py = _mm_set_ps(p0.q.v.y, p1.q.v.y, p2.q.v.y, p3.q.v.y);
	__m128 pz = _mm_set_ps(p0.q.v.z, p1.q.v.z, p2.q.v.z, p3.q.v.z);
	__m128 pw = _mm_set_ps(p0.q.w, p1.q.w, p2.q.w, p3.q.w);



	__m128 qtx = _mm_set_ps(q0.t.x, q1.t.x, q2.t.x, q3.t.x);
	__m128 qty = _mm_set_ps(q0.t.y, q1.t.y, q2.t.y, q3.t.y);
	__m128 qtz = _mm_set_ps(q0.t.z, q1.t.z, q2.t.z, q3.t.z);

	__m128 ptx = _mm_set_ps(p0.t.x, p1.t.x, p2.t.x, p3.t.x);
	__m128 pty = _mm_set_ps(p0.t.y, p1.t.y, p2.t.y, p3.t.y);
	__m128 ptz = _mm_set_ps(p0.t.z, p1.t.z, p2.t.z, p3.t.z);


	__m128 qwpw = _mm_mul_ps(qw, pw);
	__m128 qxpx = _mm_mul_ps(qx, px);
	__m128 qypy = _mm_mul_ps(qy, py);
	__m128 qzpz = _mm_mul_ps(qz, pz);
	__m128 resqw = _mm_sub_ps(qwpw, _mm_add_ps(qxpx, _mm_add_ps(qypy, qzpz)));

	__m128 qwpx = _mm_mul_ps(qw, px);
	__m128 pwqx = _mm_mul_ps(pw, qx);
	__m128 qwpy = _mm_mul_ps(qw, py);
	__m128 pwqy = _mm_mul_ps(pw, qy);
	__m128 qwpz = _mm_mul_ps(qw, pz);
	__m128 pwqz = _mm_mul_ps(pw, qz);

	__m128 qypz = _mm_mul_ps(qy, pz);
	__m128 qzpy = _mm_mul_ps(qz, py);
	__m128 resqx = _mm_add_ps(qwpx, _mm_add_ps(pwqx, _mm_sub_ps(qypz, qzpy)));

	__m128 qzpx = _mm_mul_ps(qz, px);
	__m128 qxpz = _mm_mul_ps(qx, pz);
	__m128 resqy = _mm_add_ps(qwpy, _mm_add_ps(pwqy, _mm_sub_ps(qzpx, qxpz)));

	__m128 qxpy = _mm_mul_ps(qx, py);
	__m128 qypx = _mm_mul_ps(qy, px);
	__m128 resqz = _mm_add_ps(qwpz, _mm_add_ps(pwqz, _mm_sub_ps(qxpy, qypx)));

	// normalization

	/*
	__m128 norm = _mm_add_ps(_mm_mul_ps(resqx ,resqx), _mm_add_ps(_mm_mul_ps(resqy, resqy), _mm_add_ps(_mm_mul_ps(resqz, resqz), _mm_mul_ps(resqw, resqw))));
	norm = _mm_sqrt_ps(norm);
	resqw = _mm_div_ps(resqw, norm);
	resqx = _mm_div_ps(resqx, norm);
	resqy = _mm_div_ps(resqy, norm);
	resqz = _mm_div_ps(resqz, norm);
	*/

	__m128 qwvx = _mm_mul_ps(qw, ptx);
	__m128 qwvy = _mm_mul_ps(qw, pty);
	__m128 qwvz = _mm_mul_ps(qw, ptz);

	__m128 r2x = _mm_add_ps(qwvx, _mm_sub_ps(_mm_mul_ps(qy, ptz), _mm_mul_ps(qz, pty)));
	__m128 r2y = _mm_add_ps(qwvy, _mm_sub_ps(_mm_mul_ps(qz, ptx), _mm_mul_ps(qx, ptz)));
	__m128 r2z = _mm_add_ps(qwvz, _mm_sub_ps(_mm_mul_ps(qx, pty), _mm_mul_ps(qy, ptx)));

	__m128 restx = _mm_sub_ps(_mm_mul_ps(r2z,qy), _mm_mul_ps(r2y,qz));
	__m128 resty = _mm_sub_ps(_mm_mul_ps(r2x,qz), _mm_mul_ps(r2z,qx));
	__m128 restz = _mm_sub_ps(_mm_mul_ps(r2y,qx), _mm_mul_ps(r2x,qy));

	restx = _mm_add_ps(qtx, _mm_add_ps(restx, _mm_add_ps(restx, ptx)));
	resty = _mm_add_ps(qty,_mm_add_ps(resty, _mm_add_ps(resty, pty)));
	restz = _mm_add_ps(qtz,_mm_add_ps(restz, _mm_add_ps(restz, ptz)));

#if defined(PS3)
	union __m128_ext
	{
		vec_float4 q;
		float m128_f32[4];
	} resqx_ext, resqy_ext, resqz_ext, resqw_ext, restx_ext, resty_ext, restz_ext;

	resqx_ext.q = (vec_float4)resqx;
	resqy_ext.q = (vec_float4)resqy;
	resqz_ext.q = (vec_float4)resqz;
	resqw_ext.q = (vec_float4)resqw;
	restx_ext.q = (vec_float4)restx;
	resty_ext.q = (vec_float4)resty;
	restz_ext.q = (vec_float4)restz;

	#define resqx resqx_ext
	#define resqy resqy_ext
	#define resqz resqz_ext
	#define resqw resqw_ext
	#define restx restx_ext
	#define resty resty_ext
	#define restz restz_ext
#endif
	res0.q.v.x = resqx.m128_f32[3];
	res1.q.v.x = resqx.m128_f32[2];
	res2.q.v.x = resqx.m128_f32[1];
	res3.q.v.x = resqx.m128_f32[0];

	res0.q.v.y = resqy.m128_f32[3];
	res1.q.v.y = resqy.m128_f32[2];
	res2.q.v.y = resqy.m128_f32[1];
	res3.q.v.y = resqy.m128_f32[0];

	res0.q.v.z = resqz.m128_f32[3];
	res1.q.v.z = resqz.m128_f32[2];
	res2.q.v.z = resqz.m128_f32[1];
	res3.q.v.z = resqz.m128_f32[0];

	res0.q.w = resqw.m128_f32[3];
	res1.q.w = resqw.m128_f32[2];
	res2.q.w = resqw.m128_f32[1];
	res3.q.w = resqw.m128_f32[0];

	res0.t.x = restx.m128_f32[3];
	res1.t.x = restx.m128_f32[2];
	res2.t.x = restx.m128_f32[1];
	res3.t.x = restx.m128_f32[0];

	res0.t.y = resty.m128_f32[3];
	res1.t.y = resty.m128_f32[2];
	res2.t.y = resty.m128_f32[1];
	res3.t.y = resty.m128_f32[0];

	res0.t.z = restz.m128_f32[3];
	res1.t.z = restz.m128_f32[2];
	res2.t.z = restz.m128_f32[1];
	res3.t.z = restz.m128_f32[0];
#if defined(PS3)
	#undef resqx
	#undef resqy
	#undef resqz
	#undef resqw
	#undef restx
	#undef resty
	#undef restz
#endif
}
#endif

///////////////////////////////////////////////////////////////////////////////
void CSkeletonPose::SetSkinningSkeletonMaster(f32 fScaling) 
{

#ifdef VTUNE_PROFILE 
	g_pISystem->VTuneResume();
#endif

	//float fColor[4] = {0,1,0,1};
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.4f, fColor, false,"fScaling: %f",fScaling);	
	//g_YLine+=16.0f;

	const char* name = m_pInstance->m_pModel->GetModelFilePath();

	int nCurrentFrameID = g_pCharacterManager->m_nUpdateCounter;//g_pIRenderer->GetFrameID(false);
	if (m_pInstance->m_LastUpdateFrameID_Post == nCurrentFrameID)
		return;
	//		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,name,	"several updates per frame: FrameID: %x",nCurrentFrameID);

  
	m_pInstance->m_iBackFrame = m_pInstance->m_iActiveFrame;
  
  if( m_pInstance->m_bUpdateMotionBlurSkinnning )
	  ++m_pInstance->m_iActiveFrame;

	if (m_pInstance->m_iActiveFrame >= MAX_MOTIONBLUR_FRAMES)
		m_pInstance->m_iActiveFrame = 0;

	uint32 iActiveFrame=m_pInstance->m_iActiveFrame;
	//----------------------------------------------------------
	//--  its a master skeleton. do the full initialization   --
	//----------------------------------------------------------
	uint32 numJoints = m_AbsolutePose.size();
	if (Console::GetInst().ca_SphericalSkinning) 
	{
		{
			QuatT DevInvPose	 = m_parrModelJoints[0].m_DefaultAbsPoseInverse;
			DevInvPose.t *= fScaling;
			QuatD qd = QuatD(m_AbsolutePose[0] * DevInvPose);
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][0].q		= qd.nq*fScaling;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][0].t.x	=	qd.dq.v.x;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][0].t.y	=	qd.dq.v.y;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][0].t.z	=	qd.dq.v.z;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][0].s	  =	qd.dq.w;
		}

		for (uint32 i=1; i<numJoints; i++)
		{
			QuatT DevInvPose	 = m_parrModelJoints[i].m_DefaultAbsPoseInverse;	DevInvPose.t *= fScaling;
			QuatD qd = QuatD(m_AbsolutePose[i] * DevInvPose);
			int32 p = m_parrModelJoints[i].m_idxParent; 
			f32 cosine =	qd.nq|m_pInstance->m_arrNewSkinQuat[iActiveFrame][p].q;
			if (cosine<0)
			{
				qd.nq= -qd.nq;
				qd.dq= -qd.dq;
			}
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i].q		= qd.nq*fScaling;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i].t.x	=	qd.dq.v.x;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i].t.y	=	qd.dq.v.y;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i].t.z	=	qd.dq.v.z;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i].s	  =	qd.dq.w;
		}
	}
	else
	{

		uint32 i = 0;
		uint32 total = numJoints/4;
		total *= 4;
		int rest = numJoints  - total;
		QuatTS q;
		
		for (uint32 i=0; i<numJoints; i++)
		{ 
			QuatT DevInvPose	 = m_parrModelJoints[i].m_DefaultAbsPoseInverse;
			DevInvPose.t *= fScaling;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i]		= m_AbsolutePose[i] * DevInvPose;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i].s	= fScaling;
		}

    m_pInstance->m_bUpdateMotionBlurSkinnning = true;

/*
		for (i = 0; i < total; i+=4)
		{
			
//#if defined(_CPU_SSE) && !defined(_DEBUG)
		//	FastQuatTMultiplicationTest(m_pInstance->m_arrNewSkinQuat[iActiveFrame][i], m_pInstance->m_arrNewSkinQuat[iActiveFrame][i+1], m_pInstance->m_arrNewSkinQuat[iActiveFrame][i+2], m_pInstance->m_arrNewSkinQuat[iActiveFrame][i+3],
		//		m_AbsolutePose[i], m_AbsolutePose[i+1], m_AbsolutePose[i+2], m_AbsolutePose[i+3], 
		//		m_parrModelJoints[i].m_DefaultAbsPoseInverse, m_parrModelJoints[i+1].m_DefaultAbsPoseInverse, m_parrModelJoints[i+2].m_DefaultAbsPoseInverse, m_parrModelJoints[i+3].m_DefaultAbsPoseInverse);
//#else

			QuatT DevPose0 = m_parrModelJoints[i+0].m_DefaultAbsPoseInverse;
			DevPose0.t *= fScaling;

			pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_DefaultAbsPoseInverse = pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_DefaultAbsPose.GetInverted();
			pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_DefaultAbsPoseInverse.q.Normalize();

			QuatT DevInvPose0 = m_parrModelJoints[i+0].m_DefaultAbsPoseInverse;
			DevInvPose0.t /= fScaling;
			QuatT DevInvPose1 = m_parrModelJoints[i+1].m_DefaultAbsPoseInverse;
			DevInvPose1.t /= fScaling;
			QuatT DevInvPose2 = m_parrModelJoints[i+2].m_DefaultAbsPoseInverse;
			DevInvPose2.t /= fScaling;
			QuatT DevInvPose3 = m_parrModelJoints[i+3].m_DefaultAbsPoseInverse;
			DevInvPose3.t /= fScaling;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i+0]	= m_AbsolutePose[i+0] * DevInvPose0;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i+1]	= m_AbsolutePose[i+1] * DevInvPose1;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i+2]	= m_AbsolutePose[i+2] * DevInvPose2;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i+3]	= m_AbsolutePose[i+3] * DevInvPose3;
//#endif

			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i+0].s = fScaling;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i+1].s = fScaling;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i+2].s = fScaling;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i+3].s = fScaling;
		}

		for (; i<numJoints; i++)
		{ 
			QuatT DevInvPose = m_parrModelJoints[i].m_DefaultAbsPoseInverse;
			DevInvPose.t /= fScaling;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i]		= m_AbsolutePose[i] * DevInvPose;
			m_pInstance->m_arrNewSkinQuat[iActiveFrame][i].s	= fScaling;
		}*/

	}


#ifdef VTUNE_PROFILE 
	g_pISystem->VTunePause();
#endif

} 





//////////////////////////////////////////////////////////////////////////
// calculates the bounding box for this instance using the moving skeleton
//////////////////////////////////////////////////////////////////////////
void CSkeletonPose::UpdateBBox(uint32 update)
{


	uint32 nErrorCode=0;
	const f32 fMaxBBox=13000.0f;
	m_AABB.min.Set(+99999.0f,+99999.0f,+99999.0f);
	m_AABB.max.Set(-99999.0f,-99999.0f,-99999.0f);


	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Update BBox:  NumBones:%d   Model: %s", numJoints, m_pInstance->GetModelFilePath() );	
	//g_YLine+=16.0f;

	uint32 numCGAJoints = m_arrCGAJoints.size();
	uint32 numJoints = m_AbsolutePose.size();
	if (numCGAJoints)
	{
		//if we have a CGA, then us this code
		for (uint32 i=0; i<numCGAJoints; ++i) 
		{
			if (m_arrCGAJoints[i].m_CGAObjectInstance)
			{
				QuatT bone = m_AbsolutePose[i];
				AABB aabb = AABB::CreateTransformedAABB(Matrix34(bone),m_arrCGAJoints[i].m_CGAObjectInstance->GetAABB());
				m_AABB.Add(aabb);	
			}
		}
	}
	else
	{
		//if we have a CHR, then us this code
		for (uint32 i=1; i<numJoints; i++)
			m_AABB.Add( m_AbsolutePose[i].t );
	}

	if (numCGAJoints==0 && numJoints<=1)
	{
		m_AABB.min=Vec3(-2,-2,-2);
		m_AABB.max=Vec3( 2, 2, 2);
	}
//----------------------------------------------------------------------------------------------------------

	uint32 minValid = m_AABB.min.IsValid();
	if (minValid==0) nErrorCode|=0x4000;
	uint32 maxValid = m_AABB.max.IsValid();
	if (maxValid==0) nErrorCode|=0x4000;

//----------------------------------------------------------------------------------------------------------

	IAttachmentManager* pIAttachmentManager = m_pInstance->GetIAttachmentManager();
	if (pIAttachmentManager)
	{
		uint32 numAttachments = pIAttachmentManager->GetAttachmentCount();
		for (uint32 i=0; i<numAttachments; i++)
		{
			CAttachment* pAttachment =  (CAttachment*)pIAttachmentManager->GetInterfaceByIndex(i);
			if (pAttachment)
			{
				if ( (pAttachment->m_Type==CA_BONE || pAttachment->m_Type==CA_FACE) && (pAttachment->IsAttachmentHidden()==0) ) 
				{
					IAttachmentObject* pIAttachmentObject = pAttachment->GetIAttachmentObject();
					if (pIAttachmentObject)
					{
						uint32 e = (pIAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Entity);
						uint32 c = (pIAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Character);
						uint32 s = (pIAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_StatObj);

//#if (EXTREME_TEST==1)
						if (e)
						{
							AABB eAABB = pIAttachmentObject->GetAABB();
							uint32 eminValid =	eAABB.min.IsValid();
							uint32 emaxValid =	eAABB.max.IsValid();
							if (eminValid==0 || emaxValid==0)
							{
								AnimWarning("CryAnimation: Invalid BBox of eAttachment_Entity" );
								assert(0);
							}
							uint32 nErrorCode=0;
							if (eAABB.max.x < eAABB.min.x) nErrorCode|=0x0001;
							if (eAABB.max.y < eAABB.min.y) nErrorCode|=0x0001;
							if (eAABB.max.z < eAABB.min.z) nErrorCode|=0x0001;
							if (eAABB.min.x < -fMaxBBox) nErrorCode|=0x0002;
							if (eAABB.min.y < -fMaxBBox) nErrorCode|=0x0004;
							if (eAABB.min.z < -fMaxBBox) nErrorCode|=0x0008;
							if (eAABB.max.x > +fMaxBBox) nErrorCode|=0x0010;
							if (eAABB.max.y > +fMaxBBox) nErrorCode|=0x0020;
							if (eAABB.max.z > +fMaxBBox) nErrorCode|=0x0040;
							assert(nErrorCode==0);
							if (nErrorCode)
							{
								IStatObj* pIStatObj =  pIAttachmentObject->GetIStatObj();
								if (pIStatObj)
								{
									const char* strFolderName = pIStatObj->GetFolderName();
									AnimWarning("CryAnimation: Invalid BBox of eAttachment_Entity (%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f). ModenPath: '%s' ObjPathName '%s'", 	eAABB.min.x, eAABB.min.y, eAABB.min.z, eAABB.max.x, eAABB.max.y, eAABB.max.z, 	m_pInstance->m_pModel->GetModelFilePath(), strFolderName);
								}
								else
								{
									AnimWarning("CryAnimation: Invalid BBox of eAttachment_Entity (%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f). ModenPath: '%s' ", 	eAABB.min.x, eAABB.min.y, eAABB.min.z, eAABB.max.x, eAABB.max.y, eAABB.max.z, 	m_pInstance->m_pModel->GetModelFilePath());
								}
								assert(0);
							}

						}

						if (c)
						{
							AABB cAABB = pIAttachmentObject->GetAABB();
							uint32 cminValid =	cAABB.min.IsValid();
							uint32 cmaxValid =	cAABB.max.IsValid();
							if (cminValid==0 || cmaxValid==0)
							{
								AnimWarning("CryAnimation: Invalid BBox of eAttachment_Character" );
								assert(0);
							}
							uint32 nErrorCode=0;
							if (cAABB.max.x < cAABB.min.x) nErrorCode|=0x0001;
							if (cAABB.max.y < cAABB.min.y) nErrorCode|=0x0001;
							if (cAABB.max.z < cAABB.min.z) nErrorCode|=0x0001;
							if (cAABB.min.x < -fMaxBBox) nErrorCode|=0x0002;
							if (cAABB.min.y < -fMaxBBox) nErrorCode|=0x0004;
							if (cAABB.min.z < -fMaxBBox) nErrorCode|=0x0008;
							if (cAABB.max.x > +fMaxBBox) nErrorCode|=0x0010;
							if (cAABB.max.y > +fMaxBBox) nErrorCode|=0x0020;
							if (cAABB.max.z > +fMaxBBox) nErrorCode|=0x0040;
							assert(nErrorCode==0);
							if (nErrorCode)
							{
								AnimWarning("CryAnimation: Invalid BBox of eAttachment_Character (%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f). ModenPath: '%s'  ErrorCode: %08x", 	cAABB.min.x, cAABB.min.y, cAABB.min.z, cAABB.max.x, cAABB.max.y, cAABB.max.z, 	m_pInstance->m_pModel->GetModelFilePath(), nErrorCode);
								assert(0);
							}
						}

						if (s)
						{
							AABB sAABB = pIAttachmentObject->GetAABB();
							uint32 sminValid =	sAABB.min.IsValid();
							uint32 smaxValid =	sAABB.max.IsValid();
							if (sminValid==0 || smaxValid==0)
							{
								AnimWarning("CryAnimation: Invalid BBox of eAttachment_StatObj" );
								assert(0);
							}
							uint32 nErrorCode=0;
							if (sAABB.max.x < sAABB.min.x) nErrorCode|=0x0001;
							if (sAABB.max.y < sAABB.min.y) nErrorCode|=0x0001;
							if (sAABB.max.z < sAABB.min.z) nErrorCode|=0x0001;
							if (sAABB.min.x < -fMaxBBox) nErrorCode|=0x0002;
							if (sAABB.min.y < -fMaxBBox) nErrorCode|=0x0004;
							if (sAABB.min.z < -fMaxBBox) nErrorCode|=0x0008;
							if (sAABB.max.x > +fMaxBBox) nErrorCode|=0x0010;
							if (sAABB.max.y > +fMaxBBox) nErrorCode|=0x0020;
							if (sAABB.max.z > +fMaxBBox) nErrorCode|=0x0040;
							assert(nErrorCode==0);
							if (nErrorCode)
							{
								AnimWarning("CryAnimation: Invalid BBox of eAttachment_StatObj (%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f). ModenPath: '%s'  ErrorCode: %08x", 	sAABB.min.x, sAABB.min.y, sAABB.min.z, sAABB.max.x, sAABB.max.y, sAABB.max.z, 	m_pInstance->m_pModel->GetModelFilePath(), nErrorCode);
								assert(0);
							}
						}
//#endif


						if (e|c|s)
						{
							AABB aabb = pIAttachmentObject->GetAABB();
							assert(aabb.min.IsValid());
							assert(aabb.max.IsValid());
							AABB taabb; taabb.SetTransformedAABB(pAttachment->GetAttModelRelative(),aabb);
							assert(taabb.min.IsValid());
							m_AABB.Add(taabb.min);
							assert(taabb.max.IsValid());
							m_AABB.Add(taabb.max);	
						}
					}
				}
			}
		}
	}


	if (m_AABB.max.x < m_AABB.min.x) nErrorCode=0x0001;
	if (m_AABB.max.y < m_AABB.min.y) nErrorCode=0x0001;
	if (m_AABB.max.z < m_AABB.min.z) nErrorCode=0x0001;

	minValid = m_AABB.min.IsValid();
	if (minValid==0) nErrorCode|=0x8000;
	maxValid = m_AABB.max.IsValid();
	if (maxValid==0) nErrorCode|=0x8000;


	if (m_AABB.min.x < -fMaxBBox) nErrorCode|=0x0002;
	if (m_AABB.min.y < -fMaxBBox) nErrorCode|=0x0004;
	if (m_AABB.min.z < -fMaxBBox) nErrorCode|=0x0008;
	if (m_AABB.max.x > +fMaxBBox) nErrorCode|=0x0010;
	if (m_AABB.max.y > +fMaxBBox) nErrorCode|=0x0020;
	if (m_AABB.max.z > +fMaxBBox) nErrorCode|=0x0040;
	assert(nErrorCode==0);

	Vec3 vSize=m_AABB.max-m_AABB.min;
	if (vSize.x<0.4f)
	{
		m_AABB.max.x+=0.2f;
		m_AABB.min.x-=0.2f;
	}
	if (vSize.y<0.4f)
	{
		m_AABB.max.y+=0.2f;
		m_AABB.min.y-=0.2f;
	}
	if (vSize.z<0.4f)
	{
		m_AABB.max.z+=0.2f;
		m_AABB.min.z-=0.2f;
	}

	if (nErrorCode)
	{
		AnimWarning("CryAnimation: Invalid BBox (%.3f,%.3f,%.3f)-(%.3f,%.3f,%.3f). ModenPath: '%s'  ErrorCode: %08x", 	m_AABB.min.x, m_AABB.min.y, m_AABB.min.z, m_AABB.max.x, m_AABB.max.y, m_AABB.max.z, 	m_pInstance->m_pModel->GetModelFilePath(), nErrorCode);
		assert(!"Invalid BBox");
	}


	if (nErrorCode)
	{
		m_AABB.min=Vec3(-2,-2,-2);
		m_AABB.max=Vec3( 2, 2, 2);
	}


}





//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
void CSkeletonPose::DrawArrow( const QuatT& location, f32 length, ColorB col )
{
	SAuxGeomRenderFlags renderFlags( e_Def3DPublicRenderflags );
	g_pAuxGeom->SetRenderFlags( renderFlags );

	Vec3 absAxisY	=	location.q.GetColumn1();

	const f32 scale=1.0f;
	const f32 size=0.009f;
	AABB yaabb = AABB(Vec3(-size*scale, -0.0f*scale, -size*scale),Vec3(size*scale,	 length*scale, size*scale));

	OBB obb =	OBB::CreateOBBfromAABB( Matrix33(location.q), yaabb );
	g_pAuxGeom->DrawOBB(obb,location.t,1,col,eBBD_Extremes_Color_Encoded);
	g_pAuxGeom->DrawCone(location.t+absAxisY*length*scale,absAxisY,0.03f,0.15f,col);
}

