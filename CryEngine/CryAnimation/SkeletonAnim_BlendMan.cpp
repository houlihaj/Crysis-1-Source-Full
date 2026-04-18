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




/////////////////////////////////////////////////////////////////////////////////
// this function is handling animation-update, interpolation and transitions   //
/////////////////////////////////////////////////////////////////////////////////
uint32 CSkeletonAnim::BlendManager(	f32 fDeltaTime,	std::vector<CAnimation>& arrAFIFO, uint32 nLayer )
{

	f32 fColor[4] = {1,0,0,1};



	uint32 NumAnimsInQueue = arrAFIFO.size();
	if (NumAnimsInQueue==0)
		return 0;

	
	//the first animation in the queue MUST be activated
	arrAFIFO[0].m_bActivated = true;  


//	uint32 nMaxActiveInQueue=2;	
	uint32 nMaxActiveInQueue=MAX_EXEC_QUEUE;	

	if (nMaxActiveInQueue>NumAnimsInQueue)
		nMaxActiveInQueue=NumAnimsInQueue;

	if (NumAnimsInQueue>nMaxActiveInQueue)
		NumAnimsInQueue=nMaxActiveInQueue;


	//-------------------------------------------------------------------------------
	//---                            evaluate transition flags                    ---
	//-------------------------------------------------------------------------------
	uint32 i=0;
	for (i=1; i<NumAnimsInQueue; i++)
	{
		if (arrAFIFO[i].m_bActivated)
			continue; //an activated motion will stay activated

		uint32 IsLooping			= arrAFIFO[i-1].m_AnimParams.m_nFlags & CA_LOOP_ANIMATION;

		uint32 StartAtKeytime = arrAFIFO[i].m_AnimParams.m_nFlags & CA_START_AT_KEYTIME;
		uint32 StartAfter			= arrAFIFO[i].m_AnimParams.m_nFlags & CA_START_AFTER;
		uint32 Idle2Move			= arrAFIFO[i].m_AnimParams.m_nFlags & CA_IDLE2MOVE;
		uint32 Move2Idle			= arrAFIFO[i].m_AnimParams.m_nFlags & CA_MOVE2IDLE;


		//can we use the "Idle2Move" flag?
		if (Idle2Move)
		{
			Idle2Move=0;  //lets be pessimistic and assume we can't use this flag at all
			assert(arrAFIFO[i].m_bActivated==0);
			int32 global_lmg_id=arrAFIFO[i-1].m_LMG0.m_nGlobalLMGID;
			if (global_lmg_id>=0)
			{
				GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[global_lmg_id];
				assert(rGlobalAnimHeader.m_nBlendCodeLMG);
				uint32 bc=rGlobalAnimHeader.m_nBlendCodeLMG;
				if (bc==*(uint32*)"I2M1") Idle2Move=1;  //for this LMG we can use it
				if (bc==*(uint32*)"I2M2") Idle2Move=1;  //for this LMG we can use it
			}
		}

		//can we use the "StartAfter" flag?
		if (IsLooping && StartAfter)
			StartAfter=0; //if first animation is looping, then start transition immediately to prevent a hang in the FIFO

		//-------------------------------------------------------------------------------------------------
		//-- If there are several animations in the FIFO it depends on the transition flags whether      --
		//-- we can start a transition immediately. Maybe we want to play one animation after another.   --
		//-------------------------------------------------------------------------------------------------
		uint32 TransitionDelay = StartAtKeytime+StartAfter+Idle2Move+Move2Idle;
		if (TransitionDelay)
		{

			if (StartAtKeytime)
			{
				assert(arrAFIFO[i].m_bActivated==0);
				f32 atnew=arrAFIFO[i-1].m_fAnimTime;
				f32 atold=arrAFIFO[i-1].m_fAnimTime-fDeltaTime*2;
				if (atold<arrAFIFO[i].m_AnimParams.m_fKeyTime  && atnew>arrAFIFO[i].m_AnimParams.m_fKeyTime)
					arrAFIFO[i-1].m_nKeyPassed++;

				if (arrAFIFO[i-1].m_nKeyPassed)
					arrAFIFO[i].m_bActivated=true;
			}

			if (StartAfter)
			{
				assert(arrAFIFO[i].m_bActivated==0);
				if (arrAFIFO[i-1].m_nRepeatCount)
					arrAFIFO[i].m_bActivated=true;
			}

			if (Idle2Move)
			{
				uint32 nSegCount=arrAFIFO[i-1].m_LMG0.m_nSegmentCounter[0];
				Vec2 vLockedStrafeVector = arrAFIFO[i-1].m_LMG0.m_BlendSpace.m_strafe; 
				if (vLockedStrafeVector.x<0.001f)
				{
					if (nSegCount)	
						arrAFIFO[i].m_bActivated=true;	//move to the left
				}
				else
				{
					if (nSegCount && arrAFIFO[i-1].m_fAnimTime>0.50f) 	
						arrAFIFO[i].m_bActivated=true;	//move to the right
				}
			}


			if (Move2Idle)
			{
				assert(arrAFIFO[i].m_bActivated==0);
				if (arrAFIFO[i-1].m_fAnimTime<0.40f)
					arrAFIFO[i].m_bActivated=true;
			}


			if (arrAFIFO[i].m_bActivated==0) 
				break; //all other animations in the FIFO will remain not activated
		} 
		else 
		{
			//No transition-delay flag set. Thats means we can activate the transition immediately
			arrAFIFO[i].m_bActivated=true;
		}
	}

	//-----------------------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------------------

	if (i>nMaxActiveInQueue)
		i=nMaxActiveInQueue;

	nMaxActiveInQueue=i;

#ifdef _DEBUG
	for (uint32 a=0; a<nMaxActiveInQueue; a++)
	{
		assert(arrAFIFO[a].m_bActivated);
		f32 t=arrAFIFO[a].m_fAnimTime;
		assert(t>=0.0f && t<=1.0f);  //most likely this is coming from outside if CryAnimation
	}
#endif





	//----------------------------------------------------------------------------------------
	//-------            transitions between several animations                        -------
	//----------------------------------------------------------------------------------------
	//if (nMaxActiveInQueue>1)
	//{
	//init time-warping
		for (uint32 i=1; i<nMaxActiveInQueue; i++)
		{
			assert(arrAFIFO[i].m_bActivated);// = true;
			//the new animation determines if we use time-warping or not
			uint32 timewarp = arrAFIFO[i].m_AnimParams.m_nFlags & CA_TRANSITION_TIMEWARPING;
			if (timewarp)
			{
				//animations are time-warped, so we have to adjust the delta-time
				f32 tp = arrAFIFO[i].m_fTransitionPriority;
				if (tp==0)
				{
					int32 LMG_OLD = arrAFIFO[i-1].m_LMG0.m_nGlobalLMGID;
					int32 LMG_NEW = arrAFIFO[i].m_LMG0.m_nGlobalLMGID;
					if (LMG_OLD >= 0 && LMG_OLD==LMG_NEW)
						arrAFIFO[i].m_LMG0 = arrAFIFO[i-1].m_LMG0;

					arrAFIFO[i].m_fAnimTimePrev=arrAFIFO[i-1].m_fAnimTimePrev; //copy the prev. time from previous
					arrAFIFO[i].m_fAnimTime=arrAFIFO[i-1].m_fAnimTime; //copy the time from previous
					assert(arrAFIFO[i].m_fAnimTime>=0.0f && arrAFIFO[i].m_fAnimTime<=1.0f);
					arrAFIFO[i].m_nSegCounter=arrAFIFO[i-1].m_nSegCounter; //copy the segment from previous
				}
			}
		}


		//-----------------------------------------------------------------------------
		//-----     update the TRANSITION-TIME of all animations in the queue     -----
		//-----------------------------------------------------------------------------
		arrAFIFO[0].m_fTransitionPriority	= 1.0f;
	//	if (arrAFIFO[0].m_fTransitionPriority>1.0f)	arrAFIFO[0].m_fTransitionPriority=1.0f;
		for (uint32 i=1; i<nMaxActiveInQueue; i++)
		{
			f32 BlendTime   = arrAFIFO[i].m_AnimParams.m_fTransTime;
			if (BlendTime<0) BlendTime = 1.0f; //if transition-time not specified, just put the time to 1
			if (BlendTime==0.0f) BlendTime=0.0001f; //we don't want DivByZero

			f32 ttime = fabsf(fDeltaTime)/BlendTime;
			if (m_TrackViewExclusive)	
				ttime = m_pInstance->m_fOriginalDeltaTime/BlendTime;

			arrAFIFO[i].m_fTransitionPriority += ttime;  //update transition time
			
		if (arrAFIFO[i].m_fTransitionPriority>1.0f)	
			arrAFIFO[i].m_fTransitionPriority=1.0f;
		}
		

		//---------------------------------------------------------------------------------------
		//---  here we adjust the the TRANSITION-WEIGHTS between all animations in the queue  ---
		//----------------------------------------------------------------------------------------
		arrAFIFO[0].m_fTransitionWeight = 1.0f; //the first in the queue will always have the highest priority
		for (uint32 i=1; i<nMaxActiveInQueue; i++)
		{
			arrAFIFO[i].m_fTransitionWeight = arrAFIFO[i].m_fTransitionPriority;
			f32 scale_previous = 1.0f - arrAFIFO[i].m_fTransitionWeight;
			for (uint32 j=0; j<i; j++)
				arrAFIFO[j].m_fTransitionWeight *= scale_previous;
		}

#ifdef _DEBUG
	for (uint32 j=0; j<nMaxActiveInQueue; j++)
	{
		if (arrAFIFO[j].m_fTransitionWeight<0.0f) 
			assert(0);
		if (arrAFIFO[j].m_fTransitionWeight>1.0f) 
			assert(0);
		if (arrAFIFO[j].m_fTransitionPriority<0.0f) 
			assert(0);
		if (arrAFIFO[j].m_fTransitionPriority>1.0f) 
			assert(0);
	}
#endif

		f32 TotalWeights=0.0f;
		for (uint32 i=0; i<nMaxActiveInQueue; i++) 
			TotalWeights += arrAFIFO[i].m_fTransitionWeight;
	//		assert( fabsf(TotalWeights-1.0f) < 0.001f );

	f32 fTotal = fabsf(TotalWeights-1.0f);
	if (fTotal>0.01f)
	{
		assert(!"sum of transition-weights is wrong");	
		CryError("CryAnimation: sum of transition-weights is wrong: %f %s",fTotal,m_pInstance->m_pModel->GetModelFilePath());
	}



	//-------------------------------------------------------------------
	//--------           blend-weight calculation for LMGs         ------
	//-------------------------------------------------------------------
	//UpdateMotionParamDescsForActiveMotions();
	for (uint32 a=0; a<nMaxActiveInQueue; a++)
	{
		assert(arrAFIFO[a].m_bActivated);
		UpdateMotionParamDescs(arrAFIFO[a].m_LMG0, arrAFIFO[a].m_fTransitionWeight);
		UpdateMotionParamDescs(arrAFIFO[a].m_LMG1, arrAFIFO[a].m_fTransitionWeight);
	}

	// editor sets parameter elsewhere, so avoid setting parameters here, which overwrites editor parameters.
	if (m_CharEditMode==0)
		SetDesiredMotionParamsFromDesiredLocalLocation(fDeltaTime);
	UpdateMotionParamBlendSpacesForActiveMotions(fDeltaTime);


	for (uint32 a=0; a<nMaxActiveInQueue; a++)
	{
		assert(arrAFIFO[a].m_bActivated);
		if (arrAFIFO[a].m_AnimParams.m_nFlags & CA_FORCE_SKELETON_UPDATE)
			m_pSkeletonPose->m_bFullSkeletonUpdate=1; //This means we have to do a full-skeleton update

		if (nLayer == 0)
			arrAFIFO[a].m_fCurrentPlaybackSpeed = arrAFIFO[a].m_LMG0.m_params[eMotionParamID_TravelSpeed].blendspace.m_fProceduralScale;
		else
			arrAFIFO[a].m_fCurrentPlaybackSpeed = 1.0f;

		arrAFIFO[a].m_fCurrentPlaybackSpeed *= arrAFIFO[a].m_AnimParams.m_fPlaybackSpeed;


		// Here the new blendspace is converted into the old blendspace
		//UpdateOldMotionBlendSpace(arrAFIFO[a].m_LMG0, m_pInstance->m_fDeltaTime, arrAFIFO[a].m_fTransitionWeight);
		//UpdateOldMotionBlendSpace(arrAFIFO[a].m_LMG1, m_pInstance->m_fDeltaTime, arrAFIFO[a].m_fTransitionWeight);

		WeightComputation(arrAFIFO[a].m_LMG0);
		f32 fDuration0=GetTimewarpedDuration(arrAFIFO[a].m_LMG0);
		WeightComputation(arrAFIFO[a].m_LMG1);
		f32 fDuration1=GetTimewarpedDuration(arrAFIFO[a].m_LMG1);
		f32 tw=arrAFIFO[a].m_fIWeight;
		arrAFIFO[a].m_fCurrentDuration = fDuration0;
		if (fDuration1>0)
			arrAFIFO[a].m_fCurrentDuration = fDuration0*(1-tw) + fDuration1*tw;	//slot time-warping

		f32 SpeedUp = arrAFIFO[a].m_fCurrentPlaybackSpeed;
		f32 fDuration = arrAFIFO[a].m_fCurrentDuration;
		assert(fDuration>0.0f);
		arrAFIFO[a].m_fCurrentDeltaTime = (fDeltaTime/fDuration)*SpeedUp;
	}
	//	}



		//---------------------------------------------------------------------------------------
		//---    In case animations are time-warped,  we have to adjust the ANIMATION-TIME    ---
	//---                 in relation to the TRANSITION-WEIGHTS                           ---
		//---------------------------------------------------------------------------------------
	//	if (nMaxActiveInQueue>1)
	//	{
	uint8 twflag=1;
	arrAFIFO[0  ].m_bTWFlag=0;
		for (uint32 i=1; i<nMaxActiveInQueue; i++)
		{
			uint32 timewarp = arrAFIFO[i].m_AnimParams.m_nFlags & CA_TRANSITION_TIMEWARPING;
			if (timewarp)
			{
			arrAFIFO[i-1].m_bTWFlag=twflag;
			arrAFIFO[i  ].m_bTWFlag=twflag;
			}
			else
			{
				twflag++;
			arrAFIFO[i  ].m_bTWFlag=0;
			}
		}


		f32 fTransitionDelta=0;
		f32 fTransitionWeight=0;
		uint32 start=0;
		uint32 accumented=0;
		for (uint32 i=0; i<(nMaxActiveInQueue+1); i++)
		{
		if (i<nMaxActiveInQueue && arrAFIFO[i].m_bTWFlag)
			{
			if (accumented==0) start=i;
				fTransitionWeight += arrAFIFO[i].m_fTransitionWeight;
				fTransitionDelta	+= arrAFIFO[i].m_fCurrentDeltaTime*arrAFIFO[i].m_fTransitionWeight;
				accumented++;
			} 
			else 
			{
				f32 tt=0.0f;
			if (fTransitionWeight) tt=fTransitionDelta/fTransitionWeight;
				//all time-warped animation will get the same delta-time 
				for (uint32 a=start; a<(start+accumented); a++)
					arrAFIFO[a].m_fCurrentDeltaTime=tt;
			}
		}
	//} 












	//-----------------------------------------------------------------------------------
	//------              create transition information for Aim-IK                  -----
	//-----------------------------------------------------------------------------------
	if (nLayer==0)
	{
		CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;
		const ModelAnimationHeader* pAnimHeader=0;
		m_pSkeletonPose->m_AimIK.m_numActiveAnims=nMaxActiveInQueue;
		for (uint32 i=0; i<nMaxActiveInQueue; i++)
		{
			assert(arrAFIFO[0].m_bActivated);

			m_pSkeletonPose->m_AimIK.m_AimInfo[i].m_numAimPoses=0;
			m_pSkeletonPose->m_AimIK.m_AimInfo[i].m_fWeight		=	arrAFIFO[i].m_fTransitionWeight;
			m_pSkeletonPose->m_AimIK.m_AimInfo[i].m_fAnimTime	=	arrAFIFO[i].m_fAnimTime;

			if (arrAFIFO[i].m_nAnimAimID0>=0)
			{
				pAnimHeader = pAnimationSet->GetModelAnimationHeader(arrAFIFO[i].m_nAnimAimID0);
				if (pAnimHeader)
				{
					m_pSkeletonPose->m_AimIK.m_AimInfo[i].m_nGlobalAimID0	= pAnimHeader->m_nGlobalAnimId; assert(pAnimHeader->m_nGlobalAnimId);
					m_pSkeletonPose->m_AimIK.m_AimInfo[i].m_numAimPoses++;   	

					if (arrAFIFO[i].m_nAnimAimID1>=0)
					{
						pAnimHeader = pAnimationSet->GetModelAnimationHeader(arrAFIFO[i].m_nAnimAimID1);
						if (pAnimHeader)
						{
							m_pSkeletonPose->m_AimIK.m_AimInfo[i].m_nGlobalAimID1	= pAnimHeader->m_nGlobalAnimId; assert(pAnimHeader->m_nGlobalAnimId);
							m_pSkeletonPose->m_AimIK.m_AimInfo[i].m_numAimPoses++;   	
						}
	} 

				}
			}
		}
	}





	//update anim-time and push them into the exec-buffer
	for (uint32 a=0; a<nMaxActiveInQueue; a++)
	{
		assert(arrAFIFO[a].m_bActivated);
		UpdateAndPushIntoExecBuffer( arrAFIFO, nLayer, NumAnimsInQueue,a );
		AnimCallback(arrAFIFO,a,nMaxActiveInQueue);

		f32 rad = Ang3::CreateRadZ( Vec2(0,1),arrAFIFO[a].m_LMG0.m_BlendSpace.m_strafe );
		SmoothCD(arrAFIFO[a].m_LMG0.m_BlendSpace.m_fAllowLeaningSmooth, arrAFIFO[a].m_LMG0.m_BlendSpace.m_fAllowLeaningSmoothRate, m_pInstance->m_fOriginalDeltaTime, f32(fabsf(rad)<0.02f), 0.25f);
	  //	f32 fColor[4] = {1,1,0,1};
	  //	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"lmg.m_BlendSpace.m_fAllowLeaningSmooth:%f  DEG: %f",arrAFIFO[a].m_LMG0.m_BlendSpace.m_fAllowLeaningSmooth, RAD2DEG(0.01f) );	g_YLine+=0x20;
	}


	return 0;
}



