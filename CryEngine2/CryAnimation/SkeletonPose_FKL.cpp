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

#include "CharacterManager.h"
#include "CharacterInstance.h"
#include "Model.h"
#include "ModelSkeleton.h"
#include <float.h>
#include "ControllerPQ.h"


////////////////////////////////////////////////////////////////////////////
//                  playback of an animation in a layer                   //
////////////////////////////////////////////////////////////////////////////
void CSkeletonPose::Playback (const CAnimation& rAnim,uint32 nVLayerNo, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling  )
{
	//optimized for playback of one animation per queue
	if (rAnim.m_LMG1.m_nAnimID[0]<0)
		Playback_1( rAnim,nVLayerNo,nRedirectLayer, parrRelQuats, parrControllerStatus, fScaling  );
	else
		Playback_2( rAnim,nVLayerNo,nRedirectLayer, parrRelQuats, parrControllerStatus, fScaling  );
}


///////////////////////////////////////////////////////////////////////////////////////////
//        evaluation of two animations and blending into one single animation            //
///////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonPose::Transition( CAnimation* parrActiveAnims, uint32 numAnims, uint32 nVLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling  )
{
	assert(numAnims>1);

	//	float fColor[4] = {0,1,0,1};
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 2.3f, fColor, false,"numAnims: %d",numAnims );	g_YLine+=16.0f;

	uint32 numJoints=m_AbsolutePose.size(); 
	CAnimation rAnim0 = parrActiveAnims[0 ];
	Playback( rAnim0,nVLayer,TMPREG00, parrRelQuats, parrControllerStatus, fScaling );

	f32 t=0.0f;
	for (uint32 i=1; i<numAnims; i++)
	{
		CAnimation rAnim1 = parrActiveAnims[i];
		Playback( rAnim1,nVLayer,TMPREG01, parrRelQuats, parrControllerStatus, fScaling );

		t = rAnim1.m_fTransitionPriority;
		//	t= t*t * ( 3 - 2 * t );
		//	t= t*t * ( 3 - 2 * t );
		//	t= t*t * ( 3 - 2 * t );

		//multi-layer blending
		TwoWayBlend(TMPREG00,TMPREG01,TMPREG02,t, parrRelQuats, parrControllerStatus );
		if ( i!=(numAnims-1) )
			TwoWayBlend(TMPREG02,TMPREG02,TMPREG00,0, parrRelQuats, parrControllerStatus );
	}


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	f32 lblend	=	m_pSkeletonAnim->m_arrLayerBlending[nVLayer]*m_pSkeletonAnim->m_BlendedRoot.m_fAllowMultilayerAnim;
	lblend -= 0.5f;
	lblend = 0.5f+lblend/(0.5f+2.0f*lblend*lblend);

	for (uint32 j=0; j<numJoints; j++)
	{
		Status4 status = parrControllerStatus[numJoints*TMPREG02+j];
		assert(status.o<2);
		assert(status.p<2);
		if (status.o)
		{
			if (m_arrControllerInfo[j].o)
				m_RelativePose[j].q.SetNlerp(m_RelativePose[j].q, parrRelQuats[numJoints*TMPREG02+j].q, lblend);
			else
				m_RelativePose[j].q = parrRelQuats[numJoints*TMPREG02+j].q;
		}

		if (status.p)
		{
			if (m_arrControllerInfo[j].p)
				m_RelativePose[j].t.SetLerp( m_RelativePose[j].t, parrRelQuats[numJoints*TMPREG02+j].t, lblend);
			else
				m_RelativePose[j].t = parrRelQuats[numJoints*TMPREG02+j].t;
		}
		m_arrControllerInfo[j].ops |= status.ops;
		assert(m_arrControllerInfo[j].o<2);
		assert(m_arrControllerInfo[j].p<2);
	}

}






////////////////////////////////////////////////////////////////////////////
//                    playback of one single animation                    //
////////////////////////////////////////////////////////////////////////////
void CSkeletonPose::Playback_1 (const CAnimation& rAnim, uint32 nVLayer, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling  )
{
	if (nVLayer==0)
		CryError("nVLayer==0");

	if (rAnim.m_LMG0.m_nLMGID<0)
	{
		if (nRedirectLayer==FINALREL)
			return SingleEvaluation (CopyAnim0(rAnim,0), nVLayer, parrRelQuats, parrControllerStatus, fScaling);
		else
			return SingleEvaluation (CopyAnim0(rAnim,0), nVLayer, nRedirectLayer, parrRelQuats, parrControllerStatus, fScaling);
	}
	else
	{
		if (nRedirectLayer==FINALREL)
			return SingleEvaluationLMG (rAnim, nVLayer, nRedirectLayer, parrRelQuats, parrControllerStatus, fScaling);
		else
			return SingleEvaluationLMG (rAnim, nVLayer, nRedirectLayer, parrRelQuats, parrControllerStatus, fScaling);
	}
}


