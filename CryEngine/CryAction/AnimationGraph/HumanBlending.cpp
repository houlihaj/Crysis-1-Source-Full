#include "StdAfx.h"
#include "HumanBlending.h"
#include "IAnimationGraph.h"
#include "AnimationGraphCVars.h"

// Avoid conflict with g_YLine in CryAnimation.
f32 g_YLine2;


ILINE f32 GetYaw( const Quat& quat)
{
	Vec3 zaxis	= quat.GetColumn2();
	Vec3 xaxis0 = Vec3(0,1,0);
	Vec3 xaxis1 = quat.GetColumn1(); xaxis1.z=0; xaxis1.Normalize();
	Vec3 cross	= xaxis0%xaxis1;
	f32 sign		= (cross.z<0) ? -1.0f : 1.0f;
	f32 yaw			= atan2f( sign*cross.GetLength(), xaxis0|xaxis1 );
	return yaw;
}

SAnimationBlendingParams *CHumanBlending::Update(IEntity *pIEntity,  Vec3 DesiredBodyDirection, Vec3 DesiredMoveDirection, f32 fDesiredMovingSpeed2)
{
	m_params.m_yawAngle = 0; 
	m_params.m_speed = 0; 
	m_params.m_strafeParam = 0; 
	m_params.m_turnAngle = 0;
	m_params.m_diffBodyMove = 0;
	m_params.m_fBlendedDesiredSpeed = 0;

	IRenderer* pIRenderer = gEnv->pRenderer;
	IRenderAuxGeom*	g_pAuxGeom = pIRenderer->GetIRenderAuxGeom();
	g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );


	ICharacterInstance* pICharacterInstance = pIEntity->GetCharacter(0);
	if (!pICharacterInstance)
		return &m_params;

	ISkeletonAnim* pISkeleton = pICharacterInstance->GetISkeletonAnim();

	//one day, this will be a global solution
	f32 AvrgFrameTime = pICharacterInstance->GetAverageFrameTime();

/*
	//TEST: fix the Turn/Strafe problem
	f32 dot = DesiredBodyDirection|DesiredMoveDirection;
	if (dot>0.7f)
		DesiredBodyDirection = DesiredMoveDirection;
	if (dot<-0.7f)
		DesiredBodyDirection = -DesiredMoveDirection;
	SmoothCD(m_vSmoothFinalDesiredBodyDir, m_vSmoothFinalDesiredBodyDirRate, AvrgFrameTime, DesiredBodyDirection, 0.20f);
	DesiredBodyDirection=m_vSmoothFinalDesiredBodyDir;
*/


	/*
	if (CAnimationGraphCVars::Get().m_AverageTravelSpeed == 0)
	{
		AvrgFrameTime *= 8.0f;
	}
	*/

	float fColor2[4] = {1,0,0,1};
	//pIRenderer->Draw2dLabel( 1,g_YLine2, 1.3f, fColor2, false,"AvrgFrameTime: %f",AvrgFrameTime );	
	//g_YLine2+=16.0f;


//-----------------------------------------------------------------------------
/*
	uint32 numEntries = m_arrDesiredSpeedSmoothing.size();
	for (int32 i=(numEntries-2); i>-1; i--)
		m_arrDesiredSpeedSmoothing[i+1] = m_arrDesiredSpeedSmoothing[i];
	m_arrDesiredSpeedSmoothing[0] = fDesiredMovingSpeed; 

	//get smoothed SpeedMSec
	uint32 numAvrgFrames = 1;

	if (CAnimationGraphCVars::Get().m_AverageTravelSpeed != 0)
	{
		if (AvrgFrameTime)
		{
			numAvrgFrames = uint32(0.95f/AvrgFrameTime+0.5f); //average the frame-times for a certain time-period (sec)
			if (numAvrgFrames>numEntries)	numAvrgFrames=numEntries;
			if (numAvrgFrames<1)	numAvrgFrames=1;
		}
	}




	f32 AverageSpeedMSec=0;
	for (uint32 i=0; i<numAvrgFrames; i++)	
		AverageSpeedMSec += m_arrDesiredSpeedSmoothing[i];
	AverageSpeedMSec /= numAvrgFrames;

	m_params.m_fBlendedDesiredSpeed=AverageSpeedMSec;
	*/
	m_params.m_fBlendedDesiredSpeed = fDesiredMovingSpeed2;

//-----------------------------------------------------------


