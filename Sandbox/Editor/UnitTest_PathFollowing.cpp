//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File: PathFollowing.cpp
//  Implementation of the unit test to preview path-following
//
//	History:
//	October 16, 2006: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ModelViewport.h"
#include "CharacterEditor\CharPanel_Animation.h"
#include "ICryAnimation.h"
#include <I3DEngine.h>
#include <IPhysics.h>
#include <ITimer.h>
#include "IRenderAuxGeom.h"

extern uint32 g_ypos;



//------------------------------------------------------------------------------
//---              simple path-following test-application                    ---
//------------------------------------------------------------------------------

uint32 rcr_key_Z=0,flip_Z=2;
uint32 rcr_key_H=0,flip_H=2;
uint32 rcr_key_J=0,flip_J=1;
uint32 rcr_key_T=0,flip_T=0;

void CModelViewport::PathFollowing_UnitTest( ICharacterInstance* pInstance,IAnimationSet* pIAnimationSet, SRendParams rp )
{

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
	f32 FrameTime = GetIEditor()->GetSystem()->GetITimer()->GetFrameTime();

	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();
	pISkeletonAnim->SetCharEditMode(1);

	GetISystem()->GetIAnimationSystem()->SetScalingLimits( Vec2(0.5f, 2.0f) );


	m_absCurrentSpeed			=	pISkeletonAnim->GetCurrentVelocity().GetLength();
	m_absCurrentSlope			=	pISkeletonAnim->GetCurrentSlope();

	SMotionParams mp = EntityFollowing(g_ypos);
	PathFollowing(mp, mp.m_fCatchUpSpeed);

	{
		rcr_key_Z<<=1;
		if (CheckVirtualKey('Z'))	rcr_key_Z|=1;
		if ((rcr_key_Z&3)==1) flip_Z++;
		flip_Z=flip_Z&1;

		static f32 rad=0.0f; 
		rad -= FrameTime*0.5f;
		m_vWorldAimBodyDirection	=	Vec2(Matrix33::CreateRotationZ(rad).GetColumn1());

		AABB aabb = AABB(Vec3(-0.03f,-0.03f,-0.03f),Vec3(+0.03f,+0.03f,+0.03f));
		OBB obb=OBB::CreateOBBfromAABB( Matrix33::CreateRotationZ(0),aabb );
		Vec3 wsource = m_AnimatedCharacter.t+Vec3(0,0,1);
		Vec3 wtarget = wsource+m_vWorldAimBodyDirection*4;
		pAuxGeom->DrawLine( wsource,RGBA8(0x00,0x00,0x00,0x00), wtarget,RGBA8(0xff,0xff,0xff,0x00) );
		pAuxGeom->DrawOBB(obb,wtarget,1,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);

		pISkeletonPose->SetLookIK(1,DEG2RAD(120),wtarget);
		pISkeletonPose->SetAimIK(flip_Z,wtarget);
	}


	m_relCameraRotZ=0;
	m_relCameraRotX=0;
	if (m_pCharPanel_Animation)
	{
		GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_JustRootUpdate")->Set( mv_showRootUpdate );
	}

	//update animation
	pISkeletonAnim->SetFuturePathAnalyser(0);
	pISkeletonAnim->SetAnimationDrivenMotion(1);

	int LocomotionCallback(ICharacterInstance* pInstance,void* pPlayer);
	pISkeletonAnim->SetPreProcessCallback(LocomotionCallback,this);

	pISkeletonPose->SetPostProcessCallback0(0,this);


	//	QuatTS Offset =  m_PhysEntityLocation.GetInverted()*m_AnimatedCharacter;
//	pInstance->SetWorldLocation(m_AnimatedCharacter);
	pInstance->SkeletonPreProcess( m_PhysEntityLocation,m_AnimatedCharacter, GetCamera(),0  );

	//this is the "faked" physical update. It doesn't matter when we do it, 
	//as long as it happens before "SkeletonPostProcess" or after rendering
	static f32 RadomRot=0.0f;
	//	RadomRot+=FrameTime*43;
	m_PhysEntityLocation.q.SetRotationZ( RadomRot );
	m_PhysEntityLocation.t = m_vSmoothEntityPosition;
	m_vSmoothEntityPosition.z=0;

//	pInstance->SetWorldLocation(m_AnimatedCharacter);
	pInstance->SkeletonPostProcess( m_PhysEntityLocation, m_AnimatedCharacter, 0, (m_PhysEntityLocation.t-m_absCameraPos).GetLength(), 0 ); 

	m_EntityMat			=	Matrix34(m_PhysEntityLocation);
	rp.pMatrix			= &m_EntityMat;
	rp.pPrevMatrix	= &m_PrevEntityMat;
	rp.fDistance		= (m_PhysEntityLocation.t-m_absCameraPos).GetLength();
	pInstance->Render( rp, IDENTITY );

	if (mv_showGrid)
		DrawHeightField();

	//------------------------------------------------------
	//---   draw the path of the past
	//------------------------------------------------------
	uint32 numEntries=m_arrAnimatedCharacterPath.size();
	for (int32 i=(numEntries-2); i>-1; i--)
		m_arrAnimatedCharacterPath[i+1] = m_arrAnimatedCharacterPath[i];
	m_arrAnimatedCharacterPath[0] = m_AnimatedCharacter.t; 
	AABB aabb;
	for (uint32 i=0; i<numEntries; i++)
	{
		aabb.min=Vec3(-0.01f,-0.01f,-0.01f)+m_arrAnimatedCharacterPath[i];
		aabb.max=Vec3(+0.01f,+0.01f,+0.01f)+m_arrAnimatedCharacterPath[i];
		pAuxGeom->DrawAABB(aabb,1, RGBA8(0x00,0x00,0xff,0x00),eBBD_Extremes_Color_Encoded );
	}

	numEntries=m_arrSmoothEntityPath.size();
	for (int32 i=(numEntries-2); i>-1; i--)
		m_arrSmoothEntityPath[i+1] = m_arrSmoothEntityPath[i];
	m_arrSmoothEntityPath[0] = m_vSmoothEntityPosition; 
	for (uint32 i=0; i<numEntries; i++)
	{
		aabb.min=Vec3(-0.01f,-0.01f,-0.01f)+m_arrSmoothEntityPath[i];
		aabb.max=Vec3(+0.01f,+0.01f,+0.01f)+m_arrSmoothEntityPath[i];
		pAuxGeom->DrawAABB(aabb,1, RGBA8(0xff,0x00,0x00,0x00),eBBD_Extremes_Color_Encoded );
	}


	GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_DrawSkeleton")->Set( mv_showSkeleton );
}



