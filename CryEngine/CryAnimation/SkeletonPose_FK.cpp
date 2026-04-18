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



//to prevent crash - should be fixed by ANIM CODE
#define SAFE_ANIM_IDX(a)	((a)!=0xffffffff ? (a) : 0)



uint32 g_RootUpdates=0;
uint32 g_AnimationUpdates=0;
uint32 g_SkeletonUpdates=0;

int32 g_nGlobalAnimID=-1;
int32 g_nAnimID=-1;
std::vector< std::vector<DebugJoint> > g_arrSkeletons;





ILINE f32 GetYaw2( const Quat& quat)
{
	Vec3 forward = quat.GetColumn1(); 
	assert( fabsf(forward.z)<0.01f );
	forward.z=0; 
	forward.Normalize();
	return -atan2f( forward.x,forward.y );
}


////////////////////////////////////////////////////////////////////////////
//                  playback of an animation in a layer                   //
////////////////////////////////////////////////////////////////////////////
RMovement CSkeletonPose::RPlayback (const CAnimation& rAnim, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo, f32 fScaling  )
{
	//optimized for playback of one animation per queue
	if (rAnim.m_LMG1.m_nAnimID[0]<0)
		return RPlayback_1( rAnim, nRedirectLayer, parrRelQuats, parrControllerStatus, pRootInfo, fScaling );
	else
		return RPlayback_2( rAnim, nRedirectLayer, parrRelQuats, parrControllerStatus, pRootInfo, fScaling );
}

///////////////////////////////////////////////////////////////////////////////////////////
//        evaluation of two animations and blending into one single animation            //
///////////////////////////////////////////////////////////////////////////////////////////
RMovement CSkeletonPose::RTransition( CAnimation* parrActiveAnims, uint32 numAnims, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo, f32 fScaling  )
{
	assert(numAnims>1);
	//	float fColor[4] = {0,1,0,1};
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 2.3f, fColor, false,"numAnims: %d",numAnims );	g_YLine+=16.0f;

	CAnimation rAnim0 = parrActiveAnims[0];
	pRootInfo[TMPREG00]=RPlayback( rAnim0,TMPREG00, parrRelQuats, parrControllerStatus, pRootInfo, fScaling );

	f32 r=0.0f;
	f32 t=0.0f;
	for (uint32 i=1; i<numAnims; i++)
	{
		CAnimation rAnim1 = parrActiveAnims[ i ];
		
		pRootInfo[TMPREG01]=RPlayback( rAnim1, TMPREG01, parrRelQuats, parrControllerStatus, pRootInfo, fScaling  );
		t = rAnim1.m_fTransitionPriority;
		t -= 0.5f;
		t = 0.5f+t/(0.5f+2.0f*t*t);
		r = t;  

		uint32 RootPriority = rAnim1.m_AnimParams.m_nFlags&CA_FULL_ROOT_PRIORITY;
		if (RootPriority)
		{
			//float fColor[4] = {0,1,0,1};
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 2.3f, fColor, false,"RootPriority: %x",RootPriority );	g_YLine+=16.0f;
			r=1.0f;
		}

		//full-body blending
		if ( i!=(numAnims-1) )
		{
			pRootInfo[TMPREG00] = RTwoWayBlend(TMPREG00,TMPREG01,TMPREG00,r,t, parrRelQuats, parrControllerStatus, pRootInfo  );
			//pRootInfo[TMPREG00] = RTwoWayBlend(TMPREG02,TMPREG02,TMPREG00,0, parrRelQuats, parrControllerStatus, pRootInfo  );
		}
	}
	return RTwoWayBlend(TMPREG00,TMPREG01,FINALREL,r,t, parrRelQuats, parrControllerStatus, pRootInfo  );
}


////////////////////////////////////////////////////////////////////////////
//                    playback of one single animation                    //
////////////////////////////////////////////////////////////////////////////
RMovement CSkeletonPose::RPlayback_1 (const CAnimation& rAnim, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo, f32 fScaling )
{
	if (rAnim.m_LMG0.m_nLMGID<0)
	{
		if (nRedirectLayer==FINALREL)
			return RSingleEvaluation (CopyAnim0(rAnim,0), parrRelQuats, parrControllerStatus, fScaling );
		else
			return RSingleEvaluation (CopyAnim0(rAnim,0), nRedirectLayer, parrRelQuats, parrControllerStatus, pRootInfo, fScaling );
	}
	else
	{
		if (nRedirectLayer==FINALREL)
			return RSingleEvaluationLMG (rAnim,  nRedirectLayer, parrRelQuats, parrControllerStatus, pRootInfo, fScaling  );
		else
			return RSingleEvaluationLMG (rAnim,  nRedirectLayer, parrRelQuats, parrControllerStatus, pRootInfo, fScaling  );
	}
}


////////////////////////////////////////////////////////////////////////////
//               playback of 2 interpolated animations                    //
////////////////////////////////////////////////////////////////////////////
RMovement CSkeletonPose::RPlayback_2 (const CAnimation& rAnim, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo, f32 fScaling  )
{

	uint32 slots=3;

	if (rAnim.m_fIWeight>0.99f)
		slots=2;
	if (rAnim.m_fIWeight<0.01f)
		slots=1;


	if (slots==1)
	{
		if (nRedirectLayer==FINALREL)
		{
			return RPlayback_1( rAnim, nRedirectLayer, parrRelQuats, parrControllerStatus, pRootInfo, fScaling  );
		}
		else
		{
			pRootInfo[nRedirectLayer]=RPlayback_1( rAnim, nRedirectLayer, parrRelQuats, parrControllerStatus, pRootInfo, fScaling );
			return pRootInfo[nRedirectLayer];
		}
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
			return RPlayback_1( a1, nRedirectLayer, parrRelQuats, parrControllerStatus, pRootInfo, fScaling );
		}
		else
		{
			pRootInfo[nRedirectLayer]=RPlayback_1( a1, nRedirectLayer, parrRelQuats, parrControllerStatus, pRootInfo, fScaling  );
			return pRootInfo[nRedirectLayer];
		}
	}


	//	if (slots==3)
	{
		//blend both slots
		pRootInfo[TMPREG02]=RPlayback_1( rAnim, TMPREG02, parrRelQuats, parrControllerStatus, pRootInfo, fScaling );

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
		pRootInfo[TMPREG03]=RPlayback_1( a1, TMPREG03, parrRelQuats, parrControllerStatus, pRootInfo, fScaling  );

		f32	t = rAnim.m_fIWeight;

		if (nRedirectLayer==FINALREL)
		{
			return RTwoWayBlend(TMPREG02,TMPREG03,FINALREL,t,t, parrRelQuats, parrControllerStatus, pRootInfo );
		}
		else
		{
			pRootInfo[nRedirectLayer] = RTwoWayBlend(TMPREG02,TMPREG03,nRedirectLayer,t,t, parrRelQuats, parrControllerStatus, pRootInfo );
			return pRootInfo[nRedirectLayer];
		}

	}
}














//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//     evaluation of one single animation and copy the result into the final joint-array                    //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
RMovement CSkeletonPose::RSingleEvaluation (const SAnimation& rAnim, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling )
{
	//DEFINE_PROFILER_FUNCTION();

	assert(rAnim.m_nEAnimID>=0);
	assert(rAnim.m_nEGlobalID>=0);

	uint32 numJoints = m_AbsolutePose.size();
	uint32 PartialBodyUpdates = rAnim.m_EAnimParams.m_nFlags&CA_PARTIAL_SKELETON_UPDATE;
	f32 fMultiLayerBlend = m_pSkeletonAnim->m_BlendedRoot.m_fAllowMultilayerAnim;

	f32 TimeFlow  = m_pInstance->m_fDeltaTime;
	f32 time_prev	=	rAnim.m_fETimeOldPrev;
	f32 time_old	=	rAnim.m_fETimeOldPrev; //rAnim.m_fETimeOld;
	f32 time_new	=	rAnim.m_fETimeNew;

	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID];
	SFootPlant& rFootPlantVectors = rGlobalAnimHeader.m_FootPlantVectors;
	rGlobalAnimHeader.m_nPlayedCount=1;

//	float fColor[4] = {0,1,0,1};
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




	RMovement Movement;

	uint32 numPoses=rGlobalAnimHeader.m_FootPlantBits.size();
	if (numPoses)
	{
		uint32 idx = min(uint32(time_new*(numPoses-1)), numPoses-1);
		Movement.m_nFootBits=rGlobalAnimHeader.m_FootPlantBits[idx];
	}


	Movement.m_fCurrentDuration		= rGlobalAnimHeader.m_fTotalDuration;
	Movement.m_fCurrentSlope			=	rGlobalAnimHeader.m_fSlope;
	Movement.m_fCurrentTurn			  =	rGlobalAnimHeader.m_fTurnSpeed;
	Movement.m_vCurrentVelocity		=	rGlobalAnimHeader.m_vVelocity;
//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_vCurrentVelocity: %f %f %f",rGlobalAnimHeader.m_vVelocity.x,rGlobalAnimHeader.m_vVelocity.y,rGlobalAnimHeader.m_vVelocity.z );	
//	g_YLine+=16.0f;

	Movement.m_fAllowMultilayerAnim = rAnim.m_EAnimParams.m_fAllowMultilayerAnim;
	for (int i=0; i<NUM_ANIMATION_USER_DATA_SLOTS; i++)
		Movement.m_fUserData[i] = rAnim.m_EAnimParams.m_fUserData[i];

	const CModelJoint* pModelJoint = GetModelJointPointer(0);

	//int32 numAnimations = pModelJoint[0].m_arrControllersMJoint.size();
	//if (rAnim.m_nEAnimID>=numAnimations)
	//	CryError("Fatal Error: AnimID out of range: id: %d   numAnimations: %d (contact: Ivo@Crytek.de)",rAnim.m_nEAnimID,numAnimations );

	IController* pController = rAnim.m_jointControllers[0];
	m_arrControllerInfo[0].ops = 0;


