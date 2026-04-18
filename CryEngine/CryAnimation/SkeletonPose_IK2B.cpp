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



uint32 CSkeletonPose::SetCustomArmIK(const Vec3& localgoal,int32 idx0,int32 idx1,int32 idx2)
{
	if (m_bFullSkeletonUpdate==0)	return 0;
	if (m_pSkeletonAnim->m_IsAnimPlaying==0)	return 0;

	if (idx0<0) return 0;
	if (idx1<0) return 0;
	if (idx2<0) return 0;

	TwoBoneIK(idx0,idx1,idx2,localgoal);

	uint32 numJoints = m_AbsolutePose.size();
	for (uint32 i=1; i<numJoints; i++)
	{
		int32 p=m_parrModelJoints[i].m_idxParent;
		m_AbsolutePose[i]	= m_AbsolutePose[p] * m_RelativePose[i];
	}

	return 1;
}



//--------------------------------------------------------------------------------
//----                  Two bone IK for human arms and legs                         ----
//--------------------------------------------------------------------------------
uint32 CSkeletonPose::SetHumanLimbIK(const Vec3& WGoal, uint32 limp)
{
	if (m_bFullSkeletonUpdate==0)
		return 0;
	if (m_pSkeletonAnim->m_IsAnimPlaying==0)
		return 0;

	Vec3 goal=m_pInstance->m_PhysEntityLocation.GetInverted()*WGoal;

	if (limp==CA_LEG_LEFT)
		SetLFootIK(goal);
	if (limp==CA_LEG_RIGHT)
		SetRFootIK(goal);
	if (limp==CA_ARM_LEFT)
		SetLArmIK(goal);
	if (limp==CA_ARM_RIGHT)
		SetRArmIK(goal);

	return 1;
};




uint32 CSkeletonPose::SetLArmIK(const Vec3& localgoal)
{
	int32 b0	=	m_pModelSkeleton->m_IdxArray[eIM_LUpperArmIdx];
	int32 b1	=	m_pModelSkeleton->m_IdxArray[eIM_LForeArmIdx];
	int32 b2	=	m_pModelSkeleton->m_IdxArray[eIM_LHandIdx];
	if (b0<0) return 0;
	if (b1<0) return 0;
	if (b2<0) return 0;
	TwoBoneIK(b0,b1,b2,localgoal);
	uint32 numChilds = m_pModelSkeleton->m_arrLShoulderChilds.size();
	for (uint32 i=0; i<numChilds; i++)
	{
		int32 c=m_pModelSkeleton->m_arrLShoulderChilds[i];
		int32 p=m_parrModelJoints[c].m_idxParent;
		m_AbsolutePose[c]	= m_AbsolutePose[p] * m_RelativePose[c];
	}
	return 1;
}


uint32 CSkeletonPose::SetRArmIK(const Vec3& localgoal)
{
	int32 b0	=	m_pModelSkeleton->m_IdxArray[eIM_RUpperArmIdx];
	int32 b1	=	m_pModelSkeleton->m_IdxArray[eIM_RForeArmIdx];
	int32 b2	=	m_pModelSkeleton->m_IdxArray[eIM_RHandIdx];
	if (b0<0) return 0;
	if (b1<0) return 0;
	if (b2<0) return 0;
	TwoBoneIK(b0,b1,b2,localgoal);
	uint32 numChilds = m_pModelSkeleton->m_arrRShoulderChilds.size();
	for (uint32 i=0; i<numChilds; i++)
	{
		int32 c=m_pModelSkeleton->m_arrRShoulderChilds[i];
		int32 p=m_parrModelJoints[c].m_idxParent;
		m_AbsolutePose[c]	= m_AbsolutePose[p] * m_RelativePose[c];
	}
	return 1;
}

