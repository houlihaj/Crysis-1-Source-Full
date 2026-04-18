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



//--------------------------------------------------------------------------------------
//--------                     set Aim-IK parameters                           --------
//--------------------------------------------------------------------------------------
void CSkeletonPose::SetAimIK(uint32 ik, const Vec3& vAimAtTarget) 
{
	uint32 IsValid = vAimAtTarget.IsValid();
	if (IsValid==0)
	{
		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,m_pInstance->GetFilePath(),	"CryAnimation: AimTarget Invalid (%f %f %f)",vAimAtTarget.x,vAimAtTarget.y,vAimAtTarget.z );
		g_pISystem->debug_LogCallStack();
	}


	m_AimIK.m_UseAimIK=ik > 0;	
	if (IsValid)
		m_AimIK.m_AimIKTarget = vAimAtTarget;
}

//--------------------------------------------------------------------------------------
//--------                  Aim-IK                                              --------
//--------------------------------------------------------------------------------------
struct Tri
{
	uint16 x,y,z;	ColorB rgb;
	ILINE Tri( uint16 vx, uint16 vy, uint16 vz, ColorB c ) { x=vx; y=vy; z=vz; rgb=c; };
};


Tri g_AimTriangles[] = 
{
	//first ring
	Tri( 16, 23, 24,RGBA8(0x00,0xff,0x00,0xff)),
	Tri( 16, 24, 17,RGBA8(0x00,0xff,0x00,0xff)),
	Tri( 24, 18, 17,RGBA8(0x00,0xff,0x00,0xff)),
	Tri( 24, 25, 18,RGBA8(0x00,0xff,0x00,0xff)),

	Tri( 24, 23, 30,RGBA8(0x00,0xff,0x00,0xff)),
	Tri( 24, 30, 31,RGBA8(0x00,0xff,0x00,0xff)),
	Tri( 24, 31, 32,RGBA8(0x00,0xff,0x00,0xff)),
	Tri( 24, 32, 25,RGBA8(0x00,0xff,0x00,0xff)),

	//second ring
	Tri( 8, 15, 16,RGBA8(0xff,0x00,0x00,0xff)),
	Tri( 8, 16,  9,RGBA8(0xff,0x00,0x00,0xff)),
	Tri( 9, 16, 17,RGBA8(0xff,0x00,0x00,0xff)),
	Tri( 9, 17, 10,RGBA8(0xff,0x00,0x00,0xff)),

	Tri(10, 17, 11,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(17, 18, 11,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(11, 18, 12,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(18, 19, 12,RGBA8(0xff,0x00,0x00,0xff)),

	Tri(18, 25, 19,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(25, 26, 19,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(25, 32, 33,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(25, 33, 26,RGBA8(0xff,0x00,0x00,0xff)),

	Tri(32, 39, 40,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(32, 40, 33,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(31, 38, 39,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(31, 39, 32,RGBA8(0xff,0x00,0x00,0xff)),

	Tri(30, 37, 31,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(37, 38, 31,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(29, 36, 30,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(36, 37, 30,RGBA8(0xff,0x00,0x00,0xff)),

	Tri(22, 29, 23,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(23, 29, 30,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(15, 22, 23,RGBA8(0xff,0x00,0x00,0xff)),
	Tri(15, 23, 16,RGBA8(0xff,0x00,0x00,0xff)),


	//extrapolation ring
	Tri( 0,  7,  8,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri( 0,  8,  1,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri( 1,  8,  9,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri( 1,  9,  2,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri( 2,  9, 10,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri( 2, 10,  3,RGBA8(0xff,0x0f,0xff,0xff)),

	Tri( 4,  3, 10,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri( 4, 10, 11,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri( 5,  4, 11,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri( 5, 11, 12,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri( 6,  5, 12,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri( 6, 12, 13,RGBA8(0xff,0x0f,0xff,0xff)),



	Tri(13, 12, 19,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(13, 19, 20,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(19, 26, 20,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(20, 26, 27,RGBA8(0xff,0x0f,0xff,0xff)),

	Tri(26, 33, 34,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(26, 34, 27,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(33, 40, 41,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(33, 41, 34,RGBA8(0xff,0x0f,0xff,0xff)),

	Tri(40, 47, 48,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(40, 48, 41,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(39, 46, 47,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(39, 47, 40,RGBA8(0xff,0x0f,0xff,0xff)),

	Tri(38, 45, 46,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(38, 46, 39,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(38, 44, 45,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(38, 37, 44,RGBA8(0xff,0x0f,0xff,0xff)),

	Tri(37, 43, 44,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(37, 36, 43,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(36, 42, 43,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(36, 35, 42,RGBA8(0xff,0x0f,0xff,0xff)),

	Tri(29, 35, 36,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(29, 28, 35,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(22, 28, 29,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(22, 21, 28,RGBA8(0xff,0x0f,0xff,0xff)),

	Tri(14, 21, 22,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri(14, 22, 15,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri( 7, 14, 15,RGBA8(0xff,0x0f,0xff,0xff)),
	Tri( 7, 15,  8,RGBA8(0xff,0x0f,0xff,0xff))
};


Tri g_AimTriangles2[] = 
{
	Tri(  8, 22, 24,RGBA8(0x00,0xff,0x00,0xff)),
	Tri(  8, 24, 10,RGBA8(0x00,0xff,0x00,0xff)),
	Tri( 10, 24, 12,RGBA8(0x00,0xff,0x00,0xff)),
	Tri( 24, 26, 12,RGBA8(0x00,0xff,0x00,0xff)),

	Tri( 22, 36, 24,RGBA8(0x00,0xff,0x00,0xff)),
	Tri( 36, 38, 24,RGBA8(0x00,0xff,0x00,0xff)),
	Tri( 24, 38, 40,RGBA8(0x00,0xff,0x00,0xff)),
	Tri( 24, 40, 26,RGBA8(0x00,0xff,0x00,0xff))
};




ILINE Vec3 Barycentric( const Line &line, const Vec3 &v0, const Vec3 &v1, const Vec3 &v2)
{
	//find vectors for two edges sharing v0
	Vec3r dir	=	line.direction;
	Vec3r edge_1 = v0-v2;
	Vec3r edge_2 = v1-v2;
	//begin calculating determinant - also used to calculate U parameter
	Vec3r pvec = dir % edge_1; 

	//if determinat is near zero, ray lies in plane of triangel 
	f64 det = edge_2 | pvec;
	if ( fabs(det)<0.0001 )
		return Vec3(0,0,0);

	//calculate distance from v0 to ray origin
	Vec3r tvec=line.pointonline-v2;
	//prepare to test V parameter
	Vec3r qvec=tvec % edge_2;


	//calculate U parameter and test bounds
	f64 u= (dir | qvec);
	//calculate V parameter and test bounds
	f64 v=tvec | pvec;

	//------------------------------------------------------
	Vec3r output;
	output.x =u;
	output.y =v;
	output.z =det-(u+v);

	f64 sum=output.x+output.y+output.z;
	assert(sum);

	Vec3 barycentric;
	barycentric.x=f32(output.x/sum);
	barycentric.y=f32(output.y/sum);
	barycentric.z=f32(output.z/sum);
	return barycentric;
}


ILINE Vec3 ClosestPtPointTriangle(Vec3 p, Vec3 a, Vec3 b, Vec3 c)
{
	Vec3 ba=b-a,ab=a-b,ca=c-a;
	Vec3 cb=c-b,bc=b-c,ac=a-c;
	Vec3 pa=p-a,pb=p-b,pc=p-c;
	Vec3 ap=a-p,bp=b-p,cp=c-p;

	f32 snom=pa|ba;
	f32 unom=pb|cb;
	f32 tnom=pa|ca;

	f32 sdenom = pb|ab;
	f32 tdenom = pc|ac;
	f32 udenom = pc|bc;

	if (snom <= 0.0f && tnom <= 0.0f) return a; // Vertex region early out
	if (sdenom <= 0.0f && unom <= 0.0f) return b; // Vertex region early out
	if (tdenom <= 0.0f && udenom <= 0.0f) return c; // Vertex region early out

	// P is outside (or on) AB if the triple scalar product [N PA PB] <= 0
	Vec3 n = ba%ca;
	f32 vc = n|(ap%bp);
	// If P outside AB and within feature region of AB, return projection of P onto AB
	if (vc <= 0.0f && snom >= 0.0f && sdenom >= 0.0f)
		return a + snom / (snom + sdenom) * ba;

	// P is outside (or on) BC if the triple scalar product [N PB PC] <= 0
	f32 va = n|(bp%cp);
	// If P outside BC and within feature region of BC, return projection of P onto BC
	if (va <= 0.0f && unom >= 0.0f && udenom >= 0.0f)
		return b + unom / (unom + udenom) * cb;

	// P is outside (or on) CA if the triple scalar product [N PC PA] <= 0
	f32 vb = n|(cp%ap);
	// If P outside CA and within feature region of CA, return projection of P onto CA
	if (vb <= 0.0f && tnom >= 0.0f && tdenom >= 0.0f)
		return a + tnom / (tnom + tdenom) * ca;

	// P must project inside face region. Compute Q using barycentric coordinates
	f32 u = va / (va + vb + vc);
	f32 v = vb / (va + vb + vc);
	f32 w = 1.0f - u - v; // = vc / (va + vb + vc)
	return u*a + v*b + w*c;
}




void CSkeletonPose::AimIK( const QuatT& rPhysLocationNext, const QuatTS& rAnimLocationNext )
{
	f32 d=99999.0f;

	//by default we are in fadeout mode
	m_AimIK.m_AimIKFadeout = 1;

	if( Console::GetInst().ca_UseAimIK==0 )
		return;
	if (m_pModelSkeleton->IsHuman()==0)
		return;


	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------

	int16 hasAimIKSkeleton=1;
	int16 arrAimIKBoneIdx[15];

	arrAimIKBoneIdx[ 0] = m_pModelSkeleton->m_IdxArray[eIM_Spine0Idx];
	arrAimIKBoneIdx[ 1] = m_pModelSkeleton->m_IdxArray[eIM_Spine1Idx];
	arrAimIKBoneIdx[ 2] = m_pModelSkeleton->m_IdxArray[eIM_Spine2Idx];
	arrAimIKBoneIdx[ 3] = m_pModelSkeleton->m_IdxArray[eIM_Spine3Idx];
	arrAimIKBoneIdx[ 4] = m_pModelSkeleton->m_IdxArray[eIM_NeckIdx];
	arrAimIKBoneIdx[ 5] = m_pModelSkeleton->m_IdxArray[eIM_RClavicleIdx];
	arrAimIKBoneIdx[ 6] = m_pModelSkeleton->m_IdxArray[eIM_RUpperArmIdx];
	arrAimIKBoneIdx[ 7] = m_pModelSkeleton->m_IdxArray[eIM_RForeArmIdx];
	arrAimIKBoneIdx[ 8] = m_pModelSkeleton->m_IdxArray[eIM_RHandIdx];
	arrAimIKBoneIdx[ 9] = m_pModelSkeleton->m_IdxArray[eIM_WeaponBoneIdx];

	arrAimIKBoneIdx[10] = m_pModelSkeleton->m_IdxArray[eIM_HeadIdx];
	arrAimIKBoneIdx[11] = m_pModelSkeleton->m_IdxArray[eIM_LClavicleIdx];
	arrAimIKBoneIdx[12] = m_pModelSkeleton->m_IdxArray[eIM_LUpperArmIdx];
	arrAimIKBoneIdx[13] = m_pModelSkeleton->m_IdxArray[eIM_LForeArmIdx];
	arrAimIKBoneIdx[14] = m_pModelSkeleton->m_IdxArray[eIM_LHandIdx];

	uint32 numIKBones=sizeof(arrAimIKBoneIdx)/sizeof(int16);
	for (uint32 i=0; i<numIKBones; i++ )
		if (arrAimIKBoneIdx[i]<0)	hasAimIKSkeleton=0;



	//------------------------------------------------------------------------
	//------------------------------------------------------------------------
	//------------------------------------------------------------------------

	Vec3 vAimIKTarget = m_AimIK.m_AimIKTarget;
	int32 neckidx	=	m_pModelSkeleton->m_IdxArray[eIM_NeckIdx];
	Vec3 NeckBoneWorld	= rPhysLocationNext*m_AbsolutePose[neckidx].t;
	Vec3 vDistance=vAimIKTarget-NeckBoneWorld;


	uint32 numJoints = m_AbsolutePose.size();

	m_AimIK.m_AimIKInfluence = 0;
	if (m_pModelSkeleton->m_IdxArray[eIM_RHandIdx] < 0) return;
	if (m_pModelSkeleton->m_IdxArray[eIM_WeaponBoneIdx] < 0) return;
	if (m_timeStandingUp>=0) return;
	if (hasAimIKSkeleton==0) return;

	if (m_AimIK.m_numActiveAnims==0)
		return; //no aim in base-layer, no aim-pose

	float fColor[4] = {1,1,0,1};
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_AimIK.m_nLastValidAimPose: %d",m_AimIK.m_nLastValidAimPose);	g_YLine+=16.0f;



	//---------------------------------------------------------------------------
	//----           make sure every base-animation has a valid aim-pose     ----
	//---------------------------------------------------------------------------
	uint32 numActiveAnims = m_AimIK.m_numActiveAnims;
	for (uint32 c=0; c<numActiveAnims; c++)
	{
		for (uint32 i=1; i<numActiveAnims; i++)
		{
			if (m_AimIK.m_AimInfo[i-1].m_nGlobalAimID0<0 && m_AimIK.m_AimInfo[i].m_nGlobalAimID0>=0)
			{ 
				m_AimIK.m_AimInfo[i-1].m_nGlobalAimID0=m_AimIK.m_AimInfo[i].m_nGlobalAimID0;
				m_AimIK.m_AimInfo[i-1].m_numAimPoses=1;
			}
		}
	}

	m_AimIK.m_AimIKFadeout = 0;  // don't fade out

	if (m_AimIK.m_AimInfo[numActiveAnims-1].m_nGlobalAimID0<0)
		m_AimIK.m_AimIKFadeout = 1;  //last anim has no aim-pose -> start fading out

	for (uint32 c=0; c<numActiveAnims; c++)
	{
		for (uint32 i=0; i<(numActiveAnims-1); i++)
		{
			if (m_AimIK.m_AimInfo[i+1].m_nGlobalAimID0<0 && m_AimIK.m_AimInfo[i].m_nGlobalAimID0>=0)
			{ 
				m_AimIK.m_AimInfo[i+1].m_nGlobalAimID0=m_AimIK.m_AimInfo[i].m_nGlobalAimID0;
				m_AimIK.m_AimInfo[i+1].m_numAimPoses=1;
			}
		}
	}

	//do we have a single valid aim-pose
	if (m_AimIK.m_AimInfo[0].m_nGlobalAimID0<0)
	{
		if (m_AimIK.m_nLastValidAimPose<0)
			return;
		for (uint32 i=0; i<numActiveAnims; i++)
		{
			m_AimIK.m_AimInfo[i].m_nGlobalAimID0 =m_AimIK.m_nLastValidAimPose;
			m_AimIK.m_AimInfo[i].m_numAimPoses=1;
		}
	}
	m_AimIK.m_nLastValidAimPose=m_AimIK.m_AimInfo[numActiveAnims-1].m_nGlobalAimID0;

	if (m_AimIK.m_AimInfo[0].m_nGlobalAimID0<0) 
		return; //no aimpose at all



	Vec3 BodyDirectionWorld	= rAnimLocationNext.q.GetColumn1();  //this is the VIEW direction 
	Vec3 AimIKTargetProj		= m_AimIK.m_AimIKTargetSmooth;
	Vec3 AimDirectionWorld	= (AimIKTargetProj-NeckBoneWorld).GetNormalizedSafe( Vec3(0,1,0) );

	f32 cosine2=AimDirectionWorld|BodyDirectionWorld;
	if (cosine2<0)
	{
		Vec3 clamped = AimDirectionWorld.Cross(Vec3(0,0,1)).Cross(AimDirectionWorld).Cross(BodyDirectionWorld).GetNormalizedSafe(Vec3(BodyDirectionWorld));
		if (AimDirectionWorld.Dot(clamped) < 0)
			AimDirectionWorld = -clamped;
		else
			AimDirectionWorld = clamped;
		AimIKTargetProj = NeckBoneWorld + 8.f*AimDirectionWorld;
	}

	AimDirectionWorld		= (AimIKTargetProj-rPhysLocationNext.t).GetNormalizedSafe(Vec3(0,1,0));
	Vec3 localAimTarget	= AimIKTargetProj-rPhysLocationNext.t;

	m_AimIK.m_AimDirection					= AimDirectionWorld*rAnimLocationNext.q;








	//some rules to disbale aim-ik
	//RULE#1: target is too close
	if (vDistance.GetLength()<0.5f)
		m_AimIK.m_AimIKFadeout=1;

	//RULE#2: he is trying to aim sideways or behind him
	f32 cosine=AimDirectionWorld|BodyDirectionWorld;
	if (cosine<0.3f && (Console::GetInst().ca_AimIKFadeout & m_AimIK.m_AimIKFadeoutFlag))
		m_AimIK.m_AimIKFadeout=1;

	if (m_AimIK.m_AimIKBlend<0.001f)
		return;

	//------------------------------------------------------------------------------------
	//---------         here we know that we can actually use AimIK          -------------
	//------------------------------------------------------------------------------------
	if(g_pISystem->GetIConsole()->GetCVar("g_aimdebug") && g_pISystem->GetIConsole()->GetCVar("g_aimdebug")->GetIVal()!=0)
		g_pISystem->GetIRenderer()->GetIRenderAuxGeom()->DrawSphere( m_AimIK.m_AimIKTargetSmooth, 0.6f, ColorF(f32(m_AimIK.m_AimIKFadeout), m_AimIK.m_AimIKBlend, 1.0f) );


	//#define AimTRIANGLES (8)
	//#define AimTRIANGLES (32)
#define AimTRIANGLES (72)

	IPol _ipol[AimTRIANGLES];
	SAimDirection ls[AIM_POSES]; //return values

	//--------------------------------------------------------------------------
	//-----              make an local array with all 49 aimposes            ---
	//--------------------------------------------------------------------------

	f32	fAimDirScale=0.0f;
	AimIKPose arrAimIKPoses[AIM_POSES]; //blended aim-poses

	f32 fTotalWeight=0.0f;
	for (int32 a=0; a<m_AimIK.m_numActiveAnims; a++)
	{
		uint32 numAimPoses = m_AimIK.m_AimInfo[a].m_numAimPoses;
		assert(numAimPoses!=1 || numAimPoses!=2);

		assert(m_AimIK.m_AimInfo[a].m_nGlobalAimID0>=0);
		GlobalAnimationHeader& rGAH0 = g_AnimationManager.m_arrGlobalAnimations[m_AimIK.m_AimInfo[a].m_nGlobalAimID0];
		uint32 numPoses = rGAH0.m_arrAimIKPoses.size();
		assert(numPoses == AIM_POSES);

		f32 fWeight = m_AimIK.m_AimInfo[a].m_fWeight;
		if (fTotalWeight==0.0f)
		{
			if (numAimPoses==1)
			{
				fAimDirScale = rGAH0.m_fAimDirScale*fWeight;
				for (uint32 i=0; i<AIM_POSES; i++) arrAimIKPoses[i] = rGAH0.m_arrAimIKPoses[i]*fWeight;
			}
			else
			{
				assert(m_AimIK.m_AimInfo[a].m_nGlobalAimID1>=0);
				GlobalAnimationHeader& rGAH1 = g_AnimationManager.m_arrGlobalAnimations[m_AimIK.m_AimInfo[a].m_nGlobalAimID1];
				assert(rGAH1.m_arrAimIKPoses.size() == AIM_POSES);

				f32 t0 = 1.0f-m_AimIK.m_AimInfo[a].m_fAnimTime;
				f32 t1 = m_AimIK.m_AimInfo[a].m_fAnimTime;
				fAimDirScale = (rGAH0.m_fAimDirScale*t0 + rGAH1.m_fAimDirScale*t1) *fWeight;
				for (uint32 i=0; i<AIM_POSES; i++) 
				{
					AimIKPose res;	res.SetNLerp(rGAH0.m_arrAimIKPoses[i],rGAH1.m_arrAimIKPoses[i],t1); 
					arrAimIKPoses[i] = res*fWeight;
				}
			}
		}
		else
		{
			if (numAimPoses==1)
			{
				fAimDirScale += rGAH0.m_fAimDirScale*fWeight;
				for (uint32 i=0; i<AIM_POSES; i++) arrAimIKPoses[i] %= rGAH0.m_arrAimIKPoses[i]*fWeight;
			}
			else
			{
				assert(m_AimIK.m_AimInfo[a].m_nGlobalAimID1>=0);
				GlobalAnimationHeader& rGAH1 = g_AnimationManager.m_arrGlobalAnimations[m_AimIK.m_AimInfo[a].m_nGlobalAimID1];
				assert(rGAH1.m_arrAimIKPoses.size() == AIM_POSES);

				f32 t0 = 1.0f-m_AimIK.m_AimInfo[a].m_fAnimTime;
				f32 t1 = m_AimIK.m_AimInfo[a].m_fAnimTime;
				fAimDirScale += (rGAH0.m_fAimDirScale*t0+rGAH1.m_fAimDirScale*t1)*fWeight;
				for (uint32 i=0; i<AIM_POSES; i++) 
				{
					AimIKPose res;	res.SetNLerp(rGAH0.m_arrAimIKPoses[i],rGAH1.m_arrAimIKPoses[i],t1); 
					arrAimIKPoses[i] %= res*fWeight;
				}
			}
		}
		fTotalWeight += fWeight;
	}
	assert( fabsf(fTotalWeight-1.0f) < 0.001f );

	for (uint32 i=0; i<AIM_POSES; i++)
		arrAimIKPoses[i].Normalize();

	//--------------------------------------------------------------------------
	m_AimIK.m_AimIKInfluence=m_AimIK.m_AimIKBlend;
	uint32 numAimins1=m_pSkeletonAnim->GetNumAnimsInFIFO(1);
	uint32 numAimins2=m_pSkeletonAnim->GetNumAnimsInFIFO(2);
	if (numAimins1 || numAimins2)
	{
		f32 fB1 = 1.0f-m_pSkeletonAnim->m_arrLayerBlending[1];
		f32 fB2 = 1.0f-m_pSkeletonAnim->m_arrLayerBlending[2];

		f32 fUpperBodyBlend = min(fB1,fB2);
		fUpperBodyBlend -= 0.5f;
		fUpperBodyBlend = 0.5f+fUpperBodyBlend/(0.5f+2.0f*fUpperBodyBlend*fUpperBodyBlend);

		if (m_AimIK.m_AimIKInfluence>fUpperBodyBlend)
			m_AimIK.m_AimIKInfluence=fUpperBodyBlend;
	}

	f32 fIKBlend = clamp(m_AimIK.m_AimIKInfluence,0.0f,1.0f);
	//fIKBlend = fIKBlend * fIKBlend * ( 3 - 2 * fIKBlend );
	fIKBlend -= 0.5f;
	fIKBlend = 0.5f+fIKBlend/(0.5f+2.0f*fIKBlend*fIKBlend);
	//	fIKBlend = fIKBlend * fIKBlend * ( 3 - 2 * fIKBlend );
	//	fIKBlend = fIKBlend * fIKBlend * ( 3 - 2 * fIKBlend );


	if( Console::GetInst().ca_DrawAimPoses==2)
	{
		QuatT* _pRelativeQuatIK = (QuatT*)alloca( numJoints*sizeof(QuatT) );
		QuatT* _pAbsoluteQuatIK = (QuatT*)alloca( numJoints*sizeof(QuatT) );
		for (uint32 i=0; i<numJoints; i++)
		{
			_pRelativeQuatIK[i] = m_RelativePose[i];
			_pAbsoluteQuatIK[i] = m_AbsolutePose[i];
		}

		IPol _ipol2[AimTRIANGLES];
		SAimDirection ls2[AIM_POSES]; 
		for (uint32 c=0; c<AimTRIANGLES; c++)
		{
			uint32 k0	=g_AimTriangles[c].x;
			uint32 k1	=g_AimTriangles[c].y;
			uint32 k2	=g_AimTriangles[c].z;
			ColorB rgb=g_AimTriangles[c].rgb;

			if (ls[k0].evaluated==0) ls[k0]=EvaluatePose(rPhysLocationNext, k0, &arrAimIKPoses[0], fAimDirScale, arrAimIKBoneIdx);
			if (ls[k1].evaluated==0) ls[k1]=EvaluatePose(rPhysLocationNext, k1, &arrAimIKPoses[0], fAimDirScale, arrAimIKBoneIdx);
			if (ls[k2].evaluated==0) ls[k2]=EvaluatePose(rPhysLocationNext, k2, &arrAimIKPoses[0], fAimDirScale, arrAimIKBoneIdx);

			IPol ipol;
			ipol.k0=k0;		ipol.k1=k1;		ipol.k2=k2;
			Vec3 s0=ls[k0].start;	Vec3 s1=ls[k1].start;	Vec3 s2=ls[k2].start;
			Vec3 t0=ls[k0].end;		Vec3 t1=ls[k1].end;		Vec3 t2=ls[k2].end;

			Vec3 WPos = rPhysLocationNext.t;
			ColorB tricol = RGBA8(uint8(rgb.r*fIKBlend),uint8(rgb.g*fIKBlend),uint8(rgb.b*fIKBlend),0xff);
			g_pAuxGeom->DrawLine(WPos+s0,tricol, WPos+s1,tricol );
			g_pAuxGeom->DrawLine(WPos+s1,tricol, WPos+s2,tricol );
			g_pAuxGeom->DrawLine(WPos+s2,tricol, WPos+s0,tricol );
			g_pAuxGeom->DrawLine(WPos+t0,tricol, WPos+t1,tricol );
			g_pAuxGeom->DrawLine(WPos+t1,tricol, WPos+t2,tricol );
			g_pAuxGeom->DrawLine(WPos+t2,tricol, WPos+t0,tricol );

			g_pAuxGeom->DrawLine(	WPos+s0,RGBA8(0x7f,0x7f,0x7f,0xff), WPos+t0,RGBA8(0xff,0xff,0xff,0xff) );
			g_pAuxGeom->DrawLine(	WPos+s1,RGBA8(0x7f,0x7f,0x7f,0xff), WPos+t1,RGBA8(0xff,0xff,0xff,0xff) );
			g_pAuxGeom->DrawLine(	WPos+s2,RGBA8(0x7f,0x7f,0x7f,0xff), WPos+t2,RGBA8(0xff,0xff,0xff,0xff) );
		}
		for (uint32 i=0; i<numJoints; i++)
		{
			m_RelativePose[i]	=	_pRelativeQuatIK[i];
			m_AbsolutePose[i]	=	_pAbsoluteQuatIK[i];
		}
	}

	if( Console::GetInst().ca_DrawAimPoses==3)
	{
		QuatT* _pRelativeQuatIK = (QuatT*)alloca( numJoints*sizeof(QuatT) );
		QuatT* _pAbsoluteQuatIK = (QuatT*)alloca( numJoints*sizeof(QuatT) );
		for (uint32 i=0; i<numJoints; i++)
		{
			_pRelativeQuatIK[i] = m_RelativePose[i];
			_pAbsoluteQuatIK[i] = m_AbsolutePose[i];
		}

		IPol _ipol2[8];
		SAimDirection ls2[AIM_POSES];
		for (uint32 c=0; c<8; c++)
		{
			uint32 k0	=g_AimTriangles2[c].x;
			uint32 k1	=g_AimTriangles2[c].y;
			uint32 k2	=g_AimTriangles2[c].z;
			ColorB rgb=g_AimTriangles2[c].rgb;

			if (ls[k0].evaluated==0) ls[k0]=EvaluatePose( rPhysLocationNext, k0, &arrAimIKPoses[0], fAimDirScale, arrAimIKBoneIdx);
			if (ls[k1].evaluated==0) ls[k1]=EvaluatePose( rPhysLocationNext, k1, &arrAimIKPoses[0], fAimDirScale, arrAimIKBoneIdx);
			if (ls[k2].evaluated==0) ls[k2]=EvaluatePose( rPhysLocationNext, k2, &arrAimIKPoses[0], fAimDirScale, arrAimIKBoneIdx);

			IPol ipol;
			ipol.k0=k0;		ipol.k1=k1;		ipol.k2=k2;
			Vec3 s0=ls[k0].start;	Vec3 s1=ls[k1].start;	Vec3 s2=ls[k2].start;
			Vec3 t0=ls[k0].end;		Vec3 t1=ls[k1].end;		Vec3 t2=ls[k2].end;

			Vec3 WPos = rPhysLocationNext.t;
			ColorB tricol = RGBA8(uint8(rgb.r*fIKBlend),uint8(rgb.g*fIKBlend),uint8(rgb.b*fIKBlend),0xff);
			g_pAuxGeom->DrawLine(WPos+s0,tricol, WPos+s1,tricol );
			g_pAuxGeom->DrawLine(WPos+s1,tricol, WPos+s2,tricol );
			g_pAuxGeom->DrawLine(WPos+s2,tricol, WPos+s0,tricol );
			g_pAuxGeom->DrawLine(WPos+t0,tricol, WPos+t1,tricol );
			g_pAuxGeom->DrawLine(WPos+t1,tricol, WPos+t2,tricol );
			g_pAuxGeom->DrawLine(WPos+t2,tricol, WPos+t0,tricol );

			g_pAuxGeom->DrawLine(	WPos+s0,RGBA8(0x7f,0x7f,0x7f,0xff), WPos+t0,RGBA8(0xff,0xff,0xff,0xff) );
			g_pAuxGeom->DrawLine(	WPos+s1,RGBA8(0x7f,0x7f,0x7f,0xff), WPos+t1,RGBA8(0xff,0xff,0xff,0xff) );
			g_pAuxGeom->DrawLine(	WPos+s2,RGBA8(0x7f,0x7f,0x7f,0xff), WPos+t2,RGBA8(0xff,0xff,0xff,0xff) );
		}
		for (uint32 i=0; i<numJoints; i++)
		{
			m_RelativePose[i]	=	_pRelativeQuatIK[i];
			m_AbsolutePose[i]	=	_pAbsoluteQuatIK[i];
		}
	}


	//--------------------------------------------------------------------------------------------------------------


	int32 c=0;
	uint32 InsideTriangle=0;
	for ( ; c<AimTRIANGLES; c++)
	{
		uint32 i0=g_AimTriangles[c].x;
		uint32 i1=g_AimTriangles[c].y;
		uint32 i2=g_AimTriangles[c].z;
		ColorB rgb=g_AimTriangles[c].rgb;
		uint32 debugtri = Console::GetInst().ca_DrawAimPoses==1;
		_ipol[c]=CheckTriangles( rPhysLocationNext, i0,i1,i2,rgb, ls,localAimTarget, &arrAimIKPoses[0], fAimDirScale, fIKBlend, arrAimIKBoneIdx);
		if ( _ipol[c].inside )	
		{
			InsideTriangle++;
			break;
		}; 
	}

	if (InsideTriangle==0)
	{
		c=-1;
		for (uint32 i=0; i<AimTRIANGLES; i++)
		{
			if (d>_ipol[i].distance) 
			{ 
				d=_ipol[i].distance;	c=i; 
			}
		}
	}

	if (c==-1)
	{
		uint32 asai=0;
	}

	//-------------------------------------------------------------

	IPol ipol = _ipol[0];
	if (c<0)
		m_AimIK.m_AimIKBlend=0;
	else
		ipol=_ipol[c];

	f32 sum = fabsf(ipol.iepv.x)+fabsf(ipol.iepv.y)+fabsf(ipol.iepv.z);
	if (sum==0)
		return;
	assert(sum);

	if( Console::GetInst().ca_DrawAimPoses)
	{
		Vec3 WPos = rPhysLocationNext.t;
		g_pAuxGeom->DrawSphere(	WPos+ipol.ieoutput,0.01f, RGBA8(0xff,0x00,0x00,0xff) );
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"distance: %f  Tri:%d  Dist:%f",ipol.distance,c,d);	g_YLine+=16.0f;
		g_pAuxGeom->DrawSphere(	WPos+ipol.output,0.01f, RGBA8(0x00,0x00,0xff,0xff) );

		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t0: %f %f",ipol.ipv.x,ipol.iepv.x );	g_YLine+=16.0f;
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t1: %f %f",ipol.ipv.y,ipol.iepv.y );	g_YLine+=16.0f;
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"t2: %f %f",ipol.ipv.z,ipol.iepv.z );	g_YLine+=16.0f;

		f32 sum0	=	ipol.ipv.x	+	ipol.ipv.y	+	ipol.ipv.z;
		f32 sum1	=	ipol.iepv.x	+	ipol.iepv.y	+	ipol.iepv.z;
		f32 sum2	=	ipol.iepv.x/5+ipol.iepv.y/5+ipol.iepv.z/5;
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"sum: %f %f %f",sum0,sum1,sum2 );	g_YLine+=16.0f;
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"AimIKBlend:%f   m_AimIKFadeout:%d",m_AimIK.m_AimIKBlend,m_AimIK.m_AimIKFadeout);	g_YLine+=16.0f;
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"cosine:%f",cosine);	g_YLine+=16.0f;
	}


	//---------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------

	//	uint32 layer=0;
	//	const CModelJoint* pModelJoint = GetModelJointPointer(0);

	if (m_AimIK.m_AimIKBlend)
	{
		assert(m_AimIK.m_numActiveAnims);
		assert(ipol.k0<AIM_POSES);
		assert(ipol.k1<AIM_POSES);
		assert(ipol.k2<AIM_POSES);
		Quat* parrJointController0 = &arrAimIKPoses[ipol.k0].m_Spine0;
		Quat* parrJointController1 = &arrAimIKPoses[ipol.k1].m_Spine0;
		Quat* parrJointController2 = &arrAimIKPoses[ipol.k2].m_Spine0;

		int32 s0=m_pModelSkeleton->m_IdxArray[eIM_Spine0Idx];
		int32 s1=m_pModelSkeleton->m_IdxArray[eIM_Spine1Idx];
		int32 s2=m_pModelSkeleton->m_IdxArray[eIM_Spine2Idx];
		int32 s3=m_pModelSkeleton->m_IdxArray[eIM_Spine3Idx];
		int32 n0=m_pModelSkeleton->m_IdxArray[eIM_NeckIdx];

		for (uint32 i=0; i<numIKBones; i++)
		{
			Quat aim0=parrJointController0[i];
			Quat aim1=parrJointController1[i];
			Quat aim2=parrJointController2[i];
			assert((fabs_tpl(1-(aim0|aim0)))<0.00001);   //check if unit-quaternion
			assert((fabs_tpl(1-(aim1|aim1)))<0.00001);   //check if unit-quaternion
			assert((fabs_tpl(1-(aim2|aim2)))<0.00001);   //check if unit-quaternion

			int32 j=arrAimIKBoneIdx[i];
			if (s0==j || s1==j || s2==j || s3==j || n0==j)  
			{
				Quat rel=Quat(IDENTITY);
				if (s0==j) rel=m_pSkeletonAnim->m_BlendedRoot.m_FSpine0RelQuat;
				if (s1==j) rel=m_pSkeletonAnim->m_BlendedRoot.m_FSpine1RelQuat;
				if (s2==j) rel=m_pSkeletonAnim->m_BlendedRoot.m_FSpine2RelQuat;
				if (s3==j) rel=m_pSkeletonAnim->m_BlendedRoot.m_FSpine3RelQuat;
				if (n0==j) rel=m_pSkeletonAnim->m_BlendedRoot.m_FNeckRelQuat;
				//additive_blend
				aim0 = m_RelativePose[j].q*(!rel*aim0); 
				aim1 = m_RelativePose[j].q*(!rel*aim1); 
				aim2 = m_RelativePose[j].q*(!rel*aim2); 
			} 

			//do normal blending
			Quat aq0=Quat::CreateNlerp(m_RelativePose[j].q,aim0,fIKBlend);
			Quat aq1=Quat::CreateNlerp(m_RelativePose[j].q,aim1,fIKBlend);
			Quat aq2=Quat::CreateNlerp(m_RelativePose[j].q,aim2,fIKBlend);
			if( (aq0|aq1) < 0 ) 
				aq1=-aq1;
			if( (aq0|aq2) < 0 ) 
				aq2=-aq2; 
			m_RelativePose[j].q=(aq0*ipol.iepv.x) + (aq1*ipol.iepv.y) + (aq2*ipol.iepv.z);
			m_RelativePose[j].q.Normalize();
		}
	}




	for (uint32 i=1; i<numJoints; i++)
	{
		int32 p = m_parrModelJoints[i].m_idxParent;
		m_AbsolutePose[i]	= m_AbsolutePose[p] * m_RelativePose[i];
		m_AbsolutePose[i].q.Normalize();
	}

	//Draw aim-direction
	if( Console::GetInst().ca_DrawAimPoses)
	{
		int32 WeaponIdx = m_pModelSkeleton->m_IdxArray[eIM_WeaponBoneIdx];
		Vec3 newdir	=	m_AbsolutePose[WeaponIdx].q.GetColumn1()*10;
		Vec3 RWBone	= rPhysLocationNext.q*m_AbsolutePose[WeaponIdx].t;
		Vec3 WStart = rPhysLocationNext.t+RWBone;
		Vec3 WEnd   = rPhysLocationNext.t+RWBone + rPhysLocationNext.q*newdir;
		g_pAuxGeom->DrawLine(	WStart,RGBA8(0x00,0x7f,0x7f,0xff), WEnd,RGBA8(0x00,0xff,0xff,0xff) );
	}

}



//----------------------------------------------------------
//----------------------------------------------------------
//----------------------------------------------------------
SAimDirection CSkeletonPose::EvaluatePose( const QuatT& rPhysLocationNext, uint32 aimpose, AimIKPose* parrAimIKPoses, f32 fAimDirScale, int16* parrAimIKBoneIdx )
{
	Quat* parrJointController = &parrAimIKPoses[aimpose].m_Spine0;

	int32 s0=m_pModelSkeleton->m_IdxArray[eIM_Spine0Idx];
	int32 s1=m_pModelSkeleton->m_IdxArray[eIM_Spine1Idx];
	int32 s2=m_pModelSkeleton->m_IdxArray[eIM_Spine2Idx];
	int32 s3=m_pModelSkeleton->m_IdxArray[eIM_Spine3Idx];
	int32 n0=m_pModelSkeleton->m_IdxArray[eIM_NeckIdx];

	for (uint32 i=0; i<RIGHT_ARM_AIM; i++ )
	{
		int32 j = parrAimIKBoneIdx[i];
		//	const CModelJoint* pModelJoint = GetModelJointPointer(0);
		//	const char* pBoneName = pModelJoint[j].GetJointName();
		QuatT aim(parrJointController[i],m_RelativePose[j].t);
		if (s0==j || s1==j || s2==j || s3==j || n0==j)  
		{
			Quat rel(IDENTITY);
			if (s0==j) rel=m_pSkeletonAnim->m_BlendedRoot.m_FSpine0RelQuat;
			if (s1==j) rel=m_pSkeletonAnim->m_BlendedRoot.m_FSpine1RelQuat;
			if (s2==j) rel=m_pSkeletonAnim->m_BlendedRoot.m_FSpine2RelQuat;
			if (s3==j) rel=m_pSkeletonAnim->m_BlendedRoot.m_FSpine3RelQuat;
			if (n0==j) rel=m_pSkeletonAnim->m_BlendedRoot.m_FNeckRelQuat;
			//additive_blend
			aim.q	= m_RelativePose[j].q*(!rel*aim.q);
		}
		int32 p=m_parrModelJoints[j].m_idxParent;
		m_AbsolutePose[j]	= m_AbsolutePose[p] * aim;
	}

	int32 widx		=	m_pModelSkeleton->m_IdxArray[eIM_WeaponBoneIdx];
	Vec3 newdir		=	(rPhysLocationNext.q*m_AbsolutePose[widx].q).GetColumn1();
	Vec3 RWBone0	= rPhysLocationNext.q*m_AbsolutePose[widx].t + newdir*fAimDirScale;
	Vec3 RWBone1	= RWBone0+newdir;

	if( Console::GetInst().ca_DrawAimPoses==1)
	{
		Vec3 WPos	=	rPhysLocationNext.t;
		g_pAuxGeom->DrawLine(	WPos+RWBone0,RGBA8(0x7f,0x7f,0x7f,0xff), WPos+RWBone1,RGBA8(0xff,0xff,0xff,0xff) );
	}

	//key
	return SAimDirection(RWBone0,RWBone1,1);
}


//-------------------------------------------------------------------------------


IPol CSkeletonPose::CheckTriangles( const QuatT& rPhysLocationNext, uint32 k0,uint32 k1,uint32 k2,ColorB rgb, SAimDirection* ls,const Vec3& localAimTarget, AimIKPose* parrAimIKPoses, f32 fAimDirScale, f32 fIKBlend, int16* parrAimIKBoneIdx  )
{
	if (ls[k0].evaluated==0)
		ls[k0]=EvaluatePose( rPhysLocationNext, k0, parrAimIKPoses, fAimDirScale, parrAimIKBoneIdx );
	if (ls[k1].evaluated==0)
		ls[k1]=EvaluatePose( rPhysLocationNext, k1, parrAimIKPoses, fAimDirScale, parrAimIKBoneIdx );
	if (ls[k2].evaluated==0)
		ls[k2]=EvaluatePose( rPhysLocationNext, k2, parrAimIKPoses, fAimDirScale, parrAimIKBoneIdx );

	IPol ipol;
	ipol.k0=k0;
	ipol.k1=k1;
	ipol.k2=k2;

	Vec3 s0=ls[k0].start;
	Vec3 s1=ls[k1].start;
	Vec3 s2=ls[k2].start;

	Vec3 t0=ls[k0].end;
	Vec3 t1=ls[k1].end;
	Vec3 t2=ls[k2].end;

	//float fColor[4] = {0,0,1,1};
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"s0: %f %f %f   t0: %f %f %f",s0.x,s0.y,s0.z, t0.x,t0.y,t0.z );	g_YLine+=16.0f;

	if( Console::GetInst().ca_DrawAimPoses==1)
	{
		Vec3 WPos = rPhysLocationNext.t;
		ColorB tricol = RGBA8(uint8(rgb.r*fIKBlend),uint8(rgb.g*fIKBlend),uint8(rgb.b*fIKBlend),0xff);
		g_pAuxGeom->DrawLine(WPos+s0,tricol, WPos+s1,tricol );
		g_pAuxGeom->DrawLine(WPos+s1,tricol, WPos+s2,tricol );
		g_pAuxGeom->DrawLine(WPos+s2,tricol, WPos+s0,tricol );

		g_pAuxGeom->DrawLine(WPos+t0,tricol, WPos+t1,tricol );
		g_pAuxGeom->DrawLine(WPos+t1,tricol, WPos+t2,tricol );
		g_pAuxGeom->DrawLine(WPos+t2,tricol, WPos+t0,tricol );
	}

	ipol.ipv=Vec3(0.33333f,0.33333f,0.33333f);

	Vec3 origin,direction;
	uint32 i=0;
	for (i=0; i<33; i++)
	{

		origin = (s0*ipol.ipv.x + s1*ipol.ipv.y + s2*ipol.ipv.z);
		assert( origin.IsValid() );
		direction = (localAimTarget-origin).GetNormalized();
		assert( direction.IsValid() );

		ipol.iepv			=	Barycentric(Line(origin,direction),t0,t1,t2);
		assert( ipol.iepv.IsValid() );
		f32 sum=ipol.iepv.x+ipol.iepv.y+ipol.iepv.z;
		if (sum==0)
			break;


		ipol.ieoutput	=	t0*ipol.iepv.x + t1*ipol.iepv.y + t2*ipol.iepv.z;
		assert( ipol.ieoutput.IsValid() );

		ipol.output		=	ClosestPtPointTriangle(ipol.ieoutput,t0,t1,t2);
		assert( ipol.output.IsValid() );

		ipol.ipv			=	Barycentric(Line(ipol.output,direction),t0,t1,t2);
		assert( ipol.ipv.IsValid() );

		if (ipol.ipv.x<0.0f) ipol.ipv.x=0.0f;
		if (ipol.ipv.y<0.0f) ipol.ipv.y=0.0f;
		if (ipol.ipv.z<0.0f) ipol.ipv.z=0.0f;
		if (ipol.ipv.x>1.0f) ipol.ipv.x=1.0f;
		if (ipol.ipv.y>1.0f) ipol.ipv.y=1.0f;
		if (ipol.ipv.z>1.0f) ipol.ipv.z=1.0f;
		if (i>3)
		{
			if (ipol.iepv.x<0.0f) break;
			if (ipol.iepv.y<0.0f) break;
			if (ipol.iepv.z<0.0f) break;
			if (ipol.iepv.x>1.0f) break;
			if (ipol.iepv.y>1.0f) break;
			if (ipol.iepv.z>1.0f) break;
		}
	}

	if (i==0)
	{
		ipol.distance=99999.0f;
		ipol.inside=0;
		return ipol;
	}

	ipol.distance = (ipol.ieoutput-ipol.output).GetLength();


	//	float fColor[4] = {0,1,0,1};
	//	extern float g_YLine;
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"planecheck: %f %f",distS,distE );	g_YLine+=16.0f;

	ipol.inside=1;
	if (ipol.iepv.x<0.0f) ipol.inside=0;
	if (ipol.iepv.y<0.0f) ipol.inside=0;
	if (ipol.iepv.z<0.0f) ipol.inside=0;
	if (ipol.iepv.x>1.0f) ipol.inside=0;
	if (ipol.iepv.y>1.0f) ipol.inside=0;
	if (ipol.iepv.z>1.0f) ipol.inside=0;

	/*
	if (ipol.iepv.x<-2.0f) ipol.distance=99999.0f;
	if (ipol.iepv.y<-2.0f) ipol.distance=99999.0f;
	if (ipol.iepv.z<-2.0f) ipol.distance=99999.0f;
	if (ipol.iepv.x> 3.0f) ipol.distance=99999.0f;
	if (ipol.iepv.y> 3.0f) ipol.distance=99999.0f;
	if (ipol.iepv.z> 3.0f) ipol.distance=99999.0f;
	*/

	f32 sum=ipol.iepv.x+ipol.iepv.y+ipol.iepv.z;
	if (sum==0)
		ipol.distance=99999.0f;

	Plane plane=Plane::CreatePlane(t0,t1,t2);
	f32 distS=plane|origin;
	f32 distE=plane|(origin+direction*100.0f);
	if (distS>0 && distE>0)
	{
		ipol.distance=99999.0f;
		ipol.inside=0;
	}
	if (distS<0 && distE<0)
	{
		ipol.distance=99999.0f;
		ipol.inside=0;
	}

	return ipol;
}