/*	if (pController)
	{
		QuatT SKey; 
		pController->GetOP( rAnim.m_nEGlobalID, 0, SKey.q, SKey.t );;
		QuatT EKey; 
		pController->GetOP( rAnim.m_nEGlobalID, 1, EKey.q, EKey.t );;
		float fColor[4] = {0,1,0,1};
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"SKey: Rot:(%f %f %f %f)  Pos:(%f %f %f)", SKey.q.w,SKey.q.v.x,SKey.q.v.y,SKey.q.v.z,  SKey.t.x,SKey.t.y,SKey.t.z   );	g_YLine+=16.0f;
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"EKey: Rot:(%f %f %f %f)  Pos:(%f %f %f)", EKey.q.w,EKey.q.v.x,EKey.q.v.y,EKey.q.v.z,  EKey.t.x,EKey.t.y,EKey.t.z   );	g_YLine+=16.0f;
	}*/




	//assert(time_old<=time_new);
	if (pController && GetJointMask(0,0))
	{

		//-------------------------------------------------------------------------------------
		//----                          Trajectory Extraction                             -----
		//-------------------------------------------------------------------------------------
		if (m_pSkeletonAnim->m_AnimationDrivenMotion)
		{
			g_RootUpdates++;

			QuatT _new;
			QuatT _old;

			if (rAnim.m_nAnimEOC && segcount==0)
			{
				QuatT EndKey; pController->GetOP( rAnim.m_nEGlobalID, 1, EndKey.q, EndKey.t );;
				if ((EndKey.q.v|EndKey.q.v) < 0.00001f)
					EndKey.q.SetIdentity();
				if (TimeFlow<0)
				{
					//time moves backwards
					pController->GetOP( rAnim.m_nEGlobalID, time_old, _old.q, _old.t );
					if ((_old.q.v|_old.q.v) < 0.00001f)
						_old.q.SetIdentity();
					_old=EndKey*_old; 
				}
				else
				{
					//time moves forward
					pController->GetOP( rAnim.m_nEGlobalID, time_old, _old.q, _old.t );
					if ((_old.q.v|_old.q.v) < 0.00001f)
						_old.q.SetIdentity();
					_old=EndKey.GetInverted()*_old;
			//		float fColor[4] = {0,1,0,1};
			//		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"EOC oldtime:%f ",time_old);	g_YLine+=16.0f;

				}
			}	
			else	
			{
				if (rAnim.m_nAnimEOC)
					pController->GetOP( rAnim.m_nEGlobalID, time_prev*rGlobalAnimHeader.GetSegmentDuration2(segcount), _old.q, _old.t );
				else
					pController->GetOP( rAnim.m_nEGlobalID, time_old, _old.q, _old.t );
				if ((_old.q.v|_old.q.v) < 0.00001f)
					_old.q.SetIdentity();
			}
			pController->GetOP( rAnim.m_nEGlobalID, time_new, _new.q, _new.t );
			if ((_new.q.v|_new.q.v) < 0.00001f)
				_new.q.SetIdentity();


			m_RelativePose[0].SetIdentity();

			_old.q.v.x=0; _old.q.v.y=0; _old.q.Normalize(); 
			_new.q.v.x=0; _new.q.v.y=0; _new.q.Normalize();
			Movement.m_fRelRotation			= GetYaw2(!_old.q*_new.q); 


			_old.t*=fScaling;
			_new.t*=fScaling;
			Movement.m_vRelTranslation	= (_old.t-_new.t)*_new.q;						//inverse multiplication to linearize the motion path



			f32 twDuration = rAnim.m_fETWDuration;
			assert(twDuration>0);
			f32 AssetDuration = rGlobalAnimHeader.GetSegmentDuration(segcount);
			f32 scale = twDuration/AssetDuration;
			Movement.m_vRelTranslation = Movement.m_vRelTranslation*scale;  


			/*
			g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
			Vec3 rel = Movement.m_vRelTranslation*100.0f;
			g_pAuxGeom->DrawLine(Vec3(0,0,0.1f),RGBA8(0x00,0x00,0x7f,0x00), Vec3(0,0,0.1f)+rel,RGBA8(0xff,0xff,0xff,0x00) );
			float fColor[4] = {0,1,0,1};
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"RelTrans:(%f %f %f) length: %f  radiant: %f", rel.x,rel.y,rel.z,rel.GetLength(),Movement.m_fRelRotation );	g_YLine+=16.0f;

			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"time:%f  oldTrans:(%f %f %f)",time_old, _old.t.x,_old.t.y,_old.t.z );	g_YLine+=16.0f;
			g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"time:%f  newTrans:(%f %f %f)",time_new, _new.t.x,_new.t.y,_new.t.z );	g_YLine+=16.0f;
			*/
			//---------------------------------------------------------------------------

			if (m_FootPlants)
				Movement.m_FootPlant			=	rFootPlantVectors;

			//-----------------------------------------------------------------------------
			//-----------------------------------------------------------------------------
			if (m_pSkeletonAnim->m_FuturePath && m_pInstance->m_pModel->m_ObjectType==CHR)
			{
				static SAnimRoot absFuturePath[ANIM_FUTURE_PATH_LOOKAHEAD]; //position of the root in the future (up to 1 second in the future) 
				f32 futuretime=time_new;
				f32 futuretimeabs=time_new;
				f32 fDuration = rGlobalAnimHeader.m_fEndSec-rGlobalAnimHeader.m_fStartSec;
				f32 timestep = 0.1f/fDuration;
				Vec3 CurrPosition(ZERO);
				Quat CurrRotation(IDENTITY);

				Vec3 EPosition;
				Quat ERotation;
				pController->GetOP( rAnim.m_nEGlobalID, 1, ERotation,EPosition );;

				for (uint32 i=0; i<ANIM_FUTURE_PATH_LOOKAHEAD; i++ )
				{
					pController->GetOP( rAnim.m_nEGlobalID, futuretime, absFuturePath[i].m_TransRot.q, absFuturePath[i].m_TransRot.t ); 
					absFuturePath[i].m_TransRot.q = CurrRotation*absFuturePath[i].m_TransRot.q;
					absFuturePath[i].m_TransRot.t = CurrRotation*absFuturePath[i].m_TransRot.t + CurrPosition;
					absFuturePath[i].m_NormalizedTimeAbs = futuretimeabs;
					absFuturePath[i].m_KeyTimeAbs = futuretimeabs*fDuration;
					futuretime += timestep;
					futuretimeabs += timestep;

					if (futuretime>=1.0f)
					{
						CurrPosition = CurrRotation*EPosition + CurrPosition;
						CurrRotation = CurrRotation*ERotation;
						futuretime-=1.0f;
					}
				}

				//make relative
				Quat qstart(IDENTITY); 
				Movement.m_relFuturePath[0].m_TransRot.SetIdentity();
				Movement.m_relFuturePath[0].m_NormalizedTimeAbs = absFuturePath[0].m_NormalizedTimeAbs;
				Movement.m_relFuturePath[0].m_KeyTimeAbs = absFuturePath[0].m_KeyTimeAbs;

				Quat qIdentDir = absFuturePath[0].m_TransRot.q;
				for (uint32 i=1; i<ANIM_FUTURE_PATH_LOOKAHEAD; i++)
				{
					Movement.m_relFuturePath[i].m_TransRot.q = absFuturePath[i].m_TransRot.q * !absFuturePath[i-1].m_TransRot.q;
					Movement.m_relFuturePath[i].m_TransRot.t = (absFuturePath[i].m_TransRot.t-absFuturePath[i-1].m_TransRot.t) * qIdentDir * qstart; 
					qstart *= Movement.m_relFuturePath[i].m_TransRot.q;
					qstart.Normalize();
					Movement.m_relFuturePath[i].m_NormalizedTimeAbs = absFuturePath[i].m_NormalizedTimeAbs;
					Movement.m_relFuturePath[i].m_KeyTimeAbs = absFuturePath[i].m_KeyTimeAbs;
				}
			}		

			if (m_pSkeletonAnim->m_AnimationDrivenMotion)
			{
#ifdef _DEBUG
				Quat q = m_RelativePose[0].q;
				Vec3 t = m_RelativePose[0].t;
				assert(q.IsEquivalent(IDENTITY,0.001f));
				assert(t.IsEquivalent(ZERO,0.001f));
#endif
			}

		} 
		else
		{
			m_arrControllerInfo[0] = pController->GetOP(	rAnim.m_nEGlobalID, time_new, m_RelativePose[0].q, m_RelativePose[0].t );

			if (PartialBodyUpdates==0)
			{
				if (m_arrControllerInfo[0].o==0)
					m_RelativePose[0].q = pModelJoint[0].m_DefaultRelPose.q;
				if (m_arrControllerInfo[0].p==0)
					m_RelativePose[0].t = pModelJoint[0].m_DefaultRelPose.t;
				m_arrControllerInfo[0].p = 1;
				m_arrControllerInfo[0].o = 1;
			}
			//if (m_arrControllerInfo[0].p)
			//	m_RelativePose[0].t *= fScaling;

		}
	}




	//-------------------------------------------------------------------------------------
	//----             evaluate all controllers for this animation                    -----
	//-------------------------------------------------------------------------------------
	if (m_bFullSkeletonUpdate)
	{
		extern uint32 g_SkeletonUpdates; 	g_SkeletonUpdates++;
		if (m_AimIK.m_AimIKBlend)
			SpineEvaluation( rAnim, time_new,segcount, Movement, parrRelQuats, parrControllerStatus);

		for (uint32 j=1; j<numJoints; j++)
		{
			if (GetJointMask(j,0) == 0)
				continue;

			pController =  g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID].GetControllerByJointID(pModelJoint[j].m_nJointCRC32);// pModelJoint[j].m_arrControllersMJoint[rAnim.m_nEAnimID];
			if (pController)
					m_arrControllerInfo[j] = pController->GetOP(	rAnim.m_nEGlobalID, time_new, m_RelativePose[j].q, m_RelativePose[j].t );

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
		//	if (m_arrControllerInfo[j].p)
		//		m_RelativePose[j].t *=fScaling;

		}

	}

	m_pSkeletonAnim->m_IsAnimPlaying |= (1);
	return Movement; 
}