//	float fColor2[4] = {1,0,0,1};
//	pIRenderer->Draw2dLabel( 1,g_YLine2, 1.3f, fColor2, false,"DesiredMoveDirection: %f%f%f",DesiredMoveDirection.x,DesiredMoveDirection.y,DesiredMoveDirection.z );	
//	g_YLine2+=16.0f;

	Quat rot = pIEntity->GetRotation();
	Vec3 UpVector = rot.GetColumn2();
	Vec3 pos = pIEntity->GetWorldPos(); //-Vec3(0,0,0.75f);
	//g_pAuxGeom->DrawLine( pos,RGBA8(0xff,0xff,0xff,0x00),  pos+UpVector*20.0f,RGBA8(0x00,0x00,0x00,0x00) );

	Vec3 absCurrentBodyDirection(rot * Vec3(0,1,0));
	Vec3 absCurrentMoveDirection(rot * pISkeleton->GetCurrentVelocity());
	//g_pAuxGeom->DrawLine( pos,RGBA8(0x3f,0x3f,0x3f,0x00),  pos+(m_absCurrentBodyDirection)*10.0f,RGBA8(0x00,0xff,0x00,0x00) );


//------------------------------------------------------------------------

	uint32 numANMD=m_arrAbsDesiredBodyDirection.size();
	for (int32 i=(numANMD-2); i>-1; i--)
		m_arrAbsDesiredBodyDirection[i+1] = m_arrAbsDesiredBodyDirection[i];
	m_arrAbsDesiredBodyDirection[0] = DesiredBodyDirection;

	//smooth body-vector
	uint32 avrg_dir = (uint32)(0.30f/AvrgFrameTime+0.5f);  //average the PVDir of the 0.20 seconds
	if (avrg_dir>numANMD)	avrg_dir=numANMD;
	if (avrg_dir<1)	avrg_dir=1;

	Vec3 AvrgAbsDesiredBodyDirection(ZERO);
	for (uint32 i=0; i<avrg_dir; i++)	
		AvrgAbsDesiredBodyDirection += m_arrAbsDesiredBodyDirection[i];
	AvrgAbsDesiredBodyDirection /= avrg_dir;
	AvrgAbsDesiredBodyDirection.NormalizeSafe(ZERO);

	AvrgAbsDesiredBodyDirection = DesiredBodyDirection;

	//body-radiant 
	Vec3 bforward(absCurrentBodyDirection.GetNormalized());
	Vec3 bup(UpVector);
	Vec3 bright(bforward % bup);	

	Matrix33 currBodyDirectionMatrix=Matrix33::CreateFromVectors(bforward%bup,bforward,bup);	
//	g_pAuxGeom->DrawLine(pos,RGBA8(0xff,0x00,0x00,0x00), pos+currBodyDirectionMatrix.GetColumn0(),RGBA8(0xff,0x00,0x00,0x00) );
//	g_pAuxGeom->DrawLine(pos,RGBA8(0x00,0xff,0x00,0x00), pos+currBodyDirectionMatrix.GetColumn1(),RGBA8(0x00,0xff,0x00,0x00) );
//	g_pAuxGeom->DrawLine(pos,RGBA8(0x00,0x00,0xff,0x00), pos+currBodyDirectionMatrix.GetColumn2(),RGBA8(0x00,0x00,0xff,0x00) );

//	g_pAuxGeom->DrawLine(pos,RGBA8(0xff,0x00,0x00,0x00), pos+bright,RGBA8(0xff,0x00,0x00,0x00) );
//	g_pAuxGeom->DrawLine(pos,RGBA8(0x00,0xff,0x00,0x00), pos+bforward,RGBA8(0x00,0xff,0x00,0x00) );
//	g_pAuxGeom->DrawLine(pos,RGBA8(0x00,0x00,0xff,0x00), pos+bup,RGBA8(0x00,0x00,0xff,0x00) );

	//Vec3 newBodyDirLocal=currBodyDirectionMatrix.GetInverted() * AvrgAbsDesiredBodyDirection;
	//f32 body_radiant = cry_atan2f(-newBodyDirLocal.x,newBodyDirLocal.y);// - StrafeLeftRight*gf_PI;
  
	Quat YawDelta = Quat::CreateRotationV0V1(bforward,DesiredBodyDirection );
	f32 body_radiant = GetYaw(YawDelta); 
	if (fabsf(body_radiant)<0.01f)
		body_radiant = 0;