////////////////////////////////////////////////////////////////////////////
//               playback of 2 interpolated animations                    //
////////////////////////////////////////////////////////////////////////////
void CSkeletonPose::Playback_2 (const CAnimation& rAnim, uint32 nVLayer, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling  )
{
	if (nVLayer==0)
		CryError("nVLayer==0");

	uint32 slots=3;

	if (rAnim.m_fIWeight>0.99f)
		slots=2;
	if (rAnim.m_fIWeight<0.01f)
		slots=1;


	if (slots==1)
	{
		if (nRedirectLayer==FINALREL)
		{
			//multi-layer blending
			Playback_1( rAnim,nVLayer,MULTILAYER, parrRelQuats, parrControllerStatus, fScaling);
			f32 lblend	=	m_pSkeletonAnim->m_arrLayerBlending[nVLayer];
			lblend -= 0.5f;
			lblend = 0.5f+lblend/(0.5f+2.0f*lblend*lblend);

			uint32 numJoints = m_AbsolutePose.size();
			for (uint32 j=0; j<numJoints; j++)
			{
				Status4 status = parrControllerStatus[numJoints*MULTILAYER+j];
				if (status.o)
				{
					if (m_arrControllerInfo[j].o)
						m_RelativePose[j].q.SetNlerp( m_RelativePose[j].q, parrRelQuats[numJoints*MULTILAYER+j].q, lblend);
					else
						m_RelativePose[j].q = parrRelQuats[numJoints*MULTILAYER+j].q;
				}
				if (status.p)
				{
					if (m_arrControllerInfo[j].p)
						m_RelativePose[j].t.SetLerp( m_RelativePose[j].t, parrRelQuats[numJoints*MULTILAYER+j].t, lblend);
					else
						m_RelativePose[j].t = parrRelQuats[numJoints*MULTILAYER+j].t;
				}
				m_arrControllerInfo[j].ops |= status.ops;
			}
		}
		else
		{
			Playback_1( rAnim,nVLayer,nRedirectLayer, parrRelQuats, parrControllerStatus, fScaling);
		}
		return;
	}


	if (slots==2)
	{
		CAnimation a1=rAnim;
		a1.m_LMG0.m_nLMGID				=	rAnim.m_LMG1.m_nLMGID;
		a1.m_LMG0.m_nGlobalLMGID	=	rAnim.m_LMG1.m_nGlobalLMGID;
		a1.m_LMG0.m_numAnims			=	rAnim.m_LMG1.m_numAnims;
		for(int32 i=0; i<a1.m_LMG0.m_numAnims; i++)
		{
			a1.m_LMG0.m_nAnimID[i]					= rAnim.m_LMG1.m_nAnimID[i];
			a1.m_LMG0.m_nGlobalID[i]				= rAnim.m_LMG1.m_nGlobalID[i];
			a1.m_LMG0.m_fBlendWeight[i]			= rAnim.m_LMG1.m_fBlendWeight[i];
			a1.m_LMG0.m_nSegmentCounter[i]	= rAnim.m_LMG1.m_nSegmentCounter[i];
			a1.m_LMG0.m_jointControllers[i]	= rAnim.m_LMG1.m_jointControllers[i];
		}

		if (nRedirectLayer==FINALREL)
		{
			//multi-layer blending
			Playback_1( a1,nVLayer,MULTILAYER, parrRelQuats, parrControllerStatus, fScaling);

			f32 lblend	=	m_pSkeletonAnim->m_arrLayerBlending[nVLayer];
			lblend -= 0.5f;
			lblend = 0.5f+lblend/(0.5f+2.0f*lblend*lblend);

			uint32 numJoints = m_AbsolutePose.size();
			for (uint32 j=0; j<numJoints; j++)
			{
				Status4 status = parrControllerStatus[numJoints*MULTILAYER+j];
				if (status.o)
				{
					if (m_arrControllerInfo[j].o)
						m_RelativePose[j].q.SetNlerp(m_RelativePose[j].q, parrRelQuats[numJoints*MULTILAYER+j].q, lblend);
					else
						m_RelativePose[j].q = parrRelQuats[numJoints*MULTILAYER+j].q;
				}
				if (status.p)
				{
					if (m_arrControllerInfo[j].p)
						m_RelativePose[j].t.SetLerp( m_RelativePose[j].t, parrRelQuats[numJoints*MULTILAYER+j].t, lblend);
					else
						m_RelativePose[j].t = parrRelQuats[numJoints*MULTILAYER+j].t;
				}
				m_arrControllerInfo[j].ops |= status.ops;
			}
		}
		else
		{
			Playback_1( a1,nVLayer,nRedirectLayer, parrRelQuats, parrControllerStatus, fScaling);
		}
		return;
	}


	if (slots==3)
	{
		//blend both slots
		Playback_1( rAnim,nVLayer,TMPREG02, parrRelQuats, parrControllerStatus, fScaling);

		CAnimation a1=rAnim;
		a1.m_LMG0.m_nLMGID				=	rAnim.m_LMG1.m_nLMGID;
		a1.m_LMG0.m_nGlobalLMGID	=	rAnim.m_LMG1.m_nGlobalLMGID;
		a1.m_LMG0.m_numAnims			=	rAnim.m_LMG1.m_numAnims;
		for(int32 i=0; i<a1.m_LMG0.m_numAnims; i++)
		{
			a1.m_LMG0.m_nAnimID[i]					= rAnim.m_LMG1.m_nAnimID[i];
			a1.m_LMG0.m_nGlobalID[i]				= rAnim.m_LMG1.m_nGlobalID[i];
			a1.m_LMG0.m_fBlendWeight[i]			= rAnim.m_LMG1.m_fBlendWeight[i];
			a1.m_LMG0.m_nSegmentCounter[i]	= rAnim.m_LMG1.m_nSegmentCounter[i];
			a1.m_LMG0.m_jointControllers[i]	= rAnim.m_LMG1.m_jointControllers[i];
		}
		Playback_1( a1,nVLayer,TMPREG03, parrRelQuats, parrControllerStatus, fScaling);

		f32	t = rAnim.m_fIWeight;

		if (nRedirectLayer==FINALREL)
		{
			//multi-layer blending
			TwoWayBlend(TMPREG02,TMPREG03,MULTILAYER,t, parrRelQuats, parrControllerStatus);
			f32 lblend	=	m_pSkeletonAnim->m_arrLayerBlending[nVLayer];
			lblend -= 0.5f;
			lblend = 0.5f+lblend/(0.5f+2.0f*lblend*lblend);


			uint32 numJoints = m_AbsolutePose.size();
			for (uint32 j=0; j<numJoints; j++)
			{
				Status4 status = parrControllerStatus[numJoints*MULTILAYER+j];
				if (status.o)
				{
					if (m_arrControllerInfo[j].o)
						m_RelativePose[j].q.SetNlerp(m_RelativePose[j].q, parrRelQuats[numJoints*MULTILAYER+j].q, lblend);
					else
						m_RelativePose[j].q = parrRelQuats[numJoints*MULTILAYER+j].q;
				}
				if (status.p)
				{
					if (m_arrControllerInfo[j].p)
						m_RelativePose[j].t.SetLerp( m_RelativePose[j].t, parrRelQuats[numJoints*MULTILAYER+j].t, lblend);
					else
						m_RelativePose[j].t = parrRelQuats[numJoints*MULTILAYER+j].t;
				}
				m_arrControllerInfo[j].ops |= status.ops;
			}

		}
		else
		{
			TwoWayBlend(TMPREG02,TMPREG03,nRedirectLayer,t, parrRelQuats, parrControllerStatus);
		}

	}
}