/////////////////////////////////////////////////////////////////////////////////
// this function is handling animation-update, interpolation and transitions   //
/////////////////////////////////////////////////////////////////////////////////
uint32 CSkeletonAnim::BlendManagerDebug(	std::vector<CAnimation>& arrAFIFO, uint32 nVLayer )
{
	//const char* mname = m_pInstance->GetFilePath();
	//if ( strcmp(mname,"objects/characters/alien/hunter/hunter.cdf") )
	//	return 0;

	f32 fColor[4] = {1,0,0,1};
	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;

	uint32 NumAnimsInQueue = arrAFIFO.size();
	for (uint32 i=0; i<NumAnimsInQueue; i++)
	{
		const ModelAnimationHeader &anim	=	pAnimationSet->GetModelAnimationHeaderRef(arrAFIFO[i].m_LMG0.m_nAnimID[0]);
		uint32 GlobalAnimationID0		= anim.m_nGlobalAnimId;
		GlobalAnimationHeader& rGlobalAnimHeader0 = g_AnimationManager.m_arrGlobalAnimations[GlobalAnimationID0];
		int32 LoopAnim=rGlobalAnimHeader0.IsAssetCycle();

		if (arrAFIFO[i].m_bActivated)
			fColor[3] = 1.0f;
		else
			fColor[3] = 0.5f;

		const char* pName0=0;
		if (arrAFIFO[i].m_LMG0.m_nLMGID>=0)
		{
			const ModelAnimationHeader &animlmg0	=	pAnimationSet->GetModelAnimationHeaderRef(arrAFIFO[i].m_LMG0.m_nLMGID);
			pName0=animlmg0.GetAnimName();
		}
		else 
			pName0=anim.GetAnimName();

		const char* pName1=0;
		if (arrAFIFO[i].m_LMG1.m_nLMGID>=0)
		{
			const ModelAnimationHeader &animlmg1	=	pAnimationSet->GetModelAnimationHeaderRef(arrAFIFO[i].m_LMG0.m_nLMGID);
			pName1=animlmg1.GetAnimName();
		}
		else 
			if (arrAFIFO[i].m_LMG1.m_nAnimID[0]>=0)
			{
				const ModelAnimationHeader& assetanim	=	pAnimationSet->GetModelAnimationHeaderRef(arrAFIFO[i].m_LMG1.m_nAnimID[0]);
				pName1=assetanim.GetAnimName();
			}

		if (pName0 && pName1)
		{
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"AnimInAFIFO %02d: t:%d p:%.2f %s %s time:%f Flag:%08x ttime:%f seg:%02d", nVLayer, arrAFIFO[i].m_AnimParams.m_nUserToken, arrAFIFO[i].m_fCurrentPlaybackSpeed, pName0,pName1, arrAFIFO[i].m_fAnimTime, arrAFIFO[i].m_AnimParams.m_nFlags, arrAFIFO[i].m_AnimParams.m_fTransTime, arrAFIFO[i].m_LMG0.m_nSegmentCounter[0] ); g_YLine+=0x10;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"AimPoses: %s %s  AimIK: %d  AimIKBlend: %f",arrAFIFO[i].m_strAimPosName0,arrAFIFO[i].m_strAimPosName1,m_pSkeletonPose->m_AimIK.m_UseAimIK,m_pSkeletonPose->m_AimIK.m_AimIKBlend ); g_YLine+=0x10;

			LMGCapabilities caps_slot0 = pAnimationSet->GetLMGCapabilities( arrAFIFO[i].m_LMG0 );
			LMGCapabilities caps_slot1 = pAnimationSet->GetLMGCapabilities( arrAFIFO[i].m_LMG1 );
			f32 t=arrAFIFO[i].m_fIWeight;
			LMGCapabilities caps = caps_slot0*(1-t) + caps_slot1*t;

			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"min:%f  max:%f", caps.m_vMinVelocity.GetLength(),caps.m_vMaxVelocity.GetLength() );g_YLine+=0x10;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"TransitionBlend: %f", arrAFIFO[i].m_fTransitionBlend );g_YLine+=0x10;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Blended Speed : %f", GetCurrentVelocity().GetLength() ); g_YLine+=0x10;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"BSO: (%f %f %f %f %f)  -  %f  -  (%f %f %f %f %f)", arrAFIFO[i].m_LMG0.m_BlendSpace.m_speed, arrAFIFO[i].m_LMG0.m_BlendSpace.m_strafe.x, arrAFIFO[i].m_LMG0.m_BlendSpace.m_strafe.y, arrAFIFO[i].m_LMG0.m_BlendSpace.m_turn, arrAFIFO[i].m_LMG0.m_BlendSpace.m_slope,      arrAFIFO[i].m_fIWeight,       arrAFIFO[i].m_LMG1.m_BlendSpace.m_speed,arrAFIFO[i].m_LMG1.m_BlendSpace.m_strafe.x,arrAFIFO[i].m_LMG1.m_BlendSpace.m_strafe.y,arrAFIFO[i].m_LMG1.m_BlendSpace.m_turn,arrAFIFO[i].m_LMG1.m_BlendSpace.m_slope );  g_YLine+=0x10;
		}
		else
		{

			f32 fS0 = m_pInstance->m_fAnimSpeedScale;
			f32 fS1 = m_arrLayerSpeedMultiplier[nVLayer];
			f32 fS2 =	arrAFIFO[i].m_AnimParams.m_fPlaybackSpeed;
			f32 fSpeedScale =	fS0*fS1*fS2;

			uint32 Instance=uint32(m_pInstance);
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"AnimInAFIFO %02d: t:%d  %s  ATime:%f Flag:%08x TTime:%f seg:%02d Instance:%08x", nVLayer, arrAFIFO[i].m_AnimParams.m_nUserToken,  pName0, arrAFIFO[i].m_fAnimTime, arrAFIFO[i].m_AnimParams.m_nFlags, arrAFIFO[i].m_AnimParams.m_fTransTime, arrAFIFO[i].m_LMG0.m_nSegmentCounter[0],Instance ); g_YLine+=0x10;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"AimPoses: %s %s  AimIK: %d  AimIKBlend: %f",arrAFIFO[i].m_strAimPosName0,arrAFIFO[i].m_strAimPosName1,m_pSkeletonPose->m_AimIK.m_UseAimIK,m_pSkeletonPose->m_AimIK.m_AimIKBlend ); g_YLine+=0x10;
			f32 fUpperBodyBlend = m_arrLayerBlending[nVLayer];
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"AimID: %d  SpeedScale: %f Visisble: %d  UBBlend: %f",arrAFIFO[i].m_LMG0.m_nAnimID[0], fSpeedScale, m_pSkeletonPose->m_bInstanceVisible,fUpperBodyBlend); g_YLine+=0x10;
		//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Bounding Box: (%f %f %f) - (%f %f %f)",m_pSkeletonPose->m_AABB.min.x,m_pSkeletonPose->m_AABB.min.y,m_pSkeletonPose->m_AABB.min.z, m_pSkeletonPose->m_AABB.max.x,m_pSkeletonPose->m_AABB.max.y,m_pSkeletonPose->m_AABB.max.z); g_YLine+=0x10;

			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"UserData: (%f %f %f %f)",arrAFIFO[i].m_AnimParams.m_fUserData[0],arrAFIFO[i].m_AnimParams.m_fUserData[1],arrAFIFO[i].m_AnimParams.m_fUserData[2],arrAFIFO[i].m_AnimParams.m_fUserData[3] ); g_YLine+=0x10;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"TransitionBlend: %f", arrAFIFO[i].m_fTransitionBlend );g_YLine+=0x10;
			LMGCapabilities caps = pAnimationSet->GetLMGCapabilities( arrAFIFO[i].m_LMG0 );

		//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Blended Speed: %f   Range [%f --- %f]  AnimScale: %f", GetCurrentVelocity().GetLength(),  caps.m_vMinVelocity.GetLength(),caps.m_vMaxVelocity.GetLength(),arrAFIFO[i].m_fCurrentPlaybackSpeed ); g_YLine+=0x10;
		//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Blended Radius: %f   Range [%f  --- %f", m_BlendedRoot.m_fCurrentTurn, caps.m_fFastTurnLeft, caps.m_fFastTurnRight );g_YLine+=0x10;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"speed: %f   turn: %f   slope: %f   strafe:(%f %f)", arrAFIFO[i].m_LMG0.m_BlendSpace.m_speed, arrAFIFO[i].m_LMG0.m_BlendSpace.m_turn, arrAFIFO[i].m_LMG0.m_BlendSpace.m_slope,  arrAFIFO[i].m_LMG0.m_BlendSpace.m_strafe.x, arrAFIFO[i].m_LMG0.m_BlendSpace.m_strafe.y );g_YLine+=0x10;

			g_YLine+=0x10;
		}

		fColor[3] = 1.0f;
	}
	g_YLine+=0x20;

	return 0;
}





