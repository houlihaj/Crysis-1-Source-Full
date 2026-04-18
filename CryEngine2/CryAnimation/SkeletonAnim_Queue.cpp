//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:Anim_Weights.cpp
//  Implementation of Animation class for parameterisation
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


bool CSkeletonAnim::StartAnimation (const char* szAnimName0,const char* szAnimName1, const char* szAim0,const char* szAim1,  const struct CryCharAnimationParams& Params)
{

#ifdef _DEBUG
	SCOPED_ENABLE_FLOAT_EXCEPTIONS;
#endif

	DEFINE_PROFILER_FUNCTION();
	if (m_pInstance->m_bEnableStartAnimation==0)
		return 0;

	if (Params.m_nLayerID>=numVIRTUALLAYERS || Params.m_nLayerID<0)
		return 0;

	const char* RootName = m_pSkeletonPose->m_parrModelJoints[0].GetJointName();
	if (RootName[0]=='B' && RootName[1]=='i' && RootName[2]=='p' && RootName[3]=='0' && RootName[4]=='1')
	{
		if (g_pCharacterManager->m_IsDedicatedServer)
			return 0; //we don't load animation for humans on dedicated server
	}


//	const char* mname = m_pInstance->GetFilePath();
//	if ( strcmp(mname,"objects/library/architecture/multiplayer/prototype_factory/pf_factory_gate.cga")==0 )
//		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,mname,	"StartAnimation: %s",szAnimName0);


	CryCharAnimationParams AnimPrams = Params;
	if (AnimPrams.m_nLayerID>0)
	{
		//	AnimPrams.m_nFlags|=CA_PARTIAL_BODY_UPDATE;
		uint32 loop=AnimPrams.m_nFlags&CA_LOOP_ANIMATION;
		uint32 repeat=AnimPrams.m_nFlags&CA_REPEAT_LAST_KEY;
		uint32 body=AnimPrams.m_nFlags&CA_PARTIAL_SKELETON_UPDATE;
		if (loop==0 && body && repeat==0)
		{
			AnimPrams.m_nFlags|=CA_REPEAT_LAST_KEY;
			AnimPrams.m_nFlags|=CA_FADEOUT;
		}
	}

	//----------------------------------------------------------------------------

	uint32 nDisableMultilayer = AnimPrams.m_nFlags & CA_DISABLE_MULTILAYER;
	if (nDisableMultilayer)
	{
		AnimPrams.m_fAllowMultilayerAnim=0.0f;
	}

	//----------------------------------------------------------------------------

	if (Console::GetInst().ca_EnableCoolTransitions)
	{
		uint32 nIsHuman = m_pSkeletonPose->m_pModelSkeleton->IsHuman();
		if (nIsHuman && (fabsf(AnimPrams.m_fTransTime)<0.5f))
			AnimPrams.m_fTransTime=0.5f;
	}

	bool bShowWarnings = !(Params.m_nFlags & CA_SUPPRESS_WARNINGS);
	uint32 TrackViewExclusive = Params.m_nFlags & CA_TRACK_VIEW_EXCLUSIVE;
	if (m_TrackViewExclusive && !TrackViewExclusive)
		return 0;


	if (szAnimName0==0)
	{
		g_pILog->LogError ("No name for animation specified");
		assert(!"No name for animation specified");
		return 0;
	}


	const ModelAnimationHeader* pAnim0=0;
	int32 nAnimID0			=-1;
	int32 nGlobalAimID0	=-1;
	int32 nLoopAnim0		=-1;

	const ModelAnimationHeader* pAnim1=0;
	int32 nAnimID1			=-1;
	int32 nGlobalAimID1	=-1;
	int32 nLoopAnim1		=-1;

	const ModelAnimationHeader* pAnimAim0=0; 
	int32 nAnimAimID0		=-1;
	const ModelAnimationHeader* pAnimAim1=0; 
	int32 nAnimAimID1		=-1;

	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;



	//--------------------------------------------------------------
	//---                evaluate the 1st animation              ---
	//--------------------------------------------------------------
	nAnimID0 = pAnimationSet->GetAnimIDByName( szAnimName0 );
	if (nAnimID0<0)	
	{
		if (bShowWarnings)
			AnimFileWarning(m_pInstance->m_pModel->GetModelFilePath(),"animation-name '%s' not in CAL-file", szAnimName0 );
		assert(!"animation-name not in CAL-file");
		return 0;
	}
	//check if the asset exists
	pAnim0 = pAnimationSet->GetModelAnimationHeader(nAnimID0);

	if (pAnim0==0)	
	{
		assert(0);
		return 0;
	}

	uint32 nGlobalID0 = pAnim0->m_nGlobalAnimId;
	GlobalAnimationHeader& rGlobalAnimHeader0 = g_AnimationManager.m_arrGlobalAnimations[nGlobalID0];

	if (rGlobalAnimHeader0.IsAimposeUnloaded())
	{
		if (bShowWarnings)
			AnimFileWarning(m_pInstance->m_pModel->GetModelFilePath(),"trying to play an unloaded aimpose '%s'", szAnimName0 );
		return 0;
	}

	if (rGlobalAnimHeader0.IsAssetOnDemand())
	{
		//int a = 0;
		if (rGlobalAnimHeader0.IsAssetLoaded()==0)
		{
			pAnimationSet->ReloadCAF(nGlobalID0);
			pAnim0 = pAnimationSet->GetModelAnimationHeader(nAnimID0);
			if (!pAnim0)
				return 0;
		} 
		rGlobalAnimHeader0.m_nPlayCount++;

		// LoadAnimation
	}


	nLoopAnim0=rGlobalAnimHeader0.IsAssetCycle();

	if (rGlobalAnimHeader0.IsAssetLMG())
	{
		nLoopAnim0 = 0;
		if (rGlobalAnimHeader0.IsAssetLMGValid()==0) 
		{
			if (bShowWarnings)
				AnimFileWarning(m_pInstance->m_pModel->GetModelFilePath(),"locomotion group '%s' is invalid", pAnim0->GetAnimName());

			return 0;
		}
		rGlobalAnimHeader0.m_nPlayedCount=1;
	}

	//--------------------------------------------------------------
	//---                evaluate the 2nd animation              ---
	//--------------------------------------------------------------
	if (szAnimName1)
	{
		nAnimID1 = pAnimationSet->GetAnimIDByName( szAnimName1 );
		if (nAnimID1<0)	
		{
			if (bShowWarnings)
				AnimFileWarning(m_pInstance->m_pModel->GetModelFilePath(),"animation-name '%s' not in CAL-file", szAnimName1);
			assert(!"animation-name not in CAL-file");
			return 0;
		}

		pAnim1 = pAnimationSet->GetModelAnimationHeader(nAnimID1); 
		if (pAnim1==0)	
		{
			assert(!"model animation header doeanst exist");	
			return 0;
		}
		uint32 nGlobalID1 = pAnim1->m_nGlobalAnimId;
		GlobalAnimationHeader& rGlobalAnimHeader1 = g_AnimationManager.m_arrGlobalAnimations[nGlobalID1];

		if (rGlobalAnimHeader1.IsAimposeUnloaded())
		{
			if (bShowWarnings)
				AnimFileWarning(m_pInstance->m_pModel->GetModelFilePath(),"trying to play an unloaded aimpose '%s'", szAnimName1 );
			return 0;
		}

		if (rGlobalAnimHeader1.IsAssetOnDemand())
		{
			if (rGlobalAnimHeader1.IsAssetLoaded()==0)
			{
				pAnimationSet->ReloadCAF(nGlobalID1);
				pAnim1 = pAnimationSet->GetModelAnimationHeader(nAnimID1);
				if (!pAnim1)
					return 0;
			}
			rGlobalAnimHeader1.m_nPlayCount++;
			// LoadAnimation
		}

		nLoopAnim1=rGlobalAnimHeader1.IsAssetCycle();
		if (rGlobalAnimHeader1.IsAssetLMG())
		{
			nLoopAnim1 = 0;
			if (rGlobalAnimHeader1.IsAssetLMGValid()==0) 
			{
				if (bShowWarnings)
					AnimFileWarning(m_pInstance->m_pModel->GetModelFilePath(),"locomotion group '%s' is invalid", pAnim1->GetAnimName());
				return 0;
			}
		}

	}


	//--------------------------------------------------------------
	//---                evaluate the aim-pose                   ---
	//--------------------------------------------------------------
	if (szAim0)
	{
		nAnimAimID0 = pAnimationSet->GetAnimIDByName( szAim0 );
		if (nAnimAimID0<0)	
		{
			if (bShowWarnings)
				AnimFileWarning(m_pInstance->m_pModel->GetModelFilePath(),"animation-name '%s' not in CAL-file", szAim0);
			assert(0);
			return 0;
		}
		//check if the asset exists
		pAnimAim0 = pAnimationSet->GetModelAnimationHeader(nAnimAimID0);
		if (pAnimAim0==0)	
		{
			assert(0);	return 0;
		}

		if (szAim1)
		{
			nAnimAimID1 = pAnimationSet->GetAnimIDByName( szAim1 );
			if (nAnimAimID1<0)	
			{
				if (bShowWarnings)
					AnimFileWarning(m_pInstance->m_pModel->GetModelFilePath(),"animation-name '%s' not in CAL-file", szAim1);
				assert(0);
				return 0;
			}
			//check if the asset exists
			pAnimAim1 = pAnimationSet->GetModelAnimationHeader(nAnimAimID1);
			if (pAnimAim1==0)	
			{
				assert(0);	return 0;
			}

		}
	}



	//	if (nLoopAnim0 && m_Superimposed>-1)
	if (m_pSkeletonPose->m_Superimposed)
	{
		uint32 tr=m_AnimationDrivenMotion;
		m_AnimationDrivenMotion=0;
		extern std::vector< std::vector<DebugJoint> > g_arrSkeletons;
		extern int32 g_nAnimID;
		extern int32 g_nGlobalAnimID;

		g_nAnimID				=	nAnimID0;
		g_nGlobalAnimID	=	nGlobalID0;

		pAnimationSet->SetFootplantBitsAutomatically( g_arrSkeletons, nAnimID0,nGlobalID0, m_pSkeletonPose->m_pModelSkeleton->m_IdxArray[eIM_LHeelIdx],m_pSkeletonPose->m_pModelSkeleton->m_IdxArray[eIM_RHeelIdx], m_pSkeletonPose->m_pModelSkeleton->m_IdxArray[eIM_LToe0Idx],m_pSkeletonPose->m_pModelSkeleton->m_IdxArray[eIM_RToe0Idx], m_pSkeletonPose->m_pModelSkeleton->m_IdxArray[eIM_LToe0NubIdx],m_pSkeletonPose->m_pModelSkeleton->m_IdxArray[eIM_RToe0NubIdx],1 );
		m_AnimationDrivenMotion=tr;
	}

	bool result = AnimationToQueue ( pAnim0,nAnimID0,   pAnim1,nAnimID1,   pAnimAim0,nAnimAimID0,pAnimAim1,nAnimAimID1,    Params.m_fInterpolation, -1, AnimPrams) > 0;


	if (TrackViewExclusive)
	{
		int32 Layer = Params.m_nLayerID;
		int32 numAnims = m_arrLayer_AFIFO[Layer].size()-1;
		if (numAnims>0)
			for (int32 i=0; i<numAnims; i++)
				m_arrLayer_AFIFO[Layer][i].m_AnimParams.m_fTransTime=fabsf(m_arrLayer_AFIFO[Layer][i].m_AnimParams.m_fTransTime*0.01f);
	}

	SetActiveLayer(Params.m_nLayerID,1);
	m_arrLayerBlendingTime[Params.m_nLayerID] = Params.m_fLayerBlendIn;
	return result;
}