//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//     evaluation of one single animation and copy the result into the final joint-array                    //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonPose::SingleEvaluation (const SAnimation& rAnim, uint32 nVLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling )
{
	//DEFINE_PROFILER_FUNCTION();

	assert(rAnim.m_nEAnimID>=0);
	assert(rAnim.m_nEGlobalID>=0);

	uint32 numJoints = m_AbsolutePose.size();
	uint32 PartialBodyUpdates = rAnim.m_EAnimParams.m_nFlags&CA_PARTIAL_SKELETON_UPDATE;
	f32 fMultiLayerBlend = m_pSkeletonAnim->m_BlendedRoot.m_fAllowMultilayerAnim;

	f32 TimeFlow  = m_pInstance->m_fDeltaTime;
	f32 time_prev	=	rAnim.m_fETimeOldPrev;
	f32 time_old	=	rAnim.m_fETimeOld;
	f32 time_new	=	rAnim.m_fETimeNew;

	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID];
	rGlobalAnimHeader.m_nPlayedCount=1;

	float fColor[4] = {0,1,0,1};
	extern float g_YLine;
	int32 seg = rGlobalAnimHeader.m_Segments-1;
	int32 segcount = rAnim.m_nESegmentCounter;
	f32 segdur = rGlobalAnimHeader.GetSegmentDuration(segcount);
	f32 totdur = rGlobalAnimHeader.m_fTotalDuration;
	f32 segbase = rGlobalAnimHeader.m_SegmentsTime[segcount];
	f32 percent = segdur/totdur;
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Segments: %02x   SegmentCounter: %02x", seg, segcount );	g_YLine+=10.0f;
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"SegDur: %f   TotDur: %f   percent:%f", segdur, totdur, percent );	g_YLine+=10.0f;

	time_old	=	time_old*percent+segbase;
	time_new	=	time_new*percent+segbase;
	assert(time_new>=0.0f && time_new<=1.0f);	



	const CModelJoint* pModelJoint = GetModelJointPointer(0);
	IController* pController = rAnim.m_jointControllers[0];


	//-------------------------------------------------------------------------------------
	//----             evaluate all controllers for this animation                    -----
	//-------------------------------------------------------------------------------------
	if (m_bFullSkeletonUpdate)
	{
		extern uint32 g_SkeletonUpdates; 	g_SkeletonUpdates++;

		for (uint32 j=0; j<numJoints; j++)
		{
			if (GetJointMask(j,nVLayer) == 0)
				continue;

			pController = rAnim.m_jointControllers[j];
			if (pController)
			{
				//multi-layer blending
				Quat rot;	Vec3 pos;

				f32 lblend	=	m_pSkeletonAnim->m_arrLayerBlending[nVLayer]*fMultiLayerBlend;
				lblend -= 0.5f;
				lblend = 0.5f+lblend/(0.5f+2.0f*lblend*lblend);

				Status4 status = pController->GetOP(rAnim.m_nEGlobalID, time_new, rot, pos );

				if (status.o)
				{
					if (m_arrControllerInfo[j].o)
						m_RelativePose[j].q.SetNlerp(m_RelativePose[j].q,rot,lblend);
					else 
						m_RelativePose[j].q=rot;
				}

				if (status.p)
				{
					if (m_arrControllerInfo[j].p)
						m_RelativePose[j].t.SetLerp( m_RelativePose[j].t,pos,lblend);
					else
						m_RelativePose[j].t=pos;
				}
				m_arrControllerInfo[j].ops |= status.ops;

			}

			//For a partial body-update we use just existing controllers 
			if (PartialBodyUpdates==0)
			{
				if (m_arrControllerInfo[j].o==0)
					m_RelativePose[j].q = pModelJoint[j].m_DefaultRelPose.q;
				if (m_arrControllerInfo[j].p==0)
					m_RelativePose[j].t = pModelJoint[j].m_DefaultRelPose.t;
				m_arrControllerInfo[j].p = 1;
				m_arrControllerInfo[j].o = 1;
			}
			//if (m_arrControllerInfo[j].p)
			//	m_RelativePose[j].t *= fScaling;

		}

	}

	m_pSkeletonAnim->m_IsAnimPlaying |= (1<<nVLayer);
}