//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
int LocomotionCallback(ICharacterInstance* pInstance,void* pPlayer)
{
	//process bones specific stuff (IK, torso rotation, etc)
	((CModelViewport*)pPlayer)->LocomotionCallback(pInstance);
	return 1;
}
void CModelViewport::LocomotionCallback(ICharacterInstance* pInstance)
{

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();


	float color1[4] = {1,1,1,1};
	//	m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"Path Following Callback" );
	g_ypos+=10;


	QuatT offset = pISkeletonAnim->GetRelMovement();

	// this is the kinematic update
	m_AnimatedCharacter = m_AnimatedCharacter*offset; 
	m_AnimatedCharacter.q.Normalize();
	m_AnimatedCharacter.t += pISkeletonAnim->GetRelFootSlide();
	m_AnimatedCharacter.t.z=0;

	//...........
	//here is the best place to check the difference between entity and AC and do the clamping	
	//...........

	//if we are happy with the new AC-location we set the final render position
	//	pInstance->SetAnimatedCharacter(m_AnimatedCharacter);



	return;


	/*
	//----------------------------------------------------------------
	//---     clamp position                                       ---
	//----------------------------------------------------------------
	Vec3 Dir2Entity		= (m_vSmoothEntityPosition-m_AnimatedCharacter.t);
	Vec3 nDir2Entity	= Dir2Entity.GetNormalized();
	f32	DistancePE		= Dir2Entity.GetLength();

	const f32 ClampDistance=1.5f;
	if (DistancePE>ClampDistance)
	{
	Vec3 PlayerPos=m_vSmoothEntityPosition - nDir2Entity*ClampDistance;
	m_AnimatedCharacter.SetTranslation(PlayerPos);
	}

	//----------------------------------------------------------------
	//---     clamp rotation                                       ---
	//----------------------------------------------------------------
	const f32 ClampRadian=DEG2RAD(45.0f);
	f32 RadianDeviation = Ang3::CreateRadZ(m_vAbsCurrentBodyDirection,m_absDesiredBodyDirection);
	if (RadianDeviation<-ClampRadian)
	{
	f32 Deviation=RadianDeviation+ClampRadian;
	Quat m33=Quat::CreateRotationZ(Deviation);
	m_AnimatedCharacter=m_AnimatedCharacter*m33;
	//m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"Procedural Turn Blend ");
	//ypos+=10;
	}

	if (RadianDeviation>ClampRadian)
	{
	f32 Deviation=RadianDeviation-ClampRadian;
	Quat m33=Quat::CreateRotationZ(Deviation);
	m_AnimatedCharacter=m_AnimatedCharacter*m33;
	//m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"Procedural Turn Blend");
	//ypos+=10;
	}
	*/
	Vec3 absAxisX		=	m_AnimatedCharacter.GetColumn0();
	Vec3 absAxisY		=	m_AnimatedCharacter.GetColumn1();
	Vec3 absAxisZ		=	m_AnimatedCharacter.GetColumn2();
	Vec3 absRootPos	=	m_AnimatedCharacter.t+pISkeletonPose->GetAbsJointByID(0).t;
	pAuxGeom->DrawLine( absRootPos,RGBA8(0xff,0x00,0x00,0x00), absRootPos+m_AnimatedCharacter.GetColumn0()*2.0f,RGBA8(0x00,0x00,0x00,0x00) );
	pAuxGeom->DrawLine( absRootPos,RGBA8(0x00,0xff,0x00,0x00), absRootPos+m_AnimatedCharacter.GetColumn1()*2.0f,RGBA8(0x00,0x00,0x00,0x00) );
	pAuxGeom->DrawLine( absRootPos,RGBA8(0x00,0x00,0xff,0x00), absRootPos+m_AnimatedCharacter.GetColumn2()*2.0f,RGBA8(0x00,0x00,0x00,0x00) );

}





#define STATE_STAND (0)
#define STATE_WALK (1)
#define STATE_WALKSTRAFELEFT  (2)
#define STATE_WALKSTRAFERIGHT (3)
#define STATE_RUN (4)


#define RELAXED (0)
#define COMBAT (1)
#define STEALTH (2)
#define CROUCH (3)
#define PRONE (4)



extern int32 g_ShiftTarget;
extern int32 g_ShiftSource;
extern char* g_NewName;


const uint32 LIMIT=8*5;
Vec3 g_Path[LIMIT];	