// stops the animation at the given layer, and returns true if the animation was
// actually stopped (if the layer existed and the animation was played there)
bool CSkeletonAnim::StopAnimationInLayer (int32 nLayer, f32 BlendOutTime)
{
	if (nLayer<0 || nLayer>=numVIRTUALLAYERS)
	{
		AnimFileWarning(m_pInstance->m_pModel->GetModelFilePath(),"illegal layer index used in function StopAnimationInLayer: '%d'", nLayer);
		assert(0);
		return 0;
	}

//	const char* mname = m_pInstance->GetFilePath();
//	if ( strcmp(mname,"objects/library/architecture/multiplayer/prototype_factory/pf_factory_gate.cga")==0 )
//		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,mname,	"StopAnimationInLayer");

	SetActiveLayer(nLayer,0);
	m_arrLayerBlendingTime[nLayer]=BlendOutTime;
	if (nLayer)
		return 1;

	ClearFIFOLayer(nLayer);

	return 1;
}

// stops the animation at the given layer, and returns true if the animation was
// actually stopped (if the layer existed and the animation was played there)
bool CSkeletonAnim::StopAnimationsAllLayers ()
{

//	const char* mname = m_pInstance->GetFilePath();
//	if ( strcmp(mname,"objects/library/architecture/multiplayer/prototype_factory/pf_factory_gate.cga")==0 )
//		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,mname,	"StopAnimationsAllLayers");

	for (uint32 i=0; i<numVIRTUALLAYERS; i++)
		ClearFIFOLayer(i);
	return 1;
}