////////////////////////////////////////////////////////////////////////////
//                 evaluation of one single animation                     //
////////////////////////////////////////////////////////////////////////////
RMovement CSkeletonPose::RSingleEvaluation (const SAnimation& rAnim, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo, f32 fScaling )
{
	assert(nRedirectLayer<numTMPLAYERS);
	if (nRedirectLayer>=numTMPLAYERS)
		CryError("FatalProblem");

	assert(rAnim.m_nEAnimID>=0);
	assert(rAnim.m_nEGlobalID>=0);

	uint32 numJoints = m_AbsolutePose.size();
	uint32 PartialBodyUpdates = rAnim.m_EAnimParams.m_nFlags&CA_PARTIAL_SKELETON_UPDATE;
	f32 fMultiLayerBlend = m_pSkeletonAnim->m_BlendedRoot.m_fAllowMultilayerAnim;

	memset( &parrControllerStatus[numJoints*nRedirectLayer].ops,0,numJoints*sizeof(Status4) );

	f32 TimeFlow  = m_pInstance->m_fDeltaTime;
	f32 time_prev	=	rAnim.m_fETimeOldPrev;
	f32 time_old	=	rAnim.m_fETimeOld;
	f32 time_new	=	rAnim.m_fETimeNew;

	RMovement Movement;

	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID];
	SFootPlant& rFootPlantVectors = rGlobalAnimHeader.m_FootPlantVectors;
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


	uint32 numPoses=rGlobalAnimHeader.m_FootPlantBits.size();
	if (numPoses)
	{
		uint32 idx = min(uint32(time_new*(numPoses-1)), numPoses-1);
		Movement.m_nFootBits=rGlobalAnimHeader.m_FootPlantBits[idx];
		//float fColor[4] = {0,1,0,1};
		//extern float g_YLine;
		//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"FootBits: %01x  %08d", Movement.m_nFootBits,numPoses );	g_YLine+=16.0f;
	}

	Movement.m_fCurrentDuration		= rGlobalAnimHeader.m_fTotalDuration;
	Movement.m_fCurrentSlope			=	rGlobalAnimHeader.m_fSlope;
	Movement.m_fCurrentTurn			  =	rGlobalAnimHeader.m_fTurnSpeed;
	Movement.m_vCurrentVelocity		=	rGlobalAnimHeader.m_vVelocity;

	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_vCurrentVelocity: %f %f %f",rGlobalAnimHeader.m_vVelocity.x,rGlobalAnimHeader.m_vVelocity.y,rGlobalAnimHeader.m_vVelocity.z );	
	//g_YLine+=16.0f;

	Movement.m_fAllowMultilayerAnim = rAnim.m_EAnimParams.m_fAllowMultilayerAnim;
	for (int i=0; i<NUM_ANIMATION_USER_DATA_SLOTS; i++)
		Movement.m_fUserData[i] = rAnim.m_EAnimParams.m_fUserData[i];

	const CModelJoint* pModelJoint = GetModelJointPointer(0);

	//int32 numAnimations = pModelJoint[0].m_arrControllersMJoint.size();
	//if (rAnim.m_nEAnimID>=numAnimations)
	//	CryFatalError("Fatal Error: AnimID out of range: id: %d   numAnimations: %d (contact: Ivo@Crytek.de)",rAnim.m_nEAnimID,numAnimations );

	IController* pController =  g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID].GetControllerByJointID(pModelJoint[0].m_nJointCRC32);//pModelJoint[0].m_arrControllersMJoint[rAnim.m_nEAnimID];

	//assert(time_old<=time_new);
	if (pController && GetJointMask(0,0))
	{

		//-------------------------------------------------------------------------------------
		//----                      Trajectory Extraction                                 -----
		//-------------------------------------------------------------------------------------

		if (m_pSkeletonAnim->m_AnimationDrivenMotion)
		{
			g_RootUpdates++;

			QuatT _new;
			QuatT _old;


			if (rAnim.m_nAnimEOC && segcount==0)
			{
				QuatT EndKey; 
				parrControllerStatus[numJoints*nRedirectLayer+0] = pController->GetOP( rAnim.m_nEGlobalID, 1, EndKey.q, EndKey.t );;
				if ((EndKey.q.v|EndKey.q.v) < 0.00001f)
					EndKey.q.SetIdentity();
				if (TimeFlow<0)
				{
					//time moves backwards
					parrControllerStatus[numJoints*nRedirectLayer+0] = pController->GetOP( rAnim.m_nEGlobalID, time_old, _old.q, _old.t );
					if ((_old.q.v|_old.q.v) < 0.00001f)
						_old.q.SetIdentity();
					_old=EndKey*_old; 
				}
				else
				{
					//time moves forward
					parrControllerStatus[numJoints*nRedirectLayer+0] = pController->GetOP( rAnim.m_nEGlobalID, time_old, _old.q, _old.t );
					if ((_old.q.v|_old.q.v) < 0.00001f)
						_old.q.SetIdentity();
					_old=EndKey.GetInverted()*_old;
				}
			}	
			else	
			{
				if (rAnim.m_nAnimEOC)
					parrControllerStatus[numJoints*nRedirectLayer+0] = pController->GetOP( rAnim.m_nEGlobalID, time_prev*rGlobalAnimHeader.GetSegmentDuration2(segcount), _old.q, _old.t );
				else
					parrControllerStatus[numJoints*nRedirectLayer+0] = pController->GetOP( rAnim.m_nEGlobalID, time_old, _old.q, _old.t );
				if ((_old.q.v|_old.q.v) < 0.00001f)
					_old.q.SetIdentity();
			}
			pController->GetOP( rAnim.m_nEGlobalID, time_new, _new.q, _new.t );
			if ((_new.q.v|_new.q.v) < 0.00001f)
				_new.q.SetIdentity();

			parrRelQuats[numJoints*nRedirectLayer+0].SetIdentity();

			_old.q.v.x=0; _old.q.v.y=0; _old.q.Normalize(); 
			_new.q.v.x=0; _new.q.v.y=0; _new.q.Normalize();
			Movement.m_fRelRotation			= GetYaw2(!_old.q*_new.q); 
			_old.t*=fScaling;
			_new.t*=fScaling;
			Movement.m_vRelTranslation	= (_old.t-_new.t)*_new.q;						//inverse multiplication to linearize the motion path

			f32 twDuration = rAnim.m_fETWDuration;
			assert(twDuration>0);
			f32 AssetDuration = rGlobalAnimHeader.GetSegmentDuration(segcount);
			f32 scale = twDuration/AssetDuration;
			Movement.m_vRelTranslation = Movement.m_vRelTranslation*scale;  

			//---------------------------------------------------------------------------

			if (m_FootPlants)
				Movement.m_FootPlant			=	rFootPlantVectors;

			//-----------------------------------------------------------------------------
			//-----------------------------------------------------------------------------
			if (m_pSkeletonAnim->m_FuturePath && m_pInstance->m_pModel->m_ObjectType==CHR)
			{
				static SAnimRoot absFuturePath[ANIM_FUTURE_PATH_LOOKAHEAD]; //position of the root in the future (up to 1 second in the future) 
				f32 futuretime=time_new;
				f32 futuretimeabs=time_new;
				f32 fDuration = rGlobalAnimHeader.m_fTotalDuration;
				f32 timestep = 0.1f/fDuration; //10 keys per second / 20-keys in two seconds
				Vec3 CurrPosition(ZERO);
				Quat CurrRotation(IDENTITY);

				Vec3 EPosition;
				Quat ERotation;
				pController->GetOP( rAnim.m_nEGlobalID, 1, ERotation,EPosition );
				for (uint32 i=0; i<ANIM_FUTURE_PATH_LOOKAHEAD; i++ )
				{
					pController->GetOP( rAnim.m_nEGlobalID, futuretime, absFuturePath[i].m_TransRot.q, absFuturePath[i].m_TransRot.t ); 
					absFuturePath[i].m_TransRot.q = CurrRotation*absFuturePath[i].m_TransRot.q;
					absFuturePath[i].m_TransRot.t = CurrRotation*absFuturePath[i].m_TransRot.t + CurrPosition;
					absFuturePath[i].m_NormalizedTimeAbs = futuretimeabs;
					absFuturePath[i].m_KeyTimeAbs = futuretimeabs*fDuration;
					futuretime += timestep;
					futuretimeabs += timestep;

					if (futuretime>=1.0f)
					{
						CurrPosition = CurrRotation*EPosition + CurrPosition;
						CurrRotation = CurrRotation*ERotation;
						futuretime-=1.0f;
					}
				}

				//make relative
				Quat qstart(IDENTITY); 
				Movement.m_relFuturePath[0].m_TransRot.SetIdentity();
				Movement.m_relFuturePath[0].m_NormalizedTimeAbs = absFuturePath[0].m_NormalizedTimeAbs;
				Movement.m_relFuturePath[0].m_KeyTimeAbs = absFuturePath[0].m_KeyTimeAbs;
				Quat qIdentDir = absFuturePath[0].m_TransRot.q;

				for (uint32 i=1; i<ANIM_FUTURE_PATH_LOOKAHEAD; i++)
				{
					Movement.m_relFuturePath[i].m_TransRot.q = absFuturePath[i].m_TransRot.q * !absFuturePath[i-1].m_TransRot.q;
					Movement.m_relFuturePath[i].m_TransRot.t = (absFuturePath[i].m_TransRot.t-absFuturePath[i-1].m_TransRot.t) * qIdentDir * qstart; 
					qstart *= Movement.m_relFuturePath[i].m_TransRot.q;
					qstart.Normalize();
					Movement.m_relFuturePath[i].m_NormalizedTimeAbs = absFuturePath[i].m_NormalizedTimeAbs;
					Movement.m_relFuturePath[i].m_KeyTimeAbs = absFuturePath[i].m_KeyTimeAbs;
				}
			}
			//----> end of future path

			if (m_pSkeletonAnim->m_AnimationDrivenMotion)
			{
#ifdef _DEBUG
				Quat q = parrRelQuats[numJoints*nRedirectLayer+0].q;
				Vec3 t = parrRelQuats[numJoints*nRedirectLayer+0].t;
				assert(q.IsEquivalent(IDENTITY,0.001f));
				assert(t.IsEquivalent(ZERO,0.001f));
#endif
			}

		} 
		else
		{
			//motion extraction is not used
			parrControllerStatus[numJoints*nRedirectLayer+0] = pController->GetOP(	rAnim.m_nEGlobalID, rAnim.m_fETimeNew, parrRelQuats[numJoints*nRedirectLayer+0].q, parrRelQuats[numJoints*nRedirectLayer+0].t );
			if (PartialBodyUpdates==0)
			{
				//full-body update. make sure every bone is initialized
				if (parrControllerStatus[numJoints*nRedirectLayer+0].o==0)
					parrRelQuats[numJoints*nRedirectLayer+0].q = pModelJoint[0].m_DefaultRelPose.q;
				if (parrControllerStatus[numJoints*nRedirectLayer+0].p==0)
					parrRelQuats[numJoints*nRedirectLayer+0].t = pModelJoint[0].m_DefaultRelPose.t;
				parrControllerStatus[numJoints*nRedirectLayer+0].o=1;
				parrControllerStatus[numJoints*nRedirectLayer+0].p=1;
			}
			//if (parrControllerStatus[numJoints*nRedirectLayer+0].p)
			//	parrRelQuats[numJoints*nRedirectLayer+0].t *= fScaling;
		}
	}


	//-------------------------------------------------------------------------------------
	//----             evaluate all controllers for this animation                    -----
	//-------------------------------------------------------------------------------------
	if (m_bFullSkeletonUpdate)
	{
		extern uint32 g_SkeletonUpdates; 	g_SkeletonUpdates++;
		if (m_AimIK.m_AimIKBlend)
			SpineEvaluation( rAnim, rAnim.m_fETimeNew,0, Movement, parrRelQuats, parrControllerStatus );

		for (uint32 j=1; j<numJoints; j++)
		{
			if (GetJointMask(j,0) == 0)
				continue;


			pController = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID].GetControllerByJointID(pModelJoint[j].m_nJointCRC32);//pModelJoint[j].m_arrControllersMJoint[rAnim.m_nEAnimID];
			if (pController)
				parrControllerStatus[numJoints*nRedirectLayer+j] = pController->GetOP(	rAnim.m_nEGlobalID, time_new, parrRelQuats[numJoints*nRedirectLayer+j].q, parrRelQuats[numJoints*nRedirectLayer+j].t );

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
		if (parrControllerStatus[numJoints*nRedirectLayer+j].o)
		{
			Quat q = parrRelQuats[numJoints*nRedirectLayer+j].q;
			assert(parrRelQuats[numJoints*nRedirectLayer+j].q.IsValid());
		}
		if (parrControllerStatus[numJoints*nRedirectLayer+j].p)
		{
			Vec3 t = parrRelQuats[numJoints*nRedirectLayer+j].t;
			assert(parrRelQuats[numJoints*nRedirectLayer+j].t.IsValid());
		}
	}