/////////////////////////////////////////////////////////////////////////////////
//             this function is handling blending between layers               //
/////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnim::LayerBlendManager( f32 fDeltaTime, uint32 nLayer )
{
	if (nLayer==0)
		return;

	int8 status = GetActiveLayer(nLayer);
	f32 time = m_arrLayerBlendingTime[nLayer];
	if (time<0.00001f)
		time=0.00001f;

	//f32 fColor[4] = {0.5f,0.0f,0.5f,1};
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"nLayer: %02d  status: %1d, blend: %f  ", nLayer, status, m_arrLayerBlending[nLayer]  ); 
	//g_YLine+=0x10;

	if (status)
		m_arrLayerBlending[nLayer]+=fDeltaTime/time;
	else
		m_arrLayerBlending[nLayer]-=fDeltaTime/time;

	if (m_arrLayerBlending[nLayer]>1.0f)
		m_arrLayerBlending[nLayer]=1.0f;
	if (m_arrLayerBlending[nLayer]<0.0f)
		m_arrLayerBlending[nLayer]=0.0f;

	uint32 numAnims = m_arrLayer_AFIFO[nLayer].size();
	if (numAnims&& (m_arrLayerBlending[nLayer]==0.0f) && (status==0))
		m_arrLayer_AFIFO[nLayer][0].m_bRemoveFromQueue=1;

	return;
}






