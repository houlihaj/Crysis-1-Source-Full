//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File: IdleStep.cpp
//  Implementation of the unit test to preview IdleStep LMGs
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
int32 stance=3;


//------------------------------------------------------------------------------
//---                          IdleStep  UnitTest                            ---
//------------------------------------------------------------------------------
void CModelViewport::IdleStep_UnitTest( ICharacterInstance* pInstance, IAnimationSet* pIAnimationSet, SRendParams rp )
{

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );

	ISkeletonAnim* pISkeletonAnim = pInstance->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = pInstance->GetISkeletonPose();

	GetISystem()->GetIAnimationSystem()->SetScalingLimits( Vec2(0.5f, 2.0f) );

	m_absCurrentSpeed			=	pISkeletonAnim->GetCurrentVelocity().GetLength();
	m_absCurrentSlope			=	pISkeletonAnim->GetCurrentSlope();

	IdleStep( pIAnimationSet );

	m_relCameraRotZ=0;
	m_relCameraRotX=0;
	if (m_pCharPanel_Animation)
	{
		GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ca_JustRootUpdate")->Set( mv_showRootUpdate );
	}

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



	QuatT _Entity;
	_Entity.q.SetIdentity();
	_Entity.t = m_AnimatedCharacter.t;
//	pInstance->SetWorldLocation(m_AnimatedCharacter);
	pInstance->SkeletonPreProcess( _Entity,m_AnimatedCharacter, GetCamera(),0 );
//	pInstance->SetWorldLocation(m_AnimatedCharacter);
	pInstance->SkeletonPostProcess( _Entity,m_AnimatedCharacter, 0, (m_PhysEntityLocation.t-m_absCameraPos).GetLength(), 0 );

	//m_AnimatedCharacter.t+=pISkeleton->GetRelFootSlide();

	Vec3 absAxisX		=	m_AnimatedCharacter.GetColumn0();
	Vec3 absAxisY		=	m_AnimatedCharacter.GetColumn1();
	Vec3 absAxisZ		=	m_AnimatedCharacter.GetColumn2();
	Vec3 absRootPos	=	m_AnimatedCharacter.t+pISkeletonPose->GetAbsJointByID(0).t;
	pAuxGeom->DrawLine( absRootPos,RGBA8(0xff,0x00,0x00,0x00), absRootPos+m_AnimatedCharacter.GetColumn0()*2.0f,RGBA8(0x00,0x00,0x00,0x00) );
	pAuxGeom->DrawLine( absRootPos,RGBA8(0x00,0xff,0x00,0x00), absRootPos+m_AnimatedCharacter.GetColumn1()*2.0f,RGBA8(0x00,0x00,0x00,0x00) );
	pAuxGeom->DrawLine( absRootPos,RGBA8(0x00,0x00,0xff,0x00), absRootPos+m_AnimatedCharacter.GetColumn2()*2.0f,RGBA8(0x00,0x00,0x00,0x00) );

	//use updated animations to render character
	m_EntityMat   = Matrix34(_Entity);
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


f32 StrafRadIS=0; 
Vec3 StrafeVectorWorldIS=Vec3(0,1,0);
Vec3 LocalStrafeAnimIS =Vec3(0,1,0);
Vec3 LastIdlePos =Vec3(0,0,0);

Lineseg g_MoveDirectionWorldIS = Lineseg( Vec3(ZERO), Vec3(ZERO) );

f32 g_StepScale = 1.0f;
uint32 rcr_key_Pup=0;
uint32 rcr_key_Pdown=0;
uint64 rcr_key_return=0;