#endif


	m_pSkeletonAnim->m_IsAnimPlaying |= 1;
	return Movement; 
}



///////////////////////////////////////////////////////////////////////////////////////////
//                       evaluation of a locomotion group                                //
///////////////////////////////////////////////////////////////////////////////////////////
void CSkeletonPose::SpineEvaluation(const SAnimation& rAnim, f32 time_new,int32 seg, RMovement& Movement, QuatT* parrRelQuats, Status4* parrControllerStatus )
{

	if (m_pModelSkeleton->IsHuman()==0)
		return;

	IController* pController=0;
	const CModelJoint* pModelJoint = GetModelJointPointer(0);

	int32 s0=SAFE_ANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_Spine0Idx]);
	int32 s1=SAFE_ANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_Spine1Idx]);
	int32 s2=SAFE_ANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_Spine2Idx]);
	int32 s3=SAFE_ANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_Spine3Idx]);
	int32 n0=SAFE_ANIM_IDX(m_pModelSkeleton->m_IdxArray[eIM_NeckIdx]);

	Movement.m_FSpine0RelQuat = pModelJoint[s0].m_DefaultRelPose.q;
	Movement.m_FSpine1RelQuat = pModelJoint[s1].m_DefaultRelPose.q;
	Movement.m_FSpine2RelQuat = pModelJoint[s2].m_DefaultRelPose.q;
	Movement.m_FSpine3RelQuat = pModelJoint[s3].m_DefaultRelPose.q;
	Movement.m_FNeckRelQuat		= pModelJoint[n0].m_DefaultRelPose.q;

	if (rAnim.m_numAimPoses==2)
	{
		pController = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID].GetControllerByJointID(pModelJoint[s0].m_nJointCRC32);//pModelJoint[s0].m_arrControllersMJoint[rAnim.m_nEAnimID];
		if (pController)
		{
			Quat f,l;
			pController->GetO(	rAnim.m_nEGlobalID,0,f );
			pController->GetO(	rAnim.m_nEGlobalID,1,l );
			Movement.m_FSpine0RelQuat.SetNlerp(f,l,time_new);
		}
		pController = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID].GetControllerByJointID(pModelJoint[s1].m_nJointCRC32);//pModelJoint[s1].m_arrControllersMJoint[rAnim.m_nEAnimID];
		if (pController)
		{
			Quat f,l;
			pController->GetO(	rAnim.m_nEGlobalID,0,f );
			pController->GetO(	rAnim.m_nEGlobalID,1,l );
			Movement.m_FSpine1RelQuat.SetNlerp(f,l,time_new);
		}
		pController = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID].GetControllerByJointID(pModelJoint[s2].m_nJointCRC32);//pModelJoint[s2].m_arrControllersMJoint[rAnim.m_nEAnimID]; 
		if (pController)
		{
			Quat f,l;
			pController->GetO(	rAnim.m_nEGlobalID,0,f );
			pController->GetO(	rAnim.m_nEGlobalID,1,l );
			Movement.m_FSpine2RelQuat.SetNlerp(f,l,time_new);
		}
		pController = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID].GetControllerByJointID(pModelJoint[s3].m_nJointCRC32);//pModelJoint[s3].m_arrControllersMJoint[rAnim.m_nEAnimID];
		if (pController)
		{
			Quat f,l;
			pController->GetO(	rAnim.m_nEGlobalID,0,f );
			pController->GetO(	rAnim.m_nEGlobalID,1,l );
			Movement.m_FSpine3RelQuat.SetNlerp(f,l,time_new);
		}
		pController = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID].GetControllerByJointID(pModelJoint[n0].m_nJointCRC32);//pModelJoint[n0].m_arrControllersMJoint[rAnim.m_nEAnimID]; 
		if (pController)
		{
			Quat f,l;
			pController->GetO(	rAnim.m_nEGlobalID,0,f );
			pController->GetO(	rAnim.m_nEGlobalID,1,l );
			Movement.m_FNeckRelQuat.SetNlerp(f,l,time_new);
		}
	}
	else
	{
		pController = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID].GetControllerByJointID(pModelJoint[s0].m_nJointCRC32);//pModelJoint[s0].m_arrControllersMJoint[rAnim.m_nEAnimID];
		if (pController) pController->GetO(	rAnim.m_nEGlobalID,0,Movement.m_FSpine0RelQuat );

		pController = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID].GetControllerByJointID(pModelJoint[s1].m_nJointCRC32);//pModelJoint[s1].m_arrControllersMJoint[rAnim.m_nEAnimID];
		if (pController) pController->GetO(	rAnim.m_nEGlobalID,0,Movement.m_FSpine1RelQuat );

		pController = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID].GetControllerByJointID(pModelJoint[s2].m_nJointCRC32);//pModelJoint[s2].m_arrControllersMJoint[rAnim.m_nEAnimID];
		if (pController) pController->GetO(	rAnim.m_nEGlobalID,0,Movement.m_FSpine2RelQuat );

		pController = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID].GetControllerByJointID(pModelJoint[s3].m_nJointCRC32);//pModelJoint[s3].m_arrControllersMJoint[rAnim.m_nEAnimID];
		if (pController) pController->GetO(	rAnim.m_nEGlobalID,0,Movement.m_FSpine3RelQuat );

		pController = g_AnimationManager.m_arrGlobalAnimations[rAnim.m_nEGlobalID].GetControllerByJointID(pModelJoint[n0].m_nJointCRC32);//pModelJoint[n0].m_arrControllersMJoint[rAnim.m_nEAnimID];
		if (pController) pController->GetO(	rAnim.m_nEGlobalID,0,Movement.m_FNeckRelQuat );
	}

}


///////////////////////////////////////////////////////////////////////////////////////////
//                       evaluation of a locomotion group                                //
///////////////////////////////////////////////////////////////////////////////////////////
RMovement CSkeletonPose::RSingleEvaluationLMG (const CAnimation& rAnim, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* pRootInfo, f32 fScaling  )
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

		int32 nGlobalID=rAnim.m_LMG0.m_nGlobalID[i];
		GlobalAnimationHeader& rGlobalAnimHeaderAssets = g_AnimationManager.m_arrGlobalAnimations[nGlobalID];
		rGlobalAnimHeaderAssets.m_nPlayedCount=1;

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
			pRootInfo[TMPREG05]=RSingleEvaluation( CopyAnim0(rAnim,i), TMPREG05, parrRelQuats, parrControllerStatus, pRootInfo, fScaling );
			evaluations++;
		}
		if (fBlendWeights==0.0f)
			R_BLENDLAYER_MOVE(TMPREG05,nRedirectLayer,weight, parrRelQuats, parrControllerStatus, pRootInfo);
		else 
			R_BLENDLAYER_ADD(TMPREG05,nRedirectLayer,weight, parrRelQuats, parrControllerStatus, pRootInfo);

		fBlendWeights+=weight;
		oldid=newid;
	}
	//g_YLine+=10;
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor2, false,"Evaluation: %d",evaluations );	
	//g_YLine+=10;

	f32 fTotal = fabsf(fBlendWeights-1.0f);
	if (fTotal>0.05f)
	{
		assert(0);
		//CryFatalError("CryAnimation: sum of blend-weights for LMG is wrong: %f %s",fBlendWeights,m_pInstance->m_pModel->GetModelFilePath());
	}
	m_pSkeletonAnim->m_IsAnimPlaying |= 1;
	R_BLENDLAYER_NORM(nRedirectLayer, parrRelQuats, parrControllerStatus, pRootInfo);


	//-----------------------------------------------------------------------------------------------------