uint32 CSkeletonPose::SetLFootIK(const Vec3& localgoal )
{
	int32 b0	=	m_pModelSkeleton->m_IdxArray[eIM_LThighIdx];
	int32 b1	=	m_pModelSkeleton->m_IdxArray[eIM_LCalfIdx];
	int32 b2	=	m_pModelSkeleton->m_IdxArray[eIM_LFootIdx];
	if (b0<0) return 0;
	if (b1<0) return 0;
	if (b2<0) return 0;
	TwoBoneIK(b0,b1,b2,localgoal);
	uint32 numChilds = m_pModelSkeleton->m_arrLThighChilds.size();
	for (uint32 i=0; i<numChilds; i++)
	{
		int32 c=m_pModelSkeleton->m_arrLThighChilds[i];
		int32 p=m_parrModelJoints[c].m_idxParent;
		m_AbsolutePose[c]	= m_AbsolutePose[p] * m_RelativePose[c];
	}
	return 1;
}


uint32 CSkeletonPose::SetRFootIK(const Vec3& localgoal )
{
	int32 b0	=	m_pModelSkeleton->m_IdxArray[eIM_RThighIdx];
	int32 b1	=	m_pModelSkeleton->m_IdxArray[eIM_RCalfIdx];
	int32 b2	=	m_pModelSkeleton->m_IdxArray[eIM_RFootIdx];
	if (b0<0) return 0;
	if (b1<0) return 0;
	if (b2<0) return 0;
	TwoBoneIK(b0,b1,b2,localgoal);
	uint32 numChilds = m_pModelSkeleton->m_arrRThighChilds.size();
	for (uint32 i=0; i<numChilds; i++)
	{
		int32 c=m_pModelSkeleton->m_arrRThighChilds[i];
		int32 p=m_parrModelJoints[c].m_idxParent;
		m_AbsolutePose[c]	= m_AbsolutePose[p] * m_RelativePose[c];
	}
	return 1;
}



//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
void CSkeletonPose::MoveSkeletonVertical( f32 vertical )
{
	if ( (m_pSkeletonAnim->m_IsAnimPlaying&1) && m_pSkeletonAnim->m_AnimationDrivenMotion && m_bFullSkeletonUpdate)
	{
		uint32 numJoints = m_AbsolutePose.size();
		m_RelativePose[1].t.z += vertical;
		for (uint32 i=1; i<numJoints; i++)
		{
			int32 p=m_parrModelJoints[i].m_idxParent;
			m_AbsolutePose[i].t.z += vertical;
		}
	}
}