//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------

void CSkeletonAnim::UpdateAndPushIntoExecBuffer( std::vector<CAnimation>& arrAFIFO, uint32 nLayer, uint32 NumAnimsInQueue, uint32 AnimNo )
{
	assert(arrAFIFO[AnimNo].m_fAnimTime<=2.0f);

	arrAFIFO[AnimNo].m_bEndOfCycle=0;
	uint32 numAimPoses=0;
	numAimPoses+=(arrAFIFO[AnimNo].m_nAnimAimID0>=0);
	numAimPoses+=(arrAFIFO[AnimNo].m_nAnimAimID1>=0);
	uint32 ManualUpdate			= (arrAFIFO[AnimNo].m_AnimParams.m_nFlags & CA_MANUAL_UPDATE)!=0;
	uint32 LoopAnimation			= (arrAFIFO[AnimNo].m_AnimParams.m_nFlags & CA_LOOP_ANIMATION)!=0;
	uint32 RepeatLastKey			= (arrAFIFO[AnimNo].m_AnimParams.m_nFlags & CA_REPEAT_LAST_KEY)!=0;
	if (LoopAnimation)	RepeatLastKey=0;
	if (ManualUpdate) arrAFIFO[AnimNo].m_fCurrentDeltaTime=0.0f;

	if (arrAFIFO[AnimNo].m_fCurrentDeltaTime>0.9f)	arrAFIFO[AnimNo].m_fCurrentDeltaTime=0.9f;
	if (arrAFIFO[AnimNo].m_fCurrentDeltaTime<-0.9f) arrAFIFO[AnimNo].m_fCurrentDeltaTime=-0.9f;


	f32 fCurrentDeltaTime = arrAFIFO[AnimNo].m_fCurrentDeltaTime;
	arrAFIFO[AnimNo].m_fAnimTimePrev	=	arrAFIFO[AnimNo].m_fAnimTime;
	arrAFIFO[AnimNo].m_fAnimTime		 +=	fCurrentDeltaTime;
	assert(arrAFIFO[AnimNo].m_fAnimTime<=2.0f);

	if (arrAFIFO[AnimNo].m_fAnimTime>=1.9999f)
		arrAFIFO[AnimNo].m_fAnimTime=1.9999f; //this is not supposed to happen

	if (fCurrentDeltaTime>=0.0f)
	{
		//time is running forward
		if (arrAFIFO[AnimNo].m_fAnimTime>1.0f)
		{
			arrAFIFO[AnimNo].m_nSegCounter++;

			uint32 Loop   =  arrAFIFO[AnimNo].m_nSegCounter<arrAFIFO[AnimNo].m_nSegHighest || LoopAnimation;
			uint32 Repeat = (RepeatLastKey && arrAFIFO[AnimNo].m_nSegCounter>=arrAFIFO[AnimNo].m_nSegHighest);

			if (Loop==0)
			{
				arrAFIFO[AnimNo].m_bRemoveFromQueue=1;
				arrAFIFO[AnimNo].m_fAnimTime = 1.0f;	//anim is not looping
			}

			if (Repeat)
			{
				arrAFIFO[AnimNo].m_nRepeatCount=1;
				arrAFIFO[AnimNo].m_bRemoveFromQueue=0;
				arrAFIFO[AnimNo].m_fAnimTime = 1.0f;	
	
				//just for multi-layer 
				uint32 flags = arrAFIFO[AnimNo].m_AnimParams.m_nFlags;
				uint32  pbu = flags&CA_PARTIAL_SKELETON_UPDATE;
				uint32  la	= flags&CA_LOOP_ANIMATION;
				if (NumAnimsInQueue==1 && nLayer && la==0 && pbu)
				{
					uint32  rlk = flags&CA_REPEAT_LAST_KEY;
					uint32  fo	= flags&CA_FADEOUT;
					if (rlk && fo)
						StopAnimationInLayer(nLayer,0.5f);
				}
			}

			if (Loop==1 && Repeat==0)
			{
				arrAFIFO[AnimNo].m_fAnimTime-=1.0f;	
				assert(arrAFIFO[AnimNo].m_fAnimTime>=0.0f && arrAFIFO[AnimNo].m_fAnimTime<=1.0f);

				arrAFIFO[AnimNo].m_nLoopCount++;

				uint32 numAnims0 = arrAFIFO[AnimNo].m_LMG0.m_numAnims;
				for (uint32 i=0; i<numAnims0; i++)
				{
					int32 GlobalID = arrAFIFO[AnimNo].m_LMG0.m_nGlobalID[i];
					GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[ GlobalID ];
					int32 seg = rGAH.m_Segments-1;
					arrAFIFO[AnimNo].m_LMG0.m_nSegmentCounter[i]++;
					if (arrAFIFO[AnimNo].m_LMG0.m_nSegmentCounter[i]>seg)
						arrAFIFO[AnimNo].m_LMG0.m_nSegmentCounter[i]=0;	
				}

				uint32 numAnims1 = arrAFIFO[AnimNo].m_LMG1.m_numAnims;
				for (uint32 i=0; i<numAnims1; i++)
				{
					int32 GlobalID = arrAFIFO[AnimNo].m_LMG1.m_nGlobalID[i];
					GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[ GlobalID ];
					int32 seg = rGAH.m_Segments-1;
					arrAFIFO[AnimNo].m_LMG1.m_nSegmentCounter[i]++;
					if (arrAFIFO[AnimNo].m_LMG1.m_nSegmentCounter[i]>seg)
						arrAFIFO[AnimNo].m_LMG1.m_nSegmentCounter[i]=0;	
				}

				arrAFIFO[AnimNo].m_bEndOfCycle=1;
			}	
		}
	}
	else
	{
		//time is running backwards
		if (arrAFIFO[AnimNo].m_fAnimTime<0.0f)
		{
			if (LoopAnimation==0)
			{
				arrAFIFO[AnimNo].m_bRemoveFromQueue=1;
				arrAFIFO[AnimNo].m_fAnimTime = 0.0f; 
			}
			if (RepeatLastKey)
			{
				arrAFIFO[AnimNo].m_bRemoveFromQueue=0;
				arrAFIFO[AnimNo].m_nRepeatCount=1;
				arrAFIFO[AnimNo].m_fAnimTime = 0.0f; 
			}
			if (LoopAnimation && RepeatLastKey==0)
			{
				arrAFIFO[AnimNo].m_fAnimTime+=1.0f; 	
				arrAFIFO[AnimNo].m_nLoopCount++;
				arrAFIFO[AnimNo].m_bEndOfCycle=1;
			}	
		}
	}

	assert(arrAFIFO[AnimNo].m_fAnimTime>=0.0f && arrAFIFO[AnimNo].m_fAnimTime<=1.0f);



	return;
}