/*
	const char* OldStyle = rGlobalAnimHeader.m_strOldStyle;
	uint32 hasOldStyle = OldStyle[0];
	const char* NewStyle = rGlobalAnimHeader.m_strNewStyle;
	uint32 hasNewStyle = NewStyle[0];
	if (hasOldStyle && hasNewStyle)
	{
		uint32 ADM = m_pSkeletonAnim->m_AnimationDrivenMotion;
		m_pSkeletonAnim->m_AnimationDrivenMotion=0;
		TransModelFast( rGlobalAnimHeader,rAnim,nRedirectLayer,parrRelQuats,parrControllerStatus,fScaling);
		//TransModelSlow( rGlobalAnimHeader,rAnim,nRedirectLayer);
		m_pSkeletonAnim->m_AnimationDrivenMotion			=	ADM;
	}*/


	uint32 num = rGlobalAnimHeader.m_strSpliceAnim.size();
	for (uint32 i=0; i<num; i++)
	{
		const char* strSpliceAnim = rGlobalAnimHeader.m_strSpliceAnim[i];
		AddLayer( rGlobalAnimHeader,rAnim,nRedirectLayer,parrRelQuats,parrControllerStatus,fScaling,strSpliceAnim);
	}

	pRootInfo[nRedirectLayer].m_fAllowMultilayerAnim = rAnim.m_AnimParams.m_fAllowMultilayerAnim;
	for (int i=0; i<NUM_ANIMATION_USER_DATA_SLOTS; i++)
		pRootInfo[nRedirectLayer].m_fUserData[i] = rAnim.m_AnimParams.m_fUserData[i];

	if (nRedirectLayer==TMPREG06)
	{
		m_RelativePose[0] = parrRelQuats[numJoints*TMPREG06+0];
		m_arrControllerInfo[0].ops = parrControllerStatus[numJoints*TMPREG06+0].ops;

		//--------------------------------------------------------------------------------------
		if (m_bFullSkeletonUpdate)
		{
			uint32 numJoints=m_AbsolutePose.size();
			for (uint32 i=1; i<numJoints; i++)
			{
				m_RelativePose[i]		= parrRelQuats[numJoints*TMPREG06+i];
				m_arrControllerInfo[i].ops	= parrControllerStatus[numJoints*TMPREG06+i].ops;
			}
		}
	}

	return pRootInfo[nRedirectLayer];
}


//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------

void CSkeletonPose::TransModelFast( const GlobalAnimationHeader& rGAH, const CAnimation& rAnim, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling )
{
	/*
	uint32 numJoints=m_AbsolutePose.size();
	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;

	const char* OldStyle = rGAH.m_strOldStyle;
	const char* NewStyle = rGAH.m_strNewStyle;
	int32 nAnimIDOS	= pAnimationSet->GetAnimIDByName(OldStyle);
	int32 nAnimIDNS	= pAnimationSet->GetAnimIDByName(NewStyle);
	const ModelAnimationHeader* pAnimHeaderOS = pAnimationSet->GetModelAnimationHeader(nAnimIDOS);
	const ModelAnimationHeader* pAnimHeaderNS = pAnimationSet->GetModelAnimationHeader(nAnimIDNS);


	SAnimation StyleOld;
	StyleOld.m_nEAnimID					=	nAnimIDOS;											//in.m_nEAnimID0[i];
	StyleOld.m_nEGlobalID				=	pAnimHeaderOS->m_nGlobalAnimId;	//in.m_nEGlobalID0[i];
	StyleOld.m_fEBlendWeight		=	1.0f;
	StyleOld.m_nESegmentCounter	=	0;
	StyleOld.m_nAnimEOC					=	0;	//if 1 then "end of cycle" reached
	StyleOld.m_fETimeNew				=	rAnim.m_fAnimTime; //this is a percentage value between 0-1
	StyleOld.m_fETimeOld				=	rAnim.m_fAnimTime; //this is a percentage value between 0-1
	StyleOld.m_fETimeOldPrev		=	rAnim.m_fAnimTime; //this is a percentage value between 0-1
	StyleOld.m_fETWDuration			=	0;	//in.m_fETWDuration;			//time-warped duration 
	StyleOld.m_numAimPoses			=	0;
	StyleOld.m_EAnimParams			=	rAnim.m_AnimParams;
	RSingleEvaluation( StyleOld,TMPSTYLEOLD, parrRelQuats,parrControllerStatus,0,fScaling );
	parrRelQuats[TMPSTYLEOLD*numJoints+0].SetIdentity();

	{
		QuatT* parrAbsQuats = (QuatT*)alloca( numJoints*sizeof(QuatT) );
		parrAbsQuats[0]	= parrRelQuats[TMPSTYLEOLD*numJoints+0];
		for (uint32 j=1; j<numJoints; j++)
		{
			int32 p = m_parrModelJoints[j].m_idxParent;
			parrAbsQuats[j]	= parrAbsQuats[p] * parrRelQuats[TMPSTYLEOLD*numJoints+j];
		}

		g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
		ColorB col = RGBA8(0xff,0x1f,0x1f,0xff);
		for (uint32 j=0; j<numJoints; j++)
		{
			int32 p = m_parrModelJoints[j].m_idxParent;
			if (p >= 0)
			{
				QuatT WMatChild		=	parrAbsQuats[j];
				QuatT WMatParent	=	parrAbsQuats[p];
				Vec3 JChild	  =	WMatChild.t;
				Vec3 JParent	=	WMatParent.t;
				f32 distance	= (JChild-JParent).GetLength();
				g_pAuxGeom->DrawLine( JParent,col, JChild,RGBA8(0x1f,0x1f,0x1f,0xff) );
			} 
		}
	}


	SAnimation StyleNew;
	StyleNew.m_nEAnimID					=	nAnimIDNS;											//in.m_nEAnimID0[i];
	StyleNew.m_nEGlobalID				=	pAnimHeaderNS->m_nGlobalAnimId;	//in.m_nEGlobalID0[i];
	StyleNew.m_fEBlendWeight		=	1.0f;
	StyleNew.m_nESegmentCounter	=	0;
	StyleNew.m_nAnimEOC					=	0;	//if 1 then "end of cycle" reached
	StyleNew.m_fETimeNew				=	rAnim.m_fAnimTime; //this is a percentage value between 0-1
	StyleNew.m_fETimeOld				=	rAnim.m_fAnimTime; //this is a percentage value between 0-1
	StyleNew.m_fETimeOldPrev		=	rAnim.m_fAnimTime; //this is a percentage value between 0-1
	StyleNew.m_fETWDuration			=	0;	//in.m_fETWDuration;			//time-warped duration 
	StyleNew.m_numAimPoses			=	0;
	StyleNew.m_EAnimParams			=	rAnim.m_AnimParams;
	RSingleEvaluation( StyleNew,TMPSTYLENEW, parrRelQuats,parrControllerStatus,0,fScaling );
	parrRelQuats[TMPSTYLENEW*numJoints+0].SetIdentity();

	{
		uint32 numJoints=m_AbsolutePose.size();
		QuatT* parrAbsQuats = (QuatT*)alloca( numJoints*sizeof(QuatT) );
		parrAbsQuats[0]	= parrRelQuats[TMPSTYLENEW*numJoints+0];
		for (uint32 j=1; j<numJoints; j++)
		{
			int32 p = m_parrModelJoints[j].m_idxParent;
			parrAbsQuats[j]	= parrAbsQuats[p] * parrRelQuats[TMPSTYLENEW*numJoints+j];
		}

		g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );
		ColorB col = RGBA8(0xff,0xff,0xff,0xff);
		for (uint32 j=0; j<numJoints; j++)
		{
			int32 p = m_parrModelJoints[j].m_idxParent;
			if (p >= 0)
			{
				QuatT WMatChild		=	parrAbsQuats[j];
				QuatT WMatParent	=	parrAbsQuats[p];
				Vec3 JChild	  =	WMatChild.t;
				Vec3 JParent	=	WMatParent.t;
				f32 distance	= (JChild-JParent).GetLength();
				g_pAuxGeom->DrawLine( JParent,col, JChild,RGBA8(0x1f,0x1f,0x1f,0xff) );
			} 
		}
	}

	QuatT* parrDeltaQuats = (QuatT*)alloca( numJoints*sizeof(QuatT) );
	for (uint32 j=0; j<numJoints; j++)
	{
		//parrDeltaQuats[j] = g_arrRelQuats[TMPSTYLENEW][j].GetInverted()*g_arrRelQuats[TMPSTYLEOLD][j];
		parrDeltaQuats[j] = parrRelQuats[TMPSTYLENEW*numJoints+j]*parrRelQuats[TMPSTYLEOLD*numJoints+j].GetInverted();
		//parrDeltaQuats[j] = g_arrRelQuats[TMPSTYLEOLD][j]*g_arrRelQuats[TMPSTYLENEW][j].GetInverted();
		//parrDeltaQuats[j] = g_arrRelQuats[TMPSTYLEOLD][j].GetInverted()*g_arrRelQuats[TMPSTYLENEW][j];
	}

	for (uint32 j=0; j<numJoints; j++)
		parrRelQuats[nRedirectLayer*numJoints+j] = parrDeltaQuats[j]*parrRelQuats[nRedirectLayer*numJoints+j];
*/

}