//////////////////////////////////////////////////////////////////////////
// push a new animation into the queue
//////////////////////////////////////////////////////////////////////////
uint32 CSkeletonAnim::AnimationToQueue ( const ModelAnimationHeader* pAnim0,int nID0,  const ModelAnimationHeader* pAnim1,int nID1,       const ModelAnimationHeader* pAnimAim0,int nAimID0,const ModelAnimationHeader* pAnimAim1,int nAimID1,   f32 InterpVal, f32 btime, const CryCharAnimationParams& AnimParams)
{
	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;
	uint32 status=0;

	if (pAnim0)
	{
		uint32 nGlobalAnimId = pAnim0->m_nGlobalAnimId;
		GlobalAnimationHeader& rGlobalAnimHeader0 = g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimId];
		if (rGlobalAnimHeader0.IsAssetLMG())
			status|=1;
	}

	if (pAnim1)
	{
		uint32 nGlobalAnimId = pAnim1->m_nGlobalAnimId;
		GlobalAnimationHeader& rGlobalAnimHeader1 = g_AnimationManager.m_arrGlobalAnimations[nGlobalAnimId];
		if (rGlobalAnimHeader1.IsAssetLMG())
			status|=2;
	}

	uint32 aar=AnimParams.m_nFlags&CA_ALLOW_ANIM_RESTART;
	if (aar==0)
	{
		//don't allow to start the same animation twice
		uint32 numAnimsOnStack = m_arrLayer_AFIFO[AnimParams.m_nLayerID].size();
		if (numAnimsOnStack)
		{

			//check is it the same animation only if the aim poses are different
			int32 nPrevAimID0 = m_arrLayer_AFIFO[AnimParams.m_nLayerID][numAnimsOnStack-1].m_nAnimAimID0;
			if (nPrevAimID0<0) nPrevAimID0=-1;

			if (nPrevAimID0==nAimID0)
			{
				CAnimation const & lastAnimation = m_arrLayer_AFIFO[AnimParams.m_nLayerID][numAnimsOnStack-1];

				int32 id0=lastAnimation.m_LMG0.m_nLMGID;
				if (id0<0)
					id0=lastAnimation.m_LMG0.m_nAnimID[0];
				if (id0<0) id0=-1;

				int32 id1=lastAnimation.m_LMG1.m_nLMGID;
				if (id1<0)
					id1=lastAnimation.m_LMG1.m_nAnimID[0];
				if (id1<0) id1=-1;

				int32 num=(id0>=0)+(id1>=0); 

				if (status==0) 
				{
					if (num==1)
						if (lastAnimation.m_LMG0.m_nAnimID[0]==nID0)
							return 0;
					if (num==2)
						if (lastAnimation.m_LMG0.m_nAnimID[0]==nID0 && lastAnimation.m_LMG1.m_nAnimID[0]==nID1)
							return 0;
				}

				if (status==1) 
				{
					if (num==1)
						if (lastAnimation.m_LMG0.m_nLMGID==nID0)
							return 0;
					if (num==2)
						if (lastAnimation.m_LMG0.m_nLMGID==nID0 && lastAnimation.m_LMG1.m_nAnimID[0]==nID1)
							return 0;
				}

				if (status==2) 
				{
					if (num==1)
						if (lastAnimation.m_LMG0.m_nAnimID[0]==nID0)
							return 0;
					if (num==2)
						if (lastAnimation.m_LMG0.m_nAnimID[0]==nID0 && lastAnimation.m_LMG1.m_nLMGID==nID1)
							return 0;
				}

				if (status==3) 
				{
					if (num==1)
						if (lastAnimation.m_LMG0.m_nLMGID==nID0)
							return 0;
					if (num==2)
						if (lastAnimation.m_LMG0.m_nLMGID==nID0 && lastAnimation.m_LMG1.m_nLMGID==nID1)
							return 0;
				}
			}
		}
	}


	if (AnimParams.m_nFlags&CA_REMOVE_FROM_FIFO)
	{
		uint32 numAnims=m_arrLayer_AFIFO[AnimParams.m_nLayerID].size();
		if (numAnims>0x0e)
		{
			for (uint32 i=1; i<numAnims; i++)	
				m_arrLayer_AFIFO[AnimParams.m_nLayerID][i-1]=m_arrLayer_AFIFO[AnimParams.m_nLayerID][i];

			m_arrLayer_AFIFO[AnimParams.m_nLayerID].pop_back();
		}
	}


	CAnimation AnimOnStack;

	if (pAnim0)
	{
		AnimOnStack.m_LMG0 = pAnimationSet->BuildRealTimeLMG(pAnim0,nID0);

		if (pAnimAim0)
		{
			AnimOnStack.m_strAimPosName0	=	pAnimAim0->GetAnimName();
			AnimOnStack.m_nAnimAimID0			= nAimID0;
			AnimOnStack.m_nGlobalAimID0		= pAnimAim0->m_nGlobalAnimId;

			if (pAnimAim1)
			{
				AnimOnStack.m_strAimPosName1	=	pAnimAim1->GetAnimName();
				AnimOnStack.m_nAnimAimID1			= nAimID1;
				AnimOnStack.m_nGlobalAimID1		= pAnimAim1->m_nGlobalAnimId;
			}
		}

	}

	if (pAnim1)
		AnimOnStack.m_LMG1 = pAnimationSet->BuildRealTimeLMG(pAnim1,nID1);

	// AnimParams is not initialized in the constructor anymore.
	// BuildRealTimeLMG does NOT initialize AnimParams implicitly, for cases where the caller overwrite stuff anyway. 
	AnimOnStack.m_LMG0.m_BlendSpace.Init();
	AnimOnStack.m_LMG1.m_BlendSpace.Init();

	AnimOnStack.m_nSegHighest=0;

	uint32 numAnims0 = AnimOnStack.m_LMG0.m_numAnims;
	for (uint32 i=0; i<numAnims0; i++)
	{
		int32 GlobalID = AnimOnStack.m_LMG0.m_nGlobalID[i];
		GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[ GlobalID ];
		rGAH.StartAnimation();
		int32 seg = rGAH.m_Segments;
		if (AnimOnStack.m_nSegHighest<seg)
			AnimOnStack.m_nSegHighest=seg;
	}
	uint32 numAnims1 = AnimOnStack.m_LMG1.m_numAnims;
	for (uint32 i=0; i<numAnims1; i++)
	{
		int32 GlobalID = AnimOnStack.m_LMG1.m_nGlobalID[i];
		GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[ GlobalID ];
		rGAH.StartAnimation();
		int32 seg = rGAH.m_Segments;
		if (AnimOnStack.m_nSegHighest<seg)
			AnimOnStack.m_nSegHighest=seg;
	}

	AnimOnStack.m_fIWeight						=	InterpVal;						 

	if (AnimParams.m_fKeyTime >= 0.0f)
	{
		assert(AnimParams.m_fKeyTime<=1.0f);
		AnimOnStack.m_fAnimTimePrev				=	AnimParams.m_fKeyTime;
		AnimOnStack.m_fAnimTime						=	AnimParams.m_fKeyTime;
	}
	else
	{
		AnimOnStack.m_fAnimTimePrev				=	0.0f;
		AnimOnStack.m_fAnimTime						=	0.0f;
	}

	AnimOnStack.m_fTransitionPriority	=	0.0f;
	AnimOnStack.m_fTransitionWeight   =	0.0f;
	AnimOnStack.m_nRepeatCount        = 0;
	AnimOnStack.m_AnimParams					=	AnimParams;
	assert(AnimOnStack.m_fAnimTime>=0.0f);
	assert(AnimOnStack.m_fAnimTime<=1.0f);

	UpdateMotionParamDescs(AnimOnStack.m_LMG0, 1.0f);
	UpdateMotionParamDescs(AnimOnStack.m_LMG1, 1.0f);

	m_pSkeletonPose->UpdateJointControllers(AnimOnStack);

	m_arrLayer_AFIFO[AnimParams.m_nLayerID].push_back( AnimOnStack );

	uint32 numAnimsInQueue = m_arrLayer_AFIFO[AnimParams.m_nLayerID].size();
	if (numAnimsInQueue>0x10)
	{
		AnimFileWarning(m_pInstance->m_pModel->GetModelFilePath(),"Animation-queue overflow. More then %d entries. This is a serious performance problem.",numAnimsInQueue);

		if (numAnimsInQueue>0x80)
		{
			//this might be the perfect moment for a FATAL-ERROR
		}
	}
	return 1;
}



