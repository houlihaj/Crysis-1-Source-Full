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



void CSkeletonAnim::InitSkeletonAnim(CCharInstance* pInstance, CSkeletonPose* pSkeletonPose )
{
	m_pInstance					=	pInstance;
	m_pSkeletonPose			=	pSkeletonPose;
	m_CharEditMode = 0;
	m_ShowDebugText = 0;
	m_AnimationDrivenMotion=0;
	m_MirrorAnimation=0;

	m_IsAnimPlaying=0;
	m_ActiveLayer=0;

	m_desiredLocalLocation.SetIdentity();
	m_desiredArrivalDeltaTime = 0.0f;
	m_desiredTurnSpeedMultiplier = 1.0f;
	m_fDesiredTurnSpeedBlend = 1.0f;
	m_fDesiredTurnSpeedOffset = 0.0f;
	m_fDesiredTurnSpeedScale = 0.0f;
	
	for (int id = 0; id < eMotionParamID_COUNT; id++)
	{
		m_desiredMotionParam[id] = 0.0f;
		m_CharEditBlendSpaceOverrideEnabled[id] = false;
		m_CharEditBlendSpaceOverride[id] = 0.0f;
	}

	for (int i=0; i<numVIRTUALLAYERS; i++)
	{
		m_arrLayerSpeedMultiplier[i]=1.0f;
		m_arrLayerBlending[i]=0.0f;
		m_arrLayerBlendingTime[i]=0.0f;
	}

	m_pPreProcessCallback = 0;
	m_pPreProcessCallbackData = 0;

	m_pEventCallback = 0;
	m_pEventCallbackData = 0;

	m_TrackViewExclusive=0;
	m_bReinitializeAnimGraph = false;
	m_FuturePath = false;
	m_BlendedRoot.Init();

	m_ActiveLayer=0;

	for (int i=0; i<numVIRTUALLAYERS; i++)
	{
		m_arrLayerSpeedMultiplier[i]=1.0f;
		m_arrLayerBlending[i]=0.0f;
		m_arrLayerBlendingTime[i]=0.0f;
	}

	m_vAbsDesiredMoveDirectionSmooth		=Vec2(0,1);
	m_vAbsDesiredMoveDirectionSmoothRate=Vec2(0,1);
}


//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------

