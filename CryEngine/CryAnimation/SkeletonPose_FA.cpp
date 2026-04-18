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
#include <float.h>

#define SAFEANIM_IDX(a)	((a)!=0xffffffff ? (a) : 0)




//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
//------------------------------------------------------------------------
void CSkeletonPose::FootAnchoring( const SFootPlant& BFPlant, uint8 Foot, const QuatTS& rAnimCharLocationCurr )
{

	if (m_pSkeletonAnim->m_arrLayer_AFIFO[0].size()==0)
		return;

	uint32 FVE  = m_FootAnchor.m_UseFootAnchoring;
	if (FVE==0 || m_timeStandingUp >= 0.0f) 
		return; 
	if (m_bPhysicsRelinquished==1)
		return; //its under control of physics

	uint8 FootPlant = Foot;

	f32 AnimTimeLayer0	= m_pSkeletonAnim->m_arrLayer_AFIFO[0][0].m_fAnimTime; //we assume LMGs are all in layer 0

	f32 tl = AnimTimeLayer0+0.5f; if (tl>1.0f) tl=tl-1.0f;
	f32 tr = AnimTimeLayer0;
	if ( tl>BFPlant.m_LHeelStart && tl<BFPlant.m_LHeelEnd ) FootPlant |= LHEEL;
	if ( tl>BFPlant.m_LToe0Start && tl<BFPlant.m_LToe0End ) FootPlant |= LTOE0;
	if ( tr>BFPlant.m_RHeelStart && tr<BFPlant.m_RHeelEnd ) FootPlant |= RHEEL;
	if ( tr>BFPlant.m_RToe0Start && tr<BFPlant.m_RToe0End ) FootPlant |= RTOE0;

	//float fColor[4] = {1,0,1,1};
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"FootAnchoring:  StandUp: %f  Relinquish: %d ",m_timeStandingUp,m_bPhysicsRelinquished );	
	//g_YLine+=10;


	Vec3 Toe0Slide=Vec3(ZERO);
	Vec3 HeelSlide=Vec3(ZERO);

	int32 PelvisIdx		= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_PelvisIdx]);

	int32 lThighIdx			= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LThighIdx]);
	int32 lKneeIdx			= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LKneeIdx]);
	int32 lKneeEndIdx		= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LKneeEndIdx]);
	int32 lCalfIdx			= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LCalfIdx]);
	int32 lFootIdx			= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LFootIdx]);
	int32 lHeelIdx			= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LHeelIdx]);
	int32 lToe0Idx			= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LToe0Idx]);
	int32 lToe0NubIdx		= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LToe0NubIdx]);


	int32 rThighIdx			= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RThighIdx]);
	int32 rKneeIdx			= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RKneeIdx]);
	int32 rKneeEndIdx		= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RKneeEndIdx]);
	int32 rCalfIdx			= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RCalfIdx]);
	int32 rFootIdx			= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RFootIdx]);
	int32 rHeelIdx			= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RHeelIdx]);
	int32 rToe0Idx			= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RToe0Idx]);
	int32 rToe0NubIdx		= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RToe0NubIdx]);

	uint32 LeftPlant=0;
	uint32 RightPlant=0;
	uint32 SinglyConstraint=0;
	if (FootPlant&LTOE0 && FootPlant&LHEEL)
	{
		LeftPlant=1; //DUBLY CONSTRAINT on the left foot
		m_FootAnchor.m_LAnkleIntensity=1.0f; 

		Vec2 xaxis0 = Vec2(m_FootAnchor.m_AbsoluteLToe0.t-m_FootAnchor.m_AbsoluteLHeel.t);	//xaxis0.z=0; xaxis0.Normalize(); 
		Vec2 xaxis1 = Vec2(m_FootAnchor.m_LastLToe-m_FootAnchor.m_LastLHeel);								//xaxis1.z=0; xaxis1.NormalizeSafe(Vec3(1,0,0)); 
		f32 yaw	= Ang3::CreateRadZ(xaxis0,xaxis1);
		static float minYaw = DEG2RAD(-45.0f);
		static float maxYaw = DEG2RAD(+45.0f);
		yaw = CLAMP(yaw, minYaw, maxYaw);
		m_FootAnchor.m_LAnkle = Quat::CreateRotationZ(yaw*0.5f);

		m_FootAnchor.m_AbsoluteLThigh.q					= m_FootAnchor.m_LAnkle*m_FootAnchor.m_AbsoluteLThigh.q;
		m_RelativePose[lThighIdx]		= m_FootAnchor.m_AbsolutePelvis.GetInverted() * m_FootAnchor.m_AbsoluteLThigh;
		m_FootAnchor.m_AbsoluteLFoot.q					= m_FootAnchor.m_LAnkle*m_FootAnchor.m_AbsoluteLFoot.q;
		m_RelativePose[lFootIdx]		= m_FootAnchor.m_AbsoluteLCalf.GetInverted() * m_FootAnchor.m_AbsoluteLFoot;

		m_FootAnchor.m_AbsoluteLCalf		= m_FootAnchor.m_AbsoluteLThigh * m_RelativePose[lCalfIdx];
		m_FootAnchor.m_AbsoluteLFoot		= m_FootAnchor.m_AbsoluteLCalf * m_RelativePose[lFootIdx];
		m_FootAnchor.m_AbsoluteLHeel		= m_FootAnchor.m_AbsoluteLFoot * m_RelativePose[lHeelIdx];
		m_FootAnchor.m_AbsoluteLToe0		= m_FootAnchor.m_AbsoluteLFoot * m_RelativePose[lToe0Idx];
		m_FootAnchor.m_AbsoluteLNub0		= m_FootAnchor.m_AbsoluteLToe0 * m_RelativePose[lToe0NubIdx];

		if (Console::GetInst().ca_DebugFootPlants)
		{
			float fColor[4] = {0,1,0,1};
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"dubly constraint on the left foot: %f",yaw );	g_YLine+=16.0f;
		}
		HeelSlide =(m_FootAnchor.m_AbsoluteLHeel.t-m_FootAnchor.m_LastLHeel);
		//HeelSlide+=((m_AbsoluteLHeel.t-m_LastLHeel)+(m_AbsoluteLToe0.t-m_LastLToe))*0.5f;
		HeelSlide.z=0;
		EaseOut_RAnkle();
	} 
	else 
	{
		if (FootPlant&RTOE0 && FootPlant&RHEEL)
		{
			RightPlant=1;  //DUBLY CONSTRAINT on the right foot
			m_FootAnchor.m_RAnkleIntensity=1.0f; 
			Vec2 xaxis0 = Vec2(m_FootAnchor.m_AbsoluteRToe0.t-m_FootAnchor.m_AbsoluteRHeel.t);	//xaxis0.z=0; xaxis0.Normalize(); 
			Vec2 xaxis1 = Vec2(m_FootAnchor.m_LastRToe-m_FootAnchor.m_LastRHeel);				//xaxis1.z=0; xaxis1.NormalizeSafe(Vec3(1,0,0)); 
			f32 yaw	= Ang3::CreateRadZ(xaxis0,xaxis1);
			if (yaw<-1.8f) yaw=-1.8f;
			if (yaw>+1.2f) yaw=+1.2f;
			m_FootAnchor.m_RAnkle = Quat::CreateRotationZ(yaw*0.5f);

			m_FootAnchor.m_AbsoluteRThigh.q					= m_FootAnchor.m_RAnkle*m_FootAnchor.m_AbsoluteRThigh.q;
			m_RelativePose[rThighIdx]			= m_FootAnchor.m_AbsolutePelvis.GetInverted() * m_FootAnchor.m_AbsoluteRThigh;
			m_FootAnchor.m_AbsoluteRFoot.q					= m_FootAnchor.m_RAnkle*m_FootAnchor.m_AbsoluteRFoot.q;
			m_RelativePose[rFootIdx]			= m_FootAnchor.m_AbsoluteRCalf.GetInverted() * m_FootAnchor.m_AbsoluteRFoot;

			m_FootAnchor.m_AbsoluteRCalf		= m_FootAnchor.m_AbsoluteRThigh * m_RelativePose[rCalfIdx];
			m_FootAnchor.m_AbsoluteRFoot		= m_FootAnchor.m_AbsoluteRCalf  * m_RelativePose[rFootIdx];
			m_FootAnchor.m_AbsoluteRHeel		= m_FootAnchor.m_AbsoluteRFoot  * m_RelativePose[rHeelIdx];
			m_FootAnchor.m_AbsoluteRToe0		= m_FootAnchor.m_AbsoluteRFoot  * m_RelativePose[rToe0Idx];
			m_FootAnchor.m_AbsoluteRNub0		= m_FootAnchor.m_AbsoluteRToe0  * m_RelativePose[rToe0NubIdx];

			if (Console::GetInst().ca_DebugFootPlants)
			{
				float fColor[4] = {0,1,0,1};
				g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"dubly constraint on the right foot: %f",yaw );	g_YLine+=16.0f;
			}
			HeelSlide = (m_FootAnchor.m_AbsoluteRHeel.t-m_FootAnchor.m_LastRHeel);
			//	HeelSlide+=((m_AbsoluteRHeel.t-m_LastRHeel)+(m_AbsoluteRToe0.t-m_LastRToe))*0.5f;
			HeelSlide.z=0;
			EaseOut_LAnkle();
		} 
		else 
		{
			EaseOut_LAnkle();
			EaseOut_RAnkle();
			//check if we have at least a single constraint
			if (FootPlant & LTOE0)
				Toe0Slide+=m_FootAnchor.m_AbsoluteLToe0.t-m_FootAnchor.m_LastLToe;
			if (FootPlant & RTOE0) 
				Toe0Slide+=m_FootAnchor.m_AbsoluteRToe0.t-m_FootAnchor.m_LastRToe;


			if (Toe0Slide.GetLength()<0.000001f)
			{
				if (FootPlant&LHEEL) 
					HeelSlide+=m_FootAnchor.m_AbsoluteLHeel.t-m_FootAnchor.m_LastLHeel;
				if (FootPlant&RHEEL) 
					HeelSlide+=m_FootAnchor.m_AbsoluteRHeel.t-m_FootAnchor.m_LastRHeel;
			}

			HeelSlide.z=0;
			Toe0Slide.z=0;
		}
	}




	//-----------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------

	m_AbsolutePose[0]						= m_RelativePose[0];
	m_AbsolutePose[PelvisIdx]		= m_AbsolutePose[0] * m_RelativePose[PelvisIdx];

	m_AbsolutePose[rThighIdx]		= m_AbsolutePose[PelvisIdx] * m_RelativePose[rThighIdx];
	m_AbsolutePose[rCalfIdx]		= m_AbsolutePose[rThighIdx] * m_RelativePose[rCalfIdx];
	m_AbsolutePose[rFootIdx]		= m_AbsolutePose[rCalfIdx]	* m_RelativePose[rFootIdx];
	m_AbsolutePose[rHeelIdx]		= m_AbsolutePose[rFootIdx]	* m_RelativePose[rHeelIdx];
	m_AbsolutePose[rToe0Idx]		= m_AbsolutePose[rFootIdx]	* m_RelativePose[rToe0Idx];
	m_AbsolutePose[rToe0NubIdx]	= m_AbsolutePose[rToe0Idx]	* m_RelativePose[rToe0NubIdx];

	m_AbsolutePose[lThighIdx]		= m_AbsolutePose[PelvisIdx] * m_RelativePose[lThighIdx];
	m_AbsolutePose[lCalfIdx]		= m_AbsolutePose[lThighIdx] * m_RelativePose[lCalfIdx];
	m_AbsolutePose[lFootIdx]		= m_AbsolutePose[lCalfIdx]  * m_RelativePose[lFootIdx];
	m_AbsolutePose[lHeelIdx]		= m_AbsolutePose[lFootIdx]  * m_RelativePose[lHeelIdx];
	m_AbsolutePose[lToe0Idx]		= m_AbsolutePose[lFootIdx]  * m_RelativePose[lToe0Idx];
	m_AbsolutePose[lToe0NubIdx]	= m_AbsolutePose[lToe0Idx]  * m_RelativePose[lToe0NubIdx];


	Vec3 footslide = -(Toe0Slide+HeelSlide);
	f32 slide=footslide.GetLength();

	const f32 MaxDeviation=0.2f;
	if (slide>MaxDeviation)
	{ 
		footslide.Normalize(); 
		footslide*=MaxDeviation;
	}

	m_FootAnchor.m_vFootSlide = footslide;

	/*
	Vec3 wpos = m_pInstance->m_EntityQuatT.t;
	g_pAuxGeom->DrawLine(wpos,RGBA8(0x7f,0xff,0xff,0x00), wpos+footslide*100,RGBA8(0x7f,0xff,0xff,0x00) );

	g_YLine+=32.0f;		
	float fColor[4] = {1,0,1,1};
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"footslide: %f %f ",footslide.x,footslide.y );	
	g_YLine+=10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"ToeSlide: %f %f ",Toe0Slide.x,Toe0Slide.y );	
	g_YLine+=10;
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"HeelSlide: %f %f ",HeelSlide.x,HeelSlide.y );	
	g_YLine+=10;
	*/
	if (Console::GetInst().ca_DrawFootPlants)
	{

		g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
		static Ang3 angle1(0,0,0); 
		static Ang3 angle2(0,0,0); 
		static Ang3 angle3(0,0,0); 
		angle1 += Ang3(0.01f,0.02f,0.03f);
		angle2 += Ang3(0.01f,-0.12f,0.03f);
		angle3 += Ang3(0.01f,0.01f,-0.03f);
		AABB aabb = AABB(Vec3(-0.05f,-0.05f,-0.05f),Vec3(+0.05f,+0.05f,+0.05f));
		OBB obb1=OBB::CreateOBBfromAABB( Matrix33::CreateRotationXYZ(angle1),aabb );
		OBB obb2=OBB::CreateOBBfromAABB( Matrix33::CreateRotationXYZ(angle2),aabb );
		OBB obb3=OBB::CreateOBBfromAABB( Matrix33::CreateRotationXYZ(angle3),aabb );


		QuatT offset = m_pSkeletonAnim->GetRelMovement();
		if (m_pSkeletonAnim->m_CharEditMode==1)
			offset.t = Vec3(0,0,0);

		QuatT NewAC = QuatT(rAnimCharLocationCurr)*offset;
		if (m_pSkeletonAnim->m_CharEditMode==0)
			NewAC.t += m_FootAnchor.m_vFootSlide;

		m_NewTempRenderMat	=	Matrix34(NewAC);

		if (FootPlant&LHEEL)
		{
			Vec3 vLHeel = m_NewTempRenderMat*m_AbsolutePose[lHeelIdx].t;
			g_pAuxGeom->DrawOBB(obb1,vLHeel,0,RGBA8(0x00,0x00,0xff,0xff), eBBD_Extremes_Color_Encoded);
			g_pAuxGeom->DrawOBB(obb2,vLHeel,0,RGBA8(0x00,0x00,0xff,0xff), eBBD_Extremes_Color_Encoded);
			g_pAuxGeom->DrawOBB(obb3,vLHeel,0,RGBA8(0x00,0x00,0xff,0xff), eBBD_Extremes_Color_Encoded);
		}
		if (FootPlant&RHEEL)
		{
			Vec3 vRHeel = m_NewTempRenderMat*m_AbsolutePose[rHeelIdx].t;
			g_pAuxGeom->DrawOBB(obb1,vRHeel,0,RGBA8(0x00,0x00,0xff,0xff), eBBD_Extremes_Color_Encoded);
			g_pAuxGeom->DrawOBB(obb2,vRHeel,0,RGBA8(0x00,0x00,0xff,0xff), eBBD_Extremes_Color_Encoded);
			g_pAuxGeom->DrawOBB(obb3,vRHeel,0,RGBA8(0x00,0x00,0xff,0xff), eBBD_Extremes_Color_Encoded);
		}

		if (FootPlant&LTOE0)
		{
			Vec3 vLToe0 = m_NewTempRenderMat*m_AbsolutePose[lToe0Idx].t;
			g_pAuxGeom->DrawOBB(obb1,vLToe0,0,RGBA8(0xff,0xff,0x00,0xff), eBBD_Extremes_Color_Encoded);
			g_pAuxGeom->DrawOBB(obb2,vLToe0,0,RGBA8(0xff,0xff,0x00,0xff), eBBD_Extremes_Color_Encoded);
			g_pAuxGeom->DrawOBB(obb3,vLToe0,0,RGBA8(0xff,0xff,0x00,0xff), eBBD_Extremes_Color_Encoded);
		}
		if (FootPlant&RTOE0)
		{
			Vec3 vRToe0 = m_NewTempRenderMat*m_AbsolutePose[rToe0Idx].t;
			g_pAuxGeom->DrawOBB(obb1,vRToe0,0,RGBA8(0xff,0xff,0x00,0xff), eBBD_Extremes_Color_Encoded);
			g_pAuxGeom->DrawOBB(obb2,vRToe0,0,RGBA8(0xff,0xff,0x00,0xff), eBBD_Extremes_Color_Encoded);
			g_pAuxGeom->DrawOBB(obb3,vRToe0,0,RGBA8(0xff,0xff,0x00,0xff), eBBD_Extremes_Color_Encoded);
		}


	}

}



