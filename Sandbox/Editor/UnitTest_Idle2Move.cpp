//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File: Idle2Move.cpp
//  Implementation of the unit test to preview Idle2Move LMGs
//
//	History:
//	October 16, 2006: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Controls\AnimationToolBar.h"

#include "ModelViewport.h"
#include "CharacterEditor\CharPanel_Animation.h"
#include "ICryAnimation.h"
#include <I3DEngine.h>
#include <IPhysics.h>
#include <ITimer.h>
#include "IRenderAuxGeom.h"

extern uint32 g_ypos;



//------------------------------------------------------------------------------
//---                         Idle 2 MOve  UnitTest                          ---
//------------------------------------------------------------------------------
void CModelViewport::Idle2Move_UnitTest( ICharacterInstance* pInstance,IAnimationSet* pIAnimationSet, SRendParams rp )
{

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );

	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();

	GetISystem()->GetIAnimationSystem()->SetScalingLimits( Vec2(0.5f, 2.0f) );

	m_absCurrentSpeed			=	pISkeletonAnim->GetCurrentVelocity().GetLength();
	m_absCurrentSlope			=	pISkeletonAnim->GetCurrentSlope();

	Idle2Move(pIAnimationSet);

	m_relCameraRotZ=0;
	m_relCameraRotX=0;
	uint32 useFootAnchoring=0;
	if (m_pCharPanel_Animation)
	{
		GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_JustRootUpdate")->Set( mv_showRootUpdate );
		useFootAnchoring = m_pCharPanel_Animation->GetFootAnchoring();

	}

	//update animation
	pISkeletonPose->SetFootAnchoring( useFootAnchoring );
	pISkeletonAnim->SetFuturePathAnalyser(0);
	pISkeletonAnim->SetAnimationDrivenMotion(1);

	int PreProcessCallback(ICharacterInstance* pInstance,void* pPlayer);
	pISkeletonAnim->SetPreProcessCallback(PreProcessCallback,this);

	int PostProcessCallback(ICharacterInstance* pInstance,void* pPlayer);
	pISkeletonPose->SetPostProcessCallback0(0,this);

	//	pISkeleton->SetAnimEventCallback(AnimEventCallback,this);

	QuatT _Entity;
	_Entity.q.SetIdentity();
	_Entity.t = m_AnimatedCharacter.t;

//	pInstance->SetWorldLocation(m_AnimatedCharacter);
	pInstance->SkeletonPreProcess( _Entity,m_AnimatedCharacter, GetCamera(),0 );

//	_Entity.q.SetIdentity();
//	_Entity.t = m_AnimatedCharacter.t;
	//pInstance->SetWorldLocation(m_AnimatedCharacter);
	//pInstance->SkeletonPostProcess( QuatT(m_AnimatedCharacter),m_AnimatedCharacter, 0,0 );
	pInstance->SkeletonPostProcess( _Entity,m_AnimatedCharacter, 0, (m_PhysEntityLocation.t-m_absCameraPos).GetLength(), 0 );

	//m_AnimatedCharacter.t+=pISkeleton->GetRelFootSlide();


	//use updated animations to render character
	m_EntityMat   = Matrix34(_Entity);
//	m_EntityMat   = Matrix34(m_AnimatedCharacter);
	rp.pMatrix    = &m_EntityMat;
	rp.fDistance	= (_Entity.t-m_absCameraPos).GetLength();
	pInstance->Render( rp, QuatTS(IDENTITY)  );

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

	GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_DrawSkeleton")->Set( mv_showSkeleton );
}






//0-combat walk nw
//1-combat walk pistol
//2-combat walk rifle
//3-combat walk mg

//4-combat run nw
//5-combat run pistol
//6-combat run rifle
//7-combat run mg

//8-stealth walk nw
//9-stealth walk pistol
//10-stealth walk rifle
//11-stealth walk mg