////////////////////////////////////////////////////////////////////////////
//                 evaluation of one single animation                     //
////////////////////////////////////////////////////////////////////////////
void CSkeletonPose::SingleEvaluation (const SAnimation& rAnim, uint32 nVLayer, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling )
{
	assert(nVLayer<numVIRTUALLAYERS);
	assert(nRedirectLayer<numTMPLAYERS);
	assert(rAnim.m_nEAnimID>=0);
	assert(rAnim.m_nEGlobalID>=0);

	uint32 numJoints = m_AbsolutePose.size();
	uint32 PartialBodyUpdates = rAnim.m_EAnimParams.m_nFlags&CA_PARTIAL_SKELETON_UPDATE;

	memset( &parrControllerStatus[numJoints*nRedirectLayer].ops,0,numJoints*sizeof(Status4) );

	f32 TimeFlow  = m_pInstance->m_fDeltaTime;
	f32 time_prev	=	rAnim.m_fETimeOldPrev;
	f32 time_old	=	rAnim.m_fETimeOld;
	f32 time_new	=	rAnim.m_fETimeNew;


	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID];
	rGlobalAnimHeader.m_nPlayedCount=1;

	float fColor[4] = {0,1,0,1};
	extern float g_YLine;
	int32 seg = rGlobalAnimHeader.m_Segments-1;
	int32 segcount = rAnim.m_nESegmentCounter;
	f32 segdur	= rGlobalAnimHeader.GetSegmentDuration(segcount);
	f32 totdur	= rGlobalAnimHeader.m_fTotalDuration;
	f32 segbase = rGlobalAnimHeader.m_SegmentsTime[segcount];
	f32 percent = segdur/totdur;
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Segments: %02x   SegmentCounter: %02x", seg, segcount );	g_YLine+=10.0f;
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"SegDur: %f   TotDur: %f   percent:%f", segdur, totdur, percent );	g_YLine+=10.0f;

	time_old	=	time_old*percent+segbase;
	time_new	=	time_new*percent+segbase;
	assert(time_new>=0.0f && time_new<=1.0f);	


	const CModelJoint* pModelJoint = GetModelJointPointer(0);

	IController* pController = rAnim.m_jointControllers[0];


	//-------------------------------------------------------------------------------------
	//----             evaluate all controllers for this animation                    -----
	//-------------------------------------------------------------------------------------
	if (m_bFullSkeletonUpdate)
	{
		extern uint32 g_SkeletonUpdates; 	g_SkeletonUpdates++;

		for (uint32 j=0; j<numJoints; j++)
		{
			if (GetJointMask(j,nVLayer) == 0)
				continue;

			pController = rAnim.m_jointControllers[j];
			if (pController)
				parrControllerStatus[numJoints*nRedirectLayer+j] = pController->GetOP(rAnim.m_nEGlobalID, time_new, parrRelQuats[numJoints*nRedirectLayer+j].q, parrRelQuats[numJoints*nRedirectLayer+j].t );

			if (PartialBodyUpdates==0)
			{
				//full-body update. make sure every bone is initialized
				if (parrControllerStatus[numJoints*nRedirectLayer+j].o==0)
					parrRelQuats[numJoints*nRedirectLayer+j].q = pModelJoint[j].m_DefaultRelPose.q;
				if (parrControllerStatus[numJoints*nRedirectLayer+j].p==0)
					parrRelQuats[numJoints*nRedirectLayer+j].t = pModelJoint[j].m_DefaultRelPose.t;
				parrControllerStatus[numJoints*nRedirectLayer+j].o=1;
				parrControllerStatus[numJoints*nRedirectLayer+j].p=1;
			}
			//if (parrControllerStatus[numJoints*nRedirectLayer+j].p)
			//	parrRelQuats[numJoints*nRedirectLayer+j].t *= fScaling;
		}
	}