#define EASEOUT (3.0f)

void CSkeletonPose::EaseOut_LAnkle()
{

	//	return;

	int32 lThighIdx		= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LThighIdx]);
	int32 lKneeIdx		= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LKneeIdx]);
	int32 lKneeEndIdx	= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LKneeEndIdx]);
	int32 lCalfIdx		= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LCalfIdx]);
	int32 lFootIdx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LFootIdx]);
	int32 lHeelIdx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LHeelIdx]);
	int32 lToe0Idx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LToe0Idx]);
	int32 lToe0NubIdx	=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LToe0NubIdx]);
	assert(m_FootAnchor.m_LAnkleIntensity<=1.0f);
	m_FootAnchor.m_LAnkleIntensity -= EASEOUT*fabsf(m_pInstance->m_fDeltaTime*m_pSkeletonAnim->m_arrLayerSpeedMultiplier[0]);
	if (m_FootAnchor.m_LAnkleIntensity<0) m_FootAnchor.m_LAnkleIntensity=0;
	if (m_FootAnchor.m_LAnkleIntensity)
	{
		f32 t = -(2*m_FootAnchor.m_LAnkleIntensity*m_FootAnchor.m_LAnkleIntensity*m_FootAnchor.m_LAnkleIntensity - 3*m_FootAnchor.m_LAnkleIntensity*m_FootAnchor.m_LAnkleIntensity);
		assert(t<=1.0f);
		assert(t>=0.0f);
		/*	float fColor[4] = {0,1,0,1};
		extern float g_YLine;
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"EaseOut_LAnkle %f",t);	g_YLine+=16.0f;*/

		m_FootAnchor.m_AbsoluteLThigh.q				= Quat::CreateNlerp(IDENTITY,m_FootAnchor.m_LAnkle,t)*m_FootAnchor.m_AbsoluteLThigh.q;
		m_RelativePose[lThighIdx]		= m_FootAnchor.m_AbsolutePelvis.GetInverted() * m_FootAnchor.m_AbsoluteLThigh;
		m_FootAnchor.m_AbsoluteLFoot.q				= Quat::CreateNlerp(IDENTITY,m_FootAnchor.m_LAnkle,t)*m_FootAnchor.m_AbsoluteLFoot.q;
		m_RelativePose[lFootIdx]		= m_FootAnchor.m_AbsoluteLCalf.GetInverted() * m_FootAnchor.m_AbsoluteLFoot;

		m_FootAnchor.m_AbsoluteLCalf		= m_FootAnchor.m_AbsoluteLThigh * m_RelativePose[lCalfIdx];
		m_FootAnchor.m_AbsoluteLFoot		= m_FootAnchor.m_AbsoluteLCalf  * m_RelativePose[lFootIdx];
		m_FootAnchor.m_AbsoluteLHeel		= m_FootAnchor.m_AbsoluteLFoot  * m_RelativePose[lHeelIdx];
		m_FootAnchor.m_AbsoluteLToe0		= m_FootAnchor.m_AbsoluteLFoot  * m_RelativePose[lToe0Idx];
		m_FootAnchor.m_AbsoluteLNub0		= m_FootAnchor.m_AbsoluteLToe0  * m_RelativePose[lToe0NubIdx];
		if (lKneeIdx>0 && lKneeEndIdx>0)
		{
			m_AbsolutePose[lKneeIdx]		= m_AbsolutePose[lThighIdx]	* m_RelativePose[lKneeIdx];
			m_AbsolutePose[lKneeEndIdx]	= m_AbsolutePose[lKneeIdx]	* m_RelativePose[lKneeEndIdx];
		}
	}
}