uint32 m_key_R=0;

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
f32 CModelViewport::IdleStep( IAnimationSet* pIAnimationSet )
{

	uint32 ypos = 382;

	IRenderAuxGeom* pAuxGeom = m_renderer->GetIRenderAuxGeom();
	pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );

	f32 FrameTime = GetISystem()->GetITimer()->GetFrameTime();
	ISkeletonAnim* pISkeletonAnim = m_pCharacterBase->GetISkeletonAnim();
	ISkeletonPose* pISkeletonPose = m_pCharacterBase->GetISkeletonPose();
	pISkeletonAnim->SetCharEditMode(1);

	//	m_AnimatedCharacter.q.SetRotationZ(0.0f);

	Vec3 absPlayerPos = m_AnimatedCharacter.t;

	rcr_key_Pup<<=1;
	if (CheckVirtualKey(VK_PRIOR))	rcr_key_Pup|=1;
	if ((rcr_key_Pup&3)==1) g_StepScale+=0.05f;
	rcr_key_Pdown<<=1;
	if (CheckVirtualKey(VK_NEXT))	rcr_key_Pdown|=1;
	if ((rcr_key_Pdown&3)==1) g_StepScale-=0.05f;

	rcr_key_return<<=1;
	if (CheckVirtualKey(VK_CONTROL)==0 &&  CheckVirtualKey(VK_RETURN))
	{	
		rcr_key_return|=1;
		if ((rcr_key_return&3)==1) stance++;
	}
	if (CheckVirtualKey(VK_CONTROL)!=0 &&  CheckVirtualKey(VK_RETURN))	
	{	
		rcr_key_return|=1;
		if ((rcr_key_return&3)==1) stance--;
	}

	if (stance>15) stance=0;
	if (stance< 0) stance=15;



	if (g_StepScale>1.5f)
		g_StepScale=1.5f;
	if (g_StepScale<0.0f)
		g_StepScale=0.0f;

	uint32 left=0;
	uint32 right=0;

	m_key_W=0;
	m_key_R=0;
	m_key_S=0;

	if ( CheckVirtualKey('W') )
		m_key_W=1; 
	if ( CheckVirtualKey('R') )
		m_key_R=1; 
	if ( CheckVirtualKey('S') )
		m_key_S=1; 

	if ( CheckVirtualKey(VK_LEFT) )
		left=1; 
	if ( CheckVirtualKey(VK_RIGHT) )
		right=1;  

	if (left) 	
		StrafRadIS+=0.03f;
	if (right) 	
		StrafRadIS-=0.03f;

	//always make sure we deactivate the overrides
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelSpeed, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TurnSpeed, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelAngle, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelDistScale, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelDist, 0, 0) ;
	pISkeletonAnim->SetBlendSpaceOverride(eMotionParamID_TravelSlope, 0, 0) ;
	pISkeletonAnim->SetLayerUpdateMultiplier(0,1); //no scaling



	StrafeVectorWorldIS =  Matrix33::CreateRotationZ( StrafRadIS ).GetColumn1();
	LocalStrafeAnimIS =  StrafeVectorWorldIS* m_AnimatedCharacter.q;

	Vec3 absPelvisPos	=	pISkeletonPose->GetAbsJointByID(1).t;

	//	Vec3 CurrMoveDir =  (m_absEntityMat) * pISkeleton->GetCurrentMoveDirection();
	Vec3 wpos=m_AnimatedCharacter.t;
	pAuxGeom->DrawLine( wpos+absPelvisPos,RGBA8(0xff,0xff,0x00,0x00), wpos+absPelvisPos+StrafeVectorWorldIS*2,RGBA8(0x00,0x00,0x00,0x00) );
	//pAuxGeom->DrawLine( absRootPos+Vec3(0,0,0.1f),RGBA8(0xff,0xff,0x00,0x00), absRootPos+Vec3(0,0,0.1f)-CurrMoveDir*4,RGBA8(0x00,0x00,0x00,0x00) );

	CAnimation animation;
	uint32 numAnimsLayer0 = pISkeletonAnim->GetNumAnimsInFIFO(0);
	if (numAnimsLayer0)
		animation=pISkeletonAnim->GetAnimFromFIFO(0,0);


	CryCharAnimationParams AParams;

	if (m_State<0)
		m_State=STATE_STAND;

	if (m_key_S && m_State==STATE_RUN && numAnimsLayer0<=1)
	{
		m_State=STATE_MOVE2IDLE;
	}


	LMGCapabilities caps;
	for (f32 rad=0.0f; rad<gf_PI*2; rad=rad+0.02f )
	{
		Vec3 strafe =  Matrix33::CreateRotationZ( rad ).GetColumn1();
		if (stance==0) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLESTEP_MG_01", Vec2(strafe), 0,0  );
		if (stance==1) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLESTEP_NW_01", Vec2(strafe), 0,0  );
		if (stance==2) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLESTEP_PISTOL_01", Vec2(strafe), 0,0  );
		if (stance==3) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLESTEP_RIFLE_01", Vec2(strafe), 0,0  );
		if (stance==4) caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLESTEP_ROCKET_01", Vec2(strafe), 0,0  );

		if (stance==5) caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLESTEP_MG_01", Vec2(strafe), 0,0  );
		if (stance==6) caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLESTEP_NW_01", Vec2(strafe), 0,0  );
		if (stance==7) caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLESTEP_PISTOL_01", Vec2(strafe), 0,0  );
		if (stance==8) caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLESTEP_RIFLE_01", Vec2(strafe), 0,0  );
		if (stance==9) caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLESTEP_ROCKET_01", Vec2(strafe), 0,0  );

		if (stance==10) caps = pIAnimationSet->GetLMGPropertiesByName( "_CROUCH_IDLESTEP_MG_01", Vec2(strafe), 0,0  );
		if (stance==11) caps = pIAnimationSet->GetLMGPropertiesByName( "_CROUCH_IDLESTEP_NW_01", Vec2(strafe), 0,0  );
		if (stance==12)	caps = pIAnimationSet->GetLMGPropertiesByName( "_CROUCH_IDLESTEP_PISTOL_01", Vec2(strafe), 0,0  );
		if (stance==13)	caps = pIAnimationSet->GetLMGPropertiesByName( "_CROUCH_IDLESTEP_RIFLE_01", Vec2(strafe), 0,0  );
		if (stance==14)	caps = pIAnimationSet->GetLMGPropertiesByName( "_CROUCH_IDLESTEP_ROCKET_01", Vec2(strafe), 0,0  );

		if (stance==15)	caps = pIAnimationSet->GetLMGPropertiesByName( "_RELAXED_IDLESTEP_NW_01", Vec2(strafe), 0,0  );

		Vec3 pos=caps.m_vMaxVelocity*caps.m_fFastDuration*g_StepScale;
		pos	=	m_AnimatedCharacter.q*pos +  LastIdlePos;

		AABB aabb;
		aabb.min=Vec3(-0.01f,-0.01f,-0.01f)+pos;
		aabb.max=Vec3(+0.01f,+0.01f,+0.01f)+pos;
		pAuxGeom->DrawAABB(aabb,1, RGBA8(0xff,0x11,0x1f,0x00),eBBD_Extremes_Color_Encoded );
	}


	if (stance==0)	caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLESTEP_MG_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );
	if (stance==1)	caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLESTEP_NW_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );
	if (stance==2)	caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLESTEP_PISTOL_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );
	if (stance==3)	caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLESTEP_RIFLE_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );
	if (stance==4)	caps = pIAnimationSet->GetLMGPropertiesByName( "_COMBAT_IDLESTEP_ROCKET_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );

	if (stance==5)	caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLESTEP_MG_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );
	if (stance==6)	caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLESTEP_NW_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );
	if (stance==7)	caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLESTEP_PISTOL_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );
	if (stance==8)	caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLESTEP_RIFLE_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );
	if (stance==9)	caps = pIAnimationSet->GetLMGPropertiesByName( "_STEALTH_IDLESTEP_ROCKET_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );

	if (stance==10)	caps = pIAnimationSet->GetLMGPropertiesByName( "_CROUCH_IDLESTEP_MG_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );
	if (stance==11)	caps = pIAnimationSet->GetLMGPropertiesByName( "_CROUCH_IDLESTEP_NW_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );
	if (stance==12)	caps = pIAnimationSet->GetLMGPropertiesByName( "_CROUCH_IDLESTEP_PISTOL_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );
	if (stance==13) caps = pIAnimationSet->GetLMGPropertiesByName( "_CROUCH_IDLESTEP_RIFLE_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );
	if (stance==14) caps = pIAnimationSet->GetLMGPropertiesByName( "_CROUCH_IDLESTEP_ROCKET_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );

	if (stance==15) caps = pIAnimationSet->GetLMGPropertiesByName( "_RELAXED_IDLESTEP_NW_01", Vec2(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y) ,0,0 );

	float color1[4] = {1,1,0,1};

	if (stance== 0)	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _COMBAT_IDLESTEP_MG_01",stance  );
	if (stance== 1)	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _COMBAT_IDLESTEP_NW_01",stance  );
	if (stance== 2)	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _COMBAT_IDLESTEP_PISTOL_01",stance );
	if (stance== 3)	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _COMBAT_IDLESTEP_RIFLE_01",stance  );
	if (stance== 4)	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _COMBAT_IDLESTEP_ROCKET_01",stance  );

	if (stance== 5) m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _STEALTH_IDLESTEP_MG_01",stance  );
	if (stance== 6) m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _STEALTH_IDLESTEP_NW_01",stance  );
	if (stance== 7) m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _STEALTH_IDLESTEP_PISTOL_01",stance );
	if (stance== 8) m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _STEALTH_IDLESTEP_RIFLE_01",stance  );
	if (stance== 9) m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _STEALTH_IDLESTEP_ROCKET_01",stance  );

	if (stance==10)	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _CROUCH_IDLESTEP_MG_01",stance  );
	if (stance==11)	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _CROUCH_IDLESTEP_NW_01",stance  );
	if (stance==12)	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _CROUCH_IDLESTEP_PISTOL_01",stance );
	if (stance==13)	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _CROUCH_IDLESTEP_RIFLE_01",stance  );
	if (stance==14)	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _CROUCH_IDLESTEP_ROCKET_01",stance  );

	if (stance==15)	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"Stance: %02d _RELAXED_IDLESTEP_NW_01",stance  );
	ypos+=35;

	static f32 fTurnAngle=0;
	f32 fMaxStepDistance = caps.m_vMaxVelocity.GetLength()*caps.m_fFastDuration;
	f32 fDesiredDistance = caps.m_vMaxVelocity.GetLength()*caps.m_fFastDuration*g_StepScale;
	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"StepScale: %f   fMaxStepDistance: %f  fDesiredDistance: %f",g_StepScale,fMaxStepDistance,fDesiredDistance );	
	ypos+=15;

	float color2[4] = {0,1,0,1};
	char name[5];	const char* bc = (const char*)(&caps.m_BlendType);
	name[0]=bc[0];	name[1]=bc[1];	name[2]=bc[2];	name[3]=bc[3]; name[4]=0;
	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_BlendType: %s",name);	ypos+=16;

	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_fSlowDuration: %f",caps.m_fSlowDuration );	ypos+=16;
	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_fFastDuration: %f",caps.m_fFastDuration );	ypos+=16;

	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_fFastTurnLeft:  %f",caps.m_fFastTurnLeft *caps.m_fFastDuration );	ypos+=16;
	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_fFastTurnRight: %f",caps.m_fFastTurnRight*caps.m_fFastDuration );	ypos+=16;
	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_fSlowTurnLeft:  %f",caps.m_fSlowTurnLeft *caps.m_fSlowDuration );	ypos+=16;
	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_fSlowTurnRight: %f",caps.m_fSlowTurnRight*caps.m_fSlowDuration );	ypos+=16;

	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_vMinVelocity: %f %f  Speed: %f   StepDistance: %f",caps.m_vMinVelocity.x,caps.m_vMinVelocity.y, caps.m_vMinVelocity.GetLength(), caps.m_vMinVelocity.GetLength()*caps.m_fSlowDuration*g_StepScale );	ypos+=16;
	m_renderer->Draw2dLabel(12,ypos,1.6f,color2,false,"caps.m_vMaxVelocity: %f %f  Speed: %f   StepDistance: %f",caps.m_vMaxVelocity.x,caps.m_vMaxVelocity.y, caps.m_vMaxVelocity.GetLength(), caps.m_vMaxVelocity.GetLength()*caps.m_fFastDuration*g_StepScale );	ypos+=16;
	ypos+=20;

	//standing idle
	if (m_State==STATE_STAND && numAnimsLayer0<=1)
	{
		AParams.m_fInterpolation = 0;
		AParams.m_fTransTime=0.8312f;  //transition-time in seconds

		//blend into idle after walk-stop 
		AParams.m_nFlags = CA_LOOP_ANIMATION|CA_START_AFTER;
		AParams.m_fTransTime	=	0.15f;
		AParams.m_fKeyTime		=	-1; //use keytime of previous motion to start this motion

		if (stance==0) strAnimName="combat_step_mg_idle_01";	
		if (stance==1) strAnimName="combat_step_nw_idle_01";	
		if (stance==2) strAnimName="combat_step_pistol_idle_01";	
		if (stance==3) strAnimName="combat_step_rifle_idle_01";	
		if (stance==4) strAnimName="combat_step_rocket_idle_01";	

		if (stance==5) strAnimName="stealth_step_mg_idle_01";	
		if (stance==6) strAnimName="stealth_step_nw_idle_01";	
		if (stance==7) strAnimName="stealth_step_pistol_idle_01";	
		if (stance==8) strAnimName="stealth_step_rifle_idle_01";	
		if (stance==9) strAnimName="stealth_step_rocket_idle_01";	

		if (stance==10) strAnimName="crouch_step_mg_idle_01";	
		if (stance==11) strAnimName="crouch_step_nw_idle_01";	
		if (stance==12) strAnimName="crouch_step_pistol_idle_01";	
		if (stance==13) strAnimName="crouch_step_rifle_idle_01";	
		if (stance==14) strAnimName="crouch_step_rocket_idle_01";	

		if (stance==15) strAnimName="relaxed_step_nw_idle_01";	

		pISkeletonAnim->StartAnimation( strAnimName,0, 0,0, AParams);

		g_MoveDirectionWorldIS.start	= m_AnimatedCharacter.t+Vec3(0,0,0.01f);
		g_MoveDirectionWorldIS.end		= m_AnimatedCharacter.t+Vec3(0,0,0.01f) + StrafeVectorWorldIS*10.0f;

		LastIdlePos = m_AnimatedCharacter.t;


		Vec3 blend;
		Vec3 forward = m_AnimatedCharacter.GetColumn1();
		Vec3 desiredforward = StrafeVectorWorldIS;
		fTurnAngle	=	Ang3::CreateRadZ(forward,desiredforward);
		for (f32 t=0.0f; t<1.0f; t+=0.01f )
		{
			blend.SetSlerp(forward,desiredforward,t);	
			pAuxGeom->DrawLine( wpos+Vec3(0,0,0.01f),RGBA8(0x00,0x00,0x00,0x00), wpos+blend+Vec3(0,0,0.01f),RGBA8(0x7f,0x7f,0xff,0x00) );
		}

		m_State=STATE_STAND;
		m_Stance=0;
	}



	//make a rotation step
	if (m_key_R && m_State==STATE_STAND && numAnimsLayer0<=1)
	{
		AParams.m_fInterpolation = 0;

		AParams.m_nFlags							= CA_REPEAT_LAST_KEY; //CA_LOOP_ANIMATION;
		AParams.m_fTransTime					=	0.20f;	//start immediately 
		AParams.m_fKeyTime						=	-1;			//use keytime of previous motion to start this motion
		AParams.m_fPlaybackSpeed			= 1.0f;
		if (stance==0)	strAnimName="_COMBAT_IDLESTEPROTATE_MG_01";
		if (stance==1)	strAnimName="_COMBAT_IDLESTEPROTATE_NW_01";
		if (stance==2)	strAnimName="_COMBAT_IDLESTEPROTATE_PISTOL_01";
		if (stance==3)	strAnimName="_COMBAT_IDLESTEPROTATE_RIFLE_01";
		if (stance==4)	strAnimName="_COMBAT_IDLESTEPROTATE_ROCKET_01";

		if (stance==5) strAnimName="_STEALTH_IDLESTEPROTATE_MG_01";
		if (stance==6)	strAnimName="_STEALTH_IDLESTEPROTATE_NW_01";
		if (stance==7)	strAnimName="_STEALTH_IDLESTEPROTATE_PISTOL_01";
		if (stance==8)	strAnimName="_STEALTH_IDLESTEPROTATE_RIFLE_01";
		if (stance==9)	strAnimName="_STEALTH_IDLESTEPROTATE_ROCKET_01";

		if (stance==10)	strAnimName="_CROUCH_IDLESTEPROTATE_MG_01";
		if (stance==11)	strAnimName="_CROUCH_IDLESTEPROTATE_NW_01";
		if (stance==12)	strAnimName="_CROUCH_IDLESTEPROTATE_PISTOL_01";
		if (stance==13)	strAnimName="_CROUCH_IDLESTEPROTATE_RIFLE_01";
		if (stance==14)	strAnimName="_CROUCH_IDLESTEPROTATE_ROCKET_01";

		if (stance==15)	strAnimName="_RELAXED_IDLESTEPROTATE_NW_01";

		pISkeletonAnim->StartAnimation( strAnimName,0, 0,0, AParams);

		AParams.m_nFlags = CA_LOOP_ANIMATION|CA_START_AFTER;
		AParams.m_fTransTime	=	0.20f;   //average time for transition 
		AParams.m_fKeyTime		=	-1;			//use keytime of previous motion to start this motion
		AParams.m_fPlaybackSpeed = 1.0f;
		if (stance==0) strAnimName="combat_step_mg_idle_01";
		if (stance==1) strAnimName="combat_step_nw_idle_01";
		if (stance==2) strAnimName="combat_step_pistol_idle_01";
		if (stance==3) strAnimName="combat_step_rifle_idle_01";
		if (stance==4) strAnimName="combat_step_rocket_idle_01";

		if (stance==5) strAnimName="stealth_step_mg_idle_01";
		if (stance==6) strAnimName="stealth_step_nw_idle_01";
		if (stance==7) strAnimName="stealth_step_pistol_idle_01";
		if (stance==8) strAnimName="stealth_step_rifle_idle_01";
		if (stance==9) strAnimName="stealth_step_rocket_idle_01";

		if (stance==10) strAnimName="crouch_step_mg_idle_01";
		if (stance==11) strAnimName="crouch_step_nw_idle_01";
		if (stance==12) strAnimName="crouch_step_pistol_idle_01";
		if (stance==13) strAnimName="crouch_step_rifle_idle_01";
		if (stance==14) strAnimName="crouch_step_rocket_idle_01";

		if (stance==15) strAnimName="relaxed_step_nw_idle_01";

		pISkeletonAnim->StartAnimation( strAnimName,0, 0,0, AParams);

		g_MoveDirectionWorldIS.start	= m_AnimatedCharacter.t+Vec3(0,0,0.01f);
		g_MoveDirectionWorldIS.end		= m_AnimatedCharacter.t+Vec3(0,0,0.01f) + StrafeVectorWorldIS*10.0f;

		m_State=STATE_STAND;
		m_Stance=0;
	}


	//make a direction step	
	if (m_key_W && m_State==STATE_STAND && numAnimsLayer0<=1)
	{
		AParams.m_fInterpolation = 0;

		AParams.m_nFlags							= CA_REPEAT_LAST_KEY; //CA_LOOP_ANIMATION;
		AParams.m_fTransTime					=	0.20f;	//start immediately 
		AParams.m_fKeyTime						=	-1;			//use keytime of previous motion to start this motion
		AParams.m_fPlaybackSpeed			= 1.0f;

		if (stance==0)	strAnimName="_COMBAT_IDLESTEP_MG_01";
		if (stance==1)	strAnimName="_COMBAT_IDLESTEP_NW_01";
		if (stance==2)	strAnimName="_COMBAT_IDLESTEP_PISTOL_01";
		if (stance==3)	strAnimName="_COMBAT_IDLESTEP_RIFLE_01";
		if (stance==4)	strAnimName="_COMBAT_IDLESTEP_ROCKET_01";

		if (stance==5)	strAnimName="_STEALTH_IDLESTEP_MG_01";
		if (stance==6)	strAnimName="_STEALTH_IDLESTEP_NW_01";
		if (stance==7)	strAnimName="_STEALTH_IDLESTEP_PISTOL_01";
		if (stance==8)	strAnimName="_STEALTH_IDLESTEP_RIFLE_01";
		if (stance==9)	strAnimName="_STEALTH_IDLESTEP_ROCKET_01";

		if (stance==10)  strAnimName="_CROUCH_IDLESTEP_MG_01";
		if (stance==11)	strAnimName="_CROUCH_IDLESTEP_NW_01";
		if (stance==12)	strAnimName="_CROUCH_IDLESTEP_PISTOL_01";
		if (stance==13)	strAnimName="_CROUCH_IDLESTEP_RIFLE_01";
		if (stance==14)	strAnimName="_CROUCH_IDLESTEP_ROCKET_01";

		if (stance==15)	strAnimName="_RELAXED_IDLESTEP_NW_01";

		pISkeletonAnim->StartAnimation( strAnimName,0, 0,0, AParams);

		AParams.m_nFlags = CA_LOOP_ANIMATION|CA_START_AFTER;
		AParams.m_fTransTime	=	0.20f;   //average time for transition 
		AParams.m_fKeyTime		=	-1;			//use keytime of previous motion to start this motion
		AParams.m_fPlaybackSpeed = 1.0f;

		if (stance==0)	strAnimName="combat_step_mg_idle_01";
		if (stance==1)	strAnimName="combat_step_nw_idle_01";
		if (stance==2)	strAnimName="combat_step_pistol_idle_01";
		if (stance==3)	strAnimName="combat_step_rifle_idle_01";
		if (stance==4)	strAnimName="combat_step_rocket_idle_01";

		if (stance==5)	strAnimName="stealth_step_mg_idle_01";
		if (stance==6)	strAnimName="stealth_step_nw_idle_01";
		if (stance==7)	strAnimName="stealth_step_pistol_idle_01";
		if (stance==8)	strAnimName="stealth_step_rifle_idle_01";
		if (stance==9)	strAnimName="stealth_step_rocket_idle_01";

		if (stance==10)	strAnimName="crouch_step_mg_idle_01";
		if (stance==11)	strAnimName="crouch_step_nw_idle_01";
		if (stance==12)	strAnimName="crouch_step_pistol_idle_01";
		if (stance==13)	strAnimName="crouch_step_rifle_idle_01";
		if (stance==14)	strAnimName="crouch_step_rocket_idle_01";

		if (stance==15)	strAnimName="relaxed_step_nw_idle_01";

		pISkeletonAnim->StartAnimation( strAnimName,0, 0,0, AParams);

		g_MoveDirectionWorldIS.start	= m_AnimatedCharacter.t+Vec3(0,0,0.01f);
		g_MoveDirectionWorldIS.end		= m_AnimatedCharacter.t+Vec3(0,0,0.01f) + StrafeVectorWorldIS*10.0f;

		m_State=STATE_STAND;
		m_Stance=0;
	}


	pAuxGeom->DrawLine( g_MoveDirectionWorldIS.start,RGBA8(0xff,0xff,0xff,0x00), g_MoveDirectionWorldIS.end, RGBA8(0x00,0xff,0x00,0x00) );


	f32 fTravelAngle = -atan2f(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y);

	m_renderer->Draw2dLabel(12,ypos,2.0f,color1,false,"fTurnAngle: %f   fTravelAngle: %f   fDesiredDistance: %f",fTurnAngle,fTravelAngle,fDesiredDistance);	
	ypos+=40;

	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelAngle, fTravelAngle,			FrameTime) ;
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelSpeed,			0,						FrameTime) ;
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TravelDist,  fDesiredDistance, FrameTime) ;

	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnSpeed,		    0,		        FrameTime) ;
	pISkeletonAnim->SetDesiredMotionParam(eMotionParamID_TurnAngle,		fTurnAngle,       FrameTime) ;

	//pISkeleton->SetBlendSpaceOverride(eMotionParamID_TravelAngle, -atan2f(LocalStrafeAnimIS.x,LocalStrafeAnimIS.y), 1) ;
	//pISkeleton->SetBlendSpaceOverride(eMotionParamID_TravelDistScale, g_StepScale, 1) ;

	//	pISkeleton->SetBlendSpaceOverride(eMotionParamID_TravelDist, 2.0f, 1); //this will  overwrite the scale value
	pISkeletonAnim->SetIWeight(0, 0 );


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