#ifdef _DEBUG
	for (uint32 j=0; j<numJoints; j++)
	{
		assert(parrControllerStatus[numJoints*nRedirectLayer+j].o<2);
		if (parrControllerStatus[numJoints*nRedirectLayer+j].o)
		{
			Quat q = parrRelQuats[numJoints*nRedirectLayer+j].q;
			assert(parrRelQuats[numJoints*nRedirectLayer+j].q.IsValid());
		}
		assert(parrControllerStatus[numJoints*nRedirectLayer+j].p<2);
		if (parrControllerStatus[numJoints*nRedirectLayer+j].p)
		{
			Vec3 t = parrRelQuats[numJoints*nRedirectLayer+j].t;
			assert(parrRelQuats[numJoints*nRedirectLayer+j].t.IsValid());
		}
	}
#endif


	m_pSkeletonAnim->m_IsAnimPlaying |= (1<<nVLayer);
}




///////////////////////////////////////////////////////////////////////////////////////////
//                       evaluation of a locomotion group                                //
///////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonPose::SingleEvaluationLMG (const CAnimation& rAnim, uint32 nVLayer, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling )
{
	if (nRedirectLayer==FINALREL)
		nRedirectLayer=TMPREG06;

	CAnimation a0=rAnim;

	assert(a0.m_LMG0.m_nLMGID>=0);
	assert(a0.m_LMG0.m_nGlobalLMGID>=0);


	//-----------------------------------------------------------------------------------
	//-----              average all animations in a blend-space                   ------
	//-----------------------------------------------------------------------------------
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[a0.m_LMG0.m_nGlobalLMGID];
	uint32 bt=rGlobalAnimHeader.m_nBlendCodeLMG;

	uint32 numJoints=m_AbsolutePose.size(); 
	for (uint32 j=0; j<numJoints; j++)
	{
		parrControllerStatus[numJoints*TMPREG05+j].ops=0;
		parrControllerStatus[numJoints*TMPREG06+j].ops=0;
	}

	/*		
	g_YLine+=32.0f;		
	float fColor[4] = {1,0,0,1};
	for (uint32 i=0; i<rAnim.m_numEAnims0; i++)
	{
	f32 weight = rAnim.m_fEBlendWeight0[i];
	if (weight)
	{
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"i:%d weight:%f name:%d",i,weight,rAnim.m_nEAnimID0[i] );	
	g_YLine+=10;
	}
	}*/

	//----------------------------------------------------------------------------------
	int8 idf[MAX_LMG_ANIMS];	memset( &idf,1,MAX_LMG_ANIMS);
	int32 ids[MAX_LMG_ANIMS];
	uint32 f=0;
	for (int32 i=0; i<rAnim.m_LMG0.m_numAnims; i++)
	{
		f32 weight = rAnim.m_LMG0.m_fBlendWeight[i];
		if (weight && idf[i])
		{
			idf[i]=0;
			ids[f]=i;
			f++;
			int32 id0=rAnim.m_LMG0.m_nAnimID[i];
			for (int32 q=i+1; q<rAnim.m_LMG0.m_numAnims; q++)
			{
				int32 id1=rAnim.m_LMG0.m_nAnimID[q];
				if (id0==id1 && rAnim.m_LMG0.m_fBlendWeight[q])
				{
					idf[q]=0;
					ids[f]=q;
					f++;
				}
			}
		}
	}

	float fColor2[4] = {1,0,1,1};

	//-------------------------------------------------------------------------------------
	//----------            evaluate and blend root-joint       ---------------------------
	//-------------------------------------------------------------------------------------

	f32 fBlendWeights=0.0f;
	int32 oldid=-1;
	int32 evaluations=0;
	for (uint32 s=0; s<f; s++)
	{
		int32 i	=	ids[s];
		f32 weight  =	 rAnim.m_LMG0.m_fBlendWeight[i];
		int32 newid		=	 rAnim.m_LMG0.m_nAnimID[i];
		//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor2, false,"i:%d weight:%f name:%d",i,weight,newid );	
		//g_YLine+=10;

		if (newid != oldid)
		{
			SingleEvaluation( CopyAnim0(rAnim,i),nVLayer,TMPREG05, parrRelQuats, parrControllerStatus, fScaling  );
			evaluations++;
		}
		if (fBlendWeights==0.0f)
			BLENDLAYER_MOVE(TMPREG05,nRedirectLayer,weight, parrRelQuats, parrControllerStatus);
		else 
			BLENDLAYER_ADD(TMPREG05,nRedirectLayer,weight, parrRelQuats, parrControllerStatus);

		fBlendWeights+=weight;
		oldid=newid;
	}
	//g_YLine+=10;
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor2, false,"Evaluation: %d",evaluations );	
	//g_YLine+=10;

	f32 fTotal = fabsf(fBlendWeights-1.0f);
	if (fTotal>0.05f)
		CryError("CryAnimation: sum of blend-weights for LMG is wrong: %f %s",fBlendWeights,m_pInstance->m_pModel->GetModelFilePath());

	m_pSkeletonAnim->m_IsAnimPlaying |= (1<<nVLayer);
	BLENDLAYER_NORM(nRedirectLayer, parrRelQuats, parrControllerStatus);

	if (nRedirectLayer==TMPREG06)
	{
		m_RelativePose[0] = parrRelQuats[numJoints*TMPREG06+0];
		m_arrControllerInfo[0].ops = parrControllerStatus[numJoints*TMPREG06+0].ops;

		//--------------------------------------------------------------------------------------
		if (m_bFullSkeletonUpdate)
		{
			//	m_arrJoints[1].m_RelativePose.q			= parrRelQuats[TMPREG06][1].q;
			//	m_arrControllerInfo[1].ops	= parrControllerStatus[TMPREG06][1].ops;
			//	if (parrControllerStatus[TMPREG06][1].p)
			//		m_arrJoints[1].m_RelativePose.t		= parrRelQuats[TMPREG06][1].t;

			uint32 numJoints=m_AbsolutePose.size();
			for (uint32 i=1; i<numJoints; i++)
			{
				m_RelativePose[i]				= parrRelQuats[numJoints*TMPREG06+i];
				m_arrControllerInfo[i].ops	= parrControllerStatus[numJoints*TMPREG06+i].ops;
			}
		}
	}
}