//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
void CSkeletonPose::TransModelSlow( const GlobalAnimationHeader& rGAH, const CAnimation& rAnim, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling )
{
	/*
	uint32 numJoints=m_AbsolutePose.size();
	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;

	uint32 numPoses=33;
	QuatT* parrTransModelSrc = (QuatT*)alloca( numPoses*numJoints*sizeof(QuatT) );
	QuatT* parrTransModelDst = (QuatT*)alloca( numPoses*numJoints*sizeof(QuatT) );


	const char* OldStyle = rGAH.m_strOldStyle;
	const char* NewStyle = rGAH.m_strNewStyle;
	int32 nAnimIDOS	= pAnimationSet->GetAnimIDByName(OldStyle);
	int32 nAnimIDNS	= pAnimationSet->GetAnimIDByName(NewStyle);
	const ModelAnimationHeader* pAnimHeaderOS = pAnimationSet->GetModelAnimationHeader(nAnimIDOS);
	const ModelAnimationHeader* pAnimHeaderNS = pAnimationSet->GetModelAnimationHeader(nAnimIDNS);


	for (uint32 p=0; p<numPoses; p++)
	{
		f32 time = f32(p)/32.0f;	assert(time<=1.0f);

		SAnimation StyleOld;
		StyleOld.m_nEAnimID					=	nAnimIDOS;											//in.m_nEAnimID0[i];
		StyleOld.m_nEGlobalID				=	pAnimHeaderOS->m_nGlobalAnimId;	//in.m_nEGlobalID0[i];
		StyleOld.m_fEBlendWeight		=	1.0f;
		StyleOld.m_nESegmentCounter	=	0;
		StyleOld.m_nAnimEOC					=	0;		//if 1 then "end of cycle" reached
		StyleOld.m_fETimeNew				=	time; //this is a percentage value between 0-1
		StyleOld.m_fETimeOld				=	time; //this is a percentage value between 0-1
		StyleOld.m_fETimeOldPrev		=	time; //this is a percentage value between 0-1
		StyleOld.m_fETWDuration			=	0;		//in.m_fETWDuration;			//time-warped duration 
		StyleOld.m_numAimPoses			=	0;
		StyleOld.m_EAnimParams			=	rAnim.m_AnimParams;
		RSingleEvaluation( StyleOld,TMPSTYLEOLD, parrRelQuats,parrControllerStatus,0, fScaling );
		parrRelQuats[TMPSTYLEOLD*numJoints+0].SetIdentity();

		SAnimation StyleNew;
		StyleNew.m_nEAnimID					=	nAnimIDNS;											//in.m_nEAnimID0[i];
		StyleNew.m_nEGlobalID				=	pAnimHeaderNS->m_nGlobalAnimId;	//in.m_nEGlobalID0[i];
		StyleNew.m_fEBlendWeight		=	1.0f;
		StyleNew.m_nESegmentCounter	=	0;
		StyleNew.m_nAnimEOC					=	0;	//if 1 then "end of cycle" reached
		StyleNew.m_fETimeNew				=	time; //this is a percentage value between 0-1
		StyleNew.m_fETimeOld				=	time; //this is a percentage value between 0-1
		StyleNew.m_fETimeOldPrev		=	time; //this is a percentage value between 0-1
		StyleNew.m_fETWDuration			=	0;	//in.m_fETWDuration;			//time-warped duration 
		StyleNew.m_numAimPoses			=	0;
		StyleNew.m_EAnimParams			=	rAnim.m_AnimParams;
		RSingleEvaluation( StyleNew,TMPSTYLENEW, parrRelQuats,parrControllerStatus,0, fScaling );
		parrRelQuats[TMPSTYLENEW*numJoints+0].SetIdentity();

		uint32 nFixPose	=	p*numJoints;
		for (uint32 j=0; j<numJoints; j++)
			parrTransModelSrc[nFixPose+j] = parrRelQuats[TMPSTYLENEW*numJoints+j]*parrRelQuats[TMPSTYLEOLD*numJoints+j].GetInverted();
	}

	memcpy( parrTransModelDst, parrTransModelSrc, numPoses*numJoints*sizeof(QuatT));

	//-----------------------------------------------------------------------------

	const int32 nSpecialJoints=13;
	int32 arrIndex[nSpecialJoints];
	arrIndex[ 0]	= m_pModelSkeleton->m_IdxArray[eIM_PelvisIdx];		//01

	arrIndex[ 1]	= m_pModelSkeleton->m_IdxArray[eIM_LThighIdx];		//02
	arrIndex[ 2]	= m_pModelSkeleton->m_IdxArray[eIM_LCalfIdx];			//03
	arrIndex[ 3]	= m_pModelSkeleton->m_IdxArray[eIM_LFootIdx];			//04
	arrIndex[ 4]	= m_pModelSkeleton->m_IdxArray[eIM_LHeelIdx];			//05
	arrIndex[ 5]	= m_pModelSkeleton->m_IdxArray[eIM_LToe0Idx];			//06
	arrIndex[ 6]	= m_pModelSkeleton->m_IdxArray[eIM_LToe0NubIdx];	//07

	arrIndex[ 7]	= m_pModelSkeleton->m_IdxArray[eIM_RThighIdx];		//08
	arrIndex[ 8]	= m_pModelSkeleton->m_IdxArray[eIM_RCalfIdx];			//09
	arrIndex[ 9]	= m_pModelSkeleton->m_IdxArray[eIM_RFootIdx];			//10
	arrIndex[10]	= m_pModelSkeleton->m_IdxArray[eIM_RHeelIdx];			//11
	arrIndex[11]	= m_pModelSkeleton->m_IdxArray[eIM_RToe0Idx];			//12
	arrIndex[12]	= m_pModelSkeleton->m_IdxArray[eIM_RToe0NubIdx];	//13
	for (uint32 idx=0; idx<nSpecialJoints; idx++ )
	{
		int32 j=arrIndex[idx];
		for (uint32 pose=0; pose<numPoses; pose++ )
		{
			QuatT q_m3	= parrTransModelSrc[ ((pose-2)&0x1f) * numJoints+j ]; //.SetIdentity();
			QuatT q_m2	= parrTransModelSrc[ ((pose-2)&0x1f) * numJoints+j ]; //.SetIdentity();
			QuatT q_m1	= parrTransModelSrc[ ((pose-1)&0x1f) * numJoints+j ]; //.SetIdentity();
			QuatT q_mid = parrTransModelSrc[ ((pose+0)&0x1f) * numJoints+j ]; //.SetIdentity();
			QuatT q_p1	= parrTransModelSrc[ ((pose+1)&0x1f) * numJoints+j ]; //.SetIdentity();
			QuatT q_p2	= parrTransModelSrc[ ((pose+2)&0x1f) * numJoints+j ]; //.SetIdentity();
			QuatT q_p3	= parrTransModelSrc[ ((pose+2)&0x1f) * numJoints+j ]; //.SetIdentity();
			if ( (q_mid.q|q_m3.q)<0.0f) q_m3.q = -q_m3.q;
			if ( (q_mid.q|q_m2.q)<0.0f) q_m2.q = -q_m2.q;
			if ( (q_mid.q|q_m1.q)<0.0f) q_m1.q = -q_m1.q;
			if ( (q_mid.q|q_p1.q)<0.0f) q_p1.q = -q_p1.q;
			if ( (q_mid.q|q_p2.q)<0.0f) q_p2.q = -q_p2.q;
			if ( (q_mid.q|q_p3.q)<0.0f) q_p3.q = -q_p3.q;
			parrTransModelDst[ pose*numJoints+j ].q = (q_m1.q +q_mid.q+ q_p1.q).GetNormalized();
			parrTransModelDst[ pose*numJoints+j ].t = (q_m1.t +q_mid.t+ q_p1.t)/3.0f;
			//parrTransModelDst[ pose*numJoints+j ].q = (q_m2.q+q_m1.q +q_mid.q+ q_p1.q+q_p2.q).GetNormalized();
			//parrTransModelDst[ pose*numJoints+j ].t = (q_m2.t+q_m1.t +q_mid.t+ q_p1.t+q_p2.t)/5.0f;
			//parrTransModelDst[ pose*numJoints+j ].q = (q_m3.q+q_m2.q+q_m1.q +q_mid.q+ q_p1.q+q_p2.q+q_p3.q).GetNormalized();
			//parrTransModelDst[ pose*numJoints+j ].t = (q_m3.t+q_m2.t+q_m1.t +q_mid.t+ q_p1.t+q_p2.t+q_p3.t)/7.0f;
		}
	}

	//-----------------------------------------------------------------------------

	f32 fFixedPose = f32( rAnim.m_fAnimTime*(numPoses-1) );
	uint32 nFixedPose0 = uint32( fFixedPose );
	uint32 nFixedPose1 = (nFixedPose0+1) & 0x1f;
	f32 fBlend = f32( fFixedPose-nFixedPose0 );

	assert(nFixedPose0<33);
	uint32 nFixPose0=nFixedPose0*numJoints;
	uint32 nFixPose1=nFixedPose1*numJoints;
	for (uint32 j=0; j<numJoints; j++)
	{
		QuatT tm; tm.SetNLerp(parrTransModelDst[nFixPose0+j],parrTransModelDst[nFixPose1+j],fBlend);
		parrRelQuats[nRedirectLayer*numJoints+j] = tm*parrRelQuats[nRedirectLayer*numJoints+j];
	}
*/
}