SMotionParams CModelViewport::EntityFollowing( uint32& ypos )
{

	AABB aabb;
	SMotionParams mp;
	float color1[4] = {1,1,1,1};
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );

	f32 FrameTime = GetISystem()->GetITimer()->GetFrameTime();
	f32 TimeScale = GetISystem()->GetITimer()->GetTimeScale();


	f32 fPredictedTime	=	0.50f;
	Vec3 vFutureMoveDir	=	Vec3(0,1,0);
	Vec3 vDistance			=	Vec3(0,0,0);

	f32 _DesiredSpeed = 2.20f;	


	rcr_key_H<<=1;
	if (CheckVirtualKey('H'))	rcr_key_H|=1;
	if ((rcr_key_H&3)==1) flip_H++;
	flip_H=flip_H&3;


	if (flip_H==0) _DesiredSpeed=1.5f;
	if (flip_H==1) _DesiredSpeed=2.5f;
	if (flip_H==2) _DesiredSpeed=4.5f;
	if (flip_H==3) _DesiredSpeed=6.0f;


	rcr_key_J<<=1;
	if (CheckVirtualKey('J'))	rcr_key_J|=1;
	if ((rcr_key_J&3)==1) flip_J++;
	flip_J=flip_J&1;

	rcr_key_T<<=1;
	if (CheckVirtualKey('T'))	rcr_key_T|=1;
	if ((rcr_key_T&3)==1) flip_T++;
	flip_T=flip_T&1;

	f32 fBodyMoveRadian = Ang3::CreateRadZ( m_vWorldDesiredBodyDirectionSmooth, m_vWorldDesiredMoveDirectionSmooth );
	f32 fStrafeSlowDown = (gf_PI-fabsf(fBodyMoveRadian*0.60f))/gf_PI;
	m_MoveSpeedMSec *= fStrafeSlowDown;

	static f32 SmoothDesiredSpeed = 4.20f;
	static f32 SmoothDesiredSpeedRate = 4.20f;
	SmoothCD(SmoothDesiredSpeed, SmoothDesiredSpeedRate, FrameTime, _DesiredSpeed*fStrafeSlowDown, 0.25f);
	PathCreation( SmoothDesiredSpeed );


	static uint32 PathPoint0	= 0;
	static uint32 PathPoint1	= 0;

	static Vec3 EntityPosition	= g_Path[PathPoint0];
	static f32 EntityPercentage	= 0;
	static Vec3 EntityDirection	= Vec3(0,0,0);

	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"DesiredSpeed: %f  fStrafeSlowDown:%f  SmoothDesiredSpeed: %f",_DesiredSpeed,fStrafeSlowDown,SmoothDesiredSpeed);
	ypos+=10;




	//-----------------------------------------------------------------------------
	//----               move the entity along the sharp path                  ----
	//-----------------------------------------------------------------------------
	PathPoint1=PathPoint0+1;	if (PathPoint1>=LIMIT) PathPoint1-=LIMIT;

	Vec3 p0 = g_Path[PathPoint0];
	Vec3 p1 = g_Path[PathPoint1];
	Vec3 direction = (p1-p0).GetNormalized();; 

	f32 t=EntityPercentage;
	EntityPosition = p0*(1.0f-t) + p1*t;

	EntityPosition += direction*(SmoothDesiredSpeed*FrameTime)*flip_J;
	EntityPercentage = (EntityPosition-p0).GetLength() / (p1-p0).GetLength();
	EntityDirection =	direction;

	Plane plane1 = Plane::CreatePlane(direction,p1);
	f32 Distance2NewEdge = plane1|EntityPosition;

	//check if we are beyond the line-segment
	if (Distance2NewEdge>0)
	{
		PathPoint0++;	if (PathPoint0>=LIMIT) PathPoint0-=LIMIT;
		PathPoint1=PathPoint0+1;	if (PathPoint1>=LIMIT) PathPoint1-=LIMIT;
		Vec3 p0=g_Path[PathPoint0];
		Vec3 p1=g_Path[PathPoint1];
		Vec3 newdirection = (p1-p0).GetNormalized();; 
		EntityPosition	= g_Path[PathPoint0] + newdirection*Distance2NewEdge;
		EntityDirection =	newdirection;
		EntityPercentage = (EntityPosition-p0).GetLength()/(p1-p0).GetLength();
	}
	EntityPosition.z=0;



	//-----------------------------------------------------------------------------
	//----               move tracker along the path                           ----
	//-----------------------------------------------------------------------------
	const uint32 NumTRACKER=32; //amount of trackers for one second

	Vec3 PosTracker[NumTRACKER*2 +1]; //trackers go in both directions
	Vec3 DirTracker[NumTRACKER*2 +1];

	f32 TrackersDistance = SmoothDesiredSpeed/NumTRACKER;
	Vec3 TempPosTracker=EntityPosition;
	Vec3 TempDirTracker=EntityDirection;
	PosTracker[NumTRACKER]=EntityPosition;
	DirTracker[NumTRACKER]=EntityDirection;

	int32 TempPoint0=PathPoint0;
	int32 TempPoint1=PathPoint0;



	for (int32 i=(NumTRACKER-1); i>-1; i--)	
	{
		TempPoint1=TempPoint0+1;	if (TempPoint1>=LIMIT) TempPoint1-=LIMIT;

		Vec3 p0 = g_Path[TempPoint0];
		Vec3 p1 = g_Path[TempPoint1];
		Vec3 direction = (p1-p0).GetNormalized();; 
		TempPosTracker -= direction*TrackersDistance;
		TempDirTracker = direction;

		Plane plane0 = Plane::CreatePlane(direction,p0);
		f32 Distance2OldEdge = plane0|TempPosTracker;

		if (Distance2OldEdge<0)
		{
			TempPoint0--;	 if (TempPoint0<0) TempPoint0+=LIMIT;
			TempPoint1=TempPoint0+1;	if (TempPoint1>=LIMIT) TempPoint1-=LIMIT;
			Vec3 p0=g_Path[TempPoint0];
			Vec3 p1=g_Path[TempPoint1];
			Vec3 newdirection = (p1-p0).GetNormalized();; 
			TempPosTracker	= g_Path[TempPoint1] + newdirection*Distance2OldEdge;
			TempDirTracker	= newdirection;
		}
		TempPosTracker.z=0;
		PosTracker[i]=TempPosTracker;
		DirTracker[i]=TempDirTracker;
	}
	if (flip_T)
	{
		for (uint32 i=0; i<NumTRACKER; i++)
		{
			Vec3 pos=PosTracker[i]; 
			aabb = AABB(Vec3(-0.01f,-0.01f, 0.0f)+pos,Vec3(+0.01f,+0.01f,+0.10f)+pos);
			pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);
			//	pAuxGeom->DrawLine( Vec3(pos.x,pos.y,0.1f),RGBA8(0x00,0x00,0x00,0x00), Vec3(pos.x,pos.y,0.1f)+DirTracker[i]*0.1f,RGBA8(0xff,0xff,0xff,0x00) );
		}
	}


	if (flip_T)
	{
		Vec3 pos=PosTracker[NumTRACKER]; 
		aabb = AABB(Vec3(-0.01f,-0.01f, 0.0f)+pos,Vec3(+0.01f,+0.01f,+0.20f)+pos);
		pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0xff,0xff,0xff), eBBD_Extremes_Color_Encoded);
		pAuxGeom->DrawLine( Vec3(pos.x,pos.y,0.1f),RGBA8(0x00,0x00,0x00,0x00), Vec3(pos.x,pos.y,0.1f)+DirTracker[NumTRACKER]*0.1f,RGBA8(0xff,0xff,0xff,0x00) );
	}


	TempPosTracker=EntityPosition;
	TempPoint0=PathPoint0;
	TempPoint1=PathPoint0;
	for (uint32 i=(NumTRACKER+1); i<(NumTRACKER*2+1); i++)	
	{
		TempPoint1=TempPoint0+1;	if (TempPoint1>=LIMIT) TempPoint1-=LIMIT;
		Vec3 p0 = g_Path[TempPoint0];
		Vec3 p1 = g_Path[TempPoint1];
		Vec3 direction = (p1-p0).GetNormalized();; 
		TempPosTracker += direction*TrackersDistance;
		TempDirTracker = direction;

		Plane plane1 = Plane::CreatePlane(direction,p1);
		f32 Distance2NewEdge = plane1|TempPosTracker;

		if (Distance2NewEdge>0)
		{
			TempPoint0++;	if (TempPoint0>=LIMIT) TempPoint0-=LIMIT;
			TempPoint1=TempPoint0+1;	if (TempPoint1>=LIMIT) TempPoint1-=LIMIT;
			Vec3 p0=g_Path[TempPoint0];
			Vec3 p1=g_Path[TempPoint1];
			Vec3 newdirection = (p1-p0).GetNormalized();; 
			TempPosTracker	= g_Path[TempPoint0] + newdirection*Distance2NewEdge;
			TempDirTracker	= newdirection;
		}
		TempPosTracker.z=0;
		PosTracker[i]=TempPosTracker;
		DirTracker[i]=TempDirTracker;
	}

	if (flip_T)
	{
		for (uint32 i=(NumTRACKER+1); i<(NumTRACKER*2+1); i++)
		{
			Vec3 pos=PosTracker[i]; 
			aabb = AABB(Vec3(-0.01f,-0.01f, 0.0f)+pos,Vec3(+0.01f,+0.01f,+0.10f)+pos);
			pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0x00,0xff,0x00,0xff), eBBD_Extremes_Color_Encoded);
			//	pAuxGeom->DrawLine( Vec3(pos.x,pos.y,0.1f),RGBA8(0x00,0x00,0x00,0x00), Vec3(pos.x,pos.y,0.1f)+DirTracker[i]*0.1f,RGBA8(0xff,0xff,0xff,0x00) );
		}
	}



	//--------------------------------------------------------
	//----     pseudo-curve fitting                       ----
	//--------------------------------------------------------
	uint32 skip=16;

	uint32 s0=NumTRACKER-skip;
	uint32 e0=NumTRACKER+skip;
	m_vSmoothEntityPosition=Vec3(ZERO);
	for (uint32 i=s0; i<=e0; i++) m_vSmoothEntityPosition+=PosTracker[i]; 
	m_vSmoothEntityPosition /= 1+(e0-s0);


	Vec3 OldPos = m_vSmoothEntityCatchUpPos;
	uint32 s1=NumTRACKER-skip+1;
	uint32 e1=NumTRACKER+skip+1;
	m_vSmoothEntityCatchUpPos=Vec3(ZERO);
	for (uint32 i=s1; i<=e1; i++) m_vSmoothEntityCatchUpPos+=PosTracker[i]; 
	m_vSmoothEntityCatchUpPos /= 1+(e1-s1);
	Vec3 NewPos = m_vSmoothEntityCatchUpPos;
	f32 EntityRealSpeed = (NewPos-OldPos).GetLength() * (1/m_AverageFrameTime);

	uint32 s2=NumTRACKER-skip+6;
	uint32 e2=NumTRACKER+skip+6;
	m_vSmoothEntityDirection=Vec3(ZERO);
	for (uint32 i=s2; i<=e2; i++) 	m_vSmoothEntityDirection+=DirTracker[i]; 
	m_vSmoothEntityDirection.Normalize();


	uint32 nInFuture = uint32(NumTRACKER*fPredictedTime);
	Vec3 FuturePos = PosTracker[NumTRACKER+nInFuture];
	vFutureMoveDir = DirTracker[NumTRACKER+nInFuture];

	vDistance=FuturePos-m_AnimatedCharacter.t;
	if (flip_T)
	{
		aabb = AABB(Vec3(-0.35f,-0.1f, 0.0f),Vec3(+0.35f,+0.4f,+2.0f));
		f32 radian = Ang3::CreateRadZ(Vec3(0,1,0),m_vSmoothEntityDirection);
		OBB obb=OBB::CreateOBBfromAABB( Matrix33::CreateRotationZ(radian),aabb );
		//	pAuxGeom->DrawOBB(obb,m_vSmoothEntityPosition,0,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);

		aabb = AABB(Vec3(-0.01f,-0.01f, 0.0f)+m_vSmoothEntityCatchUpPos,Vec3(+0.01f,+0.01f,+3.0f)+m_vSmoothEntityCatchUpPos);
		//	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);

		pAuxGeom->DrawLine( m_AnimatedCharacter.t+Vec3(0,0,1.0f),RGBA8(0x3f,0x3f,0x3f,0x00), m_AnimatedCharacter.t+vDistance+Vec3(0,0,1.0f),RGBA8(0xff,0xff,0xff,0x00) );

		pAuxGeom->DrawLine( FuturePos+Vec3(0,0,1.01f),RGBA8(0x3f,0x3f,0x00,0x00), FuturePos+Vec3(0,0,1.01f)+vFutureMoveDir,RGBA8(0xff,0xff,0x00,0x00) );
		pAuxGeom->DrawLine( FuturePos+Vec3(0,0,1.02f),RGBA8(0x3f,0x00,0x00,0x00), FuturePos+Vec3(0,0,1.02f)+m_vWorldAimBodyDirection,RGBA8(0xff,0x00,0x00,0x00) );

		aabb = AABB(Vec3(-0.01f,-0.01f, 0.0f)+FuturePos,Vec3(+0.01f,+0.01f,+3.0f)+FuturePos);
		pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0xff,0x00,0xff), eBBD_Extremes_Color_Encoded);
	}


	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------
	//-----------------------------------------------------------------------------

	//		pAuxGeom->DrawLine( Vec3(EntityPosition.x,EntityPosition.y,2),RGBA8(0x00,0x00,0x00,0x00), Vec3(EntityPosition.x,EntityPosition.y,2)+direction*3,RGBA8(0xff,0xff,0xff,0x00) );
	//		aabb = AABB(Vec3(-0.1f,-0.1f, 0.0f)+EntityPosition,Vec3(+0.1f,+0.1f,+0.3f)+EntityPosition);
	//		pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0x7f,0x7f,0x7f,0xff), eBBD_Extremes_Color_Encoded);


	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"EntityRealSpeed: %f",EntityRealSpeed);
	ypos+=10;





	//-----------------------------------------------------------------------------------------
	//---                  Here we choose the MOVE-direction of the AC                      ---
	//---  to find the correct MOVE direction we make a blend of the entity-move direction  ---
	//---                and the distance-vector of the AC to the entity                    ---
	//---  The closer the AC to the entity, the higher the priority of the entity direction. ---
	//---  If the AC is far away from the entity, then we try to run directly               ---
	//---  to the entity position                                                           ---
	//-----------------------------------------------------------------------------------------
	Vec3 Dir2Entity		= (m_vSmoothEntityCatchUpPos-m_AnimatedCharacter.t)*0.5f;
	m_vWorldDesiredMoveDirection = Vec2( m_vSmoothEntityDirection + Dir2Entity ).GetNormalized();


	//----------------------------------------------------------------------------------
	//---                  choose the player-speed                              --------
	//----------------------------------------------------------------------------------

	Plane entityplane1 = Plane::CreatePlane(m_vWorldDesiredMoveDirection,m_vSmoothEntityPosition);
	f32 PlayerFrontBack1 = entityplane1|m_AnimatedCharacter.t;

	Plane entityplane2 = Plane::CreatePlane(m_vWorldDesiredMoveDirection,m_vSmoothEntityCatchUpPos);
	f32 PlayerFrontBack2 = entityplane2|m_AnimatedCharacter.t;

	f32 CatchUpSpeed = EntityRealSpeed - PlayerFrontBack2*1;
	if (CatchUpSpeed>9.0f) CatchUpSpeed=9.0f;
	if (CatchUpSpeed<0.5f) CatchUpSpeed=0.5f;

	static f32 SmoothCatchUpSpeed = 0;
	static f32 SmoothCatchUpSpeedRate = 0;
	SmoothCD(SmoothCatchUpSpeed, SmoothCatchUpSpeedRate, m_AverageFrameTime, CatchUpSpeed, 0.15f);

	m_renderer->Draw2dLabel(12,ypos,2.2f,color1,false,"PlayerFrontBack1: %f  CatchUpSpeed: %f",PlayerFrontBack1,SmoothCatchUpSpeed);
	ypos+=20;


	//------------------------------------------------------------------------------------------------
	//---   choose the right BODY direction (and then adjust the local MOVE direction) ---------------
	//------------------------------------------------------------------------------------------------

	static f32 SmoothStrafeTurn=0;
	static f32 SmoothStrafeTurnRate=0;
	f32 fRadMoveAim = Ang3::CreateRadZ(m_vWorldAimBodyDirection,m_vWorldDesiredMoveDirection);
	SmoothCD(SmoothStrafeTurn, SmoothStrafeTurnRate, m_AverageFrameTime, f32(flip_Z), fabsf(fRadMoveAim*0.4f) );


	//by default we assume that we are running forward: BODY=MOVE 
	//if not, then we blend into the aim-direction:     BODY=MOVE*f+AIM*s
	f32 f=1.0f-SmoothStrafeTurn;
	f32 s=SmoothStrafeTurn;
	m_vWorldDesiredBodyDirection	=	m_vWorldDesiredMoveDirection*f + m_vWorldAimBodyDirection*s;

	//recalculate the new "local" move direction
	f32 fRadMoveBody = Ang3::CreateRadZ(m_vWorldDesiredBodyDirection,m_vWorldDesiredMoveDirection);
	m_vLocalDesiredMoveDirection = Vec2(-cry_sinf(fRadMoveBody),cry_cosf(fRadMoveBody));

	//because the changes in MOVE- and BODY-direction can be extreme from frame to frame, we have to smooth them out. 
	SmoothCD(m_vLocalDesiredMoveDirectionSmooth,m_vLocalDesiredMoveDirectionSmoothRate, m_AverageFrameTime, m_vLocalDesiredMoveDirection, 0.15f);
	SmoothCD(m_vWorldDesiredBodyDirectionSmooth,m_vWorldDesiredBodyDirectionSmoothRate, m_AverageFrameTime, m_vWorldDesiredBodyDirection, 0.15f);
	//	m_vLocalDesiredMoveDirectionSmooth = m_vLocalDesiredMoveDirection;
	//	m_vWorldDesiredBodyDirectionSmooth = m_vWorldDesiredBodyDirection;


	f32 fRadBodyDesired = -atan2f(m_vWorldDesiredBodyDirectionSmooth.x,m_vWorldDesiredBodyDirectionSmooth.y);
	Matrix33 MatBodyDesired33=Matrix33::CreateRotationZ(fRadBodyDesired); 
	m_vWorldDesiredMoveDirectionSmooth=MatBodyDesired33*m_vLocalDesiredMoveDirectionSmooth;


	m_renderer->Draw2dLabel(12,ypos,2.2f,color1,false,"SmoothStrafeTurn: %f   flip_Z: %d",SmoothStrafeTurn,flip_Z);
	ypos+=20;

	mp.m_fCatchUpSpeed		=	SmoothCatchUpSpeed;
	return mp;
}