void CSkeletonAnim::ProcessAnimations( const QuatT& rPhysLocationCurr, const QuatTS& rAnimLocationCurr, uint32 OnRender )
{

	if (GetCharEditMode()==0)
	{
		int nCurrentFrameID = g_pCharacterManager->m_nUpdateCounter; 
		if ( m_pInstance->m_LastUpdateFrameID_Pre == nCurrentFrameID )
		{
			//multiple updates in the same frame can be a problem
			//const char* name = m_pInstance->m_pModel->GetModelFilePath();
			//g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,name,	"several pre-updates: FrameID: %x  Old: %x  Now: %x",nCurrentFrameID,m_pInstance->m_LastUpdateFrameID_PreType, OnRender);
			return;
		}
	}

	/*
	const char* mname = m_pInstance->GetFilePath();
	if ( strcmp(mname,"objects/characters/human/asian/rifleman_light/rifleman_light_standard.cdf")==0 )
	{
		uint32 Inst=(uint32)m_pInstance;
		float fC1[4] = {0,1,0,1};
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.6f, fC1, false,"Pre Update: Instance: %x OnRender: %x  Model: %s ",Inst,OnRender,mname );	
		g_YLine+=16.0f;
	} 
	*/

	ProcessAnimationUpdate();
	if (m_pSkeletonPose->m_bFullSkeletonUpdate)
		ProcessForwardKinematics( rPhysLocationCurr,  rAnimLocationCurr ); 	//DANGEROUS: character is not visible, so we do no update at all


	//check if the oldest animation in the queue is still needed
	for(uint32 nVLayerNo=0; nVLayerNo<numVIRTUALLAYERS; nVLayerNo++)
	{
		uint32 numAnimsInLayer = m_arrLayer_AFIFO[nVLayerNo].size();
		if (numAnimsInLayer==0) 
			continue;

		if (m_arrLayer_AFIFO[nVLayerNo][0].m_bRemoveFromQueue || m_arrLayer_AFIFO[nVLayerNo][0].m_fTransitionWeight==0)
		{
			uint32 na=m_arrLayer_AFIFO[nVLayerNo].size();
			UnloadAnimationAssets(m_arrLayer_AFIFO[nVLayerNo], 0);
			for (uint32 i=1; i<na; i++)	m_arrLayer_AFIFO[nVLayerNo][i-1]=m_arrLayer_AFIFO[nVLayerNo][i];
			m_arrLayer_AFIFO[nVLayerNo].pop_back();
		}
	}

	//----------------------------------------------------------------------
	//callback into the game to move and clamp the AC to the entity
	if (m_pPreProcessCallback)		
		(*m_pPreProcessCallback)(m_pInstance,m_pPreProcessCallbackData);
	//----------------------------------------------------------------------

/*
  QuatT relmove = GetRelMovement();
  QuatTS rAnimLocationNext=rAnimLocationCurr*relmove;
	rAnimLocationNext.q.Normalize();
	rAnimLocationNext.t += GetRelFootSlide();
*/
//	m_pSkeletonPose->SkeletonPostProcess2(rPhysLocationCurr, m_pInstance->m_RenderLocation, OnRender );

	//m_pSkeletonPose->SkeletonPostProcess2(rPhysLocationCurr, rAnimLocationCurr, OnRender );
	//	m_pSkeletonPose->SkeletonPostProcess2(rPhysLocationCurr, m_pInstance->m_AnimCharLocation, OnRender );

	m_pInstance->m_HadUpdate=1;

	m_pInstance->m_LastUpdateFrameID_PreType = OnRender;
	if (OnRender!=1)
		m_pInstance->m_LastUpdateFrameID_Pre = g_pCharacterManager->m_nUpdateCounter; //g_pIRenderer->GetFrameID(false);

//	m_BlendedRoot.m_fRelRotation=0;
//	m_BlendedRoot.m_vRelTranslation=Vec3(ZERO);
//	m_pSkeletonPose->m_FootAnchor.m_vFootSlide=Vec3(ZERO);

}


//------------------------------------------------------------------------
//---                        ANIMATION-UPDATE                          ---
//-----    we have to do this even if the character is not visible   -----
//------------------------------------------------------------------------
void CSkeletonAnim::ProcessAnimationUpdate()
{
	DEFINE_PROFILER_FUNCTION();

#ifdef VTUNE_PROFILE 
	g_pISystem->VTuneResume();
#endif


	m_pSkeletonPose->m_AimIK.m_AimDirection	=	Vec3(ZERO); 
	m_pSkeletonPose->m_AimIK.m_numActiveAnims=0;
	for (uint32 i=0; i<MAX_EXEC_QUEUE; i++)
	{
		m_pSkeletonPose->m_AimIK.m_AimInfo[i].m_fWeight		=0.0f;
		m_pSkeletonPose->m_AimIK.m_AimInfo[i].m_numAimPoses	=0;
		m_pSkeletonPose->m_AimIK.m_AimInfo[i].m_nGlobalAimID0=-1;
		m_pSkeletonPose->m_AimIK.m_AimInfo[i].m_nGlobalAimID1=-1;
		m_pSkeletonPose->m_AimIK.m_AimInfo[i].m_fAnimTime	=0.0f;
	}

	m_pSkeletonPose->m_LookIK.m_LookDirection	=	Vec3(ZERO); 
	m_BlendedRoot.m_fRelRotation=0;
	m_BlendedRoot.m_vRelTranslation=Vec3(ZERO);
	if (Console::GetInst().ca_NoAnim)
		return;	

	for(uint32 nVLayerNo=0; nVLayerNo<numVIRTUALLAYERS; nVLayerNo++)
	{
		uint32 numAnimsPerLayer=m_arrLayer_AFIFO[nVLayerNo].size();
		if (numAnimsPerLayer)
		{
			BlendManager( m_pInstance->m_fDeltaTime*m_arrLayerSpeedMultiplier[nVLayerNo],  m_arrLayer_AFIFO[nVLayerNo], nVLayerNo );
			if (Console::GetInst().ca_DebugText || m_ShowDebugText)
				BlendManagerDebug(  m_arrLayer_AFIFO[nVLayerNo], nVLayerNo );
		}
		LayerBlendManager( m_pInstance->m_fDeltaTime, nVLayerNo );
	}
}



