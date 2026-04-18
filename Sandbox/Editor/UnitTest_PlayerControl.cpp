//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File: PathFollowing.cpp
//  Implementation of the unit test to preview player-control
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

#define NUM_FUTURE_POINTS (40)
extern uint32 g_ypos;


//------------------------------------------------------------------------------
//---              simple player-control test-application                    ---
//------------------------------------------------------------------------------
void CModelViewport::PlayerControl_UnitTest( ICharacterInstance* pInstance,IAnimationSet* pIAnimationSet, SRendParams rp )
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
	f32 FrameTime = GetIEditor()->GetSystem()->GetITimer()->GetFrameTime();

	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();
	GetISystem()->GetIAnimationSystem()->SetScalingLimits( Vec2(1.0f, 1.0f) );

	AABB aabb;Vec3 wpos;
	wpos	=	Vec3(2,2,0);
	aabb = AABB(Vec3(-0.2f,-0.2f,-0.00f)+wpos,Vec3(+0.2f,+0.2f,+2.0f)+wpos);
	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0x00,0xff,0x1f,0xff), eBBD_Extremes_Color_Encoded);
	wpos	=	Vec3(4,2,0);
	aabb = AABB(Vec3(-0.2f,-0.2f,-0.00f)+wpos,Vec3(+0.2f,+0.2f,+2.0f)+wpos);
	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0x00,0x0f,0x4f,0xff), eBBD_Extremes_Color_Encoded);
	wpos	=	Vec3(5.5f,2,0);
	aabb = AABB(Vec3(-0.2f,-0.2f,-0.00f)+wpos,Vec3(+0.2f,+0.2f,+2.0f)+wpos);
	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0x00,0x0f,0x7f,0xff), eBBD_Extremes_Color_Encoded);

	wpos	=	Vec3(0, 5,0);
	aabb = AABB(Vec3(-0.2f,-0.2f,-0.00f)+wpos,Vec3(+0.2f,+0.2f,+2.0f)+wpos);
	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0x7f,0x7f,0x7f,0xff), eBBD_Extremes_Color_Encoded);
	wpos	=	Vec3(0, 8,0);
	aabb = AABB(Vec3(-0.2f,-0.2f,-0.00f)+wpos,Vec3(+0.2f,+0.2f,+2.0f)+wpos);
	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0x7f,0x7f,0x7f,0xff), eBBD_Extremes_Color_Encoded);
	wpos	=	Vec3(0,11,0);
	aabb = AABB(Vec3(-0.2f,-0.2f,-0.00f)+wpos,Vec3(+0.2f,+0.2f,+2.0f)+wpos);
	pAuxGeom->DrawAABB(aabb,Matrix34(IDENTITY),1,RGBA8(0x7f,0x7f,0x7f,0xff), eBBD_Extremes_Color_Encoded);


	m_absCurrentSpeed			=	pISkeletonAnim->GetCurrentVelocity().GetLength();
	m_absCurrentSlope			=	pISkeletonAnim->GetCurrentSlope();


	uint32 ypos = 2;
	float color1[4] = {1,1,1,1};

	f32	yaw_radiant=PlayerControlHuman();

	//if (m_State!=0)
	{
		//procedural direction control
		//	f32 ProcBlend=1; //pISkeleton->GetProcBlend();
		//	Quat m33 = Quat::CreateRotationZ(yaw_radiant*m_AverageFrameTime*1*ProcBlend);
		//	m_AnimatedCharacter=m_AnimatedCharacter*m33;

		//if (m_DebugOutputInViewPort)
		//{
		//	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"Procedural Turn Blend: %f",ProcBlend);
		//	ypos+=10;
		//}
	}

	m_relCameraRotZ=0;
	m_relCameraRotX=0;

	if (m_pCharPanel_Animation)
	{
		GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_JustRootUpdate")->Set( mv_showRootUpdate );
	}

	//update animation
	pISkeletonAnim->SetFuturePathAnalyser(1);

	int PreProcessCallback(ICharacterInstance* pInstance,void* pPlayer);
	pISkeletonAnim->SetPreProcessCallback(PreProcessCallback,this);

	int PostProcessCallback_PlayControl(ICharacterInstance* pInstance,void* pPlayer);
	pISkeletonPose->SetPostProcessCallback0(PostProcessCallback_PlayControl,this);

	pISkeletonAnim->SetEventCallback(0,this);

	//		QuatTS Offset =  m_PhysEntityLocation.GetInverted()*m_AnimatedCharacter;
//	pInstance->SetWorldLocation(m_AnimatedCharacter);
	pInstance->SkeletonPreProcess( QuatT(m_AnimatedCharacter), m_AnimatedCharacter, GetCamera(),0 );
//	pInstance->SetWorldLocation(m_AnimatedCharacter);
	pInstance->SkeletonPostProcess( QuatT(m_AnimatedCharacter), m_AnimatedCharacter, 0, (m_AnimatedCharacter.t-m_absCameraPos).GetLength(), 0 );
	//	m_AnimatedCharacter.t += pISkeleton->GetRelFootSlide();

	//use updated animations to render character
	m_AnimatedCharacterMat = Matrix34(m_AnimatedCharacter);
	rp.pMatrix    =  &m_AnimatedCharacterMat;
	rp.pPrevMatrix   = &m_AnimatedCharacterMat;
	rp.fDistance	= (m_AnimatedCharacter.t-m_absCameraPos).GetLength();
	pInstance->Render( rp, QuatTS(IDENTITY) );

	Vec3 absAxisX		=	m_AnimatedCharacter.GetColumn0();
	Vec3 absAxisY		=	m_AnimatedCharacter.GetColumn1();
	Vec3 absAxisZ		=	m_AnimatedCharacter.GetColumn2();
	Vec3 absRootPos	=	m_AnimatedCharacter.t+pISkeletonPose->GetAbsJointByID(0).t;
	pAuxGeom->DrawLine( absRootPos,RGBA8(0xff,0x00,0x00,0x00), absRootPos+m_AnimatedCharacter.GetColumn0()*2.0f,RGBA8(0x00,0x00,0x00,0x00) );
	pAuxGeom->DrawLine( absRootPos,RGBA8(0x00,0xff,0x00,0x00), absRootPos+m_AnimatedCharacter.GetColumn1()*2.0f,RGBA8(0x00,0x00,0x00,0x00) );
	pAuxGeom->DrawLine( absRootPos,RGBA8(0x00,0x00,0xff,0x00), absRootPos+m_AnimatedCharacter.GetColumn2()*2.0f,RGBA8(0x00,0x00,0x00,0x00) );


	//---------------------------------------
	//---   draw the path of the past
	//---------------------------------------
	uint32 numEntries=m_arrAnimatedCharacterPath.size();
	for (int32 i=(numEntries-2); i>-1; i--)
		m_arrAnimatedCharacterPath[i+1] = m_arrAnimatedCharacterPath[i];
	m_arrAnimatedCharacterPath[0] = m_AnimatedCharacter.t; 
	for (uint32 i=0; i<numEntries; i++)
	{
		aabb.min=Vec3(-0.01f,-0.01f,-0.01f)+m_arrAnimatedCharacterPath[i];
		aabb.max=Vec3(+0.01f,+0.01f,+0.01f)+m_arrAnimatedCharacterPath[i];
		pAuxGeom->DrawAABB(aabb,1, RGBA8(0x00,0x00,0xff,0x00),eBBD_Extremes_Color_Encoded );
	}

	//---------------------------------------
	//---   draw future-path
	//---------------------------------------
	if (1)
	{
		SAnimRoot arrRelFuturePath[NUM_FUTURE_POINTS];
		pISkeletonAnim->GetFuturePath(arrRelFuturePath); 
		Quat qabsolute(IDENTITY);
		Vec3 vabsolute(ZERO);
		for (uint32 i=0; i<NUM_FUTURE_POINTS; i++)
		{
			vabsolute += qabsolute*arrRelFuturePath[i].m_TransRot.t;
			qabsolute *= arrRelFuturePath[i].m_TransRot.q;
			Vec3 pos=m_AnimatedCharacter*vabsolute;
			aabb.min=Vec3(-0.01f,-0.01f,-0.01f)+pos;
			aabb.max=Vec3(+0.01f,+0.01f,+0.01f)+pos;
			pAuxGeom->DrawAABB(aabb,1, RGBA8(0xff,0x00,0x00,0x00),eBBD_Extremes_Color_Encoded );
		}
	}

	if (mv_showGrid)
		DrawHeightField();

	GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_DrawSkeleton")->Set( mv_showSkeleton );
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



const char* strAnimName=0;	
uint32 g_EnableStrafing=1;




int32 g_ShiftTarget		= 5;
int32 g_ShiftSource		= 6;
char* g_NewName			=	"rifle ";
char name0[0x100];