//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------

void CSkeletonAnim::AnimCallback ( std::vector<CAnimation>& arrAFIFO, uint32 AnimNo, uint32 AnimQueueLen)
{

	int32 nGlobalLMGID      =     arrAFIFO[AnimNo].m_LMG0.m_nGlobalLMGID;
	int32 nAnimLMGID        =     arrAFIFO[AnimNo].m_LMG0.m_nLMGID;

	if (nGlobalLMGID<0)
	{
		nGlobalLMGID      =     arrAFIFO[AnimNo].m_LMG0.m_nGlobalID[0];
		nAnimLMGID        =     arrAFIFO[AnimNo].m_LMG0.m_nAnimID[0];
	}

	int32 nEOC                          =     arrAFIFO[AnimNo].m_bEndOfCycle;
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[nGlobalLMGID];

	f32 timeold=arrAFIFO[AnimNo].m_fAnimTimePrev;
	f32 timenew=arrAFIFO[AnimNo].m_fAnimTime;

	// prevent sending events when animation is not playing
	// like when there's an event at time 1.0f and repeat-last-key flag is used
	// otherwise it may happen to get the event each frame
	if ( timeold == timenew )
		return;

	if (nEOC)
	{
		//if (timeold<=timenew)
		//	CryError("CryAnimation: timer mismatch");
		timenew+=1.0f;
	}
	if ( fabsf(m_pInstance->m_fDeltaTime) <0.00001f)
		return;
	uint32 numEvents = rGlobalAnimHeader.m_AnimEvents.size();
	if (numEvents==0)
		return;

	for (uint32 i=0; i<numEvents; i++)
	{
		f32 time = rGlobalAnimHeader.m_AnimEvents[i].m_time;
		// This is for preventing the case that the event never triggered 
		// when the time parameter set to zero.
		if(time == 0)
			time = 0.01f;
		if ( timeold<=time && timenew>=time ) 
		{
			m_LastAnimEvent.m_time										= time; 
			m_LastAnimEvent.m_AnimPathName            =      rGlobalAnimHeader.GetPathName(); 
			m_LastAnimEvent.m_AnimID                              = nAnimLMGID;
			m_LastAnimEvent.m_nAnimNumberInQueue                          = AnimQueueLen-1 - AnimNo;  
			m_LastAnimEvent.m_fAnimPriority                 = arrAFIFO[AnimNo].m_fTransitionPriority;
			m_LastAnimEvent.m_EventName                     =      rGlobalAnimHeader.m_AnimEvents[i].m_strEventName; 
			m_LastAnimEvent.m_CustomParameter   =      rGlobalAnimHeader.m_AnimEvents[i].m_strCustomParameter;
			m_LastAnimEvent.m_BonePathName            = rGlobalAnimHeader.m_AnimEvents[i].m_strBoneName;
			m_LastAnimEvent.m_vOffset                             = rGlobalAnimHeader.m_AnimEvents[i].m_vOffset;
			m_LastAnimEvent.m_vDir                                = rGlobalAnimHeader.m_AnimEvents[i].m_vDir;
			if (m_pEventCallback)         
				(*m_pEventCallback)(m_pInstance,m_pEventCallbackData);

			/*
			const char* mname = m_pInstance->GetFilePath();
			if ( strcmp(mname,"objects/characters/alien/hunter/hunter.cdf")==0 )
			{
			float fColor[4] = {1,0,0,1};
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.9f, fColor, false,"HunterEvent: time %f  AnimNo: %d	 Priority: %f   name: %s",time,AnimNo,arrAFIFO[AnimNo].m_fTransitionPriority,rGlobalAnimHeader.m_AnimEvents[i].m_strEventName);    
			g_YLine+=16.0f;
			}
			*/

		}             
	}
}


//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------



void CSkeletonAnim::SetIWeight(uint32 nLayer, f32 bw)
{
	assert(nLayer<16);
	uint32 numAnimsOnStack = m_arrLayer_AFIFO[nLayer].size();
	for (uint32 i=0; i<numAnimsOnStack; i++)
			m_arrLayer_AFIFO[nLayer][i].m_fIWeight=bw;
}


f32 CSkeletonAnim::GetIWeight(uint32 nLayer)
{
	assert(nLayer<16);
	uint32 numAnimsOnStack = m_arrLayer_AFIFO[nLayer].size();
	if (numAnimsOnStack)
		return m_arrLayer_AFIFO[nLayer][0].m_fIWeight;
	return 0.0f; //error
}





bool CSkeletonAnim::RemoveAnimFromFIFO(uint32 nLayer, uint32 num)
{
	if (nLayer >= numVIRTUALLAYERS)
		return false;
	std::vector<CAnimation>& fifo = m_arrLayer_AFIFO[nLayer];
	if (num >= fifo.size())
		return false;
	if (fifo[num].m_bActivated)
		return false;
	fifo.erase(fifo.begin() + num);
	return true;
}