//	pIRenderer->Draw2dLabel( 1,g_YLine2, 1.3f, fColor2, false,"body_radiant: %f",body_radiant );	g_YLine2+=16.0f;
//	pIRenderer->Draw2dLabel( 1,g_YLine2, 1.3f, fColor2, false,"body_pos: %f %f %f",pos.x,pos.y,pos.z );	g_YLine2+=16.0f;
//	pIRenderer->Draw2dLabel( 1,g_YLine2, 1.3f, fColor2, false,"AvrgAbsDesiredBodyDirection: %f %f %f",AvrgAbsDesiredBodyDirection.x,AvrgAbsDesiredBodyDirection.y,AvrgAbsDesiredBodyDirection.z );	g_YLine2+=16.0f;
//	g_pAuxGeom->DrawLine( pos+Vec3(0,0,1.3f),RGBA8(0x3f,0x3f,0x00,0x00), pos+Vec3(0,0,1.3f)+(AvrgAbsDesiredBodyDirection*5.0f),RGBA8(0xff,0x00,0x00,0x00) );
//	g_pAuxGeom->DrawLine( pos+Vec3(0,0,1.3f),RGBA8(0x3f,0x3f,0x00,0x00), pos+Vec3(0,0,1.3f)+(DesiredBodyDirection*5.0f),RGBA8(0xff,0x00,0x00,0x00) );


//----------------------------------------------------

	Vec3 DesiredMoveDirectionN=DesiredMoveDirection;
	//DesiredMoveDirectionN.x	=	DesiredMoveDirection.x;
	//DesiredMoveDirectionN.y	= -DesiredMoveDirection.y;
	//DesiredMoveDirectionN.z	= 0.0f;
	if (DesiredMoveDirection|DesiredMoveDirection)
		DesiredMoveDirectionN.Normalize();
	
	numANMD=m_arrAbsDesiredMoveDirection.size();
	for (int32 i=(numANMD-2); i>-1; i--)
		m_arrAbsDesiredMoveDirection[i+1] = m_arrAbsDesiredMoveDirection[i];
	m_arrAbsDesiredMoveDirection[0] = DesiredMoveDirectionN;

	//smooth body-vector
	avrg_dir = (uint32)(0.50f/AvrgFrameTime+0.5f);  //average the PVDir of the 0.20 seconds
	if (avrg_dir>numANMD)	avrg_dir=numANMD;
	if (avrg_dir<1)	avrg_dir=1;


	//	float fColor2[4] = {1,0,0,1};
//	pIRenderer->Draw2dLabel( 1,g_YLine2, 1.3f, fColor2, false,"AvrgFrameTime: %f",AvrgFrameTime );	
//	g_YLine2+=16.0f;
//	pIRenderer->Draw2dLabel( 1,g_YLine2, 1.3f, fColor2, false,"avrg_dir: %d",avrg_dir );	
//	g_YLine2+=16.0f;
	

	Vec3 AvrgAbsDesiredMoveDirection(ZERO);
	for (uint32 i=0; i<avrg_dir; i++)	
		AvrgAbsDesiredMoveDirection += m_arrAbsDesiredMoveDirection[i];
	AvrgAbsDesiredMoveDirection /= avrg_dir;
	//AvrgAbsDesiredMoveDirection.NormalizeSafe(ZERO);

	AvrgAbsDesiredMoveDirection = DesiredMoveDirection;