//12-stealth run nw
//13-stealth run pistol
//14-stealth run rifle
//15-stealth run mg

uint32 STANCE=7;


#define STATE_STAND (0)
#define STATE_WALK (1)
#define STATE_WALKSTRAFELEFT  (2)
#define STATE_WALKSTRAFERIGHT (3)
#define STATE_RUN (4)
#define STATE_MOVE2IDLE (5)


#define RELAXED (0)
#define COMBAT (1)
#define STEALTH (2)
#define CROUCH (3)
#define PRONE (4)


extern const char* strAnimName;	


f32 strafrad=0; 
Vec3 StrafeVectorWorld=Vec3(0,1,0);
Vec3 LocalStrafeAnim =Vec3(0,1,0);

Lineseg g_MoveDirectionWorld = Lineseg( Vec3(ZERO), Vec3(ZERO) );

f32 g_LocomotionSpeed = 4.5f;
uint64 _rcr_key_Pup=0;
uint64 _rcr_key_Pdown=0;
uint64 _rcr_key_return=0;

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
f32 CModelViewport::Idle2Move( IAnimationSet* pIAnimationSet )
{
	uint32 ypos = 380;

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	f32 FrameTime = GetISystem()->GetITimer()->GetFrameTime();
	f32 TimeScale = GetISystem()->GetITimer()->GetTimeScale();
	ISkeletonAnim* pISkeletonAnim = m_pCharacterBase->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = m_pCharacterBase->GetISkeletonPose();
	pISkeletonAnim->SetCharEditMode(1);

	Vec3 absPlayerPos = m_AnimatedCharacter.t;


	_rcr_key_Pup<<=1;
	if (CheckVirtualKey(VK_PRIOR)) _rcr_key_Pup|=1;
	if ((_rcr_key_Pup&3)==1) g_LocomotionSpeed+=0.10f;
	if (_rcr_key_Pup==0xffffffffffffffff) g_LocomotionSpeed+=FrameTime;

	_rcr_key_Pdown<<=1;
	if (CheckVirtualKey(VK_NEXT))	_rcr_key_Pdown|=1;
	if ((_rcr_key_Pdown&3)==1) g_LocomotionSpeed-=0.10f;
	if (_rcr_key_Pdown==0xffffffffffffffff) g_LocomotionSpeed-=FrameTime;

	if (g_LocomotionSpeed>16.0f)
		g_LocomotionSpeed=16.0f;
	if (g_LocomotionSpeed<1.0f)
		g_LocomotionSpeed=1.0f;



	_rcr_key_return<<=1;
	if (CheckVirtualKey(VK_CONTROL)==0 &&  CheckVirtualKey(VK_RETURN))
	{	
		_rcr_key_return|=1;
		if ((_rcr_key_return&3)==1) STANCE++;
	}
	if (CheckVirtualKey(VK_CONTROL)!=0 &&  CheckVirtualKey(VK_RETURN))	
	{	
		_rcr_key_return|=1;
		if ((_rcr_key_return&3)==1) STANCE--;
	}

	if (STANCE>20) STANCE=0;
	if (STANCE<0) STANCE=20;


	float color1[4] = {1,1,0,1};
	float color2[4] = {0,1,0,1};

	if (STANCE== 0) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _COMBAT_IDLE2WALK_NW_01",STANCE );
	if (STANCE== 1) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _COMBAT_IDLE2WALK_PISTOL_01",STANCE  );
	if (STANCE== 2) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _COMBAT_IDLE2WALK_RIFLE_01",STANCE  );
	if (STANCE== 3) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _COMBAT_IDLE2WALK_MG_01",STANCE  );
	if (STANCE== 4) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _COMBAT_IDLE2WALK_ROCKET_01",STANCE  );

	if (STANCE== 5) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _COMBAT_IDLE2RUN_NW_01",STANCE  );
	if (STANCE== 6) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _COMBAT_IDLE2RUN_PISTOL_01",STANCE  );
	if (STANCE== 7) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _COMBAT_IDLE2RUN_RIFLE_01",STANCE  );
	if (STANCE== 8) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _COMBAT_IDLE2RUN_MG_01",STANCE  );
	if (STANCE== 9) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _COMBAT_IDLE2RUN_ROCKET_01",STANCE  );

	if (STANCE==10)	m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _STEALTH_IDLE2WALK_NW_01",STANCE  );
	if (STANCE==11)	m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _STEALTH_IDLE2WALK_PISTOL_01",STANCE  );
	if (STANCE==12) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _STEALTH_IDLE2WALK_RIFLE_01",STANCE  );
	if (STANCE==13) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _STEALTH_IDLE2WALK_MG_01",STANCE  );
	if (STANCE==14) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _STEALTH_IDLE2WALK_ROCKET_01",STANCE  );

	if (STANCE==15) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _STEALTH_IDLE2RUN_NW_01",STANCE  );
	if (STANCE==16) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _STEALTH_IDLE2RUN_PISTOL_01",STANCE  );
	if (STANCE==17) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _STEALTH_IDLE2RUN_RIFLE_01",STANCE  );
	if (STANCE==18) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _STEALTH_IDLE2RUN_MG_01",STANCE  );
	if (STANCE==19) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _STEALTH_IDLE2RUN_ROCKET_01",STANCE  );

	if (STANCE==20) m_renderer->Draw2dLabel(12,ypos,2.0f,color2,false,"Stance: %02d _RELAXED_IDLE2WALK_NW_01",STANCE  );
	ypos+=20;


	int32 left=0;
	int32 right=0;
	m_key_W=0;
	m_key_S=0;
	if ( CheckVirtualKey('W') )
		m_key_W=1; 
	if ( CheckVirtualKey('S') )
		m_key_S=1; 

	if ( CheckVirtualKey(VK_LEFT) )
		left=+3; 
	if ( CheckVirtualKey(VK_RIGHT) )
		right=-3;  



//	static f32 SmoothFrameTime = 0.00f;
//	static f32 SmoothFrameTimeRate = 0.00f;
//	SmoothCD(SmoothFrameTime, SmoothFrameTimeRate, FrameTime, FrameTime*left+FrameTime*right, 0.25f);
	strafrad+=FrameTime*left+FrameTime*right;

	StrafeVectorWorld =  Matrix33::CreateRotationZ( strafrad ).GetColumn1();
	LocalStrafeAnim =  StrafeVectorWorld* m_AnimatedCharacter.q;


	Vec3 wpos=m_AnimatedCharacter.t+Vec3(0,0,1);
	pAuxGeom->DrawLine( wpos,RGBA8(0xff,0xff,0x00,0x00), wpos+StrafeVectorWorld*2,RGBA8(0x00,0x00,0x00,0x00) );

	CAnimation animation;
	uint32 numAnimsLayer0 = pISkeletonAnim->GetNumAnimsInFIFO(0);
	if (numAnimsLayer0)
		animation=pISkeletonAnim->GetAnimFromFIFO(0,0);


	CryCharAnimationParams AParams;

	if (m_State<0)
		m_State=STATE_STAND;

	if (m_key_S && m_State==STATE_RUN && numAnimsLayer0<=2)
	{
		m_State=STATE_MOVE2IDLE;
	}



	static f32 LastTurnRadianSec7 = 0;
	static f32 LastTurnRadianSec6 = 0;
	static f32 LastTurnRadianSec5 = 0;
	static f32 LastTurnRadianSec4 = 0;
	static f32 LastTurnRadianSec3 = 0;
	static f32 LastTurnRadianSec2 = 0;
	static f32 LastTurnRadianSec1 = 0;
	static f32 TurnRadianSec      = 0;




	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelSpeed, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TurnSpeed, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelAngle, 0, 0) ;
	pISkeletonAnim->SetLayerUpdateMultiplier(0,1); //no scaling



	f32 fTravelSpeed	= g_LocomotionSpeed;
	f32 fTravelAngle	= -atan2f(LocalStrafeAnim.x,LocalStrafeAnim.y);
	f32 fTurnSpeed		= 0;



	LMGCapabilities caps;

	if (STANCE== 0) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLE2WALK_NW_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE== 1) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLE2WALK_PISTOL_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE== 2) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLE2WALK_RIFLE_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE== 3) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLE2WALK_MG_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE== 4) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLE2WALK_ROCKET_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );

	if (STANCE== 5) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLE2RUN_NW_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE== 6) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLE2RUN_PISTOL_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE== 7) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLE2RUN_RIFLE_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE== 8) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLE2RUN_MG_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE== 9) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLE2RUN_ROCKET_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );

	if (STANCE==10)	caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLE2WALK_NW_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE==11)	caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLE2WALK_PISTOL_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE==12) caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLE2WALK_RIFLE_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE==13) caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLE2WALK_MG_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE==14) caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLE2WALK_ROCKET_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );

	if (STANCE==15) caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLE2RUN_NW_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE==16) caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLE2RUN_PISTOL_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE==17) caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLE2RUN_RIFLE_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE==18) caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLE2RUN_MG_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );
	if (STANCE==19) caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLE2RUN_ROCKET_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );

	if (STANCE==20) caps = pIAnimationSet->GetLMGPropertiesByName( "_RELAXED_IDLE2WALK_NW_01", Vec2(LocalStrafeAnim.x,LocalStrafeAnim.y) ,0,0  );


	char name[5];	const char* bc = (const char*)(&caps.m_BlendType);
	name[0]=bc[0];	name[1]=bc[1];	name[2]=bc[2];	name[3]=bc[3]; name[4]=0;
	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_BlendType: %s %x",name,_rcr_key_Pup);	ypos+=16;

	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_fSlowDuration: %f",caps.m_fSlowDuration );	ypos+=15;
	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_fFastDuration: %f",caps.m_fFastDuration );	ypos+=15;
	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_vMinVelocity: %f %f  Speed: %f",caps.m_vMinVelocity.x,caps.m_vMinVelocity.y, caps.m_vMinVelocity.len() );	ypos+=15;
	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_vMaxVelocity: %f %f  Speed: %f",caps.m_vMaxVelocity.x,caps.m_vMaxVelocity.y, caps.m_vMaxVelocity.len() );	ypos+=15;


	//standing idle
	if (m_State==STATE_STAND && numAnimsLayer0<=1)
	{
		AParams.m_fInterpolation = 0;

		//blend into idle after walk-stop 
		AParams.m_nFlags = CA_LOOP_ANIMATION|CA_START_AFTER;
		AParams.m_fTransTime	=	0.15f;
		AParams.m_fKeyTime		=	-1; //use keytime of previous motion to start this motion

		if (STANCE== 0)	strAnimName="_COMBAT_IDLESTEP_NW_01";
		if (STANCE== 1)	strAnimName="_COMBAT_IDLESTEP_PISTOL_01";
		if (STANCE== 2)	strAnimName="_COMBAT_IDLESTEP_RIFLE_01";
		if (STANCE== 3)	strAnimName="_COMBAT_IDLESTEP_MG_01";
		if (STANCE== 4)	strAnimName="_COMBAT_IDLESTEP_ROCKET_01";

		if (STANCE== 5)	strAnimName="_COMBAT_IDLESTEP_NW_01";
		if (STANCE== 6)	strAnimName="_COMBAT_IDLESTEP_PISTOL_01";
		if (STANCE== 7)	strAnimName="_COMBAT_IDLESTEP_RIFLE_01";
		if (STANCE== 8)	strAnimName="_COMBAT_IDLESTEP_MG_01";
		if (STANCE== 9)	strAnimName="_COMBAT_IDLESTEP_ROCKET_01";

		if (STANCE==10)	strAnimName="_STEALTH_IDLESTEP_NW_01";
		if (STANCE==11)	strAnimName="_STEALTH_IDLESTEP_PISTOL_01";
		if (STANCE==12)	strAnimName="_STEALTH_IDLESTEP_RIFLE_01";
		if (STANCE==13)	strAnimName="_STEALTH_IDLESTEP_MG_01";
		if (STANCE==14)	strAnimName="_STEALTH_IDLESTEP_ROCKET_01";

		if (STANCE==15)	strAnimName="_STEALTH_IDLESTEP_NW_01";
		if (STANCE==16)	strAnimName="_STEALTH_IDLESTEP_PISTOL_01";
		if (STANCE==17)	strAnimName="_STEALTH_IDLESTEP_RIFLE_01";
		if (STANCE==18)	strAnimName="_STEALTH_IDLESTEP_MG_01";
		if (STANCE==19)	strAnimName="_STEALTH_IDLESTEP_ROCKET_01";

		if (STANCE==20)	strAnimName="_RELAXED_IDLESTEP_NW_01";

		pISkeletonAnim->StartAnimation( strAnimName,0, 0,0, AParams);

		g_MoveDirectionWorld.start	= m_AnimatedCharacter.t+Vec3(0,0,0.01f);
		g_MoveDirectionWorld.end		= m_AnimatedCharacter.t+Vec3(0,0,0.01f) + StrafeVectorWorld*10.0f;

		m_State=STATE_STAND;
		m_Stance=0;
	}



	//start "IDLE2RUN" and "running"
	if (m_key_W && m_State==STATE_STAND && numAnimsLayer0<=1)
	{

		AParams.m_fInterpolation = 0;

		AParams.m_nFlags							= CA_REPEAT_LAST_KEY; //CA_LOOP_ANIMATION;
		AParams.m_fTransTime					=	0.05f;	//start immediately 
		AParams.m_fKeyTime						=	-1;			//use keytime of previous motion to start this motion
		AParams.m_fPlaybackSpeed			= 1.0f;

		if (STANCE==0)	strAnimName="_COMBAT_IDLE2WALK_NW_01";	
		if (STANCE==1)	strAnimName="_COMBAT_IDLE2WALK_PISTOL_01";
		if (STANCE==2)	strAnimName="_COMBAT_IDLE2WALK_RIFLE_01";
		if (STANCE==3)	strAnimName="_COMBAT_IDLE2WALK_MG_01";
		if (STANCE==4)	strAnimName="_COMBAT_IDLE2WALK_ROCKET_01";

		if (STANCE==5)	strAnimName="_COMBAT_IDLE2RUN_NW_01";	
		if (STANCE==6)	strAnimName="_COMBAT_IDLE2RUN_PISTOL_01";
		if (STANCE==7)	strAnimName="_COMBAT_IDLE2RUN_RIFLE_01";
		if (STANCE==8)	strAnimName="_COMBAT_IDLE2RUN_MG_01";
		if (STANCE==9)	strAnimName="_COMBAT_IDLE2RUN_ROCKET_01";

		if (STANCE==10)	strAnimName="_STEALTH_IDLE2WALK_NW_01";	
		if (STANCE==11)	strAnimName="_STEALTH_IDLE2WALK_PISTOL_01";
		if (STANCE==12)	strAnimName="_STEALTH_IDLE2WALK_RIFLE_01";
		if (STANCE==13)	strAnimName="_STEALTH_IDLE2WALK_MG_01";
		if (STANCE==14)	strAnimName="_STEALTH_IDLE2WALK_ROCKET_01";

		if (STANCE==15)	strAnimName="_STEALTH_IDLE2RUN_NW_01";	
		if (STANCE==16)	strAnimName="_STEALTH_IDLE2RUN_PISTOL_01";
		if (STANCE==17)	strAnimName="_STEALTH_IDLE2RUN_RIFLE_01";
		if (STANCE==18)	strAnimName="_STEALTH_IDLE2RUN_MG_01";
		if (STANCE==19)	strAnimName="_STEALTH_IDLE2RUN_ROCKET_01";

		if (STANCE==20)	strAnimName="_RELAXED_IDLE2WALK_NW_01";

		pISkeletonAnim->StartAnimation( strAnimName,0, 0,0, AParams);

		AParams.m_nFlags = CA_LOOP_ANIMATION|CA_IDLE2MOVE|CA_TRANSITION_TIMEWARPING;
		AParams.m_fTransTime	=	0.55f;	  //average time for transition 
		AParams.m_fKeyTime		=	-1;			//use keytime of previous motion to start this motion
		AParams.m_fPlaybackSpeed = 1.0f;

		if (STANCE==0)	strAnimName="_COMBAT_WALKSTRAFE_NW_01";	
		if (STANCE==1)	strAnimName="_COMBAT_WALKSTRAFE_PISTOL_01";
		if (STANCE==2)	strAnimName="_COMBAT_WALKSTRAFE_RIFLE_01";
		if (STANCE==3)	strAnimName="_COMBAT_WALKSTRAFE_MG_01";
		if (STANCE==4)	strAnimName="_COMBAT_WALKSTRAFE_ROCKET_01";

		if (STANCE==5)	strAnimName="_COMBAT_RUNSTRAFE_NW_01";	
		if (STANCE==6)	strAnimName="_COMBAT_RUNSTRAFE_PISTOL_01";
		if (STANCE==7)	strAnimName="_COMBAT_RUNSTRAFE_RIFLE_01";
		if (STANCE==8)	strAnimName="_COMBAT_RUNSTRAFE_MG_01";
		if (STANCE==9)	strAnimName="_COMBAT_RUNSTRAFE_ROCKET_01";

		if (STANCE==10)	strAnimName="_STEALTH_WALKSTRAFE_NW_01";	
		if (STANCE==11)	strAnimName="_STEALTH_WALKSTRAFE_PISTOL_01";
		if (STANCE==12)	strAnimName="_STEALTH_WALKSTRAFE_RIFLE_01";
		if (STANCE==13)	strAnimName="_STEALTH_WALKSTRAFE_MG_01";
		if (STANCE==14)	strAnimName="_STEALTH_WALKSTRAFE_ROCKET_01";

		if (STANCE==15)	strAnimName="_STEALTH_RUNSTRAFE_NW_01";	
		if (STANCE==16)	strAnimName="_STEALTH_RUNSTRAFE_PISTOL_01";
		if (STANCE==17)	strAnimName="_STEALTH_RUNSTRAFE_RIFLE_01";
		if (STANCE==18)	strAnimName="_STEALTH_RUNSTRAFE_MG_01";
		if (STANCE==19)	strAnimName="_STEALTH_RUNSTRAFE_ROCKET_01";

		if (STANCE==20)	strAnimName="_RELAXED_WALKSTRAFE_NW_01";

		pISkeletonAnim->StartAnimation( strAnimName,0, 0,0, AParams);


		g_MoveDirectionWorld.start	= m_AnimatedCharacter.t+Vec3(0,0,0.01f);
		g_MoveDirectionWorld.end		= m_AnimatedCharacter.t+Vec3(0,0,0.01f) + StrafeVectorWorld*10.0f;


		m_State=STATE_RUN;
		m_Stance=0;

	}

	//stopping
	if (m_State==STATE_MOVE2IDLE && numAnimsLayer0<=2)
	{
	//	if (animation.m_fAnimTime<0.5f)
		{
			AParams.m_fInterpolation = 0;

			AParams.m_nFlags			= CA_MOVE2IDLE|CA_TRANSITION_TIMEWARPING|CA_REPEAT_LAST_KEY;
			AParams.m_fTransTime	=	0.25f;	    //average time for transition 
			AParams.m_fKeyTime		=	-1;		//use keytime of previous motion to start this motion

		//	if (STANCE==0) strAnimName="combat_walkStop_nw_forward_slow_rfoot_01";	
		//	if (STANCE==1) strAnimName="combat_walkStop_pistol_forward_slow_rfoot_01";	
		//	if (STANCE==2) strAnimName="combat_walkStop_rifle_forward_slow_rfoot_01";	
		//	if (STANCE==3) strAnimName="combat_walkStop_mg_forward_slow_rfoot_01";	
		//	if (STANCE==4) strAnimName="combat_walkStop_rocket_forward_slow_rfoot_01";	
			if (STANCE==0) strAnimName="_COMBAT_WALK2IDLE_NW_01";	
			if (STANCE==1) strAnimName="_COMBAT_WALK2IDLE_PISTOL_01";	
			if (STANCE==2) strAnimName="_COMBAT_WALK2IDLE_RIFLE_01";	
			if (STANCE==3) strAnimName="_COMBAT_WALK2IDLE_MG_01";	
			if (STANCE==4) strAnimName="_COMBAT_WALK2IDLE_ROCKET_01";	

			if (STANCE==5) strAnimName="_COMBAT_RUN2IDLE_NW_01";	
			if (STANCE==6) strAnimName="_COMBAT_RUN2IDLE_PISTOL_01";	
			if (STANCE==7) strAnimName="_COMBAT_RUN2IDLE_RIFLE_01";	
			if (STANCE==8) strAnimName="_COMBAT_RUN2IDLE_MG_01";	
			if (STANCE==9) strAnimName="_COMBAT_RUN2IDLE_ROCKET_01";	

		//	if (STANCE==10)	strAnimName="stealth_walkStop_nw_forward_slow_rfoot_01";	
		//	if (STANCE==11)	strAnimName="stealth_walkStop_pistol_forward_slow_rfoot_01";	
		//	if (STANCE==12) strAnimName="stealth_walkStop_rifle_forward_slow_rfoot_01";	
		//	if (STANCE==13) strAnimName="stealth_walkStop_mg_forward_slow_rfoot_01";	
		//	if (STANCE==14) strAnimName="stealth_walkStop_rocket_forward_slow_rfoot_01";	
			if (STANCE==10) strAnimName="_STEALTH_WALK2IDLE_NW_01";	
			if (STANCE==11) strAnimName="_STEALTH_WALK2IDLE_PISTOL_01";	
			if (STANCE==12) strAnimName="_STEALTH_WALK2IDLE_RIFLE_01";	
			if (STANCE==13) strAnimName="_STEALTH_WALK2IDLE_MG_01";	
			if (STANCE==14) strAnimName="_STEALTH_WALK2IDLE_ROCKET_01";	

			if (STANCE==15) strAnimName="_STEALTH_RUN2IDLE_NW_01";	
			if (STANCE==16) strAnimName="_STEALTH_RUN2IDLE_PISTOL_01";	
			if (STANCE==17) strAnimName="_STEALTH_RUN2IDLE_RIFLE_01";	
			if (STANCE==18) strAnimName="_STEALTH_RUN2IDLE_MG_01";	
			if (STANCE==19) strAnimName="_STEALTH_RUN2IDLE_ROCKET_01";	

			if (STANCE==20) strAnimName="relaxed_walkStop_nw_forward_slow_rfoot_01";	
			pISkeletonAnim->StartAnimation( strAnimName,0, 0,0, AParams);

			m_State=STATE_STAND;
			m_Stance=0;
		}
	}



	numAnimsLayer0 = pISkeletonAnim->GetNumAnimsInFIFO(0);
	if (m_State==STATE_RUN && numAnimsLayer0<=2)
	{
		f32 FramesPerSecond = TimeScale/m_AverageFrameTime;

		LastTurnRadianSec7	=	LastTurnRadianSec6;
		LastTurnRadianSec6	=	LastTurnRadianSec5;
		LastTurnRadianSec5	=	LastTurnRadianSec4;
		LastTurnRadianSec4	=	LastTurnRadianSec3;
		LastTurnRadianSec3	=	LastTurnRadianSec2;
		LastTurnRadianSec2	=	LastTurnRadianSec1;
		LastTurnRadianSec1	=	TurnRadianSec;

		TurnRadianSec = Ang3::CreateRadZ(m_AnimatedCharacter.q.GetColumn1(),StrafeVectorWorld) * FramesPerSecond;
		TurnRadianSec /= (FramesPerSecond/12.0f);

		f32 avrg = (TurnRadianSec+LastTurnRadianSec1)*0.5f;
		TurnRadianSec  =	avrg;
		fTravelAngle	=	0;
		fTurnSpeed		=	TurnRadianSec;

		if (fTurnSpeed>5.0f)
			fTurnSpeed=5.0f;
		if (fTurnSpeed<-5.0f)
			fTurnSpeed=-5.0f;

		fTravelSpeed *= (5.0f-fabsf(fTurnSpeed))/5.0f;
		if (fTravelSpeed<1.0f)
			fTravelSpeed=1.0f;
		m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"TurnRadianSec: %f",TurnRadianSec );	ypos+=10;
	}





	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"fTravelAngle: %f     fTravelVector: %f %f",fTravelAngle,LocalStrafeAnim.x,LocalStrafeAnim.y );	
	ypos+=10;
	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"fTravelSpeed: %f",fTravelSpeed );	
	ypos+=40;