const char* PatchName0( const char* pNameSrc )
{
	const char* pNewSrc=0;
	if (pNameSrc)
	{
		for (uint32 i=0; i<0x100; i++)
		{
			name0[i]=pNameSrc[i];
			if (name0[i]==0) break;
		}

		uint32 pos=0;
		uint8 andval=0xff;
		for (uint32 i=0; i<0xf0; i++)
		{
			uint32 c0,c1,c2,c3,c4,c5;

			c0=c1=c2=c3=c4=c5=0;
			if ( name0[i+0] == 'p' )	c0++;
			if ( name0[i+1] == 'i' )	c1++;
			if ( name0[i+2] == 's' )	c2++;
			if ( name0[i+3] == 't' )	c3++;
			if ( name0[i+4] == 'o' )	c4++;
			if ( name0[i+5] == 'l' )	c5++;
			uint32 total0 = c0+c1+c2+c3+c4+c5;

			c0=c1=c2=c3=c4=c5=0;
			if ( name0[i+0] == 'P' )	c0++;
			if ( name0[i+1] == 'I' )	c1++;
			if ( name0[i+2] == 'S' )	c2++;
			if ( name0[i+3] == 'T' )	c3++;
			if ( name0[i+4] == 'O' )	c4++;
			if ( name0[i+5] == 'L' )	c5++;
			uint32 total1 = c0+c1+c2+c3+c4+c5;
			if (total1==6)
				andval=0x20^0xff;

			if (total0==6 || total1==6)
			{
				pos=i;
				break;
			}

		}
		assert(pos);
		name0[pos+0]=g_NewName[0]&andval;	//p
		name0[pos+1]=g_NewName[1]&andval;	//i
		name0[pos+2]=g_NewName[2]&andval;	//s
		name0[pos+3]=g_NewName[3]&andval;	//t
		name0[pos+4]=g_NewName[4]&andval;	//o
		name0[pos+5]=g_NewName[5]&andval;	//l
		for (uint32 i=pos; i<0x0f0; i++)
			name0[i+g_ShiftTarget]=name0[i+g_ShiftSource];

		pNewSrc=&name0[0];
	}

	return pNewSrc;
}


//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

f32 CModelViewport::PlayerControlHuman( )
{
	uint32 ypos = 432;
	float color1[4] = {1,1,1,1};

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	Vec3 absPlayerPos = m_AnimatedCharacter.t;
	f32 FrameTime = GetISystem()->GetITimer()->GetFrameTime();

	ISkeletonAnim* pISkeletonAnim = m_pCharacterBase->GetISkeletonAnim();

	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelSpeed, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TurnSpeed, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelAngle, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelDistScale, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelDist, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelSlope, 0, 0) ;
	pISkeletonAnim->SetLayerUpdateMultiplier(0,1); //no scaling


	//-------------------------------------------------------------------
	//-------                 key-board             ---------------------
	//-------------------------------------------------------------------
	m_key_SPACE=m_key_W=m_key_A=m_key_D=m_key_S=0;
	if (CheckVirtualKey(VK_UP)    || CheckVirtualKey('W'))
	{ m_key_W=1; m_lkey_W=1; }
	if (CheckVirtualKey(VK_DOWN)  || CheckVirtualKey('S'))
	{ m_key_S=1; m_lkey_S=1; }
	if (CheckVirtualKey(VK_LEFT)  || CheckVirtualKey('A'))
	{	m_key_A=1; m_lkey_A=1; }
	if (CheckVirtualKey(VK_RIGHT) || CheckVirtualKey('D'))
	{	m_key_D=1; m_lkey_D=1; }

	if (CheckVirtualKey(VK_RETURN)) { m_key_SPACE=1; }
	m_key_SPACERCR <<= 1; 
	m_key_SPACERCR  |= m_key_SPACE; 

	if (m_key_W+m_key_A+m_key_D+m_key_S)
	{
		m_lkey_W=m_key_W;
		m_lkey_S=m_key_S;
		m_lkey_A=m_key_A;
		m_lkey_D=m_key_D;
	}

	if (m_key_W) m_key_S=0; //forward has priority
	if (m_key_A) m_key_D=0; //left has priority
	if (m_lkey_A) m_lkey_D=0; //left has priority
	if (m_lkey_W) m_lkey_S=0; //forward has priority


	//----------------------------------------------------------------------------------------------
	//---- specify move- and view-direction for the player (shows the direction in world-space)  ---
	//----------------------------------------------------------------------------------------------
	//update and clamp rotation-speed
	f32 YawRad=m_relCameraRotZ*0.001f; //yaw-speed
//	if (YawRad>+0.2f)	YawRad=+0.2f;
//	if (YawRad<-0.2f)	YawRad=-0.2f;
	m_absLookDirectionXY		=	Matrix33::CreateRotationZ(YawRad) * m_absLookDirectionXY;
	m_absLookDirectionXY.z	=	0;
	m_absLookDirectionXY.Normalize();

	m_vWorldDesiredBodyDirection	=	Vec2(m_absLookDirectionXY);


	m_vLocalDesiredMoveDirection = Vec2(ZERO);
	if (m_lkey_W)
		m_vLocalDesiredMoveDirection += Vec2(0,1);
	if (m_lkey_S)
		m_vLocalDesiredMoveDirection += Vec2(0,-1);

	if (m_lkey_A)
		m_vLocalDesiredMoveDirection += Vec2(-1,0);
	if (m_lkey_D)
		m_vLocalDesiredMoveDirection += Vec2(1,0);

	f32 length=m_vLocalDesiredMoveDirection.GetLength();
	if (length>1.0f)
		m_vLocalDesiredMoveDirection.Normalize();

	SmoothCD(m_vWorldDesiredBodyDirectionSmooth, m_vWorldDesiredBodyDirectionSmoothRate, m_AverageFrameTime, m_vWorldDesiredBodyDirection, 0.25f);
	SmoothCD(m_vLocalDesiredMoveDirectionSmooth, m_vLocalDesiredMoveDirectionSmoothRate, m_AverageFrameTime, m_vLocalDesiredMoveDirection, 0.25f);

	f32 fRadBody = -atan2f(m_vWorldDesiredBodyDirection.x,m_vWorldDesiredBodyDirection.y);
	Matrix33 MatBody33=Matrix33::CreateRotationZ(fRadBody); 
	m_vWorldDesiredMoveDirection=MatBody33*m_vLocalDesiredMoveDirection;

	f32 fRadBodyDesired = -atan2f(m_vWorldDesiredBodyDirectionSmooth.x,m_vWorldDesiredBodyDirectionSmooth.y);
	Matrix33 MatBodyDesired33=Matrix33::CreateRotationZ(fRadBodyDesired); 
	m_vWorldDesiredMoveDirectionSmooth=MatBodyDesired33*m_vLocalDesiredMoveDirectionSmooth;

	Vec2 vWorldCurrentBodyDirection = Vec2(m_AnimatedCharacter.GetColumn1());


	m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"m_vLocalDesiredMoveDirection: %f %f",m_vLocalDesiredMoveDirection.x,m_vLocalDesiredMoveDirection.y);
	g_ypos+=15;

	m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"m_vWorldDesiredBodyDirection: %f %f",m_vWorldDesiredBodyDirection.x,m_vWorldDesiredBodyDirection.y);
	g_ypos+=10;
	m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"m_vWorldDesiredMoveDirection: %f %f",m_vWorldDesiredMoveDirection.x,m_vWorldDesiredMoveDirection.y);
	g_ypos+=15;

	m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"m_vWorldDesiredBodyDirectionSmooth: %f %f",m_vWorldDesiredBodyDirectionSmooth.x,m_vWorldDesiredBodyDirectionSmooth.y);
	g_ypos+=10;
	m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"m_vWorldDesiredMoveDirectionSmooth: %f %f",m_vWorldDesiredMoveDirectionSmooth.x,m_vWorldDesiredMoveDirectionSmooth.y);
	g_ypos+=10;


	//------------------------------------------------------------

	uint32 LookIK	= m_pCharPanel_Animation->GetLookIK();
	m_pCharacterBase->GetISkeletonPose()->SetLookIK(LookIK,DEG2RAD(120),absPlayerPos+m_vWorldDesiredBodyDirection*20 );
	Vec3 AimPos=Vec3(0.0f,-10.0f,1.0f);
	uint32 AimIK	= m_pCharPanel_Animation->GetAimIK();
	m_pCharacterBase->GetISkeletonPose()->SetAimIK(AimIK,AimPos);

	pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
	static Ang3 angle=Ang3(ZERO);
	angle.x += 0.1f;
	angle.y += 0.01f;
	angle.z += 0.001f;
	AABB sAABB = AABB(Vec3(-0.1f,-0.1f,-0.1f),Vec3(+0.1f,+0.1f,+0.1f));
	OBB obb =	OBB::CreateOBBfromAABB( Matrix33::CreateRotationXYZ(angle), sAABB );
	pAuxGeom->DrawOBB(obb,AimPos,1,RGBA8(0xff,0xff,0xff,0xff),eBBD_Extremes_Color_Encoded);

	//------------------------------------------------------------------------

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


	f32 yaw_radiant = Ang3::CreateRadZ(vWorldCurrentBodyDirection,m_vWorldDesiredBodyDirectionSmooth);
	if (NewStance<0)
		StateMachine(m_Stance,yaw_radiant);
	else
		StateMachine(NewStance,yaw_radiant);




	//---------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------


	uint32 numAnimsLayer0 = pISkeletonAnim->GetNumAnimsInFIFO(0);
	if (numAnimsLayer0)
	{

		if (m_State==STATE_STAND)
		{
			m_MoveSpeedMSec=0.0;
		}


		//------------------------------------------------

		for ( f32 t=0.0f; t<1.0f; t+=(1.0f/100.0f) )
		{
			Vec3 mnew = Vec3::CreateSlerp(vWorldCurrentBodyDirection,m_vWorldDesiredBodyDirection,t);
			pAuxGeom->DrawLine(m_AnimatedCharacter.t,RGBA8(0x00,0x00,0x00,0x00), m_AnimatedCharacter.t+mnew,RGBA8(0x00,0xff,0x00,0x00) );
		}


		f32 fDesiredSpeed=5.0f;
		if (m_State==STATE_WALK)
		{
			if (m_Stance==COMBAT)	fDesiredSpeed=1.3f;
			if (m_Stance==STEALTH) fDesiredSpeed=0.8f;
		}

		if (m_State==STATE_RUN)
		{
			if (m_Stance==COMBAT) fDesiredSpeed=5.0f;
			if (m_Stance==STEALTH) fDesiredSpeed=3.5f;
		}



		//when a locomotion is slow (0.5-2.0f), then we can do this motion in all direction more or less at the same speed 
		f32 fScaleLimit=1.0f;
		if (fDesiredSpeed)
			fScaleLimit = 0.8f/fDesiredSpeed;
		fScaleLimit=clamp(fScaleLimit,0.4f,1.0f); //never scale more then 0.4 down 

		m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"fScaleLimit: %f",fScaleLimit);
		g_ypos+=10;


		//adjust desired speed for turns
		f32 fDesiredTurnAngle	= Ang3::CreateRadZ(vWorldCurrentBodyDirection,m_vWorldDesiredBodyDirection)*2.0f;
		f32 fSpeedScale=1.0f-fabsf(fDesiredTurnAngle*0.20f)/gf_PI;
		fSpeedScale=clamp(fSpeedScale, fScaleLimit,1.0f);	

		//adjust desired speed when strafing and running backward
		f32 fBodyMoveRadian = Ang3::CreateRadZ( m_vWorldDesiredBodyDirectionSmooth, m_vWorldDesiredMoveDirectionSmooth );
		f32 fStrafeSlowDown = (gf_PI-fabsf(fBodyMoveRadian*0.60f))/gf_PI;
		fStrafeSlowDown=clamp(fStrafeSlowDown, fScaleLimit,1.0f);	
	
		//BINGO! thats it
		f32 fRealMovementSpeed = fDesiredSpeed*min(fSpeedScale,fStrafeSlowDown);





		m_renderer->Draw2dLabel(12,g_ypos,1.4f,color1,false,"fDesiredSpeed: %f   fRealMovementSpeed: %f", fDesiredSpeed,fRealMovementSpeed);
		g_ypos+=10;
		m_renderer->Draw2dLabel(12,g_ypos,1.4f,color1,false,"fBodyMoveRadian: %f %f",fBodyMoveRadian,fSpeedScale);
		g_ypos+=10;

		//pISkeleton->SetDesiredMotionParam(eMotionParamID_TravelAngle,Vec2(fTravelAngle,0), 	m_AverageFrameTime);
		//Vec3 vLocalMoveDirection=Matrix33::CreateRotationZ(fTravelAngle)*Vec3(0,1,0);
    //pISkeleton->SetDesiredMotionParam(eMotionParamID_TravelAngle,Vec2(m_vAbsDesiredMoveDirectionSmooth)*m33, 	m_AverageFrameTime);

	//	pISkeleton->SetDesiredMotionParam(eMotionParamID_TravelAngle, m_vLocalDesiredMoveDirectionSmooth, 	m_AverageFrameTime);

		f32 rad=-atan2f(m_vLocalDesiredMoveDirectionSmooth.x,m_vLocalDesiredMoveDirectionSmooth.y);
		f32 scale=m_vLocalDesiredMoveDirectionSmooth.GetLength();
		pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelAngle, rad, 	m_AverageFrameTime);
		pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelDistScale,  scale,	m_AverageFrameTime) ;
		pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelSpeed,	fRealMovementSpeed,	m_AverageFrameTime);

		pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelDist,      -1,						m_AverageFrameTime) ;
		pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnSpeed,			  fDesiredTurnAngle,				m_AverageFrameTime);
	}



	//------------------------------------------------------------------------
	//---  camera control
	//------------------------------------------------------------------------
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

	SmoothCD(m_LookAt, m_LookAtRate, m_AverageFrameTime, Vec3(absPlayerPos.x,absPlayerPos.y,0.7f), 0.15f);

	Vec3 dir = (m_absLookDirectionXY+vWorldCurrentBodyDirection).GetNormalizedSafe(Vec3(0,1,0));
	//	m_absCameraPos		=	-m_absLookDirectionXY*2 + m_LookAt+Vec3(0,0,m_absCameraHigh);
	m_absCameraPos		=	-dir*2 + m_LookAt+Vec3(0,0,m_absCameraHigh);



	Matrix33 orientation = Matrix33::CreateRotationVDir( (m_LookAt-m_absCameraPos).GetNormalized(), 0 );
	if (m_pCharPanel_Animation->GetFixedCamera())
	{
		m_absCameraPos	=	Vec3(0,3,1);
		orientation			= Matrix33::CreateRotationVDir( (m_LookAt-m_absCameraPos).GetNormalized() );
	}
	SetViewTM( Matrix34(orientation,m_absCameraPos) );

	return yaw_radiant;
}