//	g_pAuxGeom->DrawLine( pos+Vec3(0,0,1.4f),RGBA8(0x3f,0x3f,0x00,0x00), (AvrgAbsDesiredMoveDirection*2.0f)+pos+Vec3(0,0,1.4f),RGBA8(0xff,0xff,0x00,0x00) );
//	g_pAuxGeom->DrawLine( pos+Vec3(0,0,1.31f),RGBA8(0x3f,0x3f,0x00,0x00), (DesiredMoveDirection*2.0f)+pos+Vec3(0,0,1.31f),RGBA8(0xff,0xff,0x00,0x00) );


	//move-radiant 
	Vec3 mforward=absCurrentMoveDirection.GetNormalizedSafe(bforward);
	Vec3 mup=UpVector;
	Vec3 mright=mforward % mup;
	Matrix33 currMoveDirectionMatrix(Matrix33::CreateFromVectors(mforward%mup,mforward,mup));
	Vec3 newMoveDirLocal(currMoveDirectionMatrix.GetInverted() * AvrgAbsDesiredMoveDirection);
	f32 move_radiant = cry_atan2f(-newMoveDirLocal.x,newMoveDirLocal.y);// - StrafeLeftRight*gf_PI;


	//uint32 LookIK	= m_pCharPanel_Animation->GetLookIK();

	int32 HumanBledingDebug = CAnimationGraphCVars::Get().m_agHumanBlending;

	if (HumanBledingDebug)
	{
		g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
		g_pAuxGeom->DrawLine(pos,RGBA8(0x3f,0x3f,0x3f,0x00), pos+DesiredBodyDirection*10.0f,RGBA8(0xff,0x00,0x00,0x00) );
		g_pAuxGeom->DrawLine(pos,RGBA8(0x3f,0x3f,0x3f,0x00), pos+DesiredMoveDirection*10.0f,RGBA8(0xff,0xff,0x00,0x00) );



		g_pAuxGeom->DrawLine(pos,RGBA8(0x3f,0x3f,0x3f,0x00), pos+absCurrentBodyDirection*4.0f,RGBA8(0x3f,0xff,0x3f,0x00) );
		g_pAuxGeom->DrawLine(pos,RGBA8(0xff,0x00,0x00,0x00), pos+(currBodyDirectionMatrix * Matrix33::CreateRotationZ(body_radiant)).GetColumn(1)*6,RGBA8(0xff,0xff,0xff,0x00) );

		if (body_radiant>0)
		{
			for ( f32 t=0.0f; t<body_radiant; t+=(body_radiant/100.0f) )
			{
				Matrix33 m33(Matrix33::CreateRotationZ(t)*currBodyDirectionMatrix);
				g_pAuxGeom->DrawLine(pos,RGBA8(0x00,0x00,0x00,0x00), pos+m33.GetColumn(1),RGBA8(0xff,0x00,0x00,0x00) );
			}
		}
		else
		{
			for ( f32 t=0.0f; t>body_radiant; t+=(body_radiant/100.0f) )
			{
				Matrix33 m33(Matrix33::CreateRotationZ(t)*currBodyDirectionMatrix);
				g_pAuxGeom->DrawLine(pos,RGBA8(0x00,0x00,0x00,0x00), pos+m33.GetColumn(1),RGBA8(0xff,0x00,0x00,0x00) );
			}
		}
		//--------------------------------------------------------------------------------------------------------

		if (move_radiant>0)
		{
			for ( f32 t=0.0f; t<move_radiant; t+=(move_radiant/100.0f) )
			{
				Matrix33 m33(Matrix33::CreateRotationZ(t)*currMoveDirectionMatrix);
				g_pAuxGeom->DrawLine(pos,RGBA8(0x00,0x00,0x00,0x00), pos+m33.GetColumn(1),RGBA8(0xff,0xff,0x00,0x00) );
			}
		}
		else
		{
			for ( f32 t=0.0f; t>move_radiant; t+=(move_radiant/100.0f) )
			{
				Matrix33 m33(Matrix33::CreateRotationZ(t)*currMoveDirectionMatrix);
				g_pAuxGeom->DrawLine(pos,RGBA8(0x00,0x00,0x00,0x00), pos+m33.GetColumn(1),RGBA8(0xff,0xff,0x00,0x00) );
			}
		}

		//---------------------------------------
		//---   draw the path if the past
		//---------------------------------------
		uint32 numEntries=m_arrMotionPath.size();
		for (int32 i=(numEntries-2); i>-1; i--)
			m_arrMotionPath[i+1] = m_arrMotionPath[i];
		m_arrMotionPath[0] = pos; 

		AABB aabb;
		for (uint32 i=0; i<numEntries; i++)
		{
			aabb.min=Vec3(-0.01f,-0.01f,-0.01f)+m_arrMotionPath[i];
			aabb.max=Vec3(+0.01f,+0.01f,+0.01f)+m_arrMotionPath[i];
			g_pAuxGeom->DrawAABB(aabb,1, RGBA8(0x00,0x00,0xff,0x00),eBBD_Extremes_Color_Encoded );
		}

	}


	//---------------------------------------------------------------------
	//--- speed-up when running on a straight line, slow down in curves ---
	//---------------------------------------------------------------------
	f32 SpeedParam=1.0f-fabsf(body_radiant/1.5f);
	if (SpeedParam<-1) SpeedParam=-1;


	f32 LeftRightParam=body_radiant*1.40f;
	if (LeftRightParam<-1.0f)	LeftRightParam=-1.0f;
	if (LeftRightParam>+1.0f)	LeftRightParam=+1.0f;

	f32 delta = LeftRightParam-m_OldLeftRightParam;