void CSkeletonPose::EaseOut_RAnkle()
{
	//	return;
	int32 rThighIdx		= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RThighIdx]);
	int32 rKneeIdx		= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RKneeIdx]);
	int32 rKneeEndIdx	= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RKneeEndIdx]);
	int32 rCalfIdx		= SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RCalfIdx]);
	int32 rFootIdx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RFootIdx]);
	int32 rHeelIdx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RHeelIdx]);
	int32 rToe0Idx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RToe0Idx]);
	int32 rToe0NubIdx	=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RToe0NubIdx]);
	assert(m_FootAnchor.m_RAnkleIntensity<=1.0f);
	m_FootAnchor.m_RAnkleIntensity -= EASEOUT*fabsf(m_pInstance->m_fDeltaTime*m_pSkeletonAnim->m_arrLayerSpeedMultiplier[0]);
	if (m_FootAnchor.m_RAnkleIntensity<0) m_FootAnchor.m_RAnkleIntensity=0;
	if (m_FootAnchor.m_RAnkleIntensity)
	{
		f32 t = -(2*m_FootAnchor.m_RAnkleIntensity*m_FootAnchor.m_RAnkleIntensity*m_FootAnchor.m_RAnkleIntensity - 3*m_FootAnchor.m_RAnkleIntensity*m_FootAnchor.m_RAnkleIntensity);
		assert(t<=1.0f);
		assert(t>=0.0f);
		/*float fColor[4] = {0,1,0,1};
		extern float g_YLine;
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"EaseOut_RAnkle %f",t);	g_YLine+=16.0f;
		*/

		m_FootAnchor.m_AbsoluteRThigh.q				= Quat::CreateNlerp(IDENTITY,m_FootAnchor.m_RAnkle,t)*m_FootAnchor.m_AbsoluteRThigh.q;
		m_RelativePose[rThighIdx]	= m_FootAnchor.m_AbsolutePelvis.GetInverted() * m_FootAnchor.m_AbsoluteRThigh;
		m_FootAnchor.m_AbsoluteRFoot.q				= Quat::CreateNlerp(IDENTITY,m_FootAnchor.m_RAnkle,t)*m_FootAnchor.m_AbsoluteRFoot.q;
		m_RelativePose[rFootIdx]	= m_FootAnchor.m_AbsoluteRCalf.GetInverted() * m_FootAnchor.m_AbsoluteRFoot;

		m_FootAnchor.m_AbsoluteRCalf		= m_FootAnchor.m_AbsoluteRThigh * m_RelativePose[rCalfIdx];
		m_FootAnchor.m_AbsoluteRFoot		= m_FootAnchor.m_AbsoluteRCalf  * m_RelativePose[rFootIdx];
		m_FootAnchor.m_AbsoluteRHeel		= m_FootAnchor.m_AbsoluteRFoot  * m_RelativePose[rHeelIdx];
		m_FootAnchor.m_AbsoluteRToe0		= m_FootAnchor.m_AbsoluteRFoot  * m_RelativePose[rToe0Idx];
		m_FootAnchor.m_AbsoluteRNub0		= m_FootAnchor.m_AbsoluteRToe0  * m_RelativePose[rToe0NubIdx];
		if (rKneeIdx>0 && rKneeEndIdx>0)
		{
			m_AbsolutePose[rKneeIdx]		= m_AbsolutePose[rThighIdx]	* m_RelativePose[rKneeIdx];
			m_AbsolutePose[rKneeEndIdx]	= m_AbsolutePose[rKneeIdx]	* m_RelativePose[rKneeEndIdx];
		}
	}
}