void CSkeletonAnim::ManualSeekAnimationInFIFO(uint32 nLayer, uint32 num, float time, bool advance)
{
	uint32 numAnimsLayer = GetNumAnimsInFIFO(nLayer);
	if (num < numAnimsLayer)
	{
		CAnimation &animation = GetAnimFromFIFO(nLayer, num);

		if (animation.m_AnimParams.m_nFlags & (CA_MANUAL_UPDATE | CA_TRACK_VIEW_EXCLUSIVE))
		{
			int32 animID = (animation.m_LMG0.m_nLMGID >= 0) ? animation.m_LMG0.m_nLMGID : animation.m_LMG0.m_nAnimID[0];
			int32 globalID = (animation.m_LMG0.m_nLMGID >= 0) ? animation.m_LMG0.m_nGlobalLMGID : animation.m_LMG0.m_nGlobalID[0];
			if (globalID >= 0 && globalID < int(g_AnimationManager.m_arrGlobalAnimations.size()))
			{
				const GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[globalID];

				f32 timeold, timenew;
				if (advance)
					timeold = animation.m_fAnimTime, timenew = time;
				else
					timeold = time, timenew = animation.m_fAnimTime;
				if (timenew < timeold)
					timenew += 1.0f;

				// prevent sending events when animation is not playing
				// otherwise it may happen to get the event each frame
				if ( timeold != timenew )
				{
					uint32 numEvents = rGlobalAnimHeader.m_AnimEvents.size();
					for (uint32 i=0; i<numEvents; i++)
					{
						f32 time = rGlobalAnimHeader.m_AnimEvents[i].m_time;
						if ((timeold<=time && timenew>=time) || (timeold<=time + 1.0f && timenew>=time + 1.0f))
						{
							m_LastAnimEvent.m_time=time; 
							m_LastAnimEvent.m_AnimPathName=	rGlobalAnimHeader.GetPathName(); 
							m_LastAnimEvent.m_AnimID = animID;
							m_LastAnimEvent.m_EventName =	rGlobalAnimHeader.m_AnimEvents[i].m_strEventName; 
							m_LastAnimEvent.m_CustomParameter	=	rGlobalAnimHeader.m_AnimEvents[i].m_strCustomParameter;
							m_LastAnimEvent.m_BonePathName = rGlobalAnimHeader.m_AnimEvents[i].m_strBoneName;
							m_LastAnimEvent.m_vOffset = rGlobalAnimHeader.m_AnimEvents[i].m_vOffset;
							m_LastAnimEvent.m_vDir = rGlobalAnimHeader.m_AnimEvents[i].m_vDir;

							if (m_pEventCallback)		
								(*m_pEventCallback)(m_pInstance,m_pEventCallbackData);
						}			
					}
				}
			}

			// Update the time.
			animation.m_fAnimTime = time;
		}
	}
}


f32 CSkeletonAnim::GetRelRotationZ()
{
	f32 assetRot = m_BlendedRoot.m_fRelRotation;

	if (m_CharEditBlendSpaceOverrideEnabled[eMotionParamID_TurnSpeed])
		return assetRot;

	f32 parameterizedRotBlended = 0.0f;
	
	int layer = 0;
	int animCount = GetNumAnimsInFIFO(layer);

	float weightSum = 0.0f;
	for (int animIndex = 0; animIndex < animCount; ++animIndex)
	{
		const CAnimation& anim = GetAnimFromFIFO(layer, animIndex);
		if (anim.m_bActivated)
		{
			const MotionParamBlendSpace& turnSpeed = anim.m_LMG0.m_params[eMotionParamID_TurnSpeed].blendspace;
			const MotionParamBlendSpace& turnAngle = anim.m_LMG0.m_params[eMotionParamID_TurnAngle].blendspace;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"turnSpeed.proceduralOffset: %f   turnSpeed.proceduralScale: %f",turnSpeed.proceduralOffset,turnSpeed.proceduralScale);	g_YLine+=16.0f;
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"turnAngle.proceduralOffset: %f   turnAngle.proceduralScale: %f",turnAngle.proceduralOffset,turnAngle.proceduralScale);	g_YLine+=16.0f;

			// Both of these parameters will not be active/used at the same time,
			// and thus either parameter will have zero offset and a scale of 1.0, which will not have any affect.
			f32 offset	= turnSpeed.m_fProceduralOffset + turnAngle.m_fProceduralOffset;
			f32 scale		= turnSpeed.m_fProceduralScale * turnAngle.m_fProceduralScale;

			// If there is no turn in the asset, the offset will be the desired turn speed.
			// If there is turn in the asset, the offset will be zero and the actual asset rotation is used untouched.
			// The scaling is used to scale the rotation from the asset to reach the desired turn speed.
			// For details on how offset and scale is calculated, see CalculateMotionParamBlendSpace().
			f32 parameterizedRot = offset*m_pInstance->m_fDeltaTime + scale*assetRot;

			// Apply each motions rotation in relation to it's transition blend weight.
			float weight = anim.m_fTransitionPriority * anim.m_fTransitionPriority;
			weightSum += weight;
			parameterizedRotBlended = LERP(parameterizedRotBlended, parameterizedRot, weight);
		}
	}

	if (weightSum > 0.0f)
		parameterizedRotBlended /= weightSum;

	return parameterizedRotBlended;

	//IVO: if we set desired turn, then we have to make sure that we really get desired turn and not something else.
	//using the blend-weight for this is giving a result that is very similar, but it is not EXACTLY what I need.
	//I will try to find a better parameterization for the turns, but this will do for now. 
	//Currently the turn are just "decoration". The accuracy is just 90% of the animation that is actually doing the turn-animation.

	//DAVID: For the system to work properly and in a generic enough way the motion-parameters to animation-blendweights calculations must be 99% accurate.
};


Vec3 CSkeletonAnim::GetRelTranslation()
{
	Vec3 translation = -m_BlendedRoot.m_vRelTranslation;
/*
	SetFuturePathAnalyser(true);
	AnimTransRotParams atp = GetBlendedAnimTransRot(m_desiredArrivalDeltaTime, true);
	if (atp.m_Valid && (atp.m_DeltaTime > 0.0f))
	{
		float frameTime = gEnv->pTimer->GetFrameTime();
		Vec3 correction;
		correction = ((m_desiredLocalLocation.t - atp.m_TransRot.t) / atp.m_DeltaTime);
		correction *= frameTime;
		translation += correction;
	}
*/
	return translation;
}

QuatT CSkeletonAnim::GetRelMovement()
{
	f32 radiant = GetRelRotationZ();
	Quat q = Quat::CreateRotationZ(radiant);
	Vec3 t = GetRelTranslation();
	return QuatT(q,q*t);
}




Vec3 CSkeletonAnim::GetRelFootSlide()
{
	return m_pSkeletonPose->m_FootAnchor.m_vFootSlide;
}