void CSkeletonAnim::ClearFIFOLayer( int32 nLayer )
{
	uint32 numAnimsInLayer=m_arrLayer_AFIFO[nLayer].size();
	for (size_t i=0; i<numAnimsInLayer; i++)
		UnloadAnimationAssets(m_arrLayer_AFIFO[nLayer],i);

	m_arrLayer_AFIFO[nLayer].clear();
}


void CSkeletonAnim::UnloadAnimationAssets(std::vector<CAnimation>& arrAFIFO, int num)
{
	CAnimation& rAnim = arrAFIFO[num];

	uint32 numAnims0 = rAnim.m_LMG0.m_numAnims;
	for (uint32 i=0; i<numAnims0; i++)
	{
		int32 GlobalID = rAnim.m_LMG0.m_nGlobalID[i];
		GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[ GlobalID ];
		g_AnimationManager.m_arrGlobalAnimations[ GlobalID ].StopAnimation();
		if (rGAH.IsAssetOnDemand())
		{
			if (rGAH.m_nPlayCount)
				rGAH.m_nPlayCount--;
			if (rGAH.m_nPlayCount==0)
				g_AnimationManager.UnloadAnimation(GlobalID);
		}
	}

	uint32 numAnims1 = rAnim.m_LMG1.m_numAnims;
	for (uint32 i=0; i<numAnims1; i++)
	{
		int32 GlobalID = rAnim.m_LMG1.m_nGlobalID[i];
		GlobalAnimationHeader& rGAH = g_AnimationManager.m_arrGlobalAnimations[ GlobalID ];
		g_AnimationManager.m_arrGlobalAnimations[ GlobalID ].StopAnimation();
		if (rGAH.IsAssetOnDemand())
		{
			if (rGAH.m_nPlayCount)
				rGAH.m_nPlayCount--;
			if (rGAH.m_nPlayCount==0)
				g_AnimationManager.UnloadAnimation(GlobalID);
		}
	}

}