//----------------------------------------------------------------------------------------------------
//--- calculate the parameters for the motion to make the character move in the desired direction  ---
//----------------------------------------------------------------------------------------------------
void CModelViewport::StateMachine(uint32 NewStance, f32 yaw_radiant )
{
	Vec3 m_absPlayerPos = m_AnimatedCharacter.t;

	CryCharAnimationParams Params (0);
	bool bTransRot			= m_pCharPanel_Animation->GetAnimationDrivenMotion();
	bool bVTimeWarping	= m_pCharPanel_Animation->GetVTimeWarping();
	bool bFVE						= m_pCharPanel_Animation->GetFootAnchoring();

	ISkeletonAnim* pISkeletonAnim = m_pCharacterBase->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = m_pCharacterBase->GetISkeletonPose();
	pISkeletonAnim->SetAnimationDrivenMotion( 1 );
	pISkeletonPose->SetFootAnchoring( bFVE );

	Params.m_nFlags |= CA_LOOP_ANIMATION;
	if (bVTimeWarping)
		Params.m_nFlags |= CA_TRANSITION_TIMEWARPING;

	Params.m_fInterpolation = 0;

	/*	if (CheckVirtualKey('1'))
	{
	m_WeaponMode=0;
	g_ShiftTarget=2;
	g_ShiftSource=6;
	g_NewName="nw    ";
	strAnimName=("combat_idle_pistol_02");
	Params.m_nFlags = CA_LOOP_ANIMATION;
	Params.m_fTransTime=0.10f;
	StartAnimation(Params,0);
	m_State=STATE_STAND;
	m_Stance=COMBAT;
	}*/

	if (CheckVirtualKey('3'))
	{
		m_WeaponMode=2;
		g_ShiftTarget=5;
		g_ShiftSource=6;
		g_NewName="rifle ";
		strAnimName=("combat_idle_pistol_02");	
		Params.m_nFlags = CA_LOOP_ANIMATION;
		Params.m_fTransTime=0.10f;
		StartAnimation(Params,0);
		m_State=STATE_STAND;
		m_Stance=COMBAT;
	}

	//---------------------------------------------------------------------------------------
	//------                           interactive control                            -------
	//---------------------------------------------------------------------------------------
	bool bCtrl = CheckVirtualKey(VK_SHIFT);// || CheckVirtualKey(VK_RCONTROL);

	//------------------------------------------------------------------------
	//----                  move forward                            ----------
	//------------------------------------------------------------------------
	if (m_key_W || m_key_A || m_key_D)
	{
		m_PassedTime=0;
		if (!bCtrl)
			RunningForward(NewStance,Params,pISkeletonAnim);
		else
			WalkingForward(NewStance,Params,pISkeletonAnim);
	}

	//------------------------------------------------------------------------
	//----                  move backwards                          ----------
	//------------------------------------------------------------------------
	if (m_key_S)
		m_PassedTime=0;

	//------------------------------------------------------------------------
	//----                     stand idle                           ----------
	//------------------------------------------------------------------------
	if (m_key_W==0 && m_key_D==0 && m_key_A==0)
	{
		m_PassedTime += m_AverageFrameTime; 
		if (m_PassedTime>0.25f)
			StandingAnims(NewStance,yaw_radiant, Params,pISkeletonAnim);	//initialize IDLE-VERB
	}


	//------------------------------------------------------------------------------------------------

	/*
	uint32 ypos = 512;
	float color1[4] = {1,1,1,1};
	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"frametime:  ft:%f   %f",m_AverageFrameTime, 0.1f/m_AverageFrameTime);
	ypos+=10;
	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"m_absPlayerPos: %f %f %f ",m_absPlayerPos.x,m_absPlayerPos.y,m_absPlayerPos.z);
	ypos+=10;
	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"m_absCameraPos: %f %f %f",m_absCameraPos.x,m_absCameraPos.y,m_absCameraPos.z);
	ypos+=10;

	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"m_relCameraRotX: %f %f",m_relCameraRotX,m_arrCamRadYaw[0]);
	ypos+=10;
	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"m_relCameraRotZ: %f",m_relCameraRotZ);
	ypos+=10;
	m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"m_Stance:%d    m_State:%d  Type:%s", m_Stance, m_State, g_NewName);
	ypos+=10;*/

}


#define TRANS_IN (0.10f)
#define TRANS_OUT (0.15f)