//	pISkeleton->SetDesiredMotionParam(eMotionParamID_TravelAngle, Vec2(LocalStrafeAnim), FrameTime);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelAngle, fTravelAngle, FrameTime);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelSpeed, fTravelSpeed, FrameTime);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelDist,      -1,       FrameTime) ;

	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnSpeed,		fTurnSpeed,		FrameTime);
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnAngle,		     0,       FrameTime) ;

	//	pISkeleton->SetBlendSpaceOverride(eMotionParamID_TravelAngle, -atan2f(LocalStrafeAnim.x,LocalStrafeAnim.y), 1) ;
	//	pISkeleton->SetBlendSpaceOverride(eMotionParamID_TravelDistScale, 1, 1) ;
	pISkeletonAnim->SetIWeight(0, 0 );

	pAuxGeom->DrawLine( g_MoveDirectionWorld.start,RGBA8(0xff,0xff,0xff,0x00), g_MoveDirectionWorld.end, RGBA8(0x00,0xff,0x00,0x00) );



	//------------------------------------------------------------------------
	//---  camera control
	//------------------------------------------------------------------------
	//	uint32 AttachedCamera = m_pCharPanel_Animation->GetAttachedCamera();
	//	if (AttachedCamera)
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


		SmoothCD(m_LookAt, m_LookAtRate, m_AverageFrameTime, Vec3(absPlayerPos.x,absPlayerPos.y,0.7f), 0.05f);
		Vec3 absCameraPos		=	m_LookAt+Vec3(0,0,m_absCameraHigh);
		absCameraPos.x=m_LookAt.x;
		absCameraPos.y=m_LookAt.y+4.0f;

		SmoothCD(m_CamPos, m_CamPosRate, m_AverageFrameTime, absCameraPos, 0.25f);
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
			//m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"AimStatus: %d %d",AimIK, status );	ypos+=10;
			//m_renderer->Draw2dLabel(12,ypos,1.2f,color1,false,"m_absCameraPos: %f %f %f",m_absCameraPos.x,m_absCameraPos.y,m_absCameraPos.z );	ypos+=10;
		}

	}

	return 0;

}