//! Set the current time of the given layer, in seconds
void CSkeletonAnim::SetLayerTime (uint32 nLayer, float fTimeSeconds)
{
	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;
	uint32 numAnims = m_arrLayer_AFIFO[nLayer].size();
	if (numAnims>0)
	{
		int32 nGlobalLMGID = m_arrLayer_AFIFO[nLayer][0].m_LMG0.m_nGlobalLMGID;
		if (nGlobalLMGID>=0)
		{
			int32 nAnimID = m_arrLayer_AFIFO[nLayer][0].m_LMG0.m_nAnimID[0];
			assert(nAnimID>=0);
			const ModelAnimationHeader &AnimHeader	=	pAnimationSet->GetModelAnimationHeaderRef(nAnimID);
			GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[AnimHeader.m_nGlobalAnimId];
			assert(rGlobalAnimHeader.m_fTotalDuration==m_arrLayer_AFIFO[nLayer][0].m_LMG0.m_fDurationQQQ[0]);
			f32 duration	=	m_arrLayer_AFIFO[nLayer][0].m_LMG0.m_fDurationQQQ[0];

			f32 percentage = fTimeSeconds/rGlobalAnimHeader.m_fTotalDuration;
			m_arrLayer_AFIFO[nLayer][0].m_fAnimTime = percentage;
			assert(percentage>=0.0f && percentage<=1.0f);
		} 
		else
		{
			uint32 segment=0;
			int32	agid = m_arrLayer_AFIFO[nLayer][0].m_LMG0.m_nGlobalID[0];
			GlobalAnimationHeader& rsubGAH0 = g_AnimationManager.m_arrGlobalAnimations[ agid ];

			f32 percentage = fTimeSeconds/rsubGAH0.m_fTotalDuration;

			if (percentage>=rsubGAH0.m_SegmentsTime[0] && percentage<=rsubGAH0.m_SegmentsTime[1])
			{							
				segment=0;

			}
			else
				if (percentage>=rsubGAH0.m_SegmentsTime[1] && percentage<=rsubGAH0.m_SegmentsTime[2])
				{							
					segment=1;
				}
				else
					if (percentage>=rsubGAH0.m_SegmentsTime[2] && percentage<=rsubGAH0.m_SegmentsTime[3])
					{
						segment=2;
					};

			m_arrLayer_AFIFO[nLayer][0].m_LMG0.m_nSegmentCounter[0]=segment;

			f32 t0	=	rsubGAH0.m_SegmentsTime[segment+0];
			f32 t1	=	rsubGAH0.m_SegmentsTime[segment+1];
			f32 d		=	t1-t0;
			f32 t		=	percentage-t0;
			m_arrLayer_AFIFO[nLayer][0].m_fAnimTime = t/d;
			assert(m_arrLayer_AFIFO[nLayer][0].m_fAnimTime>=0.0f && m_arrLayer_AFIFO[nLayer][0].m_fAnimTime<=1.0f);
		}
	}
	if (numAnims>1)
	{
		int32 nAnimID = m_arrLayer_AFIFO[nLayer][1].m_LMG0.m_nAnimID[0];
		assert(nAnimID>=0);
		const ModelAnimationHeader &AnimHeader		=	pAnimationSet->GetModelAnimationHeaderRef(nAnimID);
		GlobalAnimationHeader& rGlobalAnimHeader	= g_AnimationManager.m_arrGlobalAnimations[AnimHeader.m_nGlobalAnimId];
		assert(rGlobalAnimHeader.m_fTotalDuration==m_arrLayer_AFIFO[nLayer][1].m_LMG0.m_fDurationQQQ[0]);

		f32 duration=m_arrLayer_AFIFO[nLayer][1].m_LMG0.m_fDurationQQQ[0];
		m_arrLayer_AFIFO[nLayer][1].m_fAnimTime=fTimeSeconds/duration;
		assert(m_arrLayer_AFIFO[nLayer][1].m_fAnimTime>=0.0f && m_arrLayer_AFIFO[nLayer][1].m_fAnimTime<=1.0f);
	}
	if (numAnims>2)
	{
		int32 nAnimID = m_arrLayer_AFIFO[nLayer][2].m_LMG0.m_nAnimID[0];
		assert(nAnimID>=0);
		const ModelAnimationHeader &AnimHeader		=	pAnimationSet->GetModelAnimationHeaderRef(nAnimID);
		GlobalAnimationHeader& rGlobalAnimHeader	= g_AnimationManager.m_arrGlobalAnimations[AnimHeader.m_nGlobalAnimId];
		assert(rGlobalAnimHeader.m_fTotalDuration==m_arrLayer_AFIFO[nLayer][2].m_LMG0.m_fDurationQQQ[0]);

		f32 duration=m_arrLayer_AFIFO[nLayer][2].m_LMG0.m_fDurationQQQ[0];
		m_arrLayer_AFIFO[nLayer][2].m_fAnimTime=fTimeSeconds/duration;
		assert(m_arrLayer_AFIFO[nLayer][2].m_fAnimTime>=0.0f && m_arrLayer_AFIFO[nLayer][2].m_fAnimTime<=1.0f);
	}
};


//! Return the current time of the given layer, in seconds
float CSkeletonAnim::GetLayerTime (uint32 nLayer)const
{
	if (m_arrLayer_AFIFO[nLayer].empty())
		return 0.0f;

	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;
	int32 nAnimID = m_arrLayer_AFIFO[nLayer][0].m_LMG0.m_nAnimID[0];
	assert(nAnimID>=0);
	const ModelAnimationHeader &AnimHeader		=	pAnimationSet->GetModelAnimationHeaderRef(nAnimID);
	GlobalAnimationHeader& rGlobalAnimHeader	= g_AnimationManager.m_arrGlobalAnimations[AnimHeader.m_nGlobalAnimId];
	assert(rGlobalAnimHeader.m_fTotalDuration==m_arrLayer_AFIFO[nLayer][0].m_LMG0.m_fDurationQQQ[0]);

	f32 duration=m_arrLayer_AFIFO[nLayer][0].m_LMG0.m_fDurationQQQ[0];
	return m_arrLayer_AFIFO[nLayer][0].m_fAnimTime*duration;
};

void CSkeletonAnim::SetAnimationDrivenMotion(uint32 ts) 
{
	m_AnimationDrivenMotion=ts;  
};
void CSkeletonAnim::SetMirrorAnimation(uint32 ts) 
{
	m_MirrorAnimation=ts;  
};


// sets the animation speed scale for layers
void CSkeletonAnim::SetLayerUpdateMultiplier(int32 nLayer, f32 fSpeed)
{
	if (nLayer>=numVIRTUALLAYERS || nLayer<0)
	{
		g_pILog->LogError ("invalid layer id: %d", nLayer);
		return;
	}
	m_arrLayerSpeedMultiplier[nLayer] = fSpeed;
}

//---------------------------------------------------------

f32 CSkeletonAnim::GetFootPlantStatus() 
{ 

	f32 fFinalSteps=0.0f;
	int32 numAnimsLayer0 = m_arrLayer_AFIFO[0].size();
	for (int32 i=0; i<numAnimsLayer0; i++)
	{
		f32 fSteps0=0.0f;
		int32 numAnims0 = m_arrLayer_AFIFO[0][i].m_LMG0.m_numAnims;
		for (int32 a=0; a<numAnims0; a++)
		{
			f32 fs=m_arrLayer_AFIFO[0][i].m_LMG0.m_fFootPlants[a];
			//f32 bw=m_arrLayer_AFIFO[0][i].m_LMG0.m_fBlendWeight[a];
			//fSteps0 += fs*bw;
			if (fs) fSteps0=1.0f;
		}

		f32 fSteps1=0.0f;
		int32 numAnims1 = m_arrLayer_AFIFO[0][i].m_LMG1.m_numAnims;
		for (int32 a=0; a<numAnims1; a++)
		{
			f32 fs=m_arrLayer_AFIFO[0][i].m_LMG1.m_fFootPlants[a];
			//f32 bw=m_arrLayer_AFIFO[0][i].m_LMG1.m_fBlendWeight[a];
			//fSteps1 += fs*bw;
			if (fs) fSteps1=1.0f;
		}
		f32 t=m_arrLayer_AFIFO[0][i].m_fIWeight;
		f32 fSteps = fSteps0*(1-t)+fSteps0*t;

		fFinalSteps += fSteps*m_arrLayer_AFIFO[0][i].m_fTransitionWeight;

	}

	return fFinalSteps; 
}



//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

void CSkeletonAnim::GetFuturePath( SAnimRoot* pRelFuturePath )
{
	if (m_FuturePath)
		for (uint32 i=0; i<ANIM_FUTURE_PATH_LOOKAHEAD; i++)
			pRelFuturePath[i]=m_BlendedRoot.m_relFuturePath[i];
};


Vec3 CSkeletonAnim::GetCurrentAimDirection() 
{	
	return m_pSkeletonPose->m_AimIK.m_AimDirection; 
};
Vec3 CSkeletonAnim::GetCurrentLookDirection() 
{ 
	return m_pSkeletonPose->m_LookIK.m_LookDirection; 
};



ILINE f32 GetYaw( const Quat& quat)
{
	Vec3 forward = quat.GetColumn1(); 
	assert( fabsf(forward.z)<0.01f );
	forward.z=0; 
	forward.Normalize();
	return -atan2f( forward.x,forward.y );
}