//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
uint32 CSkeletonPose::SetFootGroundAlignmentCCD( uint32 leg, const Plane& GroundPlane)
{
	float fColor[4] = {0,1,0,1};
	IRenderer* pIRenderer = g_pISystem->GetIRenderer();

	f32* pFootGroundAlign = &m_LFootGroundAlign;
	int32 ThighIdx	=	m_pModelSkeleton->m_IdxArray[eIM_LThighIdx];
	int32 CalfIdx		=	m_pModelSkeleton->m_IdxArray[eIM_LCalfIdx];	
	int32 FootIdx		=	m_pModelSkeleton->m_IdxArray[eIM_LFootIdx];
	int32 HeelIdx		=	m_pModelSkeleton->m_IdxArray[eIM_LHeelIdx];
	int32 Toe0Idx		=	m_pModelSkeleton->m_IdxArray[eIM_LToe0Idx];
	int32 ToeNIdx		=	m_pModelSkeleton->m_IdxArray[eIM_LToe0NubIdx];
	int32 e0	=m_pModelSkeleton->m_IdxArray[eIM_LKneeIdx];
	int32 e1	=m_pModelSkeleton->m_IdxArray[eIM_LKneeEndIdx];
	int32 e2	=-1;		// eIM_LLegPistol;

	if (leg==CA_LEG_RIGHT)
	{
		pFootGroundAlign = &m_RFootGroundAlign;
		ThighIdx	=m_pModelSkeleton->m_IdxArray[eIM_RThighIdx];
		CalfIdx		=m_pModelSkeleton->m_IdxArray[eIM_RCalfIdx];
		FootIdx		=m_pModelSkeleton->m_IdxArray[eIM_RFootIdx];
		HeelIdx		=m_pModelSkeleton->m_IdxArray[eIM_RHeelIdx];
		Toe0Idx		=m_pModelSkeleton->m_IdxArray[eIM_RToe0Idx];
		ToeNIdx		=m_pModelSkeleton->m_IdxArray[eIM_RToe0NubIdx];
		e0	=m_pModelSkeleton->m_IdxArray[eIM_RKneeIdx];
		e1	=m_pModelSkeleton->m_IdxArray[eIM_RKneeEndIdx];
		e2	=-1;// eIM_RLegPistol;
	}

	uint32 IkProcess=0;
	for (uint32 i=0; i<8; i++)
	{
		Quat rel_b0		= m_RelativePose[FootIdx].q;
		QuatT abs_b0  = m_AbsolutePose[FootIdx];
		QuatT abs_b1  = m_AbsolutePose[HeelIdx];
		QuatT abs_b2  = m_AbsolutePose[Toe0Idx];
		QuatT abs_b3  = m_AbsolutePose[ToeNIdx];

		SetFootGroundAlignment( GroundPlane.n, CalfIdx,FootIdx,HeelIdx,Toe0Idx,ToeNIdx );

		Vec3	FootPos	=	m_AbsolutePose[FootIdx].t;
		Vec3	HeelPos	=	m_AbsolutePose[HeelIdx].t;
		Vec3	Toe0Pos	=	m_AbsolutePose[Toe0Idx].t;

		m_RelativePose[FootIdx].q	=	rel_b0;
		m_AbsolutePose[FootIdx]		=	abs_b0;
		m_AbsolutePose[HeelIdx]		=	abs_b1;
		m_AbsolutePose[Toe0Idx]		=	abs_b2;
		m_AbsolutePose[ToeNIdx]		=	abs_b3;

		//--------------------------------------------------------------------

		Ray ray_heel( HeelPos+Vec3(0.0f,0.0f, 0.6f), Vec3(0,0,-1) );
		//	pIRenderer->Draw_Lineseg(g_AnimatedCharacter*ray_heel.origin,RGBA8(0xff,0x1f,0x1f,0x00),g_AnimatedCharacter*ray_heel.origin+ray_heel.direction*10,RGBA8(0xff,0x1f,0x1f,0x00) );

		f32 heel_dist=0;
		Vec3 output_heel(0,0,0);
		bool _heel = Intersect::Ray_Plane( ray_heel, GroundPlane, output_heel );
		if (_heel)
		{
			heel_dist = output_heel.z-HeelPos.z;
			//pIRenderer->D8Print("Heel: %x   HeelIntersect: %f %f %f   distance_heel: %f", _heel, output_heel.x,output_heel.y,output_heel.z,heel_dist );
			if (heel_dist<0.001f)
				heel_dist=0.0f;
		}

		//----------------------------------------------------------------

		Ray ray_toe0( Toe0Pos+Vec3(0.0f,0.0f, 0.6f), Vec3(0,0,-1) );
		//pIRenderer->Draw_Lineseg(g_AnimatedCharacter*ray_toe0.origin,RGBA8(0xff,0x1f,0x1f,0x00),g_AnimatedCharacter*ray_toe0.origin+ray_toe0.direction*10.0f,RGBA8(0xff,0x1f,0x1f,0x00) );

		f32 toe0_dist=0;
		Vec3 output_toe0(0,0,0);
		bool _toe0 = Intersect::Ray_Plane( ray_toe0, GroundPlane, output_toe0 );
		if (_toe0)
		{
			toe0_dist = output_toe0.z-Toe0Pos.z;
			//pIRenderer->D8Print("Toe0: %x   Toe0Intersect: %f %f %f   distance_toe: %f", _toe0, output_toe0.x,output_toe0.y,output_toe0.z,toe0_dist );
			if (toe0_dist<0.001f)
				toe0_dist=0.0f;
		}

		//do the ik
		f32 distance = max(toe0_dist,heel_dist);
		if (distance==0)
			break;

		FootPos.z += distance; 
		if (leg==CA_LEG_LEFT)
			SetLFootIK(FootPos);
		if (leg==CA_LEG_RIGHT)
			SetRFootIK(FootPos);

		IkProcess++;
	}

	//--------------------------------------------------------------

	//	pIRenderer->D8Print("  ");
	//	pIRenderer->D8Print("  ");
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"IkProcess: %d",IkProcess );	g_YLine+=16.0f;



	Quat fk_foot		= m_RelativePose[FootIdx].q;
	SetFootGroundAlignment( GroundPlane.n, CalfIdx,FootIdx,HeelIdx,Toe0Idx,ToeNIdx  );

	if (IkProcess)
	{
		*pFootGroundAlign += 3.0f*fabsf(m_pInstance->m_fDeltaTime*m_pSkeletonAnim->m_arrLayerSpeedMultiplier[0]);
		//	*pFootGroundAlign += 3.0f*fabsf(0.014f);
	}
	else
	{
		Vec3	FootPos	=	m_AbsolutePose[FootIdx].t;			
		Vec3	HeelPos	=	m_AbsolutePose[HeelIdx].t;			
		Vec3	Toe0Pos	=	m_AbsolutePose[Toe0Idx].t;			

		Ray ray_heel( HeelPos+Vec3(0.0f,0.0f, 0.6f), Vec3(0,0,-1) );
		f32 heel_dist=0;
		Vec3 output_heel(0,0,0);
		bool _heel = Intersect::Ray_Plane( ray_heel, GroundPlane, output_heel );
		if (_heel)
		{
			heel_dist = output_heel.z-HeelPos.z;
			//		pIRenderer->D8Print("Heel: %x   HeelIntersect: %f %f %f   distance_heel: %f", _heel, output_heel.x,output_heel.y,output_heel.z,heel_dist );
			if (heel_dist<0.00f)
				heel_dist=0.0f;
		}

		//----------------------------------------------------------------

		Ray ray_toe0( Toe0Pos+Vec3(0.0f,0.0f, 0.6f), Vec3(0,0,-1) );
		f32 toe0_dist=0;
		Vec3 output_toe0(0,0,0);
		bool _toe0 = Intersect::Ray_Plane( ray_toe0, GroundPlane, output_toe0 );
		if (_toe0)
		{
			toe0_dist = output_toe0.z-Toe0Pos.z;
			//	pIRenderer->D8Print("Toe0: %x   Toe0Intersect: %f %f %f   distance_toe: %f", _toe0, output_toe0.x,output_toe0.y,output_toe0.z,toe0_dist );
			if (toe0_dist<0.00f)
				toe0_dist=0.0f;
		}

		//do just alignment
		f32 distance = max(toe0_dist,heel_dist);
		if (distance>0)
		{
			*pFootGroundAlign += 3.0f*fabsf(m_pInstance->m_fDeltaTime*m_pSkeletonAnim->m_arrLayerSpeedMultiplier[0]);
			//*pFootGroundAlign += 3.0f*fabsf(0.014f);
			SetFootGroundAlignment( GroundPlane.n, CalfIdx,FootIdx,HeelIdx,Toe0Idx,ToeNIdx  );
		}
		else
		{
			*pFootGroundAlign -= 3.0f*fabsf(m_pInstance->m_fDeltaTime*m_pSkeletonAnim->m_arrLayerSpeedMultiplier[0]);
			//	*pFootGroundAlign -= 3.0f*fabsf(0.014f);
		}
	}

	if (*pFootGroundAlign>1.0f)
		*pFootGroundAlign=1.0f;
	if (*pFootGroundAlign<0.0f)
		*pFootGroundAlign=0.0f;

	f32 a=*pFootGroundAlign;
	f32 t		= -(2*a*a*a - 3*a*a);
	f32 t0	= 1.0f-t;
	f32 t1	= t;
	m_RelativePose[FootIdx].q	= fk_foot*t0 + m_RelativePose[FootIdx].q*t1;
	m_RelativePose[FootIdx].q.Normalize();

	int32 pb1=m_parrModelJoints[FootIdx].m_idxParent;
	int32 pb2=m_parrModelJoints[HeelIdx].m_idxParent;
	int32 pb3=m_parrModelJoints[Toe0Idx].m_idxParent;
	int32 pb4=m_parrModelJoints[ToeNIdx].m_idxParent;
	m_AbsolutePose[FootIdx]	  = m_AbsolutePose[pb1]   * m_RelativePose[FootIdx];
	m_AbsolutePose[HeelIdx]	  = m_AbsolutePose[pb2]   * m_RelativePose[HeelIdx];
	m_AbsolutePose[Toe0Idx]	  = m_AbsolutePose[pb3]   * m_RelativePose[Toe0Idx];
	m_AbsolutePose[ToeNIdx]	  = m_AbsolutePose[pb4]   * m_RelativePose[ToeNIdx];


	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_LFootGroundAlign: %f   t:%f",m_LFootGroundAlign,t );	g_YLine+=16.0f;
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_RFootGroundAlign: %f   t:%f",m_RFootGroundAlign,t );	g_YLine+=16.0f;

	Vec3	FootPos	=	m_AbsolutePose[FootIdx].t;			
	Quat	FootRot	=	m_AbsolutePose[FootIdx].q;			
	Vec3	HeelPos	=	m_AbsolutePose[HeelIdx].t;			
	Vec3	Toe0Pos	=	m_AbsolutePose[Toe0Idx].t;			
	//pIRenderer->Draw_Sphere( Sphere(g_AnimatedCharacter*HeelPos,0.01f),RGBA8(0xff,0x1f,0x1f,0x00) );
	//pIRenderer->Draw_Sphere( Sphere(g_AnimatedCharacter*Toe0Pos,0.01f),RGBA8(0xff,0x1f,0x1f,0x00) );

	//pIRenderer->Draw_Lineseg(g_AnimatedCharacter*FootPos,RGBA8(0xff,0x1f,0x1f,0x00),g_AnimatedCharacter*FootPos+FootRot.GetColumn0()*0.1f,RGBA8(0xff,0x1f,0x1f,0x00) );
	//pIRenderer->Draw_Lineseg(g_AnimatedCharacter*FootPos,RGBA8(0x1f,0xff,0x1f,0x00),g_AnimatedCharacter*FootPos+FootRot.GetColumn1()*0.1f,RGBA8(0x1f,0xff,0x1f,0x00) );
	//pIRenderer->Draw_Lineseg(g_AnimatedCharacter*FootPos,RGBA8(0x1f,0x1f,0xff,0x00),g_AnimatedCharacter*FootPos+FootRot.GetColumn2()*0.1f,RGBA8(0x1f,0x1f,0xff,0x00) );

	return 1;
};