//------------------------------------------------------------------------------------
//---                             SKELETON-UPDATE                                  ---
//---                  here we apply the motion-data to the bones                  ---
//---              (usually not necessary when character not visible)              ---
//------------------------------------------------------------------------------------
void CSkeletonAnim::ProcessForwardKinematics( const QuatT& rPhysLocationCurr, const QuatTS& rAnimLocationCurr )
{
	DEFINE_PROFILER_FUNCTION();

	//just for debugging
	extern uint32 g_AnimationUpdates; 	g_AnimationUpdates++;

	uint32 numJoints		= m_pSkeletonPose->m_AbsolutePose.size();
	uint32 nBufferSizeQ = numJoints*numTMPLAYERS*sizeof(QuatT);
	uint32 nBufferSizeS = numJoints*numTMPLAYERS*sizeof(Status4);
	QuatT* parrRelQuats = (QuatT*)alloca( nBufferSizeQ );
	Status4* parrControllerStatus = (Status4*)alloca( nBufferSizeS );

#ifdef _DEBUG
	for (uint32 i=0; i<(numJoints*numTMPLAYERS); i++ )
	{
		uint32 NAN32 = F32NAN;
		parrRelQuats[i].q.w		=	*(f32*)&NAN32;
		parrRelQuats[i].q.v.x	=	*(f32*)&NAN32;
		parrRelQuats[i].q.v.y	=	*(f32*)&NAN32;
		parrRelQuats[i].q.v.z	=	*(f32*)&NAN32;

		parrRelQuats[i].t.x		=	*(f32*)&NAN32;
		parrRelQuats[i].t.y		=	*(f32*)&NAN32;
		parrRelQuats[i].t.z		=	*(f32*)&NAN32;

		parrControllerStatus[i].o=0x33;
		parrControllerStatus[i].p=0x44;
		parrControllerStatus[i].s=0x55;
	}
#endif

	/*
	f32 zeroa=0;
	f32 naga=-2;

	Vec3 v32;
	Vec3r v64;

	Quat q32;
	Quatr q64;

	uint32 NAN32 = F32NAN;
	f32 fNAN32 = *(f32*)&NAN32; //NanTraits<f32>::GetNan();	
	f32 fTest1 = 2.0f * fNAN32 * 456.123435454f;
	f32 fTest2 = 2.0f / fNAN32 * 456.123435454f;
	f32 fTest3 = sqrtf(fNAN32);

	uint64 NAN64 = F64NAN;
	f64 fNAN64 = *(f64*)&NAN64;  // NanTraits<f64>::GetNan();	
	f64 dTest1 = 2.0 * fNAN64 * 456.123435454;
	f64 dTest2 = 2.0 / fNAN64 * 456.123435454;
	f64 dTest3 = sqrt(fNAN64);

	f64 dTest5 = sqrt(naga);  //caught by _EM_INVALID
	f64 dTest6 = acos_tpl(naga);  //caught by _EM_INVALID
	f64 dTest7 = 2.0 / zeroa; //caught by _EM_ZERODIVIDE
	*/


	if ( Console::GetInst().ca_DrawPositionPre &1) 
	{
		//draw the entity in blue
		Vec3 wpos = rPhysLocationCurr.t+Vec3(0.0f,0.0f,0.0f);
		g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
		static Ang3 angle(0,0,0);		angle += Ang3(0.01f,+0.03f,-0.07f);
		AABB aabb = AABB(Vec3(-0.05f,-0.05f,-0.05f),Vec3(+0.05f,+0.05f,+0.05f));
		OBB obb=OBB::CreateOBBfromAABB( Matrix33::CreateRotationXYZ(angle), aabb );
		g_pAuxGeom->DrawOBB(obb,wpos,1,RGBA8(0x00,0x00,0xff,0x00), eBBD_Extremes_Color_Encoded);

		Vec3 axisX = rPhysLocationCurr.q.GetColumn0();
		Vec3 axisY = rPhysLocationCurr.q.GetColumn1();
		Vec3 axisZ = rPhysLocationCurr.q.GetColumn2();
		g_pAuxGeom->DrawLine(wpos,RGBA8(0x7f,0x00,0x00,0x00), wpos+axisX,RGBA8(0x7f,0x00,0x00,0x00) );
		g_pAuxGeom->DrawLine(wpos,RGBA8(0x00,0x7f,0x00,0x00), wpos+axisY,RGBA8(0x00,0x7f,0x00,0x00) );
		g_pAuxGeom->DrawLine(wpos,RGBA8(0x00,0x00,0x7f,0x00), wpos+axisZ,RGBA8(0x00,0x00,0x7f,0x00) );

		Vec3 line = rPhysLocationCurr.q.GetColumn2()*5;

		g_pAuxGeom->DrawLine(wpos,RGBA8(0x00,0x00,0xff,0x00), wpos+line,RGBA8(0x00,0x00,0xff,0x00) );
	}

	if ( Console::GetInst().ca_DrawPositionPre &2)
	{ 
		//draw the animated character in red
		Vec3 wpos = rAnimLocationCurr.t+Vec3(0.0f,0.0f,0.0f);
		g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
		static Ang3 angle(0,0,0); 
		angle += Ang3(0.01f,-0.02f,-0.03f);
		AABB aabb = AABB(Vec3(-0.05f,-0.05f,-0.05f),Vec3(+0.05f,+0.05f,+0.05f));
		OBB obb=OBB::CreateOBBfromAABB( Matrix33::CreateRotationXYZ(angle), aabb );
		g_pAuxGeom->DrawOBB(obb,wpos,1,RGBA8(0xff,0x00,0x00,0x00), eBBD_Extremes_Color_Encoded);

		Vec3 axisX = rAnimLocationCurr.q.GetColumn0();
		Vec3 axisY = rAnimLocationCurr.q.GetColumn1();
		Vec3 axisZ = rAnimLocationCurr.q.GetColumn2();
		g_pAuxGeom->DrawLine(wpos,RGBA8(0xff,0x00,0x00,0x00), wpos+axisX,RGBA8(0xff,0x00,0x00,0x00) );
		g_pAuxGeom->DrawLine(wpos,RGBA8(0x00,0xff,0x00,0x00), wpos+axisY,RGBA8(0x00,0xff,0x00,0x00) );
		g_pAuxGeom->DrawLine(wpos,RGBA8(0x00,0x00,0xff,0x00), wpos+axisZ,RGBA8(0x00,0x00,0xff,0x00) );

		Vec3 line = rAnimLocationCurr.q.GetColumn2()*5;
		g_pAuxGeom->DrawLine(wpos,RGBA8(0xff,0x00,0x00,0x00), wpos+line,RGBA8(0xff,0x00,0x00,0x00) );
	}

//	float fColor[4] = {1,1,0,1};
//	QuatTS ac = m_pInstance->m_AnimCharLocation;
//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_AnimCharLocation: rot: (%f %f %f %f)  pos: (%f %f %f)",ac.q.w,ac.q.v.x,ac.q.v.y,ac.q.v.z,    ac.t.x,ac.t.y,ac.t.z );	
//	g_YLine+=16.0f;
//	const char* mname = m_pInstance->m_pModel->GetModelFilePath();
//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"rAnimLocationCurr: %f %f %f  Model:%s",rAnimLocationCurr.t.x,rAnimLocationCurr.t.y,rAnimLocationCurr.t.z, mname  );	
//	g_YLine+=16.0f;


	memset( &m_pSkeletonPose->m_arrControllerInfo[0].ops,0,numJoints*sizeof(Status4) );

	for(uint32 nVLayerNo=0; nVLayerNo<numVIRTUALLAYERS; nVLayerNo++)
	{
		uint32 numAnimsInLayer = m_arrLayer_AFIFO[nVLayerNo].size();

		uint32 numActiveAnims=0;
		for (uint32 a=0; a<numAnimsInLayer; a++)
		{
			numActiveAnims += m_arrLayer_AFIFO[nVLayerNo][a].m_bActivated;	
			if (m_arrLayer_AFIFO[nVLayerNo][a].m_bActivated==0)
				break;
		}

		if (numActiveAnims==0) 
			continue;

		if (nVLayerNo==0)
		{
			RMovement g_RootInfo[numTMPLAYERS]; //here we store root trajectory informations
			if (numActiveAnims==1) 
				m_BlendedRoot=m_pSkeletonPose->RPlayback( m_arrLayer_AFIFO[0][0], FINALREL, parrRelQuats, parrControllerStatus, g_RootInfo, rAnimLocationCurr.s );
			else
				m_BlendedRoot=m_pSkeletonPose->RTransition( &m_arrLayer_AFIFO[0][0], numActiveAnims,  parrRelQuats, parrControllerStatus, g_RootInfo, rAnimLocationCurr.s );
		}
		else
		{
			//higher layer
			if (numActiveAnims==1) 
				m_pSkeletonPose->Playback( m_arrLayer_AFIFO[nVLayerNo][0],nVLayerNo,FINALREL, parrRelQuats, parrControllerStatus, rAnimLocationCurr.s );
			else
				m_pSkeletonPose->Transition( &m_arrLayer_AFIFO[nVLayerNo][0], numActiveAnims, nVLayerNo, parrRelQuats, parrControllerStatus, rAnimLocationCurr.s );
		}


#ifdef _DEBUG
		//check if we have valid values in the skeleton-pose
		for (uint32 j=0; j<numJoints; j++)
		{
			Status4 s4 = m_pSkeletonPose->m_arrControllerInfo[j];
			assert(m_pSkeletonPose->m_RelativePose[j].q.IsValid());
			assert(m_pSkeletonPose->m_RelativePose[j].t.IsValid());
		}
#endif

#if (EXTREME_TEST==1)
		for (uint32 i=0; i<numJoints; i++)
		{
			Status4 s4 = m_pSkeletonPose->m_arrControllerInfo[i];
			if (s4.o==0x33)
			{
				assert(0);
				CryError("CryAnimation: rot-controller for joint nr. %d not initialized:  %s",i, m_pInstance->GetFilePath());
			}
			if (s4.p==0x44)
			{
				assert(0);
				CryError("CryAnimation: pos-controller for joint nr. %d not initialized:  %s",i, m_pInstance->GetFilePath());
			}

			QuatT qp=m_pSkeletonPose->m_RelativePose[i];
			uint32 valid = m_pSkeletonPose->m_RelativePose[i].t.IsValid();
			if (valid==0)
			{
				assert(0);
				CryError("CryAnimation: relative joint nr. %d is invalid after reading controllers:  %s",i, m_pInstance->GetFilePath());
			}
		}
#endif

	}


	uint32 nObjectType =	m_pInstance->m_pModel->m_ObjectType;
	if (nObjectType==CHR)
	{
		if (m_MirrorAnimation)
		{
			float fColor[4] = {0,1,0,1};
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 3.3f, fColor, false,"Partial Body Mirrored Animation"  );	g_YLine+=16.0f;
		}

		std::vector<QuatT>		m_AbsoluteQuatMirror; 
		std::vector<QuatT>		m_RelativeQuatMirror; 
		m_AbsoluteQuatMirror.resize(numJoints);
		m_RelativeQuatMirror.resize(numJoints);

		m_pSkeletonPose->m_AbsolutePose[0] = m_pSkeletonPose->m_RelativePose[0];
		for(uint32 i=1; i<numJoints; i++)
		{
			int32 p=m_pSkeletonPose->m_parrModelJoints[i].m_idxParent;
			m_pSkeletonPose->m_AbsolutePose[i] = m_pSkeletonPose->m_AbsolutePose[p] * m_pSkeletonPose->m_RelativePose[i];
		}

		m_AbsoluteQuatMirror[0]	= m_pSkeletonPose->m_AbsolutePose[0];
		m_RelativeQuatMirror[0] = m_AbsoluteQuatMirror[0];
		for(uint32 i=1; i<numJoints; i++)
		{
			if (m_MirrorAnimation==0)
				continue;
			int32 m = m_pSkeletonPose->m_pModelSkeleton->m_arrMirrorJoints[i];
			if (m>0)
			{
				Quat temp;
				temp.w	 = -m_pSkeletonPose->m_AbsolutePose[i].q.v.y;
				temp.v.x = -m_pSkeletonPose->m_AbsolutePose[i].q.v.z;
				temp.v.y = -m_pSkeletonPose->m_AbsolutePose[i].q.w;
				temp.v.z = -m_pSkeletonPose->m_AbsolutePose[i].q.v.x;
				m_AbsoluteQuatMirror[m].q=temp;

				m_AbsoluteQuatMirror[m].t.x		= -m_pSkeletonPose->m_AbsolutePose[i].t.x;
				m_AbsoluteQuatMirror[m].t.y		=  m_pSkeletonPose->m_AbsolutePose[i].t.y;
				m_AbsoluteQuatMirror[m].t.z		=  m_pSkeletonPose->m_AbsolutePose[i].t.z;

				int32 p=m_pSkeletonPose->m_parrModelJoints[m].m_idxParent;
				m_pSkeletonPose->m_RelativePose[m] = m_AbsoluteQuatMirror[p].GetInverted() * m_AbsoluteQuatMirror[m];
				m_pSkeletonPose->m_RelativePose[m].q.Normalize();
			}
		}


		//-----------------------------------------------------------------------------
		//-----------------------------------------------------------------------------
		//-----------------------------------------------------------------------------

		for (uint32 j=0; j<numJoints; j++)
		{
			if (m_pSkeletonPose->m_arrControllerInfo[j].p)
				m_pSkeletonPose->m_RelativePose[j].t *= rAnimLocationCurr.s;
		}
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------

	//f32 fColor[4] = { 0.0f, 0.5f, 1.0f, 1 };
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false, "m_BlendedRoot.m_fAllowMultilayerAnim: %f",m_BlendedRoot.m_fAllowMultilayerAnim);	g_YLine+=0x10;


	if ( Console::GetInst().ca_debugCaps )
	{
		int layer = 0;
		int animCount = GetNumAnimsInFIFO(layer);

		char* szMotionParamID[eMotionParamID_COUNT] = 
		{
			"TravelAngle", "TravelDistScale", "TravelSpeed", "TravelDist", "TravelSlope", "TurnSpeed", "TurnAngle", "Duration"
		};

		for (int i = 0; i < animCount; ++i)
		{
			f32 stride = f32(i * (eMotionParamID_COUNT + 1));
			CAnimation anim = GetAnimFromFIFO(layer, i);
			if (anim.m_bActivated)
			{
				const SLocoGroup& lmg = anim.m_LMG0;
				for (int id = 0; id < eMotionParamID_COUNT; ++id)
				{
					const MotionParamDesc& desc = lmg.m_params[id].desc;
					if (desc.m_nUsage != eMotionParamUsage_None)
					{
						if (!lmg.m_params[id].desc.m_bLocked)
						{
							const ColorF cCol(0.7f,1.0f,0.7f,1.0f);
							gEnv->pRenderer->Draw2dLabel(750, 100 + (stride + id) * 20, 1.3f, (float*)&cCol, false,	"%s [%3.2f %3.2f %3.2f %3.2f] %3.2f", szMotionParamID[id], desc.m_fMin, desc.m_fMinAsset, desc.m_fMaxAsset, desc.m_fMax, desc.m_fMaxChangeRate);
						}
						else
						{
							const ColorF cCol(1.0f,1.0f,0.7f,1.0f);
							gEnv->pRenderer->Draw2dLabel(750, 100 + (stride + id) * 20, 1.3f, (float*)&cCol, false,	"%s [%3.2f %3.2f %3.2f %3.2f] LOCK", szMotionParamID[id], desc.m_fMin, desc.m_fMinAsset, desc.m_fMaxAsset, desc.m_fMax);
						}
					}
					else
					{
						const ColorF cCol(1.0f,0.7f,0.7f,1.0f);
						gEnv->pRenderer->Draw2dLabel(750, 100 + (stride + id) * 20, 1.3f, (float*)&cCol, false,	"%s [ not supported ]", szMotionParamID[id]);
					}
				}
			}
		}
	}


	//--------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------
	//--------------------------------------------------------------------------------------
	/*
	m_parrModelJoints[0].m_DefaultAbsPose = m_parrModelJoints[0].m_DefaultRelPose;
	for (uint32 i=1; i<numJoints; i++)
	{
	int32 p = m_parrModelJoints[i].m_idxParent;
	m_parrModelJoints[i].m_DefaultAbsPose	= m_parrModelJoints[p].m_DefaultAbsPose * m_parrModelJoints[i].m_DefaultRelPose;
	}
	DrawDefaultSkeleton(Matrix34(rPhysLocationCurr));
	*/

	if (Console::GetInst().ca_ForceNullAnimation && m_pSkeletonPose->m_pModelSkeleton->IsHuman())
	{
		uint32 numJoints = m_pSkeletonPose->m_AbsolutePose.size();
		for (uint32 i=0; i<numJoints; i++)
			m_pSkeletonPose->m_RelativePose[i] = m_pSkeletonPose->m_pModelSkeleton->m_arrModelJoints[i].m_DefaultRelPose;
	}

	if (m_AnimationDrivenMotion || (m_AnimationDrivenMotion && m_pInstance->GetResetMode()) ) 
	{ 
		m_pSkeletonPose->m_RelativePose[0].t.x=0;
		m_pSkeletonPose->m_RelativePose[0].t.y=0;
		m_pSkeletonPose->m_RelativePose[0].t.z=0;
	}