void CSkeletonPose::AbsoluteLegPosition( const QuatTS& rAnimCharLocationOld )
{

	uint32 mPidx=SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_PelvisIdx]);

	uint32 lThighIdx	=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LThighIdx]);
	uint32 lCalfIdx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LCalfIdx]);
	uint32 lFootIdx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LFootIdx]);
	uint32 lHeelIdx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LHeelIdx]);
	uint32 lToe0Idx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LToe0Idx]);
	uint32 lToe0NubIdx=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_LToe0NubIdx]);

	uint32 rThighIdx	=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RThighIdx]);
	uint32 rCalfIdx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RCalfIdx]);
	uint32 rFootIdx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RFootIdx]);
	uint32 rHeelIdx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RHeelIdx]);
	uint32 rToe0Idx		=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RToe0Idx]);
	uint32 rToe0NubIdx=	SAFEANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_RToe0NubIdx]);


	Vec3 trans = rAnimCharLocationOld.q * m_pSkeletonAnim->m_BlendedRoot.m_vRelTranslation;
	m_FootAnchor.m_absSkeletonInWorld -= trans;


	QuatT Root = m_RelativePose[0];
	QuatT Pelvis = m_RelativePose[mPidx];

	m_FootAnchor.m_AbsoluteRoot.q			= m_RelativePose[0].q * rAnimCharLocationOld.q;

	m_FootAnchor.m_AbsoluteRoot.t.x		=	m_FootAnchor.m_absSkeletonInWorld.x;
	m_FootAnchor.m_AbsoluteRoot.t.y		=	m_FootAnchor.m_absSkeletonInWorld.y;
	m_FootAnchor.m_AbsoluteRoot.t.z		=	m_RelativePose[0].t.z;

	m_FootAnchor.m_AbsolutePelvis		= m_FootAnchor.m_AbsoluteRoot   * m_RelativePose[mPidx]; //m_RelativePelvis;

	m_FootAnchor.m_AbsoluteLThigh		= m_FootAnchor.m_AbsolutePelvis * m_RelativePose[lThighIdx];
	m_FootAnchor.m_AbsoluteLCalf		= m_FootAnchor.m_AbsoluteLThigh * m_RelativePose[lCalfIdx];
	m_FootAnchor.m_AbsoluteLFoot		= m_FootAnchor.m_AbsoluteLCalf  * m_RelativePose[lFootIdx];
	m_FootAnchor.m_AbsoluteLHeel		= m_FootAnchor.m_AbsoluteLFoot  * m_RelativePose[lHeelIdx];
	m_FootAnchor.m_AbsoluteLToe0		= m_FootAnchor.m_AbsoluteLFoot  * m_RelativePose[lToe0Idx];
	m_FootAnchor.m_AbsoluteLNub0		= m_FootAnchor.m_AbsoluteLToe0  * m_RelativePose[lToe0NubIdx];

	m_FootAnchor.m_AbsoluteRThigh		= m_FootAnchor.m_AbsolutePelvis * m_RelativePose[rThighIdx];
	m_FootAnchor.m_AbsoluteRCalf		= m_FootAnchor.m_AbsoluteRThigh * m_RelativePose[rCalfIdx];
	m_FootAnchor.m_AbsoluteRFoot		= m_FootAnchor.m_AbsoluteRCalf  * m_RelativePose[rFootIdx];
	m_FootAnchor.m_AbsoluteRHeel		= m_FootAnchor.m_AbsoluteRFoot  * m_RelativePose[rHeelIdx];
	m_FootAnchor.m_AbsoluteRToe0		= m_FootAnchor.m_AbsoluteRFoot  * m_RelativePose[rToe0Idx];
	m_FootAnchor.m_AbsoluteRNub0		= m_FootAnchor.m_AbsoluteRToe0  * m_RelativePose[rToe0NubIdx];

	m_FootAnchor.m_AbsoluteRoot.q.Normalize();
	m_FootAnchor.m_AbsolutePelvis.q.Normalize();

	m_FootAnchor.m_AbsoluteLThigh.q.Normalize();
	m_FootAnchor.m_AbsoluteLCalf.q.Normalize();
	m_FootAnchor.m_AbsoluteLFoot.q.Normalize();
	m_FootAnchor.m_AbsoluteLHeel.q.Normalize();
	m_FootAnchor.m_AbsoluteLToe0.q.Normalize();
	m_FootAnchor.m_AbsoluteLNub0.q.Normalize();

	m_FootAnchor.m_AbsoluteRThigh.q.Normalize();
	m_FootAnchor.m_AbsoluteRCalf.q.Normalize();
	m_FootAnchor.m_AbsoluteRFoot.q.Normalize();
	m_FootAnchor.m_AbsoluteRHeel.q.Normalize();
	m_FootAnchor.m_AbsoluteRToe0.q.Normalize();
	m_FootAnchor.m_AbsoluteRNub0.q.Normalize();


	/*
	g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
	g_pAuxGeom->DrawLine(m_AbsoluteLThigh.t,RGBA8(0xff,0x00,0x00,0),	m_AbsolutePelvis.t,RGBA8(0xff,0xff,0xff,0));
	g_pAuxGeom->DrawLine(m_AbsoluteLCalf.t, RGBA8(0xff,0x00,0x00,0),	m_AbsoluteLThigh.t,RGBA8(0xff,0xff,0xff,0));
	g_pAuxGeom->DrawLine(m_AbsoluteLFoot.t, RGBA8(0xff,0x00,0x00,0),	m_AbsoluteLCalf.t, RGBA8(0xff,0xff,0xff,0));
	g_pAuxGeom->DrawLine(m_AbsoluteLHeel.t, RGBA8(0xff,0x00,0x00,0),	m_AbsoluteLFoot.t, RGBA8(0xff,0xff,0xff,0));
	g_pAuxGeom->DrawLine(m_AbsoluteLToe0.t, RGBA8(0xff,0x00,0x00,0),	m_AbsoluteLFoot.t, RGBA8(0xff,0xff,0xff,0));
	g_pAuxGeom->DrawLine(m_AbsoluteLNub0.t, RGBA8(0xff,0x00,0x00,0),	m_AbsoluteLToe0.t, RGBA8(0xff,0xff,0xff,0));

	g_pAuxGeom->DrawLine(m_AbsoluteRThigh.t,RGBA8(0xff,0x00,0x00,0),	m_AbsolutePelvis.t,RGBA8(0xff,0xff,0xff,0));
	g_pAuxGeom->DrawLine(m_AbsoluteRCalf.t, RGBA8(0xff,0x00,0x00,0),  m_AbsoluteRThigh.t,RGBA8(0xff,0xff,0xff,0));
	g_pAuxGeom->DrawLine(m_AbsoluteRFoot.t, RGBA8(0xff,0x00,0x00,0),  m_AbsoluteRCalf.t, RGBA8(0xff,0xff,0xff,0));
	g_pAuxGeom->DrawLine(m_AbsoluteRHeel.t, RGBA8(0xff,0x00,0x00,0),  m_AbsoluteRFoot.t, RGBA8(0xff,0xff,0xff,0));
	g_pAuxGeom->DrawLine(m_AbsoluteRToe0.t, RGBA8(0xff,0x00,0x00,0),  m_AbsoluteRFoot.t, RGBA8(0xff,0xff,0xff,0));
	g_pAuxGeom->DrawLine(m_AbsoluteRNub0.t, RGBA8(0xff,0x00,0x00,0),  m_AbsoluteRToe0.t, RGBA8(0xff,0xff,0xff,0));
	*/
}