//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
void CSkeletonPose::AddLayer( const GlobalAnimationHeader& rGAH, const CAnimation& rAnim, uint32 nRedirectLayer, QuatT* parrRelQuats, Status4* parrControllerStatus, f32 fScaling, const char* strSpliceAnim  )
{
	CAnimationSet* pAnimationSet = &m_pInstance->m_pModel->m_AnimationSet;

	int32 nAnimIDNS	= pAnimationSet->GetAnimIDByName(strSpliceAnim);
	if (nAnimIDNS<0)
		return;
	const ModelAnimationHeader* pAnimHeaderNS = pAnimationSet->GetModelAnimationHeader(nAnimIDNS);

	
	SAnimation StyleNew;
	StyleNew.m_nEAnimID					=	nAnimIDNS;
	StyleNew.m_nEGlobalID				=	pAnimHeaderNS->m_nGlobalAnimId;
	StyleNew.m_fEBlendWeight		=	1.0f;
	StyleNew.m_nESegmentCounter	=	0;
	StyleNew.m_nAnimEOC					=	0;	//if 1 then "end of cycle" reached
	StyleNew.m_fETimeNew				=	rAnim.m_fAnimTime; //this is a percentage value between 0-1
	StyleNew.m_fETimeOld				=	rAnim.m_fAnimTime; //this is a percentage value between 0-1
	StyleNew.m_fETimeOldPrev		=	rAnim.m_fAnimTime; //this is a percentage value between 0-1
	StyleNew.m_fETWDuration			=	0;	//in.m_fETWDuration;			//time-warped duration 
	StyleNew.m_numAimPoses			=	0;
	StyleNew.m_EAnimParams			=	rAnim.m_AnimParams;

	assert(StyleNew.m_nEAnimID>=0);
	assert(StyleNew.m_nEGlobalID>=0);
	GlobalAnimationHeader& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalAnimations[StyleNew.m_nEGlobalID];
	rGlobalAnimHeader.m_nPlayedCount=1;
	assert(rAnim.m_fAnimTime>=0.0f && rAnim.m_fAnimTime<=1.0f);	

	//-------------------------------------------------------------------------------------
	//----             evaluate all controllers for this animation                    -----
	//-------------------------------------------------------------------------------------
	const CModelJoint* pModelJoint = GetModelJointPointer(0);
	IController* pController = g_AnimationManager.m_arrGlobalAnimations[StyleNew.m_nEGlobalID].GetControllerByJointID(pModelJoint[0].m_nJointCRC32); ;//pModelJoint[0].m_arrControllersMJoint[rAnim.m_nEAnimID];
	uint32 numJoints=m_AbsolutePose.size();
	for (uint32 j=0; j<numJoints; j++)
	{
		pController = g_AnimationManager.m_arrGlobalAnimations[StyleNew.m_nEGlobalID].GetControllerByJointID(pModelJoint[j].m_nJointCRC32);// pModelJoint[j].m_arrControllersMJoint[rAnim.m_nEAnimID];
		if (pController)
		{
			Quat rot;	Vec3 pos;
			Status4 status = pController->GetOP(	StyleNew.m_nEGlobalID, rAnim.m_fAnimTime, rot, pos );
			if (status.o)
				parrRelQuats[nRedirectLayer*numJoints+j].q=rot;
			if (status.p)
				parrRelQuats[nRedirectLayer*numJoints+j].t=pos;
		}
	}


}

//---------------------------------------------------------------------------
//-----                  Foot-Plant Detection                           -----
//---------------------------------------------------------------------------
SFootPlant CSkeletonPose::BlendFootPlants( const SFootPlant& fp0, const SFootPlant& fp1, f32 t)
{

	f32 olhs	= fp0.m_LHeelStart;		f32 nlhs	= fp1.m_LHeelStart; 
	f32 olhe	= fp0.m_LHeelEnd;			f32 nlhe	= fp1.m_LHeelEnd; 
	f32 olts	= fp0.m_LToe0Start;		f32 nlts	= fp1.m_LToe0Start; 
	f32 olte	= fp0.m_LToe0End;			f32 nlte	= fp1.m_LToe0End; 
	f32 orhs	= fp0.m_RHeelStart;		f32 nrhs	= fp1.m_RHeelStart; 
	f32 orhe	= fp0.m_RHeelEnd;			f32 nrhe	= fp1.m_RHeelEnd; 
	f32 orts	= fp0.m_RToe0Start;		f32 nrts	= fp1.m_RToe0Start; 
	f32 orte	= fp0.m_RToe0End;			f32 nrte	= fp1.m_RToe0End; 
	if (olhs<0 && olhe<0)	nlhs=nlhe=-100000;
	if (olts<0 && olte<0)	nlts=nlte=-100000;
	if (orhs<0 && orhe<0)	nrhs=nrhe=-100000;
	if (orts<0 && orte<0)	nrts=nrte=-100000;
	if (nlhs<0 && nlhe<0)	olhs=olhe=-100000;
	if (nlts<0 && nlte<0)	olts=olte=-100000;
	if (nrhs<0 && nrhe<0)	orhs=orhe=-100000;
	if (nrts<0 && nrte<0)	orts=orte=-100000;
	SFootPlant BFPlant;
	BFPlant.m_LHeelStart	= olhs*(1-t) + nlhs*t; 
	BFPlant.m_LHeelEnd		= olhe*(1-t) + nlhe*t; 
	BFPlant.m_LToe0Start	= olts*(1-t) + nlts*t; 
	BFPlant.m_LToe0End		= olte*(1-t) + nlte*t; 
	BFPlant.m_RHeelStart	= orhs*(1-t) + nrhs*t; 
	BFPlant.m_RHeelEnd		= orhe*(1-t) + nrhe*t; 
	BFPlant.m_RToe0Start	= orts*(1-t) + nrts*t; 
	BFPlant.m_RToe0End		= orte*(1-t) + nrte*t; 

	return BFPlant;
}




RMovement CSkeletonPose::RTwoWayBlend(uint32 src0,uint32 src1,uint32 dst, f32 r,f32 t, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* g_RootInfo )
{
	assert(t<=1);
	assert(t>=0);
	RMovement BMovement;

	if (src0 >= numTMPLAYERS || src1 >= numTMPLAYERS) 
	{
		assert(0);
		return BMovement;
	}


	f32 fColor[4] = { 0.0f, 0.5f, 1.0f, 1 };
	if (Console::GetInst().ca_DebugText || m_pSkeletonAnim->m_ShowDebugText)
	{
		//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Vertical Blending; %f", t );
		//g_YLine+=0x20;
	}


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


	f32 DeltaYPR0	= g_RootInfo[src0].m_fRelRotation; 
	f32 duration0		= g_RootInfo[src0].m_fCurrentDuration; 
	f32 slope0			= g_RootInfo[src0].m_fCurrentSlope; 

	f32 DeltaYPR1	= g_RootInfo[src1].m_fRelRotation; 
	f32 duration1		= g_RootInfo[src1].m_fCurrentDuration; 
	f32 slope1			= g_RootInfo[src1].m_fCurrentSlope; 

	//parrRelQuats[numJoints*src0+0].q.Normalize();
	//parrRelQuats[numJoints*src1+0].q.Normalize();

	BMovement.m_fRelRotation= DeltaYPR0*(1-t) + DeltaYPR1*t;

	BMovement.m_fCurrentDuration	= duration0*(1-r)	+ duration1*r;
	BMovement.m_fCurrentSlope			= slope0*(1-r)		+ slope1*r;
	BMovement.m_nFootBits					=	g_RootInfo[src0].m_nFootBits&g_RootInfo[src1].m_nFootBits;

	if (m_pSkeletonAnim->m_FuturePath)
		for (uint32 i=0; i<ANIM_FUTURE_PATH_LOOKAHEAD; i++ )
		{
			BMovement.m_relFuturePath[i].m_TransRot.SetNLerp( g_RootInfo[src0].m_relFuturePath[i].m_TransRot, g_RootInfo[src1].m_relFuturePath[i].m_TransRot, r );
			BMovement.m_relFuturePath[i].m_NormalizedTimeAbs = g_RootInfo[src0].m_relFuturePath[i].m_NormalizedTimeAbs*(1-t) + g_RootInfo[src1].m_relFuturePath[i].m_NormalizedTimeAbs*r;
			BMovement.m_relFuturePath[i].m_KeyTimeAbs = g_RootInfo[src0].m_relFuturePath[i].m_KeyTimeAbs*(1-t) + g_RootInfo[src1].m_relFuturePath[i].m_KeyTimeAbs*r;
			//	assert(g_RootInfo[src0].m_relFuturePath[i].m_AbsoluteTime==g_RootInfo[src1].m_relFuturePath[i].m_AbsoluteTime);
		}


		Vec3 TravelVec0 = g_RootInfo[src0].m_vCurrentVelocity;
		Vec3 TravelVec1 = g_RootInfo[src1].m_vCurrentVelocity;
		BMovement.m_vCurrentVelocity.SetLerp(TravelVec0,TravelVec1, r);

		f32 fTurnRad0 = g_RootInfo[src0].m_fCurrentTurn;
		f32 fTurnRad1 = g_RootInfo[src1].m_fCurrentTurn;
		BMovement.m_fCurrentTurn= fTurnRad0*(1.0f-r) + fTurnRad1*r;

		Vec3 RelTrans0	=	g_RootInfo[src0].m_vRelTranslation;
		Vec3 RelTrans1	=	g_RootInfo[src1].m_vRelTranslation;
		BMovement.m_vRelTranslation.SetLerp(RelTrans0,RelTrans1,r);

		//		float fColor[4] = {0,1,0,1};
		//		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"RelTrans0: %f %f %f",RelTrans0.x,RelTrans0.y,RelTrans0.z );	g_YLine+=16.0f;
		//		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"RelTrans1: %f %f %f",RelTrans1.x,RelTrans1.y,RelTrans1.z );	g_YLine+=16.0f;
		//		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"BMovement.m_relCITranslation: %f %f %f",BMovement.m_relCITranslation.x,BMovement.m_relCITranslation.y,BMovement.m_relCITranslation.z );	g_YLine+=16.0f;

		BMovement.m_FootPlant =	BlendFootPlants(g_RootInfo[src0].m_FootPlant,  g_RootInfo[src1].m_FootPlant,r);

		BMovement.m_fAllowMultilayerAnim = g_RootInfo[src0].m_fAllowMultilayerAnim * (1-r) + g_RootInfo[src1].m_fAllowMultilayerAnim * r;
		for (int i=0; i<NUM_ANIMATION_USER_DATA_SLOTS; i++)
			BMovement.m_fUserData[i] = g_RootInfo[src0].m_fUserData[i] * (1-r) + g_RootInfo[src1].m_fUserData[i] * r;

		if (dst==FINALREL)
		{
			if (parrControllerStatus[numJoints*src0+0].o)
				m_RelativePose[0].q.SetSlerp( parrRelQuats[numJoints*src0+0].q, parrRelQuats[numJoints*src1+0].q, r );
			if (parrControllerStatus[numJoints*src0+0].p)
				m_RelativePose[0].t.SetLerp ( parrRelQuats[numJoints*src0+0].t, parrRelQuats[numJoints*src1+0].t, r );
			m_arrControllerInfo[0].ops      = parrControllerStatus[numJoints*src0+0].ops|parrControllerStatus[numJoints*src1+0].ops;
			assert(m_arrControllerInfo[0].o<2);
			assert(m_arrControllerInfo[0].p<2);
		}
		else
		{
			if (parrControllerStatus[numJoints*src0+0].o)
				parrRelQuats[numJoints*dst+0].q.SetSlerp( parrRelQuats[numJoints*src0+0].q, parrRelQuats[numJoints*src1+0].q, r );//	=	BMovement.m_RootOrientation;
			if (parrControllerStatus[numJoints*src0+0].p)
				parrRelQuats[numJoints*dst+0].t.SetLerp ( parrRelQuats[numJoints*src0+0].t, parrRelQuats[numJoints*src1+0].t, r );

			parrControllerStatus[numJoints*dst+0].ops = parrControllerStatus[numJoints*src0+0].ops|parrControllerStatus[numJoints*src1+0].ops;
			assert(parrControllerStatus[numJoints*dst+0].o<2);
			assert(parrControllerStatus[numJoints*dst+0].p<2);
		}


		//----------------------------------------------------------------------------------------------
		//----                  joint blending                           -------------------------------
		//----------------------------------------------------------------------------------------------

		BMovement.m_FSpine0RelQuat.SetNlerp( g_RootInfo[src0].m_FSpine0RelQuat,g_RootInfo[src1].m_FSpine0RelQuat,t ); 
		BMovement.m_FSpine1RelQuat.SetNlerp( g_RootInfo[src0].m_FSpine1RelQuat,g_RootInfo[src1].m_FSpine1RelQuat,t ); 
		BMovement.m_FSpine2RelQuat.SetNlerp( g_RootInfo[src0].m_FSpine2RelQuat,g_RootInfo[src1].m_FSpine2RelQuat,t ); 
		BMovement.m_FSpine3RelQuat.SetNlerp( g_RootInfo[src0].m_FSpine3RelQuat,g_RootInfo[src1].m_FSpine3RelQuat,t ); 
		BMovement.m_FNeckRelQuat.SetNlerp(   g_RootInfo[src0].m_FNeckRelQuat,  g_RootInfo[src1].m_FNeckRelQuat,  t ); 

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

		return BMovement;
}