#ifdef _DEBUG
	if ( (m_IsAnimPlaying&1) && m_AnimationDrivenMotion && nObjectType==CHR)
	{
		Quat q = m_pSkeletonPose->m_RelativePose[0].q;
		Vec3 t = m_pSkeletonPose->m_RelativePose[0].t;
		assert(q.IsEquivalent(IDENTITY,0.001f));
		assert(t.IsEquivalent(ZERO,0.001f));
	}
#endif

	if (m_pSkeletonPose->m_nForceSkeletonUpdate==0)
	{
		if (m_IsAnimPlaying==0 && nObjectType==CGA)
			m_pSkeletonPose->m_bFullSkeletonUpdate=0;	//DANGEROUS: if we don't play an animation, we don't update the skeleton
	}


	if (m_pSkeletonPose->m_bFullSkeletonUpdate) 
	{
		//------------------------------------------------------------------------------
		//--------                    Foot-Anchoring                              ------
		//------------------------------------------------------------------------------
		if (Console::GetInst().ca_FootAnchoring && (m_IsAnimPlaying&1))
		{
			uint32 HasOldPosition=0;
			uint32 CurrentFrameID = g_pCharacterManager->m_nUpdateCounter; //g_pIRenderer->GetFrameID(false);
			uint32 DeltaID = (CurrentFrameID-m_pSkeletonPose->m_FootAnchor.m_LastLegUpdate);
			m_pSkeletonPose->m_FootAnchor.m_LastLegUpdate=CurrentFrameID;
			if (DeltaID==1 || DeltaID==2)
				HasOldPosition=1;

			if (m_pSkeletonPose->m_pModelSkeleton->IsHuman())
			{
				m_pSkeletonPose->AbsoluteLegPosition(rAnimLocationCurr);
				if (HasOldPosition)
					m_pSkeletonPose->FootAnchoring( m_BlendedRoot.m_FootPlant, m_BlendedRoot.m_nFootBits, rAnimLocationCurr );
				m_pSkeletonPose->m_FootAnchor.m_LastLHeel=m_pSkeletonPose->m_FootAnchor.m_AbsoluteLHeel.t;	m_pSkeletonPose->m_FootAnchor.m_LastLToe=m_pSkeletonPose->m_FootAnchor.m_AbsoluteLToe0.t;
				m_pSkeletonPose->m_FootAnchor.m_LastRHeel=m_pSkeletonPose->m_FootAnchor.m_AbsoluteRHeel.t;	m_pSkeletonPose->m_FootAnchor.m_LastRToe=m_pSkeletonPose->m_FootAnchor.m_AbsoluteRToe0.t;
			}
		}
	}