//	f32 m_absCurrentMoveSpeed=pISkeleton->GetCurrentSpeed();
	f32 m_absCurrentMoveSpeed=pISkeleton->GetCurrentVelocity().GetLength();
	f32 maxdelta=(6.5f/0.00001f)*AvrgFrameTime;
	if (m_absCurrentMoveSpeed)
		maxdelta = (6.5f/m_absCurrentMoveSpeed)*AvrgFrameTime;

	if (delta>+maxdelta) delta=+maxdelta;
	if (delta<-maxdelta) delta=-maxdelta;
	LeftRightParam=m_OldLeftRightParam+delta;
	m_OldLeftRightParam=LeftRightParam;


	m_params.m_speed			=	SpeedParam;
	m_params.m_turnAngle	=	LeftRightParam;


	//------------------------------------------

	Vec3 AvrgAbsDesiredMoveDirection2=Vec3(ZERO);
	if (AvrgAbsDesiredMoveDirection|AvrgAbsDesiredMoveDirection)
		AvrgAbsDesiredMoveDirection2 = AvrgAbsDesiredMoveDirection.GetNormalized();

	f32 RadiantBodyMove;
	Vec3 FlatAvgDesiredBodyDir(AvrgAbsDesiredBodyDirection.x, AvrgAbsDesiredBodyDirection.y, 0);
	Vec3 FlatAvgDesiredMoveDir(AvrgAbsDesiredMoveDirection2.x, AvrgAbsDesiredMoveDirection2.y, 0);

	f32 dmydot					= (FlatAvgDesiredBodyDir | FlatAvgDesiredMoveDir);
	Vec3 dmycross				= (FlatAvgDesiredBodyDir % FlatAvgDesiredMoveDir);
	f32 dmysign					= (dmycross.z<0) ? -1.0f : 1.0f;
	RadiantBodyMove	= atan2f( dmysign*dmycross.GetLength(), dmydot );

	m_params.m_StrafeVector		= Matrix33::CreateRotationZ(RadiantBodyMove)*(Vec3(0,1,0)*FlatAvgDesiredMoveDir.GetLength()); 
	//m_params.m_StrafeVector.y	=-m_params.m_StrafeVector.y; 

	//g_pAuxGeom->DrawLine( pos,RGBA8(0x3f,0x3f,0x00,0x00), (m_params.m_StrafeVector*1.0f)+pos,RGBA8(0xff,0xff,0x00,0x00) );

//	float fColor2[4] = {1,0,0,1};
//	pIRenderer->Draw2dLabel( 1,g_YLine2, 1.3f, fColor2, false,"m_params.m_StrafeVector: %f %f",m_params.m_StrafeVector.x,m_params.m_StrafeVector.y );	
//	g_YLine2+=16.0f;

	m_params.m_strafeParam = RadiantBodyMove/gf_PI;
	//m_params.m_strafeParam = 0.0f;
	if (fabsf(m_params.m_strafeParam) < 0.003f)
		m_params.m_strafeParam = 0;

	m_params.m_diffBodyMove = 1.0f-fabsf(2*m_params.m_strafeParam );
	if (m_params.m_diffBodyMove<0)	
		m_params.m_diffBodyMove=0;


/*
	g_YLine2+=96.0f;
	float fColor2[4] = {1,0,0,1};
	pIRenderer->Draw2dLabel( 1,g_YLine2, 1.3f, fColor2, false,"diffBodyMove: %f    StrafeLR:%f  BlendRunStrafe:%f",diffBodyMove,StrafeLR,fBlendRunStrafe);	
	g_YLine2+=16.0f;
*/
	//float fColor[4] = {0,1,0,1};
	//pIRenderer->Draw2dLabel( 1,g_YLine2, 1.3f, fColor, false,"SpeedParam: %f   LeftRightParam: %f   CurrentSpeed: %f   Strafe: %f",SpeedBlend,TurnLeftRight,m_absCurrentMoveSpeed,StrafeParam );	
	//g_YLine2+=16.0f;


//---------------------------------------------------------------------

	if (IPhysicalEntity * pPE = pIEntity->GetPhysics())
	{
		pe_status_living livStat;
		pPE->GetStatus( &livStat );

		float angToNorm = AvrgAbsDesiredBodyDirection.Dot(livStat.groundSlope);
		angToNorm = cry_acosf(CLAMP(angToNorm, -1, 1));
		float angToGround = angToNorm - gf_PI*0.5f;
		float scaledAngle = RAD2DEG(angToGround)/22.0f;
		m_params.m_fUpDownParam = CLAMP( scaledAngle, -1, 1 );
	}

	//---------------------------------------------------------------------
	//---------------------------------------------------------------------
	//---------------------------------------------------------------------

	f32 t = (m_params.m_diffBodyMove*2)-0.5f;
	if (t<0)
		t=0;
	if (t>1)
		t=1;
	m_params.m_diffBodyMove = t*t;


//---------------------------------------------------------------------
	m_params.m_yawAngle=body_radiant;
	return &m_params;//body_radiant;
};