void CSkeletonPose::R_BLENDLAYER_MOVE(uint32 src,uint32 dst,f32 t, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* g_RootInfo )
{
	if (src >= numTMPLAYERS) 
		return;

	//--------------------------------------------------------------------
	uint32 numJoints=m_AbsolutePose.size();
	g_RootInfo[dst].m_fRelRotation			= g_RootInfo[src].m_fRelRotation*t; 
	g_RootInfo[dst].m_fCurrentDuration  = g_RootInfo[src].m_fCurrentDuration*t;
	g_RootInfo[dst].m_fCurrentSlope	    = g_RootInfo[src].m_fCurrentSlope*t;

	g_RootInfo[dst].m_fCurrentTurn	    =	g_RootInfo[src].m_fCurrentTurn*t;
	g_RootInfo[dst].m_vCurrentVelocity	=	g_RootInfo[src].m_vCurrentVelocity*t;
	g_RootInfo[dst].m_vRelTranslation		=	g_RootInfo[src].m_vRelTranslation*t;

	g_RootInfo[dst].m_nFootBits					=	g_RootInfo[src].m_nFootBits;
	g_RootInfo[dst].m_FootPlant					= g_RootInfo[src].m_FootPlant*t;


	if (m_pSkeletonAnim->m_FuturePath)
	{
		for (uint32 i=0; i<ANIM_FUTURE_PATH_LOOKAHEAD; i++ )
		{
			g_RootInfo[dst].m_relFuturePath[i].m_TransRot.q = g_RootInfo[src].m_relFuturePath[i].m_TransRot.q*t;
			g_RootInfo[dst].m_relFuturePath[i].m_TransRot.t = g_RootInfo[src].m_relFuturePath[i].m_TransRot.t*t;
			g_RootInfo[dst].m_relFuturePath[i].m_NormalizedTimeAbs = g_RootInfo[src].m_relFuturePath[i].m_NormalizedTimeAbs*t;
			g_RootInfo[dst].m_relFuturePath[i].m_KeyTimeAbs = g_RootInfo[src].m_relFuturePath[i].m_KeyTimeAbs*t;
		}
	}


	parrRelQuats[numJoints*dst+0].q =	parrRelQuats[numJoints*src+0].q*t;
	parrRelQuats[numJoints*dst+0].t =	parrRelQuats[numJoints*src+0].t*t;
	parrControllerStatus[numJoints*dst+0].ops = parrControllerStatus[numJoints*src+0].ops;

	//------------------------------------------------------------------------------------------------------


	if (m_bFullSkeletonUpdate)
	{
		g_RootInfo[dst].m_FSpine0RelQuat			=  g_RootInfo[src].m_FSpine0RelQuat*t; 
		g_RootInfo[dst].m_FSpine1RelQuat			=  g_RootInfo[src].m_FSpine1RelQuat*t; 
		g_RootInfo[dst].m_FSpine2RelQuat			=  g_RootInfo[src].m_FSpine2RelQuat*t; 
		g_RootInfo[dst].m_FSpine3RelQuat			=  g_RootInfo[src].m_FSpine3RelQuat*t; 
		g_RootInfo[dst].m_FNeckRelQuat				=  g_RootInfo[src].m_FNeckRelQuat*t; 

		for (uint32 i=1; i<numJoints; i++)
		{
			if (parrControllerStatus[numJoints*src+i].o)
				parrRelQuats[numJoints*dst+i].q = parrRelQuats[numJoints*src+i].q*t;
			if (parrControllerStatus[numJoints*src+i].p)
				parrRelQuats[numJoints*dst+i].t = parrRelQuats[numJoints*src+i].t*t;
			parrControllerStatus[numJoints*dst+i].ops = parrControllerStatus[numJoints*src+i].ops;
		}
	}


}




void CSkeletonPose::R_BLENDLAYER_ADD(uint32 src,uint32 dst,f32 t, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* g_RootInfo)
{
	if (src >= numTMPLAYERS) 
		return;

	//--------------------------------------------------------------------
	uint32 numJoints=m_AbsolutePose.size();
	g_RootInfo[dst].m_fRelRotation				+= g_RootInfo[src].m_fRelRotation*t; 
	g_RootInfo[dst].m_fCurrentDuration		+= g_RootInfo[src].m_fCurrentDuration*t;
	g_RootInfo[dst].m_fCurrentSlope				+= g_RootInfo[src].m_fCurrentSlope*t;

	g_RootInfo[dst].m_fCurrentTurn		    +=	g_RootInfo[src].m_fCurrentTurn*t;
	g_RootInfo[dst].m_vCurrentVelocity		+=	g_RootInfo[src].m_vCurrentVelocity*t;
	g_RootInfo[dst].m_vRelTranslation			+=	g_RootInfo[src].m_vRelTranslation*t;

	g_RootInfo[dst].m_nFootBits						&=	g_RootInfo[src].m_nFootBits;
	g_RootInfo[dst].m_FootPlant						+=	g_RootInfo[src].m_FootPlant*t;

	if (m_pSkeletonAnim->m_FuturePath)
	{
		for (uint32 i=0; i<ANIM_FUTURE_PATH_LOOKAHEAD; i++ )
		{
			g_RootInfo[dst].m_relFuturePath[i].m_TransRot.q %= g_RootInfo[src].m_relFuturePath[i].m_TransRot.q*t;
			g_RootInfo[dst].m_relFuturePath[i].m_TransRot.t += g_RootInfo[src].m_relFuturePath[i].m_TransRot.t*t;
			g_RootInfo[dst].m_relFuturePath[i].m_NormalizedTimeAbs += g_RootInfo[src].m_relFuturePath[i].m_NormalizedTimeAbs*t;
			g_RootInfo[dst].m_relFuturePath[i].m_KeyTimeAbs += g_RootInfo[src].m_relFuturePath[i].m_KeyTimeAbs*t;
		}
	}


	parrRelQuats[numJoints*dst+0].q %= parrRelQuats[numJoints*src+0].q*t;
	parrRelQuats[numJoints*dst+0].t += parrRelQuats[numJoints*src+0].t*t;
	parrControllerStatus[numJoints*dst+0].ops |= parrControllerStatus[numJoints*src+0].ops;


	//------------------------------------------------------------------------------------------------------

	if (m_bFullSkeletonUpdate)
	{
		g_RootInfo[dst].m_FSpine0RelQuat				%=  g_RootInfo[src].m_FSpine0RelQuat*t; 
		g_RootInfo[dst].m_FSpine1RelQuat				%=  g_RootInfo[src].m_FSpine1RelQuat*t; 
		g_RootInfo[dst].m_FSpine2RelQuat				%=  g_RootInfo[src].m_FSpine2RelQuat*t; 
		g_RootInfo[dst].m_FSpine3RelQuat				%=  g_RootInfo[src].m_FSpine3RelQuat*t; 
		g_RootInfo[dst].m_FNeckRelQuat					%=  g_RootInfo[src].m_FNeckRelQuat*t; 

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
void CSkeletonPose::R_BLENDLAYER_NORM(uint32 dst, QuatT* parrRelQuats, Status4* parrControllerStatus, RMovement* g_RootInfo )
{
	if (m_pSkeletonAnim->m_FuturePath)
	{
		for (uint32 i=0; i<ANIM_FUTURE_PATH_LOOKAHEAD; i++ )
			g_RootInfo[dst].m_relFuturePath[i].m_TransRot.q.Normalize();
	}

	uint32 numJoints=m_AbsolutePose.size();
	parrRelQuats[numJoints*dst+0].q.v.x=0;
	parrRelQuats[numJoints*dst+0].q.v.y=0;
	parrRelQuats[numJoints*dst+0].q.Normalize();
	//	g_RootInfo[dst].m_relRotation.Normalize();

	//---------------------------------------------------

	if (m_bFullSkeletonUpdate)
	{
		g_RootInfo[dst].m_FSpine0RelQuat.Normalize(); 
		g_RootInfo[dst].m_FSpine1RelQuat.Normalize(); 
		g_RootInfo[dst].m_FSpine2RelQuat.Normalize(); 
		g_RootInfo[dst].m_FSpine3RelQuat.Normalize(); 
		g_RootInfo[dst].m_FNeckRelQuat.Normalize(); 

		for (uint32 i=1; i<numJoints; i++)
			if (parrControllerStatus[numJoints*dst+i].o)
				parrRelQuats[numJoints*dst+i].q.Normalize();
	}

}