AnimTransRotParams CSkeletonAnim::GetBlendedAnimTransRot(f32 fDeltaTime, uint32 clamp) 
{
	AnimTransRotParams atp;

	if (m_IsAnimPlaying==0)
		return atp;
	if (m_FuturePath==0)
		return atp;
	if (fDeltaTime<0.0f)
		return atp;

	if (fDeltaTime>2.0f)
		fDeltaTime=2.0f;

	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"%d  m_AbsoluteTime: %f   DeltaTime: %f",i, time0, fDeltaTime);	
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"FutureTimeAbs: %f", FutureTimeAbs);	
	//	g_YLine+=16.0f;

	//	f32 fColor[4] = {1,1,0,1};
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false, "m_arrLayerSpeedMultiplier[0]: %f", m_arrLayerSpeedMultiplier[0] ); g_YLine+=0x0d;

	f32 fKeyPerSec = 10.0f;
	Vec3 arrAbsFuturePathPos[ANIM_FUTURE_PATH_LOOKAHEAD];
	Vec3 arrRelFuturePathTrans[ANIM_FUTURE_PATH_LOOKAHEAD];
	Quat arrAbsFuturePathRot[ANIM_FUTURE_PATH_LOOKAHEAD];

	static bool newVersion = true;
	if (newVersion)
	{
		static float curveAngleCenter = 45.0f;
		static float curveAngleSpan = 5.0f;
		float travelAngle = GetDesiredMotionParam(eMotionParamID_TravelAngle);
		float proceduralCurvature = CLAMP(abs(travelAngle - curveAngleCenter) / curveAngleSpan, 0.0f, 1.0f);
		proceduralCurvature = 0.0f;

		arrAbsFuturePathPos[0] = ZERO;
		arrRelFuturePathTrans[0] = ZERO;
		arrAbsFuturePathRot[0] = IDENTITY;
		for (uint32 i = 1; i < ANIM_FUTURE_PATH_LOOKAHEAD; ++i)
		{
			QuatT originalMovement = m_BlendedRoot.m_relFuturePath[i].m_TransRot;
			originalMovement.t = originalMovement.t * m_arrLayerSpeedMultiplier[0];
			arrAbsFuturePathPos[i] = arrAbsFuturePathPos[i-1] + arrAbsFuturePathRot[i-1] * originalMovement.t;
			arrRelFuturePathTrans[i] = arrAbsFuturePathPos[i] - arrAbsFuturePathPos[i-1];
			arrAbsFuturePathRot[i] = arrAbsFuturePathRot[i-1] * originalMovement.q;
		}

		static float proceduralTurnEnabled = 1.0f;
		float proceduralTurn = 1.0f - GetDesiredMotionParam(eMotionParamID_Curving) * proceduralTurnEnabled;
		float turnSpeed = GetDesiredMotionParam(eMotionParamID_TurnSpeed);
		Quat proceduralRotQ = Quat::CreateRotationZ(proceduralTurn * turnSpeed/fKeyPerSec);
		Quat proceduralRotT = Quat::CreateRotationZ(proceduralTurn * proceduralCurvature * turnSpeed/fKeyPerSec);
		Quat accumulatedProceduralRotQ(IDENTITY);
		Quat accumulatedProceduralRotT(IDENTITY);
		for (uint32 i = 1; i < ANIM_FUTURE_PATH_LOOKAHEAD; ++i)
		{
			accumulatedProceduralRotQ = accumulatedProceduralRotQ * proceduralRotQ;
			accumulatedProceduralRotT = accumulatedProceduralRotT * proceduralRotT;
			arrAbsFuturePathPos[i] = arrAbsFuturePathPos[i-1] + accumulatedProceduralRotT * arrRelFuturePathTrans[i];
			arrAbsFuturePathRot[i] = arrAbsFuturePathRot[i] * accumulatedProceduralRotQ;
		}
	}
	else
	{
		if (0)
		{
			static float straighten = 0.0f;

			//	f32  radz = GetRelRotationZ();
			f32 fKeyPerSec=10.0f;
			f32 AvrgFrameRate = g_AverageFrameTime;
			float turnSpeed = GetDesiredMotionParam(eMotionParamID_TurnSpeed);
			Quat procrot = Quat::CreateRotationZ(turnSpeed/fKeyPerSec);
			procrot.SetNlerp(procrot, IDENTITY, straighten);
			Quat qabsolute(IDENTITY);
			Vec3 vabsolute(ZERO);
			for (uint32 i=0; i<ANIM_FUTURE_PATH_LOOKAHEAD; i++)
			{
				vabsolute += qabsolute*m_BlendedRoot.m_relFuturePath[i].m_TransRot.t*m_arrLayerSpeedMultiplier[0];
				qabsolute *= procrot;
				arrAbsFuturePathRot[i] = qabsolute;
				arrAbsFuturePathPos[i] = vabsolute;
			}
		}
		else 
		{
			Quat qabsolute(IDENTITY);
			Vec3 vabsolute(ZERO);
			for (uint32 i=0; i<ANIM_FUTURE_PATH_LOOKAHEAD; i++)
			{
				f32 RadianAnim = GetYaw(m_BlendedRoot.m_relFuturePath[i].m_TransRot.q);
				vabsolute += qabsolute*m_BlendedRoot.m_relFuturePath[i].m_TransRot.t*m_arrLayerSpeedMultiplier[0];
				qabsolute *= Quat::CreateRotationZ(RadianAnim*m_arrLayerSpeedMultiplier[0]);
				arrAbsFuturePathRot[i] = qabsolute;
				arrAbsFuturePathPos[i] = vabsolute;
			}
		}
	}



	atp = GetTransRot( fDeltaTime, &arrAbsFuturePathPos[0], &arrAbsFuturePathRot[0]);
	if (atp.m_Valid==0)
		return atp;

	if (clamp)
	{
		if (atp.m_NormalizedTimeAbs>1.0f)
		{
			f32 fDuration = m_BlendedRoot.m_fCurrentDuration;				
			f32 over = atp.m_KeyTimeAbs-fDuration;
			f32 clampdelta = fDeltaTime - over;
			atp = GetTransRot( clampdelta, &arrAbsFuturePathPos[0], &arrAbsFuturePathRot[0]);
			return atp;
		}
	}

	return atp;
};





AnimTransRotParams CSkeletonAnim::GetTransRot(f32 fDeltaTime, const Vec3* parrAbsFuturePathPos, const Quat* parrAbsFuturePathRot)
{
	AnimTransRotParams atp;

	f32 StartTimeAbs  = m_BlendedRoot.m_relFuturePath[0].m_KeyTimeAbs;
	f32 FutureTimeAbs = StartTimeAbs+fDeltaTime;

	for (uint32 i=0; i<(ANIM_FUTURE_PATH_LOOKAHEAD-1); i++)
	{
		f32 time0 = m_BlendedRoot.m_relFuturePath[i+0].m_KeyTimeAbs;
		f32 time1 = m_BlendedRoot.m_relFuturePath[i+1].m_KeyTimeAbs;

		if (FutureTimeAbs>=time0 && FutureTimeAbs<=time1)
		{
			f32 t = 0;
			f32 t0 = (FutureTimeAbs - time0);
			f32 t1 = (time1-time0);
			if (t1)
				t=t0/t1;

			atp.m_Valid							=	1;
			atp.m_DeltaTime					= fDeltaTime;
			atp.m_TransRot.t.SetLerp( parrAbsFuturePathPos[i], parrAbsFuturePathPos[i+1], t);
			atp.m_TransRot.q.SetNlerp( parrAbsFuturePathRot[i], parrAbsFuturePathRot[i+1], t);
			atp.m_KeyTimeAbs				= m_BlendedRoot.m_relFuturePath[i].m_KeyTimeAbs*(1-t) + m_BlendedRoot.m_relFuturePath[i+1].m_KeyTimeAbs*t;
			atp.m_NormalizedTimeAbs = m_BlendedRoot.m_relFuturePath[i].m_NormalizedTimeAbs*(1-t) + m_BlendedRoot.m_relFuturePath[i+1].m_NormalizedTimeAbs*t;
			return atp;
		}
	}

	return atp;
}