uint32 CSkeletonPose::SetFootGroundAlignment(const Vec3& normal2, int32 CalfIdx,int32 FootIdx,int32 HeelIdx,int32 Toe0Idx,int32 ToeNIdx  )
{
	Vec3 normal = Vec3(normal2.x,normal2.y,normal2.z).GetNormalized();
	assert((fabs_tpl(1-(normal|normal)))<0.001); //check if unit-vector

	/*	
	normal = (normal+normal2).GetNormalized();
	if (normal.z<0.707f)
	normal.z = 0.707f;
	normal.Normalize();
	*/

	int32 pb1=m_parrModelJoints[FootIdx].m_idxParent;
	int32 pb2=m_parrModelJoints[HeelIdx].m_idxParent;
	int32 pb3=m_parrModelJoints[Toe0Idx].m_idxParent;
	int32 pb4=m_parrModelJoints[ToeNIdx].m_idxParent;

	Vec3 AxisX = -m_AbsolutePose[FootIdx].GetColumn0();

	m_AbsolutePose[FootIdx].q = Quat::CreateRotationV0V1(AxisX,normal)*m_AbsolutePose[FootIdx].q;
	m_AbsolutePose[FootIdx].q.Normalize();
	m_RelativePose[FootIdx].q	= !m_AbsolutePose[pb1].q * m_AbsolutePose[FootIdx].q;
	m_RelativePose[FootIdx].q.Normalize();

	Vec3 v0	=	(m_AbsolutePose[CalfIdx].t-m_AbsolutePose[FootIdx].t).GetNormalized();
	Vec3 v1 =  m_AbsolutePose[FootIdx].GetColumn1();
	f32 angle	= acos_tpl( v0|v1 ); 

	f32 up_limit=0.90f;
	f32 down_limit=1.70f;
	f32 rad=0;

	if (angle<up_limit)
		rad = angle-up_limit;
	if (angle>down_limit)
		rad = angle-down_limit;

	Vec3 AxisZ = m_AbsolutePose[FootIdx].GetColumn2();
	Quat qrad=Quat::CreateRotationAA(rad,AxisZ);

	m_AbsolutePose[FootIdx].q	= qrad*m_AbsolutePose[FootIdx].q;
	m_RelativePose[FootIdx].q	=	!m_AbsolutePose[pb1].q * m_AbsolutePose[FootIdx].q;
	m_AbsolutePose[HeelIdx]	  = m_AbsolutePose[pb2]   * m_RelativePose[HeelIdx];
	m_AbsolutePose[Toe0Idx]	  = m_AbsolutePose[pb3]   * m_RelativePose[Toe0Idx];
	m_AbsolutePose[ToeNIdx]	  = m_AbsolutePose[pb4]   * m_RelativePose[ToeNIdx];

	//	IRenderer* pIRenderer = g_pISystem->GetIRenderer();
	//	pIRenderer->D8Print("angle: %f   rad: %f",angle,rad );

	return 1;
};