void CModelViewport::StandingAnims(uint32 NewStance, f32 yaw_radiant, const CryCharAnimationParams& Params, ISkeletonAnim* pISkeletonAnim)
{

	CryCharAnimationParams AParams;//=Params;
	//	AParams.m_fTransTime=0.8312f;  //transition-time in seconds

	int32 id=-7;
	int32 id0,id1,id2,id3=-8;;

	uint32 numAnimsLayer0 = pISkeletonAnim->GetNumAnimsInFIFO(0);
	//	if (numAnimsLayer0>1)
	//		return;

	IAnimationSet* pIAnimationSet = m_pCharacterBase->GetIAnimationSet();


	if (numAnimsLayer0)
	{
		CAnimation& animation=pISkeletonAnim->GetAnimFromFIFO(0,numAnimsLayer0-1);
		if (animation.m_LMG0.m_nLMGID>=0)
			id = animation.m_LMG0.m_nLMGID;		//get id of current locomotion group
		else
			id = animation.m_LMG0.m_nAnimID[0];  //get id of current animation
	}



	if (m_State==0)
	{
		//m_MoveSpeedMSec=1.0f;

		switch(m_Stance)
		{
		case COMBAT:
			id0=pIAnimationSet->GetAnimIDByName(PatchName0("combat_idle_pistol_01"));
			id1=pIAnimationSet->GetAnimIDByName(PatchName0("combat_idle_pistol_02"));
			id2=pIAnimationSet->GetAnimIDByName(PatchName0("combat_idle_pistol_03"));
			//			id3=pIAnimationSet->GetIDByName(PatchName0("_COMBAT_ROTATE_PISTOL_01"));
			if ( id==id0 ||id==id1 || id==id2 || id==id3 || numAnimsLayer0==0)
			{
				if (NewStance==COMBAT)
				{
					f32 hpi	=	1.3f;
					static f32 fTurnAngleLocked=0;
					f32 fTurnAngle	= yaw_radiant;			
	
					float color1[4] = {1,1,1,1};
					m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"IdleRotate: %f",fTurnAngle );
					g_ypos+=10;

					if (numAnimsLayer0<2 && (fTurnAngle<-hpi || fTurnAngle>+hpi))
					{
						fTurnAngleLocked=fTurnAngle*1.12f;;

						//turn right
						AParams.m_nFlags							= CA_REPEAT_LAST_KEY; //CA_LOOP_ANIMATION;
						AParams.m_fTransTime					=	0.30f;	//start immediately 
						AParams.m_fKeyTime						=	-1.0f;			//use keytime of previous motion to start this motion
						AParams.m_fPlaybackSpeed			= 1.0f;
						strAnimName=PatchName0("_COMBAT_IDLESTEPROTATE_PISTOL_01");
						pISkeletonAnim->StartAnimation( strAnimName,0, 0,0, AParams);

						AParams.m_nFlags = CA_LOOP_ANIMATION|CA_START_AFTER;
						AParams.m_fTransTime					=	0.10f;	//average time for transition 
						AParams.m_fKeyTime						=	-1.0f;			//use keytime of previous motion to start this motion
						AParams.m_fPlaybackSpeed			= 1.0f;
						strAnimName=PatchName0("combat_idle_pistol_02");
						pISkeletonAnim->StartAnimation( strAnimName,0, 0,0, AParams);

						m_State=STATE_STAND;
						m_Stance=NewStance;
					}
					pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnAngle,	fTurnAngleLocked,   m_AverageFrameTime);
					break;
				}

				if (NewStance==STEALTH)
				{
					AParams.m_nFlags = CA_REPEAT_LAST_KEY;
					AParams.m_fTransTime=(TRANS_IN);         
					//we use horizontal time-warping to create a turn animation with different angles  
					AParams.m_fPlaybackSpeed=1.0f;        
					strAnimName="combat_toStealth_pistol_01";	
					StartAnimation(AParams,0);

					//only when the previous animation is OVER, we start to play the idle-animation
					AParams.m_nFlags = CA_START_AFTER|CA_LOOP_ANIMATION; //thats the only we flag we need
					AParams.m_fTransTime=TRANS_OUT;
					AParams.m_fPlaybackSpeed=1.0f;        
					strAnimName="stealth_idle_pistol_01";	
					StartAnimation(AParams,0);
					m_State=STATE_STAND;
					m_Stance=NewStance;
					break;
				}

				if (NewStance==CROUCH)
				{
					AParams.m_nFlags = CA_REPEAT_LAST_KEY;
					AParams.m_fTransTime=(TRANS_IN);         
					AParams.m_fPlaybackSpeed=1.0f;        
					//we use horizontal time-warping to create a turn animation with different angles  
					strAnimName="combat_toCrouch_pistol_01";	
					StartAnimation(AParams,0);
					//only when the previous animation is OVER, we start to play the idle-animation
					AParams.m_nFlags = CA_START_AFTER|CA_LOOP_ANIMATION; //thats the only we flag we need
					AParams.m_fTransTime=TRANS_OUT;
					AParams.m_fPlaybackSpeed=1.0f;        
					strAnimName="crouch_idle_pistol_01";
					StartAnimation(AParams,0);
					m_State=STATE_STAND;
					m_Stance=NewStance;
					break;
				}

				if (NewStance==PRONE)
				{
					AParams.m_nFlags = CA_REPEAT_LAST_KEY;
					AParams.m_fTransTime=(TRANS_IN);         
					AParams.m_fPlaybackSpeed=1.0f;        
					//we use horizontal time-warping to create a turn animation with different angles  
					strAnimName="combat_toProne_pistol_01";		
					StartAnimation(AParams,0);
					//only when the previous animation is OVER, we start to play the idle-animation
					AParams.m_nFlags = CA_START_AFTER|CA_LOOP_ANIMATION; //thats the only we flag we need
					AParams.m_fTransTime=TRANS_OUT;
					AParams.m_fPlaybackSpeed=1.0f;        
					strAnimName="prone_idle_pistol_01";	
					StartAnimation(AParams,0);
					m_State=STATE_STAND;
					m_Stance=NewStance;
					break;
				}
				if (NewStance==RELAXED)
				{
					AParams.m_nFlags = CA_LOOP_ANIMATION; //thats the only we flag we need
					AParams.m_fTransTime=0.20f;
					strAnimName="relaxed_idle_pistol_01";	
					StartAnimation(AParams,0);
					m_State=STATE_STAND;
					m_Stance=NewStance;
					break;
				}
			}
			break;





			//-------------------------------------------------
		case CROUCH:
			if (NewStance==COMBAT)
			{
				AParams.m_nFlags = CA_REPEAT_LAST_KEY;
				AParams.m_fTransTime=(TRANS_IN);         
				AParams.m_fPlaybackSpeed=1.0f;        
				//we use horizontal time-warping to create a turn animation with different angles  
				strAnimName="crouch_toCombat_pistol_01";
				StartAnimation(AParams,0);
				//only when the previous animation is OVER, we start to play the idle-animation
				AParams.m_nFlags = CA_START_AFTER|CA_LOOP_ANIMATION; //thats the only we flag we need
				AParams.m_fTransTime=TRANS_OUT;
				AParams.m_fPlaybackSpeed=1.0f;        
				strAnimName="combat_idle_pistol_02";
				StartAnimation(AParams,0);
				m_State=STATE_STAND;
				m_Stance=NewStance;
				break;
			}
			if (NewStance==STEALTH)
			{
				AParams.m_nFlags = CA_REPEAT_LAST_KEY;
				AParams.m_fTransTime=(TRANS_IN);         
				AParams.m_fPlaybackSpeed=1.0f;        
				//we use horizontal time-warping to create a turn animation with different angles  
				strAnimName="crouch_toStealth_pistol_01";	
				StartAnimation(AParams,0);
				//only when the previous animation is OVER, we start to play the idle-animation
				AParams.m_nFlags = CA_START_AFTER|CA_LOOP_ANIMATION; //thats the only we flag we need
				AParams.m_fTransTime=TRANS_OUT;
				AParams.m_fPlaybackSpeed=1.0f;        
				strAnimName="Stealth_idle_pistol_01";	
				StartAnimation(AParams,0);
				m_State=STATE_STAND;
				m_Stance=NewStance;
				break;
			}
			if (NewStance==PRONE)
			{
				AParams.m_nFlags = CA_REPEAT_LAST_KEY;
				AParams.m_fTransTime=(TRANS_IN);         
				AParams.m_fPlaybackSpeed=1.0f;        
				//we use horizontal time-warping to create a turn animation with different angles  
				strAnimName="crouch_toProne_pistol_01";
				StartAnimation(AParams,0);
				//only when the previous animation is OVER, we start to play the idle-animation
				AParams.m_nFlags = CA_START_AFTER|CA_LOOP_ANIMATION; //thats the only we flag we need
				AParams.m_fTransTime=TRANS_OUT;
				AParams.m_fPlaybackSpeed=1.0f;        
				strAnimName="Prone_idle_pistol_01";
				StartAnimation(AParams,0);
				m_State=STATE_STAND;
				m_Stance=NewStance;
				break;
			}
			if (NewStance==RELAXED)
			{
				AParams.m_nFlags = CA_LOOP_ANIMATION; //thats the only we flag we need
				AParams.m_fTransTime=0.20f;
				strAnimName="relaxed_idle_pistol_01";
				StartAnimation(AParams,0);
				m_State=STATE_STAND;
				m_Stance=NewStance;
				break;
			}
			break;




			//-------------------------------------------------
		case STEALTH:
			id0=pIAnimationSet->GetAnimIDByName(PatchName0("stealth_idle_pistol_01"));
			id1=pIAnimationSet->GetAnimIDByName(PatchName0("stealth_idle_pistol_02"));
			id2=pIAnimationSet->GetAnimIDByName(PatchName0("stealth_idle_pistol_03"));
			if ( id==id0 ||id==id1 || id==id2 || numAnimsLayer0==0)
			{
				if (NewStance==COMBAT)
				{
					AParams.m_nFlags = CA_REPEAT_LAST_KEY;
					AParams.m_fTransTime=(TRANS_IN);         
					AParams.m_fPlaybackSpeed=1.0f;        
					//we use horizontal time-warping to create a turn animation with different angles  
					strAnimName="stealth_toCombat_pistol_01";
					StartAnimation(AParams,0);
					//only when the previous animation is OVER, we start to play the idle-animation
					AParams.m_nFlags = CA_START_AFTER|CA_LOOP_ANIMATION; //thats the only we flag we need
					AParams.m_fTransTime=TRANS_OUT;
					AParams.m_fPlaybackSpeed=1.0f;        
					strAnimName="combat_idle_pistol_02";
					StartAnimation(AParams,0);
					m_State=STATE_STAND;
					m_Stance=NewStance;
					break;
				}

				if (NewStance==CROUCH)
				{
					AParams.m_nFlags = CA_REPEAT_LAST_KEY;
					AParams.m_fTransTime=(TRANS_IN);         
					AParams.m_fPlaybackSpeed=1.0f;        
					//we use horizontal time-warping to create a turn animation with different angles  
					strAnimName="stealth_toCrouch_pistol_01";
					StartAnimation(AParams,0);
					//only when the previous animation is OVER, we start to play the idle-animation
					AParams.m_nFlags = CA_START_AFTER|CA_LOOP_ANIMATION; //thats the only we flag we need
					AParams.m_fTransTime=TRANS_OUT;
					AParams.m_fPlaybackSpeed=1.0f;        
					strAnimName="Crouch_idle_pistol_02";
					StartAnimation(AParams,0);
					m_State=STATE_STAND;
					m_Stance=NewStance;
					break;
				}

				if (NewStance==PRONE)
				{
					AParams.m_nFlags = CA_REPEAT_LAST_KEY;
					AParams.m_fTransTime=(TRANS_IN);         
					AParams.m_fPlaybackSpeed=1.0f;        
					//we use horizontal time-warping to create a turn animation with different angles  
					strAnimName="stealth_toProne_pistol_01";
					StartAnimation(AParams,0);
					//only when the previous animation is OVER, we start to play the idle-animation
					AParams.m_nFlags = CA_START_AFTER|CA_LOOP_ANIMATION; //thats the only we flag we need
					AParams.m_fTransTime=TRANS_OUT;
					AParams.m_fPlaybackSpeed=1.0f;        
					strAnimName="prone_idle_pistol_01";
					StartAnimation(AParams,0);
					m_State=STATE_STAND;
					m_Stance=NewStance;
					break;
				}
				if (NewStance==RELAXED)
				{
					AParams.m_nFlags = CA_LOOP_ANIMATION; //thats the only we flag we need
					AParams.m_fTransTime=0.20f;
					strAnimName="relaxed_idle_pistol_01";
					StartAnimation(AParams,0);
					m_State=STATE_STAND;
					m_Stance=NewStance;
					break;
				}
			}
			break;



			//-------------------------------------------------
		case PRONE:
			id0=pIAnimationSet->GetAnimIDByName(PatchName0("prone_idle_pistol_01"));
			id1=pIAnimationSet->GetAnimIDByName(PatchName0("prone_idle_pistol_01"));
			id2=pIAnimationSet->GetAnimIDByName(PatchName0("prone_idle_pistol_01"));
			if ( id==id0 ||id==id1 || id==id2 || numAnimsLayer0==0)
			{
				if (NewStance==COMBAT)
				{
					AParams.m_nFlags = CA_REPEAT_LAST_KEY;
					AParams.m_fTransTime=(TRANS_IN);        
					AParams.m_fPlaybackSpeed=1.0f;        
					//we use horizontal time-warping to create a turn animation with different angles  
					strAnimName="prone_toCombat_pistol_01";
					StartAnimation(AParams,0);
					//only when the previous animation is OVER, we start to play the idle-animation
					AParams.m_nFlags = CA_START_AFTER|CA_LOOP_ANIMATION; //thats the only we flag we need
					AParams.m_fTransTime=TRANS_OUT;
					AParams.m_fPlaybackSpeed=1.0f;        
					strAnimName="combat_idle_pistol_02";
					StartAnimation(AParams,0);
					m_State=STATE_STAND;
					m_Stance=NewStance;
					break;
				}
				if (NewStance==CROUCH)
				{
					AParams.m_nFlags = CA_REPEAT_LAST_KEY;
					AParams.m_fTransTime=(TRANS_IN);         
					AParams.m_fPlaybackSpeed=1.0f;        
					//we use horizontal time-warping to create a turn animation with different angles  
					strAnimName="prone_toCrouch_pistol_01";
					StartAnimation(AParams,0);
					//only when the previous animation is OVER, we start to play the idle-animation
					AParams.m_nFlags = CA_START_AFTER|CA_LOOP_ANIMATION; //thats the only we flag we need
					AParams.m_fTransTime=TRANS_OUT;
					AParams.m_fPlaybackSpeed=1.0f;        
					strAnimName="Crouch_idle_pistol_01";
					StartAnimation(AParams,0);
					m_State=STATE_STAND;
					m_Stance=NewStance;
					break;
				}
				if (NewStance==STEALTH)
				{
					AParams.m_nFlags = CA_REPEAT_LAST_KEY;
					AParams.m_fTransTime=(TRANS_IN);         
					AParams.m_fPlaybackSpeed=1.0f;        
					//we use horizontal time-warping to create a turn animation with different angles  
					strAnimName="prone_toStealth_pistol_01";
					StartAnimation(AParams,0);
					//only when the previous animation is OVER, we start to play the idle-animation
					AParams.m_nFlags = CA_START_AFTER|CA_LOOP_ANIMATION; //thats the only we flag we need
					AParams.m_fTransTime=TRANS_OUT;
					AParams.m_fPlaybackSpeed=1.0f;        
					strAnimName="Stealth_idle_pistol_01";
					StartAnimation(AParams,0);
					m_State=STATE_STAND;
					m_Stance=NewStance;
					break;
				}
				if (NewStance==RELAXED)
				{
					AParams.m_nFlags = CA_LOOP_ANIMATION; //thats the only we flag we need
					AParams.m_fTransTime=0.20f;
					strAnimName="relaxed_idle_pistol_01";
					StartAnimation(AParams,0);
					m_State=STATE_STAND;
					m_Stance=NewStance;
					break;
				}
			}
			break;

		} //end of switch 1







	}
	else
	{
		//moving animations are not problem
		switch(NewStance)
		{
		case COMBAT:
			id0 = pIAnimationSet->GetAnimIDByName(PatchName0("_COMBAT_WALKSTRAFE_PISTOL_01"));
			if (id==id0 && numAnimsLayer0)
			{
				//Transition-Delay: only when the walk-loop is passing a certain keyframe, we start the Move2Idle 
				AParams.m_nFlags = CA_MOVE2IDLE|CA_TRANSITION_TIMEWARPING|CA_REPEAT_LAST_KEY;
				AParams.m_fTransTime	=	0.25f;	//fast transition from MOVE to MOVE2IDLE
				AParams.m_fKeyTime		=	-1.0f;  //keytime of previous motion
				strAnimName="combat_walkStop_pistol_forward_slow_rfoot_01";
				StartAnimation(AParams,0);
			}

			id0 = pIAnimationSet->GetAnimIDByName(PatchName0("_COMBAT_RUNSTRAFE_PISTOL_01"));
			if (id==id0 && numAnimsLayer0)
			{
				//Transition-Delay: only when the run-loop is passing a certain keyframe, we start the Move2Idle 
				AParams.m_nFlags = CA_MOVE2IDLE|CA_TRANSITION_TIMEWARPING|CA_REPEAT_LAST_KEY;
				AParams.m_fTransTime	=	0.25f;	//fast transition from MOVE to MOVE2IDLE
				AParams.m_fKeyTime		=	-1.0f;  //keytime of previous motion
				strAnimName="combat_runStopShort_pistol_forward_fast_rfoot_01";
				StartAnimation(AParams,0);
			}

			//blend into idle after run-stop 
			AParams.m_nFlags = CA_START_AFTER|CA_LOOP_ANIMATION;
			AParams.m_fTransTime	=	0.15f;
			AParams.m_fKeyTime		=	-1.0f; //use keytime of previous motion to start this motion
			strAnimName="combat_idle_pistol_02";
			StartAnimation(AParams,0);
			m_State=STATE_STAND;
			m_Stance=NewStance;
			break;

		case CROUCH:
			strAnimName="crouch_idle_pistol_01";
			StartAnimation(AParams,0);
			m_State=STATE_STAND;
			m_Stance=NewStance;
			break;

		case STEALTH:
			strAnimName="stealth_idle_pistol_01";
			StartAnimation(AParams,0);
			m_State=STATE_STAND;
			m_Stance=NewStance;
			break;

		case PRONE:
			strAnimName="prone_idle_pistol_01";
			StartAnimation(AParams,0);
			m_State=STATE_STAND;
			m_Stance=NewStance;
			break;

		case RELAXED:
			strAnimName="relaxed_idle_pistol_01";
			StartAnimation(AParams,0);
			m_State=STATE_STAND;
			m_Stance=NewStance;
			break;
		}
	}
}