//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
f32 CModelViewport::PathCreation( f32 fDesiredSpeed  )
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );

	AABB aabb;
	f32 div=0.60f;
	f32 radiants[LIMIT];	

	g_Path[ 0] = Vec3(ZERO);  //these are the points we calculate in real-time based on the travel-speed and path-corner
	g_Path[ 1] = Vec3(ZERO);
	g_Path[ 2] = Vec3( 15*div, 15*div, 0.003f);
	g_Path[ 3] = Vec3(ZERO);  //these are the points we calculate in real-time based on the travel-speed and path-corner
	g_Path[ 4] = Vec3(ZERO);

	g_Path[ 5] = Vec3(ZERO);
	g_Path[ 6] = Vec3(ZERO);
	g_Path[ 7] = Vec3(  0*div, 10*div, 0.003f);
	g_Path[ 8] = Vec3(ZERO);
	g_Path[ 9] = Vec3(ZERO);

	g_Path[10] = Vec3(ZERO);
	g_Path[11] = Vec3(ZERO);
	g_Path[12] = Vec3(-25*div, 25*div, 0.003f); //peak
	g_Path[13] = Vec3(ZERO);
	g_Path[14] = Vec3(ZERO);

	g_Path[15] = Vec3(ZERO);
	g_Path[16] = Vec3(ZERO);
	g_Path[17] = Vec3( -5*div,  0*div, 0.003f);
	g_Path[18] = Vec3(ZERO);
	g_Path[19] = Vec3(ZERO);

	g_Path[20] = Vec3(ZERO);
	g_Path[21] = Vec3(ZERO);
	g_Path[22] = Vec3(-15*div,-15*div, 0.003f);
	g_Path[23] = Vec3(ZERO);
	g_Path[24] = Vec3(ZERO);

	g_Path[25] = Vec3(ZERO);
	g_Path[26] = Vec3(ZERO);
	g_Path[27] = Vec3(  0*div,-20*div, 0.003f);
	g_Path[28] = Vec3(ZERO);
	g_Path[29] = Vec3(ZERO);

	g_Path[30] = Vec3(ZERO);
	g_Path[31] = Vec3(ZERO);
	g_Path[32] = Vec3( 15*div,-15*div, 0.003f);
	g_Path[33] = Vec3(ZERO);
	g_Path[34] = Vec3(ZERO);

	g_Path[35] = Vec3(ZERO);
	g_Path[36] = Vec3(ZERO);
	g_Path[37] = Vec3( 19*div,  0*div, 0.003f);
	g_Path[38] = Vec3(ZERO);
	g_Path[39] = Vec3(ZERO);


	aabb = AABB(Vec3(-0.05f,-0.05f, 0.0f)+g_Path[2],Vec3(+0.05f,+0.05f,+0.5f)+g_Path[2]);
	//	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);


	//calculate the empty path-points
	for (uint32 i=0; i<LIMIT; i=i+5)
	{
		uint32 i2=i+2; if (i2>=LIMIT) i2-=LIMIT; //this is the first point on line-segment
		uint32 i3=i+3; if (i3>=LIMIT) i3-=LIMIT; //dynamic point-1
		uint32 i4=i+4; if (i4>=LIMIT) i4-=LIMIT; //dynamic point-2

		uint32 i5=i+5; if (i5>=LIMIT) i5-=LIMIT; //dynamic point-3
		uint32 i6=i+6; if (i6>=LIMIT) i6-=LIMIT; //dynamic point-4
		uint32 i7=i+7; if (i7>=LIMIT) i7-=LIMIT; //this is the second point on line-segment 
		pAuxGeom->DrawLine( g_Path[i2],RGBA8(0x00,0xff,0x00,0x00), g_Path[i7],RGBA8(0xff,0xff,0xff,0xff) );

		Vec3 vDistance=g_Path[i7]-g_Path[i2];
		f32 fLength=vDistance.GetLength();
		f32 fHLength=fLength*0.4f;
		Vec3 n=vDistance/fLength; //DANGEROUS: can be a DivByZero

		f32 ds = fDesiredSpeed;
		Vec3 vSwing = n*(ds*0.60f);
		f32 fLenSwing=vSwing.GetLength();
		if ( fHLength < fLenSwing )
			vSwing=vSwing.GetNormalized()*fHLength;	

		g_Path[i3]=g_Path[i2]+vSwing*0.5f; //swing-out point
		g_Path[i4]=g_Path[i2]+vSwing;
		g_Path[i5]=g_Path[i7]-vSwing; 
		g_Path[i6]=g_Path[i7]-vSwing*0.5f; //swing-out point
	}


	//calculate the radiants between line-segments 
	for (uint32 i=0; i<LIMIT; i++)
	{
		uint32 p0=i-1;	if (p0>=LIMIT) p0-=LIMIT;
		uint32 p1=i;		if (p1>=LIMIT) p1-=LIMIT;
		uint32 p2=i+1;	if (p2>=LIMIT) p2-=LIMIT;
		Vec3 v0=g_Path[p1]-g_Path[p0];
		Vec3 v1=g_Path[p2]-g_Path[p1];
		radiants[i]	=	Ang3::CreateRadZ(v0,v1)/gf_PI; //range [-1 ... +1] 
	}

	//swing-out when running along curves
	for (uint32 i=0; i<LIMIT; i=i+5)
	{
		uint32 i2=i+2; if (i2>=LIMIT) i2-=LIMIT;  //this is the first point on line-segment
		uint32 i3=i+3; if (i3>=LIMIT) i3-=LIMIT;
		uint32 i4=i+4; if (i4>=LIMIT) i4-=LIMIT;

		uint32 i5=i+5; if (i5>=LIMIT) i5-=LIMIT;
		uint32 i6=i+6; if (i6>=LIMIT) i6-=LIMIT;
		uint32 i7=i+7; if (i7>=LIMIT) i7-=LIMIT; //this is the second point on line-segment
		pAuxGeom->DrawLine( g_Path[i2],RGBA8(0x00,0xff,0x00,0x00), g_Path[i7],RGBA8(0xff,0xff,0xff,0xff) );


		//before the corner
		Vec3 linedir1=(g_Path[i7]-g_Path[i5])*0.35f;
		Vec3 DispDirection1 = Vec3(linedir1.y,-linedir1.x,0); //clock-wise 90-degree rotation
		g_Path[i6]=g_Path[i6]+(DispDirection1*radiants[i7]*(fDesiredSpeed/3)); //another nice banana-formula to control the path-deviation 

		//after the corner
		Vec3 linedir0=(g_Path[i4]-g_Path[i2])*0.25f;
		Vec3 DispDirection0 = Vec3(linedir0.y,-linedir0.x,0); //clock-wise 90-degree rotation
		g_Path[i3]=g_Path[i3]+(DispDirection0*radiants[i2]*(fDesiredSpeed/3)); //another nice banana-formula to control the path-deviation 

	}


	//visualisation
	/*	for (uint32 i=0; i<LIMIT; i=i+5)
	{

	uint32 i0=i+0; if (i0>=LIMIT) i0-=LIMIT;  
	uint32 i1=i+1; if (i1>=LIMIT) i1-=LIMIT;
	uint32 i2=i+2; if (i2>=LIMIT) i2-=LIMIT;//this is the first point on line-segment
	uint32 i3=i+3; if (i3>=LIMIT) i3-=LIMIT;
	uint32 i4=i+4; if (i4>=LIMIT) i4-=LIMIT;


	aabb = AABB(Vec3(-0.01f,-0.01f, 0.0f)+g_Path[i0],Vec3(+0.01f,+0.01f,+0.3f)+g_Path[i0]);
	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);
	aabb = AABB(Vec3(-0.01f,-0.01f, 0.0f)+g_Path[i1],Vec3(+0.01f,+0.01f,+0.3f)+g_Path[i1]);
	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);

	uint32 t = uint32(fabsf(radiants[i2])*0xff);
	if (t>0xff)	t=0xff;
	uint8 c=0xff-t;
	aabb = AABB(Vec3(-0.05f,-0.05f, 0.0f)+g_Path[i2],Vec3(+0.05f,+0.05f,+0.5f)+g_Path[i2]);
	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(c,c,c,0xff), eBBD_Extremes_Color_Encoded);

	aabb = AABB(Vec3(-0.01f,-0.01f, 0.0f)+g_Path[i3],Vec3(+0.01f,+0.01f,+0.3f)+g_Path[i3]);
	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);
	aabb = AABB(Vec3(-0.01f,-0.01f, 0.0f)+g_Path[i4],Vec3(+0.01f,+0.01f,+0.3f)+g_Path[i4]);
	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0xff,0x00,0x00,0xff), eBBD_Extremes_Color_Encoded);

	}*/

	return 0;
}