#ifdef _DEBUG
	if ( (m_IsAnimPlaying&1) && m_AnimationDrivenMotion && nObjectType==CHR)
	{
		Quat q = m_pSkeletonPose->m_RelativePose[0].q;
		Vec3 t = m_pSkeletonPose->m_RelativePose[0].t;
		assert(q.IsEquivalent(IDENTITY,0.001f));
		assert(t.IsEquivalent(ZERO,0.001f));
	}
#endif


#ifdef VTUNE_PROFILE 
	g_pISystem->VTunePause();//VTPause();
#endif


}




//------------------------------------------------------------
//------------------------------------------------------------
//------------------------------------------------------------
void CSkeletonAnim::Serialize(TSerialize ser)
{
	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;
	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("CSkeletonAnim");
		ser.Value("AnimationDrivenMotion", m_AnimationDrivenMotion );
		ser.Value("ForceSkeletonUpdate", m_pSkeletonPose->m_nForceSkeletonUpdate );

		for (uint32 nLayer=0; nLayer<numVIRTUALLAYERS; nLayer++)
		{
			ser.BeginGroup("FIFO");
			uint32 nAnimsInFIFO = m_arrLayer_AFIFO[nLayer].size();  
			ser.Value("nAnimsInFIFO",nAnimsInFIFO);
			if(ser.IsReading())
				m_arrLayer_AFIFO[nLayer].resize(nAnimsInFIFO);

			for (uint32 a=0; a<nAnimsInFIFO; a++)
			{
				m_arrLayer_AFIFO[nLayer][a].Serialize(ser);  

				if(ser.IsReading())
				{
					int32 nAnimID0                 =      m_arrLayer_AFIFO[nLayer][a].m_LMG0.m_nLMGID;
					int32 nGloablID0  = m_arrLayer_AFIFO[nLayer][a].m_LMG0.m_nGlobalLMGID;

					if (nAnimID0>=0)
					{
						const ModelAnimationHeader& AnimHeader = pAnimationSet->GetModelAnimationHeaderRef(nAnimID0);
						assert(AnimHeader.m_nGlobalAnimId == nGloablID0);
						m_arrLayer_AFIFO[nLayer][a].m_LMG0.m_nGlobalLMGID=AnimHeader.m_nGlobalAnimId;
					}

					int32 numAnims0=m_arrLayer_AFIFO[nLayer][a].m_LMG0.m_numAnims;
					for (int32 i=0; i<numAnims0; i++)
					{
						int32 nAnimID   = m_arrLayer_AFIFO[nLayer][a].m_LMG0.m_nAnimID[i];
						int32 nGloablID = m_arrLayer_AFIFO[nLayer][a].m_LMG0.m_nGlobalID[i];
						if (nAnimID>=0)
						{
							const ModelAnimationHeader& AnimHeader = pAnimationSet->GetModelAnimationHeaderRef(nAnimID);
							assert(AnimHeader.m_nGlobalAnimId == nGloablID);
							m_arrLayer_AFIFO[nLayer][a].m_LMG0.m_nGlobalID[i]=AnimHeader.m_nGlobalAnimId;

							GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[AnimHeader.m_nGlobalAnimId];
							if (rGlobalAnimHeader.IsAssetOnDemand())
							{
								if (rGlobalAnimHeader.IsAssetLoaded()==0)
									pAnimationSet->ReloadCAF(AnimHeader.m_nGlobalAnimId);
								rGlobalAnimHeader.m_nPlayCount++;
							}

								//pAnimationSet->ReloadCAF(AnimHeader.m_nGlobalAnimId);
						}
					}

					m_pSkeletonPose->UpdateJointControllers(m_arrLayer_AFIFO[nLayer][a]);
				}
			}
			ser.EndGroup();
		}

		ser.EndGroup();


		uint32 numAnims= m_arrLayer_AFIFO[0].size();
		uint32 fff=0;
	}
}

//////////////////////////////////////////////////////////////////////////

void CSkeletonAnim::SetCharEditMode( uint32 m ) 
{	
	m_CharEditMode = m > 0;
	m_pInstance->m_pModel->m_AnimationSet.m_CharEditMode = m;
}; 


//////////////////////////////////////////////////////////////////////////

void CSkeletonAnim::SetDebugging( uint32 debugFlags )
{
	m_ShowDebugText = debugFlags > 0 ;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
size_t CSkeletonAnim::SizeOfThis (ICrySizer * pSizer)
{
	uint32 TotalSize  = 0;
	for(uint32 i=0; i<numVIRTUALLAYERS; i++)
		TotalSize += (m_arrLayer_AFIFO[i].capacity()*sizeof(CAnimation));
	return TotalSize;
}