void CSkeletonPose::EnableFootGroundAlignment(bool enable)
{
	m_enableFootGroundAlignment = enable;
}


void CSkeletonPose::TwoBoneIK(int b0,int b1,int b2, const Vec3r& goal)
{

	if (b0<0)
		CryError("CryAnimation: bone for TwoBoneIK invalid - b0");
	if (b1<0)
		CryError("CryAnimation: bone for TwoBoneIK invalid - b1");
	if (b2<0)
		CryError("CryAnimation: bone for TwoBoneIK invalid - b2");


	Vec3r t0 = m_AbsolutePose[b0].t;
	Vec3r t1 = m_AbsolutePose[b1].t;
	Vec3r t2 = m_AbsolutePose[b2].t;

	Vec3r dif = goal-t2; 
	if ( dif.GetLength()<0.001f )
		return; //end-effector and new goal are very close

	Vec3r a = t1-t0; 
	Vec3r b = t2-t1; 
	Vec3r c = goal-t0;
	Vec3 cross=a%b; 
	if ((cross|cross)<0.00001) return;

	f64 alen = a.GetLength(); 
	f64 blen = b.GetLength(); 
	f64 clen = c.GetLength();
	if (alen*clen<0.0001)
		return;

	a/=alen; 
	c/=clen; 

	f64 cos0	= clamp((alen*alen+clen*clen-blen*blen)/(2*alen*clen),-1.0,+1.0);
	f64 sin0	=	sqrt( max(1.0-cos0*cos0,0.0) );
	a = a.GetRotated( (a%b).GetNormalized(), cos0,sin0 );
	assert((fabs_tpl(1-(a|a)))<0.001);   //check if unit-vector

	{
		Vec3r h=(a+c).GetNormalized();
		Vec3r axis=a%c;	f64 sine=axis.GetLength(); axis/=sine;
		Vec3r trans	= t0-t0.GetRotated(axis,a|c,sine);
		QuatT qt(Quat(f32(a|h),axis*(a%h).GetLength()),trans);

		m_AbsolutePose[b0] = qt * m_AbsolutePose[b0];
		m_AbsolutePose[b1] = qt * m_AbsolutePose[b1];
		m_AbsolutePose[b2] = qt * m_AbsolutePose[b2];
	}

	//---------------------------------------------------

	{
		t1 = m_AbsolutePose[b1].t;
		t2 = m_AbsolutePose[b2].t;
		c = (goal-t1).GetNormalized(); b = (t2-t1)/blen;

		Vec3r h = (b+c).GetNormalized(); 
		Vec3r axis=b%c; f64 sine=axis.GetLength(); axis/=sine; 
		Vec3r trans	= t1-t1.GetRotated(axis,b|c,sine);
		QuatT qt2( Quat(f32(b|h),axis*(b%h).GetLength()), trans );
		m_AbsolutePose[b1] = qt2 * m_AbsolutePose[b1];
	}

	int32 pb0=m_parrModelJoints[b0].m_idxParent;
	int32 pb1=m_parrModelJoints[b1].m_idxParent;
	m_RelativePose[b0].q = !m_AbsolutePose[pb0].q * m_AbsolutePose[b0].q;
	m_RelativePose[b1].q = !m_AbsolutePose[pb1].q * m_AbsolutePose[b1].q;
}