void CSkeletonPose::TwoWayBlend(uint32 src0,uint32 src1,uint32 dst, f32 t, QuatT* parrRelQuats, Status4* parrControllerStatus )
{
	assert(t<=1);
	assert(t>=0);

	if (src0 >= numTMPLAYERS || src1 >= numTMPLAYERS) 
	{
		assert(0);
	}


	f32 fColor[4] = { 0.0f, 0.5f, 1.0f, 1 };

	//------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------
	uint32 numJoints=m_AbsolutePose.size();
	uint32 channelO=0;
	assert(parrControllerStatus[numJoints*src0+0].o<2);
	assert(parrControllerStatus[numJoints*src1+0].o<2);
	if (parrControllerStatus[numJoints*src0+0].o) channelO|=1;
	if (parrControllerStatus[numJoints*src1+0].o) channelO|=2;
	if (channelO==1)
	{
		parrRelQuats[numJoints*src1+0].q = parrRelQuats[numJoints*src0+0].q; 
		parrControllerStatus[numJoints*src1+0].o = parrControllerStatus[numJoints*src0+0].o; 
	}
	if (channelO==2)
	{
		parrRelQuats[numJoints*src0+0].q = parrRelQuats[numJoints*src1+0].q; 
		parrControllerStatus[numJoints*src0+0].o = parrControllerStatus[numJoints*src1+0].o; 
	}

	uint32 channelP=0;
	assert(parrControllerStatus[numJoints*src0+0].p<2);
	assert(parrControllerStatus[numJoints*src1+0].p<2);
	if (parrControllerStatus[numJoints*src0+0].p) channelP|=1;
	if (parrControllerStatus[numJoints*src1+0].p) channelP|=2;
	if (channelP==1)
	{
		parrRelQuats[numJoints*src1+0].t = parrRelQuats[numJoints*src0+0].t; 
		parrControllerStatus[numJoints*src1+0].p = parrControllerStatus[numJoints*src0+0].p; 
	}
	if (channelP==2)
	{
		parrRelQuats[numJoints*src0+0].t = parrRelQuats[numJoints*src1+0].t; 
		parrControllerStatus[numJoints*src0+0].p = parrControllerStatus[numJoints*src1+0].p; 
	}



	//--------------------------------------------------------------------
	//parrRelQuats[numJoints*src0+0].q.Normalize();
	//parrRelQuats[numJoints*src1+0].q.Normalize();
	//		float fColor[4] = {0,1,0,1};
	//		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"RelTrans0: %f %f %f",RelTrans0.x,RelTrans0.y,RelTrans0.z );	g_YLine+=16.0f;
	//		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"RelTrans1: %f %f %f",RelTrans1.x,RelTrans1.y,RelTrans1.z );	g_YLine+=16.0f;
	//		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"BMovement.m_relCITranslation: %f %f %f",BMovement.m_relCITranslation.x,BMovement.m_relCITranslation.y,BMovement.m_relCITranslation.z );	g_YLine+=16.0f;

	if (dst==FINALREL)
	{
		if (parrControllerStatus[numJoints*src0+0].o)
			m_RelativePose[0].q.SetSlerp( parrRelQuats[numJoints*src0+0].q, parrRelQuats[numJoints*src1+0].q, t );
		if (parrControllerStatus[numJoints*src0+0].p)
			m_RelativePose[0].t.SetLerp ( parrRelQuats[numJoints*src0+0].t, parrRelQuats[numJoints*src1+0].t, t );
		m_arrControllerInfo[0].ops      = parrControllerStatus[numJoints*src0+0].ops|parrControllerStatus[numJoints*src1+0].ops;
		assert(m_arrControllerInfo[0].o<2);
		assert(m_arrControllerInfo[0].p<2);
	}
	else
	{
		if (parrControllerStatus[numJoints*src0+0].o)
			parrRelQuats[numJoints*dst+0].q.SetSlerp( parrRelQuats[numJoints*src0+0].q, parrRelQuats[numJoints*src1+0].q, t );//	=	BMovement.m_RootOrientation;
		if (parrControllerStatus[numJoints*src0+0].p)
			parrRelQuats[numJoints*dst+0].t.SetLerp ( parrRelQuats[numJoints*src0+0].t, parrRelQuats[numJoints*src1+0].t, t );

		parrControllerStatus[numJoints*dst+0].ops = parrControllerStatus[numJoints*src0+0].ops|parrControllerStatus[numJoints*src1+0].ops;
		assert(parrControllerStatus[numJoints*dst+0].o<2);
		assert(parrControllerStatus[numJoints*dst+0].p<2);
	}


	//----------------------------------------------------------------------------------------------
	//----                  joint blending                           -------------------------------
	//----------------------------------------------------------------------------------------------
	if (m_bFullSkeletonUpdate)
	{
		for (uint32 i=1; i<numJoints; i++)
		{
			channelO=0;
			assert(parrControllerStatus[numJoints*src0+i].o<2);
			assert(parrControllerStatus[numJoints*src1+i].o<2);
			if (parrControllerStatus[numJoints*src0+i].o) channelO|=1;
			if (parrControllerStatus[numJoints*src1+i].o) channelO|=2;
			if (channelO==1)
			{
				parrRelQuats[numJoints*src1+i].q = parrRelQuats[numJoints*src0+i].q; 
				parrControllerStatus[numJoints*src1+i].o = parrControllerStatus[numJoints*src0+i].o; 
				assert(parrControllerStatus[numJoints*src0+i].o<2);
			}
			if (channelO==2)
			{
				parrRelQuats[numJoints*src0+i].q = parrRelQuats[numJoints*src1+i].q; 
				parrControllerStatus[numJoints*src0+i].o = parrControllerStatus[numJoints*src1+i].o; 
				assert(parrControllerStatus[numJoints*src1+i].p<2);
			}

			channelP=0;
			assert(parrControllerStatus[numJoints*src0+i].p<2);
			assert(parrControllerStatus[numJoints*src1+i].p<2);
			if (parrControllerStatus[numJoints*src0+i].p) channelP|=1;
			if (parrControllerStatus[numJoints*src1+i].p) channelP|=2;
			if (channelP==1)
			{
				parrRelQuats[numJoints*src1+i].t = parrRelQuats[numJoints*src0+i].t; 
				parrControllerStatus[numJoints*src1+i].p = parrControllerStatus[numJoints*src0+i].p; 
				assert(parrControllerStatus[numJoints*src0+i].p<2);
			}
			if (channelP==2)
			{
				parrRelQuats[numJoints*src0+i].t = parrRelQuats[numJoints*src1+i].t; 
				parrControllerStatus[numJoints*src0+i].p = parrControllerStatus[numJoints*src1+i].p; 
				assert(parrControllerStatus[numJoints*src1+i].p<2);
			}
		}

		if (dst==FINALREL)
		{
			for (uint32 i=1; i<numJoints; i++)
			{
				assert(parrControllerStatus[numJoints*src0+i].o<2);
				if (parrControllerStatus[numJoints*src0+i].o)
					m_RelativePose[i].q.SetNlerp( parrRelQuats[numJoints*src0+i].q, parrRelQuats[numJoints*src1+i].q, t );

				assert(parrControllerStatus[numJoints*src0+i].p<2);
				if (parrControllerStatus[numJoints*src0+i].p)
					m_RelativePose[i].t.SetLerp ( parrRelQuats[numJoints*src0+i].t, parrRelQuats[numJoints*src1+i].t, t );

				m_arrControllerInfo[i].ops   = parrControllerStatus[numJoints*src0+i].ops;
				assert(m_arrControllerInfo[i].o<2);
				assert(m_arrControllerInfo[i].p<2);
			}
		}
		else
		{
			for (uint32 i=1; i<numJoints; i++)
			{
				assert(parrControllerStatus[numJoints*src0+i].o<2);
				if (parrControllerStatus[numJoints*src0+i].o)
					parrRelQuats[numJoints*dst+i].q.SetNlerp( parrRelQuats[numJoints*src0+i].q, parrRelQuats[numJoints*src1+i].q, t );

				assert(parrControllerStatus[numJoints*src0+i].p<2);
				if (parrControllerStatus[numJoints*src0+i].p)
					parrRelQuats[numJoints*dst+i].t.SetLerp ( parrRelQuats[numJoints*src0+i].t, parrRelQuats[numJoints*src1+i].t, t );

				parrControllerStatus[numJoints*dst+i].ops = parrControllerStatus[numJoints*src0+i].ops | parrControllerStatus[numJoints*src1+i].ops;
				assert(parrControllerStatus[numJoints*dst+i].o<2);
				assert(parrControllerStatus[numJoints*dst+i].p<2);
			}
		}
	}
}