void CModelViewport::WalkingForward(uint32 NewStance,const CryCharAnimationParams& Params, ISkeletonAnim* pISkeletonAnim)
{
	CryCharAnimationParams AParams=Params;


	int32 id=-7;
	int32 id0,id1,id2;
	int32 id3=-3;

	uint32 numAnimsLayer0 = pISkeletonAnim->GetNumAnimsInFIFO(0);
	if (numAnimsLayer0>1)
		return;
	IAnimationSet* pIAnimationSet = m_pCharacterBase->GetIAnimationSet();
	CAnimation& animation=pISkeletonAnim->GetAnimFromFIFO(0,0);

	id = animation.m_LMG0.m_nLMGID;  //get id of current locomotion group
	if (id<-1)
		id = animation.m_LMG0.m_nAnimID[0];  //get id of current animation

	switch(NewStance)
	{
	case COMBAT:
		if (m_State==STATE_STAND)
		{
			id0 = pIAnimationSet->GetAnimIDByName(PatchName0("combat_idle_pistol_01"));
			id1 = pIAnimationSet->GetAnimIDByName(PatchName0("combat_idle_pistol_02"));
			id2 = pIAnimationSet->GetAnimIDByName(PatchName0("combat_idle_pistol_03"));
			//	id3 = pIAnimationSet->GetIDByName(PatchName0("_COMBAT_ROTATE_PISTOL_01"));
			if (id==id0 || id==id1 || id==id2 || id==id3 || numAnimsLayer0==0)
			{
				f32 hpi=gf_PI/4.0f;
				Vec2 vWorldCurrentBodyDirection=Vec2(m_AnimatedCharacter.q.GetColumn1());
				f32 yaw   = Ang3::CreateRadZ(vWorldCurrentBodyDirection,m_vWorldDesiredMoveDirection) ;//yaw_radiant; //atan2f( ysign*ycross.GetLength(), ydot );

				AParams.m_fTransTime	=	0.15f;	//fast transition from idle to walk-transitions
				AParams.m_fKeyTime		=	-1.0f;  //keytime of previous motion

				if (yaw<+hpi && yaw>-hpi)
				{
					//start Idle2Move
					AParams.m_nFlags = CA_REPEAT_LAST_KEY;
					strAnimName="_COMBAT_IDLE2WALK_PISTOL_01";
				}
				else 
				{

					uint32 ddddd=0;

				}
				/*				else
				{
				if ( yaw<-hpi && yaw>(-hpi*3) )
				{
				//start walk-right
				AParams.m_nFlags = CA_REPEAT_LAST_KEY;
				strAnimName="combat_runStart_pistol_right_01";
				} 
				else 
				{
				if ( yaw>-hpi && yaw<(hpi*3) )
				{
				//start walk-left
				AParams.m_nFlags = CA_REPEAT_LAST_KEY;
				strAnimName="combat_runStart_pistol_left_01";
				} 
				else 
				{
				//start walk-reverse
				AParams.m_fTransTime	=	0.15f;	//fast transition from idle to walk-transitions
				AParams.m_nFlags = CA_REPEAT_LAST_KEY;
				strAnimName="combat_runStart_pistol_reverse_01";
				}
				}
				}*/
				StartAnimation(AParams,0);

				//only when the turn is OVER, we start to play the idle-animation
				AParams.m_nFlags = CA_LOOP_ANIMATION|CA_IDLE2MOVE|CA_TRANSITION_TIMEWARPING;
				AParams.m_fTransTime	=	0.30f;
				AParams.m_fKeyTime		=	-1.0f; //use keytime of previous motion to start this motion
				//m_MoveSpeedMSec=1.0f;
				strAnimName="_COMBAT_WALKSTRAFE_PISTOL_01";
				StartAnimation(AParams,0);
				m_State=STATE_WALK;
				m_Stance=NewStance;
			}
		}
		else
		{

			AParams.m_nFlags = CA_TRANSITION_TIMEWARPING|CA_LOOP_ANIMATION;
			//m_MoveSpeedMSec=1.0f;
			if (g_EnableStrafing)
				strAnimName="_COMBAT_WALKSTRAFE_PISTOL_01";
			else
				strAnimName="_COMBAT_WALK_PISTOL_01";	


			StartAnimation(AParams,0);
			m_State=STATE_WALK;
			m_Stance=NewStance;
		}
		break;

		//----------------------------------------------------------------
	case CROUCH:
		if (m_State==STATE_STAND)
		{
			id0 = pIAnimationSet->GetAnimIDByName(PatchName0("crouch_idle_pistol_01"));
			id1 = pIAnimationSet->GetAnimIDByName(PatchName0("crouch_idle_pistol_02"));
			id2 = pIAnimationSet->GetAnimIDByName(PatchName0("crouch_idle_pistol_03"));
			if (id==id0 || id==id1 || id==id2 || numAnimsLayer0==0)
			{
				AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
				AParams.m_fTransTime=0.80f;
				//m_MoveSpeedMSec=1.0f;
				strAnimName="_STEALTH_WALKSTRAFE_pistol_01";
				StartAnimation(AParams,0);
				m_State=STATE_WALK;
				m_Stance=NewStance;
			}
		}
		else
		{
			AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
			//m_MoveSpeedMSec=1.0f;
			strAnimName="_STEALTH_WALKSTRAFE_pistol_01";
			StartAnimation(AParams,0);
			m_State=STATE_WALK;
			m_Stance=NewStance;
		}
		break;


		//----------------------------------------------------------------------------
	case STEALTH:
		if (m_State==STATE_STAND)
		{
			id0 = pIAnimationSet->GetAnimIDByName(PatchName0("stealth_idle_pistol_01"));
			id1 = pIAnimationSet->GetAnimIDByName(PatchName0("stealth_idle_pistol_02"));
			id2 = pIAnimationSet->GetAnimIDByName(PatchName0("stealth_idle_pistol_03"));
			if (id==id0 || id==id1 || id==id2 || numAnimsLayer0==0)
			{
				AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
				AParams.m_fTransTime=0.80f;
				//m_MoveSpeedMSec=1.0f;
				strAnimName="_STEALTH_WALKSTRAFE_pistol_01";
				StartAnimation(AParams,0);
				m_State=STATE_WALK;
				m_Stance=NewStance;
			}
		}
		else
		{
			AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
			//m_MoveSpeedMSec=1.0f;
			strAnimName="_STEALTH_WALKSTRAFE_pistol_01";
			StartAnimation(AParams,0);
			m_State=STATE_WALK;
			m_Stance=NewStance;
		}
		break;


		//--------------------------------------------------------------
	case PRONE:
		if (m_State==STATE_STAND)
		{
			id0 = pIAnimationSet->GetAnimIDByName(PatchName0("prone_idle_pistol_01"));
			id1 = pIAnimationSet->GetAnimIDByName(PatchName0("prone_idle_pistol_02"));
			id2 = pIAnimationSet->GetAnimIDByName(PatchName0("prone_idle_pistol_03"));
			if (id==id0 || id==id1 || id==id2 || numAnimsLayer0==0)
			{
				AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
				strAnimName="prone_walk_pistol_forward_01";
				StartAnimation(AParams,0);
				m_State=STATE_WALK;
				m_Stance=NewStance;
			}
		}
		else
		{
			AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
			strAnimName="prone_walk_pistol_forward_01";
			StartAnimation(AParams,0);
			m_State=STATE_WALK;
			m_Stance=NewStance;
		}
		break;


		//--------------------------------------------------------------
	case RELAXED:
		if (m_State==STATE_STAND)
		{
			id0 = pIAnimationSet->GetAnimIDByName(PatchName0("relaxed_idle_pistol_01"));
			id1 = pIAnimationSet->GetAnimIDByName(PatchName0("relaxed_idle_pistol_02"));
			id2 = pIAnimationSet->GetAnimIDByName(PatchName0("relaxed_idle_pistol_03"));
			if (id==id0 || id==id1 || id==id2 || numAnimsLayer0==0)
			{
				AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
				strAnimName="relaxed_walk_pistol_forward_slow_01";
				StartAnimation(AParams,0);
				m_State=STATE_WALK;
				m_Stance=NewStance;
			}
		}
		else
		{
			AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
			strAnimName="relaxed_walk_pistol_forward_slow_01";
			StartAnimation(AParams,0);
			m_State=STATE_WALK;
			m_Stance=NewStance;
		}
		break;

	}
}