//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
void CModelViewport::SetLocomotionValuesIvo(const SMotionParams& mp )
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	Vec3 absEntityPos = m_AnimatedCharacter.t;
	f32 FrameTime = GetISystem()->GetITimer()->GetFrameTime();
	f32 TimeScale = GetISystem()->GetITimer()->GetTimeScale();

	f32 FramesPerSecond = TimeScale/m_AverageFrameTime;

	static f32 LastTurnRadianSec4 = 0;
	static f32 LastTurnRadianSec3 = 0;
	static f32 LastTurnRadianSec2 = 0;
	static f32 LastTurnRadianSec1 = 0;
	static f32 LastTurnRadianSec0 = 0;

	LastTurnRadianSec4	=	LastTurnRadianSec3;
	LastTurnRadianSec3	=	LastTurnRadianSec2;
	LastTurnRadianSec2	=	LastTurnRadianSec1;
	LastTurnRadianSec1	=	LastTurnRadianSec0;

	Vec2 vCurrentBodyDir = Vec2(m_AnimatedCharacter.GetColumn1());
	LastTurnRadianSec0 = Ang3::CreateRadZ(vCurrentBodyDir,m_vWorldDesiredBodyDirectionSmooth) * FramesPerSecond;
	//to avoid wild&crazy direction changes, we scale-down the body-turn based on the framerate
	LastTurnRadianSec0 /= (FramesPerSecond/12.0f);
	//a very cheesy way to do the smoothing because of all the stalls we have
	f32 TurnRadianSecAvrg = (LastTurnRadianSec0+LastTurnRadianSec1+LastTurnRadianSec2+LastTurnRadianSec3+LastTurnRadianSec4)*0.20f;

	TurnRadianSecAvrg = LastTurnRadianSec0;


	ISkeletonAnim* pISkeletonAnim = m_pCharacterBase->GetISkeletonAnim();

	//disable the overrides
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelSpeed, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TurnSpeed, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelSlope, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelAngle, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelDist, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelDistScale, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TurnAngle, 0, 0) ;

	f32 rad = -atan2f(m_vLocalDesiredMoveDirectionSmooth.x,m_vLocalDesiredMoveDirectionSmooth.y);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelAngle, rad, FrameTime) ;
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelDistScale, 1, FrameTime) ;
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnSpeed, TurnRadianSecAvrg, FrameTime) ;
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelSpeed, mp.m_fCatchUpSpeed, FrameTime) ;

	float color1[4] = {1,1,1,1};
	m_renderer->Draw2dLabel(12,g_ypos,1.8f,color1,false,"TurnRadianSecAvrg: %f",TurnRadianSecAvrg);
	g_ypos+=20;
	//m_renderer->Draw2dLabel(12,g_ypos,1.8f,color1,false,"localtraveldir: %f %f",localtraveldir.x,localtraveldir.y);
	//g_ypos+=20;

}