void CSkeletonPose::BLENDLAYER_MOVE(uint32 src,uint32 dst,f32 t, QuatT* parrRelQuats, Status4* parrControllerStatus )
{
	if (src >= numTMPLAYERS) 
		return;

	uint32 numJoints=m_AbsolutePose.size();
	parrRelQuats[numJoints*dst+0].q =	parrRelQuats[numJoints*src+0].q*t;
	parrRelQuats[numJoints*dst+0].t =	parrRelQuats[numJoints*src+0].t*t;
	parrControllerStatus[numJoints*dst+0].ops = parrControllerStatus[numJoints*src+0].ops;

	if (m_bFullSkeletonUpdate)
	{
		for (uint32 i=1; i<numJoints; i++)
		{
			parrRelQuats[numJoints*dst+i].q = parrRelQuats[numJoints*src+i].q*t;
			parrRelQuats[numJoints*dst+i].t = parrRelQuats[numJoints*src+i].t*t;
			parrControllerStatus[numJoints*dst+i].ops = parrControllerStatus[numJoints*src+i].ops;
		}
	}
}


void CSkeletonPose::BLENDLAYER_ADD(uint32 src,uint32 dst,f32 t, QuatT* parrRelQuats, Status4* parrControllerStatus )
{
	if (src >= numTMPLAYERS) 
		return;

	uint32 numJoints=m_AbsolutePose.size();
	parrRelQuats[numJoints*dst+0].q %= parrRelQuats[numJoints*src+0].q*t;
	parrRelQuats[numJoints*dst+0].t += parrRelQuats[numJoints*src+0].t*t;
	parrControllerStatus[numJoints*dst+0].ops |= parrControllerStatus[numJoints*src+0].ops;

	if (m_bFullSkeletonUpdate)
	{
		for (uint32 i=1; i<numJoints; i++)
		{
			parrRelQuats[numJoints*dst+i].q %= parrRelQuats[numJoints*src+i].q*t;
			parrRelQuats[numJoints*dst+i].t += parrRelQuats[numJoints*src+i].t*t;
			parrControllerStatus[numJoints*dst+i].ops |= parrControllerStatus[numJoints*src+i].ops;
		}
	}
}


//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
void CSkeletonPose::BLENDLAYER_NORM(uint32 dst, QuatT* parrRelQuats, Status4* parrControllerStatus )
{
	uint32 numJoints=m_AbsolutePose.size();
	parrRelQuats[numJoints*dst+0].q.v.x=0;
	parrRelQuats[numJoints*dst+0].q.v.y=0;
	parrRelQuats[numJoints*dst+0].q.Normalize();
	if (m_bFullSkeletonUpdate)
	{
		for (uint32 i=1; i<numJoints; i++)
			parrRelQuats[numJoints*dst+i].q.Normalize();
	}
}