void CModelViewport::RunningForward(uint32 NewStance, const CryCharAnimationParams& Params, ISkeletonAnim* pISkeletonAnim)
{
	CryCharAnimationParams AParams=Params;

	int32 id=-7;
	int32 id0,id1,id2;
	int32 id3=-1;

	uint32 numAnimsLayer0 = pISkeletonAnim->GetNumAnimsInFIFO(0);
	if (numAnimsLayer0>1)
		return;
	IAnimationSet* pIAnimationSet = m_pCharacterBase->GetIAnimationSet();
	CAnimation& animation=pISkeletonAnim->GetAnimFromFIFO(0,0);

	id = animation.m_LMG0.m_nLMGID;  //get id of current animation
	if (id<-1)
		id = animation.m_LMG0.m_nAnimID[0];  //get id of current animation

	switch(NewStance)
	{

	case COMBAT:
		id0 = pIAnimationSet->GetAnimIDByName(PatchName0("combat_idle_pistol_01"));
		id1 = pIAnimationSet->GetAnimIDByName(PatchName0("combat_idle_pistol_02"));
		id2 = pIAnimationSet->GetAnimIDByName(PatchName0("combat_idle_pistol_03"));
		//	id3 = pIAnimationSet->GetIDByName(PatchName0("_COMBAT_ROTATE_PISTOL_01"));
		if (m_State==STATE_STAND)
		{
			if (id==id0 || id==id1 || id==id2 || id==id3 || numAnimsLayer0==0)
			{
				f32 hpi=gf_PI/4.0f;
				Vec2 vWorldCurrentBodyDirection=Vec2(m_AnimatedCharacter.q.GetColumn1());
				f32 yaw	     = Ang3::CreateRadZ(vWorldCurrentBodyDirection,m_vWorldDesiredMoveDirection);

				AParams.m_fTransTime	=	0.15f;	//fast transition from idle to walk-transitions
				AParams.m_fKeyTime		=	-1.0f;  //keytime of previous motion

				if (yaw<+hpi && yaw>-hpi)
				{
					//start Idle2Move
					AParams.m_nFlags = CA_REPEAT_LAST_KEY;
					strAnimName="_COMBAT_IDLE2RUN_PISTOL_01";
				}
				else
				{
						uint32 ddd=0;
				} 

				/*				else
				{
				if ( yaw<-hpi && yaw>(-hpi*3) )
				{
				//start walk-right
				AParams.m_nFlags = CA_REPEAT_LAST_KEY;
				strAnimName="combat_runStart_pistol_right_01";
				} 
				else 
				{
				if ( yaw>-hpi && yaw<(hpi*3) )
				{
				//start walk-left
				AParams.m_nFlags = CA_REPEAT_LAST_KEY;
				strAnimName="combat_runStart_pistol_left_01";
				} 
				else 
				{
				//start walk-reverse
				AParams.m_fTransTime	=	0.15f;	//fast transition from idle to walk-transitions
				AParams.m_nFlags = CA_REPEAT_LAST_KEY;
				strAnimName="combat_runStart_pistol_reverse_01";
				}
				}
				}*/

				StartAnimation(AParams,0);

				//only when the Idlw2Move is over, we blend into the run-animation
				AParams.m_nFlags = CA_LOOP_ANIMATION|CA_IDLE2MOVE|CA_TRANSITION_TIMEWARPING;
				AParams.m_fTransTime	=	0.30f;
				AParams.m_fKeyTime		=	-1.0f; //use keytime of previous motion to start this motion
				//m_MoveSpeedMSec=3.5f;
				strAnimName="_COMBAT_RUNSTRAFE_PISTOL_01";
				StartAnimation(AParams,0);
				m_State=STATE_RUN;
				m_Stance=NewStance;


			}
		}
		else
		{
			AParams.m_nFlags = CA_TRANSITION_TIMEWARPING|CA_LOOP_ANIMATION;
			//m_MoveSpeedMSec=6.5f;
			strAnimName="_COMBAT_RUNSTRAFE_PISTOL_01";
			StartAnimation(AParams,0);
			m_State=STATE_RUN;
			m_Stance=NewStance;
		}
		break;

		//--------------------------------------------------------------
	case CROUCH:
		id0 = pIAnimationSet->GetAnimIDByName(PatchName0("crouch_idle_pistol_01"));
		id1 = pIAnimationSet->GetAnimIDByName(PatchName0("crouch_idle_pistol_02"));
		id2 = pIAnimationSet->GetAnimIDByName(PatchName0("crouch_idle_pistol_03"));
		if (m_State==STATE_STAND)
		{
			if (id==id0 || id==id1 || id==id2 || numAnimsLayer0==0)
			{					
				AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
				//m_MoveSpeedMSec=3.0f;
				strAnimName="_STEALTH_RUNSTRAFE_PISTOL_01";	
				StartAnimation(AParams,0);
				m_State=STATE_RUN;
				m_Stance=NewStance;
			}
		}
		else
		{
			AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
			//m_MoveSpeedMSec=3.0f;
			strAnimName="_STEALTH_RUNSTRAFE_PISTOL_01";
			StartAnimation(AParams,0);
			m_State=STATE_RUN;
			m_Stance=NewStance;
		}
		break;

		//--------------------------------------------------------------
	case STEALTH:
		id0 = pIAnimationSet->GetAnimIDByName(PatchName0("stealth_idle_pistol_01"));
		id1 = pIAnimationSet->GetAnimIDByName(PatchName0("stealth_idle_pistol_02"));
		id2 = pIAnimationSet->GetAnimIDByName(PatchName0("stealth_idle_pistol_03"));
		if (m_State==STATE_STAND)
		{
			if (id==id0 || id==id1 || id==id2 || numAnimsLayer0==0)
			{
				AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
				//m_MoveSpeedMSec=3.0f;
				strAnimName="_STEALTH_RUNSTRAFE_PISTOL_01";
				StartAnimation(AParams,0);
				m_State=STATE_RUN;
				m_Stance=NewStance;
			}
		}
		else
		{
			AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
			//m_MoveSpeedMSec=3.0f;
			strAnimName="_STEALTH_RUNSTRAFE_PISTOL_01";
			StartAnimation(AParams,0);
			m_State=STATE_RUN;
			m_Stance=NewStance;
		}
		break;

		//--------------------------------------------------------------
	case PRONE:
		id0 = pIAnimationSet->GetAnimIDByName(PatchName0("prone_idle_pistol_01"));
		id1 = pIAnimationSet->GetAnimIDByName(PatchName0("prone_idle_pistol_02"));
		id2 = pIAnimationSet->GetAnimIDByName(PatchName0("prone_idle_pistol_03"));
		if (m_State==STATE_STAND)
		{
			if (id==id0 || id==id1 || id==id2 || numAnimsLayer0==0)
			{
				AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
				strAnimName="prone_walk_pistol_forward_01";
				StartAnimation(AParams,0);
				m_State=STATE_RUN;
				m_Stance=NewStance;
			}
		}
		else
		{
			AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
			strAnimName="prone_walk_pistol_forward_01";	
			StartAnimation(AParams,0);
			m_State=STATE_RUN;
			m_Stance=NewStance;
		}
		break;


		//--------------------------------------------------------------
	case RELAXED:
		id0 = pIAnimationSet->GetAnimIDByName(PatchName0("relaxed_idle_pistol_01"));
		id1 = pIAnimationSet->GetAnimIDByName(PatchName0("relaxed_idle_pistol_02"));
		id2 = pIAnimationSet->GetAnimIDByName(PatchName0("relaxed_idle_pistol_03"));
		if (m_State==STATE_STAND)
		{
			if (id==id0 || id==id1 || id==id2 || numAnimsLayer0==0)
			{
				AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
				strAnimName="relaxed_run_pistol_forward_slow_01";
				StartAnimation(AParams,0);
				m_State=STATE_RUN;
				m_Stance=NewStance;
			}
		}
		else
		{
			AParams.m_nFlags = CA_LOOP_ANIMATION|CA_TRANSITION_TIMEWARPING;
			strAnimName="relaxed_run_pistol_forward_slow_01";
			StartAnimation(AParams,0);
			m_State=STATE_RUN;
			m_Stance=NewStance;
		}
		break;

	}
}