//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
void CModelViewport::PathFollowing(const SMotionParams& mp, f32 fDesiredSpeed  )
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	Vec3 absEntityPos = m_AnimatedCharacter.t;

	f32 FrameTime = GetISystem()->GetITimer()->GetFrameTime();
	f32 TimeScale = GetISystem()->GetITimer()->GetTimeScale();

	//-------------------------------------------------------------------
	//-------                 key-board             ---------------------
	//-------------------------------------------------------------------
	m_key_SPACE=m_key_W=m_key_A=m_key_D=m_key_S=0;
	m_key_W=1; 
	m_lkey_W=1;


	int32 NewStance=-1;

	if (m_WeaponMode==0)
	{
		g_ShiftTarget=2;
		g_ShiftSource=6;
		g_NewName="nw    ";
	}
	if (m_WeaponMode==2)
	{
		g_ShiftTarget=5;
		g_ShiftSource=6;
		g_NewName="rifle ";
	}

	if (CheckVirtualKey('5'))
		NewStance=COMBAT;
	if (CheckVirtualKey('6'))
		NewStance=STEALTH;
	if (CheckVirtualKey('7'))
		NewStance=CROUCH;
	if (CheckVirtualKey('8'))
		NewStance=PRONE;
	if (CheckVirtualKey('9'))
		NewStance=RELAXED;

	if (CheckVirtualKey('V'))
		NewStance=PRONE;


	if (NewStance<0)
		StateMachine(m_Stance,0);
	else
		StateMachine(NewStance,0);







	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------
	//-------------------------------------------------------------------------

	uint32 ypos = 512;
	float color1[4] = {1,1,1,1};

	ISkeletonAnim* pISkeletonAnim = m_pCharacterBase->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = m_pCharacterBase->GetISkeletonPose();

	f32 FramesPerSecond = TimeScale/m_AverageFrameTime;
	SetLocomotionValuesIvo(mp);


	/*
	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"TurnRadianSec: %f",TurnRadianSec);
	ypos+=10;
	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"m_FrameTime: %f   FramesPerSecond: %f",FrameTime,FramesPerSecond );
	ypos+=10;
	*/




	uint32 numAnimsLayer0 = pISkeletonAnim->GetNumAnimsInFIFO(0);
	if (numAnimsLayer0)
	{
		//pISkeleton->SetBlendSpaceControl0(0, AnimParams(0,0,0,0,0) );
		pISkeletonAnim->SetIWeight(0, 0 );
		//pISkeleton->SetBlendSpaceControl1(0, AnimParams(0,0,0,0,0) );
	}



	//------------------------------------------------------------------------
	//---  camera control
	//------------------------------------------------------------------------
	uint32 AttachedCamera = m_pCharPanel_Animation->GetAttachedCamera();
	if (AttachedCamera)
	{

		//PITCH - movement
		f32 pitch_difference = fabsf(m_arrCamRadPitch[0])*4;
		f32 pitch=1.0f;
		if (pitch_difference>1.0f)
			pitch=1.0f/pitch_difference;
		f32 pitch_speed=0.002f*pitch;

		m_absCameraHigh -= m_relCameraRotX*pitch_speed;
		if (m_absCameraHigh>3)
			m_absCameraHigh=3;
		if (m_absCameraHigh<0)
			m_absCameraHigh=0;


		SmoothCD(m_LookAt, m_LookAtRate, FrameTime, Vec3(absEntityPos.x,absEntityPos.y,0.7f), 0.05f);

		//	Vec2 vWorldCurrentBodyDirection = Vec2(m_AnimatedCharacter.GetColumn1());
		Vec2 vWorldCurrentBodyDirection = Vec2(0,1);

		Vec3 absCameraPos		=	vWorldCurrentBodyDirection*6 + m_LookAt+Vec3(0,0,m_absCameraHigh);
		//absCameraPos.x=absEntityPos.x;

		SmoothCD(m_CamPos, m_CamPosRate, FrameTime, absCameraPos, 0.25f);
		m_absCameraPos=m_CamPos;

		Matrix33 orientation = Matrix33::CreateRotationVDir( (m_LookAt-m_absCameraPos).GetNormalized(), 0 );
		if (m_pCharPanel_Animation->GetFixedCamera())
		{
			m_absCameraPos	=	Vec3(0,3,1);
			orientation			= Matrix33::CreateRotationVDir( (m_LookAt-m_absCameraPos).GetNormalized() );
		}
		m_absCameraPos=m_CamPos;
		SetViewTM( Matrix34(orientation,m_CamPos) );

		const char* RootName = pISkeletonPose->GetJointNameByID(0);
		if (RootName[0]=='B' && RootName[1]=='i' && RootName[2]=='p')
		{
			uint32 AimIK	= m_pCharPanel_Animation->GetAimIK();
			pISkeletonPose->SetAimIK(AimIK,m_absCameraPos);

			uint32 status = pISkeletonPose->GetAimIKStatus();
			//	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"AimStatus: %d %d",AimIK, status );	ypos+=10;
			//	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"m_absCameraPos: %f %f %f",m_absCameraPos.x,m_absCameraPos.y,m_absCameraPos.z );	ypos+=10;
		}

	}


}