void CModelViewport::StartAnimation( CryCharAnimationParams& Params, f32 Interpolation )
{
	const char* AnimName=PatchName0(strAnimName);

	AimPoses aim_poses = AttachAimPoses("_COMBAT_RUNSTRAFE_RIFLE_01");

	Params.m_nLayerID=0;
	Params.m_fInterpolation=Interpolation;
	Params.m_fUserData[0] = 0;
	Params.m_fUserData[0] += (AnimName!=0);

	m_pCharacterBase->GetISkeletonAnim()->StartAnimation( AnimName,0, aim_poses.n0,aim_poses.n1, Params);
	return;
}




//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

int PostProcessCallback_PlayControl(ICharacterInstance* pInstance,void* pPlayer)
{
	//process bones specific stuff (IK, torso rotation, etc)
	((CModelViewport*)pPlayer)->ExternalPostProcessing_PlayControl(pInstance);
	return 1;
}
void CModelViewport::ExternalPostProcessing_PlayControl(ICharacterInstance* pInstance)
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();

	float color1[4] = {1,1,1,1};
	//	m_renderer->Draw2dLabel(12,g_ypos,1.2f,color1,false,"PlayerControl PostprocessCallback" );
	//	g_ypos+=10;

	UseFootIKNew(pInstance);
}




//-------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------

uint32 CModelViewport::UseFootIKNew(ICharacterInstance* pInstance)
{
	float color1[4] = {1,1,1,1};
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();

	f32 fSmoothTime = 0.07f;

	static Ang3 angle=Ang3(ZERO);
	angle.x += 0.01f;
	angle.y += 0.001f;
	angle.z += 0.0001f;
	AABB sAABB = AABB(Vec3(-0.02f,-0.02f,-0.02f),Vec3(+0.02f,+0.02f,+0.02f));
	OBB obb =	OBB::CreateOBBfromAABB( Matrix33::CreateRotationXYZ(angle), sAABB );


	Matrix33 m33; AABB wAABB;

	BBox bbox;
	m_arrBBoxes.resize(0);

	m33		=	Matrix33::CreateRotationXYZ( Ang3(-0.5f,0,0) );
	wAABB	= AABB(Vec3(-10.00f,-10.00f,-0.10f),Vec3(+10.00f,+10.00f,+0.20f));
	bbox.obb		=	OBB::CreateOBBfromAABB( m33, wAABB );
	bbox.pos		=	Vec3(0,-5.0f,0);
	bbox.col    = RGBA8(0xff,0x00,0x00,0xff);
	m_arrBBoxes.push_back(bbox);		

	m33		=	Matrix33::CreateRotationXYZ( Ang3(0,0,0) );
	wAABB	= AABB(Vec3(-20.00f,-20.00f,-0.50f),Vec3(+20.00f,+20.00f,-0.01f));
	bbox.obb		=	OBB::CreateOBBfromAABB( m33, wAABB );
	bbox.pos		=	Vec3(0,-5.0f,0);
	bbox.col    = RGBA8(0x1f,0x1f,0x3f,0xff);
	m_arrBBoxes.push_back(bbox);		

	m33		=	Matrix33::CreateRotationXYZ( Ang3(0,0,0) );
	wAABB	= AABB(Vec3(-2.00f,-2.00f,-0.20f),Vec3(+2.00f,+2.00f,+0.20f));
	bbox.obb		=	OBB::CreateOBBfromAABB( m33, wAABB );
	bbox.pos		=	Vec3(5,4,0);
	bbox.col    = RGBA8(0x3f,0x1f,0x1f,0xff);
	m_arrBBoxes.push_back(bbox);		

	m33		=	Matrix33::CreateRotationXYZ( Ang3(0,0,0) );
	wAABB	= AABB(Vec3(-1.00f,-1.00f,-0.20f),Vec3(+1.00f,+1.00f,+0.30f));
	bbox.obb		=	OBB::CreateOBBfromAABB( m33, wAABB );
	bbox.pos		=	Vec3(5,4,0);
	bbox.col    = RGBA8(0x3f,0x1f,0x1f,0xff);
	m_arrBBoxes.push_back(bbox);		

	m33		=	Matrix33::CreateRotationXYZ( Ang3(0,0,0) );
	wAABB	= AABB(Vec3(-1.00f,-1.00f,-0.20f),Vec3(+1.00f,+1.00f,+0.20f));
	bbox.obb		=	OBB::CreateOBBfromAABB( m33, wAABB );
	bbox.pos		=	Vec3(-5,7,0.1f);
	bbox.col    = RGBA8(0x3f,0x1f,0x1f,0xff);
	m_arrBBoxes.push_back(bbox);		


	Vec3 vCharWorldPos = m_AnimatedCharacter.t;
	IVec CWP = CheckFootIntersection(vCharWorldPos,vCharWorldPos);

	f32 fMinHigh=m_AnimatedCharacter.t.z;
	if (fMinHigh<CWP.heel.z)
		fMinHigh=CWP.heel.z;
	Vec3 vGroundNormalRoot	=	CWP.nheel * m_AnimatedCharacter.q;


	f32 fGroundAngle		= acos_tpl(vGroundNormalRoot.z);
	f32 fGroundHeight		= fMinHigh-(fGroundAngle*0.3f);
	Vec3 gnormal	= Vec3(0,vGroundNormalRoot.y,vGroundNormalRoot.z);
	f32 cosine		=	Vec3(0,0,1)|gnormal;
	Vec3 sine			=	Vec3(0,0,1)%gnormal;
	f32 fGroundAngleMoveDir = RAD2DEG(atan2( sgn(sine.x)*sine.GetLength(),cosine ));

	//	m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"GroundAngle: rad:%f  degree:%f  fGroundAngleMoveDir:%f", fGroundAngle, RAD2DEG(fGroundAngle), fGroundAngleMoveDir );
	//	g_ypos+=14;


	//take always the lowest leg for the body position
	static f32 fGroundHeightSmooth=0.0f;
	static f32 fGroundHeightRate=0.0f;
	SmoothCD(fGroundHeightSmooth, fGroundHeightRate, m_AverageFrameTime, fGroundHeight, fSmoothTime);
	pISkeletonPose->MoveSkeletonVertical(fGroundHeightSmooth);

	//	m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"fGroundHeightSmooth: %f", fGroundHeightSmooth );
	//	g_ypos+=14;

	//	m_AnimatedCharacter.t.z = fGroundHeightSmooth; //stay on ground



	int32 LHeelIdx	=	pISkeletonPose->GetJointIDByName("Bip01 L Heel");
	if (LHeelIdx<0) return 0;
	int32 LToe0Idx	=	pISkeletonPose->GetJointIDByName("Bip01 L Toe0");
	if (LToe0Idx<0) return 0;
	uint32 FootplantL	=	0;
	Vec3 Final_LHeel		=	m_AnimatedCharacter*pISkeletonPose->GetAbsJointByID(LHeelIdx).t;
	Vec3 Final_LToe0	  =	m_AnimatedCharacter*pISkeletonPose->GetAbsJointByID(LToe0Idx).t;

	int32 RHeelIdx	=	pISkeletonPose->GetJointIDByName("Bip01 R Heel");
	if (RHeelIdx<0) return 0;
	int32 RToe0Idx	=	pISkeletonPose->GetJointIDByName("Bip01 R Toe0");
	if (RToe0Idx<0) return 0;
	uint32 FootplantR	=	0;
	Vec3 Final_RHeel	=	m_AnimatedCharacter*pISkeletonPose->GetAbsJointByID(RHeelIdx).t;
	Vec3 Final_RToe0  =	m_AnimatedCharacter*pISkeletonPose->GetAbsJointByID(RToe0Idx).t;


	//-------------------------------------------------------------------------

	IVec L4=CheckFootIntersection(Final_LHeel,Final_LToe0);
	pAuxGeom->DrawOBB(obb,L4.toe ,1,RGBA8(0xff,0xff,0xff,0xff),eBBD_Extremes_Color_Encoded);
	pAuxGeom->DrawOBB(obb,L4.heel,1,RGBA8(0x1f,0xff,0xff,0xff),eBBD_Extremes_Color_Encoded);

	IVec R4=CheckFootIntersection(Final_RHeel,Final_RToe0);
	pAuxGeom->DrawOBB(obb,R4.toe ,1,RGBA8(0xff,0xff,0xff,0xff),eBBD_Extremes_Color_Encoded);
	pAuxGeom->DrawOBB(obb,R4.heel,1,RGBA8(0x1f,0xff,0xff,0xff),eBBD_Extremes_Color_Encoded);


	{
		Plane LGroundPlane(Vec3(0,0,1),0);
		if ((L4.heel.z-Final_LHeel.z)>-0.03f)
			LGroundPlane=Plane::CreatePlane(L4.nheel*m_AnimatedCharacter.q,m_AnimatedCharacter.GetInverted()*L4.heel);
		//m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"LGroundPlane: %f %f %f  d:%f", LGroundPlane.n.x,LGroundPlane.n.y,LGroundPlane.n.z,LGroundPlane.d );
		//g_ypos+=14;

		static Plane LSmoothGroundPlane(Vec3(0,0,1),0);
		static Plane LSmoothGroundPlaneRate(Vec3(0,0,1),0);
		SmoothCD( LSmoothGroundPlane, LSmoothGroundPlaneRate, m_AverageFrameTime, LGroundPlane, fSmoothTime);
		//	m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"LSmoothGroundPlane: %f %f %f  d:%f", LSmoothGroundPlane.n.x,LSmoothGroundPlane.n.y,LSmoothGroundPlane.n.z,LSmoothGroundPlane.d );
		//	g_ypos+=14;

		uint32 l0 = ( fabsf(LSmoothGroundPlane.n.x)<0.001f );
		uint32 l1 = ( fabsf(LSmoothGroundPlane.n.y)<0.001f );
		uint32 l2 = ( fabsf(1.0f-LSmoothGroundPlane.n.z)<0.001f );
		uint32 l3 = ( fabsf(LSmoothGroundPlane.d)<0.001f );
		uint32 lik	=	!(l0&l1&l2&l3);
		if (lik)
		{
			pISkeletonPose->SetFootGroundAlignmentCCD( CA_LEG_LEFT, LSmoothGroundPlane);
			//m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"LIK-ENABLED" ); g_ypos+=14;
		}
	}

	{
		Plane RGroundPlane(Vec3(0,0,1),0);
		if ((R4.heel.z-Final_RToe0.z)>-0.03f)
			RGroundPlane=Plane::CreatePlane(R4.nheel*m_AnimatedCharacter.q,m_AnimatedCharacter.GetInverted()*R4.heel);
		//m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"RGroundPlane: %f %f %f  d:%f", RGroundPlane.n.x,RGroundPlane.n.y,RGroundPlane.n.z,RGroundPlane.d );
		//g_ypos+=14;

		static Plane RSmoothGroundPlane(Vec3(0,0,1),0);
		static Plane RSmoothGroundPlaneRate(Vec3(0,0,1),0);
		SmoothCD( RSmoothGroundPlane, RSmoothGroundPlaneRate, m_AverageFrameTime, RGroundPlane, fSmoothTime);
		//	m_renderer->Draw2dLabel(12,g_ypos,1.5f,color1,false,"RSmoothGroundPlane: %f %f %f  d:%f", RSmoothGroundPlane.n.x,RSmoothGroundPlane.n.y,RSmoothGroundPlane.n.z,RSmoothGroundPlane.d );
		//	g_ypos+=14;

		uint32 r0 = ( fabsf(RSmoothGroundPlane.n.x)<0.001f );
		uint32 r1 = ( fabsf(RSmoothGroundPlane.n.y)<0.001f );
		uint32 r2 = ( fabsf(1.0f-RSmoothGroundPlane.n.z)<0.001f );
		uint32 r3 = ( fabsf(RSmoothGroundPlane.d)<0.001f );
		uint32 rik	=	!(r0&r1&r2&r3);
		if (rik)
			pISkeletonPose->SetFootGroundAlignmentCCD( CA_LEG_RIGHT, RSmoothGroundPlane);
	}

	return 1;
}






IVec CModelViewport::CheckFootIntersection(const Vec3& Final_Heel,const Vec3& Final_Toe0)
{
	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();

	Vec3 out;
	uint32 ival=0;
	f32 HeightHeel	=-100.0f;
	f32 HeightToe0	=-100.0f;
	Vec3 normalHeel	=	Vec3(0,0,1);
	Vec3 normalToe0	=	Vec3(0,0,1);

	Ray RayHeel=Ray(Final_Heel+Vec3(0.0f,0.0f,5.5f),Vec3(0,0,-1.0f));
	Ray RayToe0=Ray(Final_Toe0+Vec3(0.0f,0.0f,5.5f),Vec3(0,0,-1.0f));
	//pAuxGeom->DrawLine(RayToeN.origin,RGBA8(0xff,0xff,0xff,0x00),RayToeN.origin+RayToeN.direction*10.0f,RGBA8(0x00,0x0,0xff,0x00));
	//pAuxGeom->DrawLine(RayHeel.origin,RGBA8(0xff,0xff,0xff,0x00),RayHeel.origin+RayHeel.direction*10.0f,RGBA8(0xff,0x0,0x00,0x00));

	uint32 numBoxes = m_arrBBoxes.size();

	for (uint32 i=0; i<numBoxes; i++)
	{
		OBB obb			=	m_arrBBoxes[i].obb;
		Vec3 pos		=	m_arrBBoxes[i].pos;
		ColorB col	=	m_arrBBoxes[i].col;
		pAuxGeom->DrawOBB(obb,pos,1,col,eBBD_Extremes_Color_Encoded);

		ival = Intersect::Ray_OBB(RayHeel,pos,obb,out);
		if (ival==0x01)
			if (HeightHeel<out.z )
			{ 
				HeightHeel	=	out.z;
				normalHeel	=	obb.m33.GetColumn2();// * m_AnimatedCharacter.q;
			};

		ival = Intersect::Ray_OBB(RayToe0,pos,obb,out);
		if (ival==0x01)
			if (HeightToe0<out.z ) 
			{
				HeightToe0		=	out.z;
				normalToe0	=	obb.m33.GetColumn2();// * m_AnimatedCharacter.q;
			};
	}

	IVec retval;
	retval.nheel	=	normalHeel.GetNormalized();
	retval.ntoe	  =	normalToe0.GetNormalized();
	retval.heel		=	Vec3(Final_Heel.x,Final_Heel.y,HeightHeel);
	retval.toe		=	Vec3(Final_Toe0.x,Final_Toe0.y,HeightToe0);
	return retval;
}

