//////////////////////////////////////////////////////////////////////
//
//  CryEngine Source code
//	
//	File:CSkinInstance.cpp
//  Implementation of CSkinInstance class
//
//	History:
//	September 23, 2004: Created by Ivo Herzeg <ivo@crytek.de>
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"

#include <I3DEngine.h>
#include <Cry_Camera.h>
#include <CryAnimationScriptCommands.h>


#include "ModelMesh.h"
#include "CharacterManager.h"
#include "CryCharAnimationParams.h"
#include "CryCharMorphParams.h"
#include "IRenderAuxGeom.h"
#include "FacialAnimation/FacialInstance.h"
#include "GeomQuery.h"
#include "CharacterInstance.h"
#include "ControllerPQ.h"


CCharInstance::CCharInstance ( const string &strFileName, CCharacterModel* pModel) : CSkinInstance( strFileName, pModel, 0xaaaabbbb,0)
{
	LOADING_TIME_PROFILE_SECTION(g_pISystem);

	uint32 ddd=0;
	m_AttachmentManager.m_pSkinInstance=this;
	m_AttachmentManager.m_pSkelInstance=this;

	if (m_pFacialInstance==0)
	{
		if (m_pModel->GetFacialModel())
		{
			m_pFacialInstance = new CFacialInstance(m_pModel->GetFacialModel(),this,this);
			m_pFacialInstance->AddRef();
		}
	}




	m_fDeltaTime=0;
	m_fOriginalDeltaTime=0;

	m_pCamera=0;
	g_nCharInstanceLoaded++;
	m_nInstanceNumber = g_nCharInstanceLoaded;
	m_RenderCharLocation.SetIdentity();
	m_PhysEntityLocation.SetIdentity();

	m_SkeletonAnim.InitSkeletonAnim(this,&this->m_SkeletonPose);
	m_SkeletonPose.InitSkeletonPose(this,&this->m_SkeletonAnim);
	m_SkeletonPose.UpdateBBox(1);
	m_SkeletonPose.m_bHasPhysics = m_pModel->m_bHasPhysics2;

	m_ResetMode=0;
	m_HideMaster=0;
	m_bWasVisible = 0;
	m_bPlayedPhysAnim = 0;
	m_bEnableStartAnimation	=	1;


	m_iActiveFrame = 0;
	m_iBackFrame = 1;

	m_fFPWeapon=0;
	m_vDifSmooth=Vec3(ZERO);
	m_vDifSmoothRate=Vec3(ZERO);
	m_fAnimSpeedScale = 1;
	m_LastUpdateFrameID_Pre=0;
	m_LastUpdateFrameID_PreType=-1;
	m_LastUpdateFrameID_Post=0;
	m_LastUpdateFrameID_PostType=-1;

	uint32 numJoints		= m_pModel->m_pModelSkeleton->m_arrModelJoints.size();
	QuatTS identityQuatTS;	identityQuatTS.SetIdentity();

	for (uint32 b=0; b<MAX_MOTIONBLUR_FRAMES; b++ )
		m_arrNewSkinQuat[b].resize(numJoints,identityQuatTS);
	m_arrShapeDeformValues[0]=0.0f;
	m_arrShapeDeformValues[1]=0.0f;
	m_arrShapeDeformValues[2]=0.0f;
	m_arrShapeDeformValues[3]=0.0f;
	m_arrShapeDeformValues[4]=0.0f;
	m_arrShapeDeformValues[5]=0.0f;
	m_arrShapeDeformValues[6]=0.0f;
	m_arrShapeDeformValues[7]=0.0f;

	rp.m_Color = ColorF(1.0f);
	rp.m_nFlags = CS_FLAG_DRAW_MODEL;
	m_HadUpdate=0;
  m_bUpdateMotionBlurSkinnning = false;
  m_bPrevFrameFPWeapon = false;

	m_pAlternativeModelForGetRandomPos = NULL;
}


void CCharInstance::Release()
{
	if (--m_nRefCounter <= 0)
	{
		const char* pFilePath = GetFilePath();
		g_pCharacterManager->ReleaseCDF(pFilePath);
		delete this;
	}
}

//////////////////////////////////////////////////////////////////////////
CCharInstance::~CCharInstance()
{
	m_SkeletonPose.DestroyPhysics();
	g_nCharInstanceLoaded--;
	g_nCharInstanceDeleted++;
}




// calculates the mask ANDed with the frame id that's used to determine whether to skin the character on this frame or not.
uint32 CCharInstance::GetUpdateFrequencyMask(const Vec3& WPos, const CCamera& rCamera)
{
	f32 fRadius  = (m_SkeletonPose.m_AABB.max-m_SkeletonPose.m_AABB.min).GetLength();
	Vec3 WCenter = (m_SkeletonPose.m_AABB.max+m_SkeletonPose.m_AABB.min)*0.5f+WPos;
	f32 fScale	= 2.0f*fRadius; 

	// on dedicated server, this path will always be taken;
	// on normal clients, this will always be rejected
	if(g_bUpdateBonesAlways)
		return 0;

	float fZoomFactor = 0.01f+0.99f*(RAD2DEG(rCamera.GetFov())/90.f);  

	float fScaledDist = WCenter.GetDistance(rCamera.GetPosition())*fZoomFactor;

	uint32 iMask = 7;  //update every 8th frame
	if(fScaledDist<(30.0f*fScale)) iMask &= 3; //update every 4th frame
	if(fScaledDist<(20.0f*fScale)) iMask &= 1; //update every 2nd frame
	if(fScaledDist<(10.0f*fScale)) iMask &= 0; //update every frame
	bool visible = rCamera.IsSphereVisible_F( Sphere(WCenter,fRadius) );
	if (visible==0) 
	{
		//not visible! update at least every 4th frame
		iMask <<= 2;
		iMask |= 3;
	}

	return iMask;
}



//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
void CCharInstance::SkeletonPreProcess(const QuatT &rPhysLocationCurr, const QuatTS &rAnimLocationCurr, const CCamera& rCamera, uint32 OnRender )
{
	DEFINE_PROFILER_FUNCTION();

	//float fColor[4] = {1,1,0,1};
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Scaling: %f",rAnimLocationCurr.s );	g_YLine+=16.0f;

#ifdef _DEBUG
	SCOPED_ENABLE_FLOAT_EXCEPTIONS;
#endif

	//_controlfp(0, _EM_INVALID|_EM_ZERODIVIDE | _PC_64 );
	m_RenderCharLocation=rAnimLocationCurr;  //just for debugging

	QuatTS AnimCharLocation=QuatTS(IDENTITY);
	m_SkeletonPose.m_FootAnchor.m_vFootSlide = Vec3(ZERO);

	m_pSkinAttachment =	0;
	uint32 numAttachments = m_AttachmentManager.m_arrAttachments.size();
	for (uint32 i=0; i<numAttachments; i++)
	{
		CAttachment* pAttachment=m_AttachmentManager.m_arrAttachments[i];
		uint32 type=pAttachment->m_Type;
		IAttachmentObject* pIAttachmentObject = pAttachment->GetIAttachmentObject();	
		if (pIAttachmentObject) 
		{
			CSkinInstance* pSkinInstance = (CSkinInstance*)pIAttachmentObject->GetICharacterInstance();
			if (pSkinInstance) 
				pSkinInstance->m_pSkinAttachment=0; 
		}
	}

	m_pCamera =	&rCamera;

	pe_params_flags pf;
	IPhysicalEntity *pCharPhys = m_SkeletonPose.GetCharacterPhysics();
	if (m_pModel->m_ObjectType==CHR && !(pCharPhys && pCharPhys->GetType()==PE_ARTICULATED && pCharPhys->GetParams(&pf) && pf.flags & aef_recorded_physics))
	{
		m_PhysEntityLocation	=	rPhysLocationCurr;
		AnimCharLocation			=	rAnimLocationCurr;
		if (Console::GetInst().ca_RandomScaling)
		{
			uint32 inst=(uint32(this)>>3)&0x0f;
			AnimCharLocation.s = f32(inst)/160.0f+0.95f;
			//float fColor[4] = {1,1,0,1};
			//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"Scaling: %x %f",inst, AnimCharLocation.s );	g_YLine+=16.0f;
		}
	}
	else
	{
		m_PhysEntityLocation	=	QuatT(rAnimLocationCurr);
		AnimCharLocation			=	rAnimLocationCurr;
	}



	uint32 nErrorCode=0;
	uint32 minValid = m_SkeletonPose.m_AABB.min.IsValid();
	if (minValid==0) nErrorCode|=0x8000;
	uint32 maxValid = m_SkeletonPose.m_AABB.max.IsValid();
	if (maxValid==0) nErrorCode|=0x8000;
	if (Console::GetInst().ca_SaveAABB && nErrorCode)
	{
		m_SkeletonPose.m_AABB.max=Vec3( 2, 2, 2);
		m_SkeletonPose.m_AABB.min=Vec3(-2,-2,-2);
	}




	/*		
	const char* mname = GetModelFilePath();
	if ( strcmp(mname,"objects/weapons/attachments/tactical_attachment/tactical_attachment_fp.chr")==0 )
	{
	float fColor[4] = {1,1,0,1};
	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"pre update: %d    Model: %s ",OnRender,mname );	g_YLine+=16.0f;
	} */





	m_fOriginalDeltaTime	= g_pITimer->GetFrameTime();
	assert(m_fOriginalDeltaTime<0.51f);
	if (m_fOriginalDeltaTime>+0.5f) m_fOriginalDeltaTime=+0.5f;
	if (m_fOriginalDeltaTime<-0.5f) m_fOriginalDeltaTime=-0.5f;

	m_fDeltaTime					=	m_fOriginalDeltaTime*m_fAnimSpeedScale;
	assert(m_fDeltaTime<4.0f);


//	float fColor[4] = {1,1,0,1};
//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.7f, fColor, false,"m_fOriginalDeltaTime: %f",m_fOriginalDeltaTime );	
//	g_YLine+=16.0f;

	//---------------------------------------------------------------------------------

	bool debug = (gEnv->pConsole->GetCVar("es_logDrawnActors")->GetIVal() != 0);

	if(debug)
	{
		IEntity * pEntity = (IEntity*) m_SkeletonPose.GetCharacterPhysics()->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
		if(pEntity)
			CryLogAlways("%s: CCharacterInstance::SkeletonPreProcess setting bFullSkeletonUpdate to 0", pEntity->GetName());
	}
	m_SkeletonPose.m_bFullSkeletonUpdate=0;
	int nCurrentFrameID = g_pCharacterManager->m_nUpdateCounter; //g_pIRenderer->GetFrameID(false);
	uint32 dif = nCurrentFrameID-m_LastRenderedFrameID;
	m_SkeletonPose.m_bInstanceVisible = (dif<5) || m_SkeletonAnim.GetTrackViewStatus();

	// depending on how far the character from the player is, the bones may be updated once per several frames
	int nUFM = GetUpdateFrequencyMask(AnimCharLocation.t,rCamera);

	m_nInstanceUpdateCounter++;
	uint32 FrameMask		= m_nInstanceUpdateCounter&nUFM;
	uint32 InstanceMask = m_nInstanceNumber&nUFM;
	bool update = m_SkeletonPose.m_timeStandingUp<0 ? FrameMask==InstanceMask : true;

	if (m_SkeletonAnim.m_TrackViewExclusive)
	{
		if(debug)
			CryLogAlways("m_TrackViewExclusive != 0 so bFullSkeletonUpdate set to 1");
		m_SkeletonPose.m_bFullSkeletonUpdate=1;
	}
	if (update==0)
	{
		if(debug)
			CryLogAlways("update == 0 so bFullSkeletonUpdate set to 0");
		m_SkeletonPose.m_bFullSkeletonUpdate=0;
	}
	if (m_SkeletonPose.m_nForceSkeletonUpdate)
	{
		if(debug)
			CryLogAlways("m_nForceSkeletonUpdate != 0 so bFullSkeletonUpdate set to 1");
		m_SkeletonPose.m_bFullSkeletonUpdate=1;
	}
	if (m_SkeletonPose.m_bInstanceVisible)
	{
		if(debug)
			CryLogAlways("m_bInstanceVisible != 0 so bFullSkeletonUpdate set to 1");
		m_SkeletonPose.m_bFullSkeletonUpdate=1;
	}
	if (Console::GetInst().ca_JustRootUpdate == 1)
	{
		if(debug)
			CryLogAlways("ca_JustRootUpdate == 1 so bFullSkeletonUpdate set to 0");
		m_SkeletonPose.m_bFullSkeletonUpdate=0;
	}
	if (Console::GetInst().ca_JustRootUpdate == 2)
	{
		if(debug)
			CryLogAlways("ca_justRootUpdate == 2 so bFullSkeletonUpdate set to 1");
		m_SkeletonPose.m_bFullSkeletonUpdate=1;
	}

	m_SkeletonAnim.m_IsAnimPlaying=0;
	m_SkeletonAnim.ProcessAnimations( m_PhysEntityLocation, AnimCharLocation, OnRender );
}


//////////////////////////////////////////////////////////////////////////
void CCharInstance::UpdatePhysicsCGA( float fScale, const QuatTS& rAnimLocationNext )
{
	DEFINE_PROFILER_FUNCTION();

#ifdef VTUNE_PROFILE 
	g_pISystem->VTuneResume();
#endif

	if (!m_SkeletonPose.GetCharacterPhysics())
		return;

	//if (m_fOriginalDeltaTime <= 0.0f)
		//return;

	pe_params_part params;
	pe_action_set_velocity asv;
	pe_status_pos sp;
	int numNodes = m_SkeletonPose.m_AbsolutePose.size();
	assert(numNodes);

	int i,iLayer,iLast=-1;
	bool bSetVel=0;
	uint32 nType= m_SkeletonPose.GetCharacterPhysics()->GetType();
	bool bBakeRotation = m_SkeletonPose.GetCharacterPhysics()->GetType()==PE_ARTICULATED;
	if (bSetVel = bBakeRotation) 
	{
		for(iLayer=0;iLayer<numVIRTUALLAYERS && m_SkeletonAnim.GetNumAnimsInFIFO(iLayer)==0;iLayer++);
		bSetVel = iLayer<numVIRTUALLAYERS;
	}
	params.bRecalcBBox = false;


	for (i=0; i<numNodes; i++)
	{
SetAgain:
		if (m_SkeletonPose.m_ppBonePhysics && m_SkeletonPose.m_ppBonePhysics[i])
		{
			sp.ipart = 0; MARK_UNUSED sp.partid;
			m_SkeletonPose.m_ppBonePhysics[i]->GetStatus(&sp);
			m_SkeletonPose.m_AbsolutePose[i].q = !rAnimLocationNext.q*sp.q;
			m_SkeletonPose.m_AbsolutePose[i].t = !rAnimLocationNext.q*(sp.pos-rAnimLocationNext.t);
			continue;
		}
		if (m_SkeletonPose.m_parrModelJoints[i].m_NodeID==~0)
			continue;
		//	params.partid = joint->m_nodeid;
		params.partid = m_SkeletonPose.m_parrModelJoints[i].m_NodeID;

		assert( m_SkeletonPose.m_AbsolutePose[i].IsValid() );
		params.q		=	m_SkeletonPose.m_AbsolutePose[i].q;

		params.pos	= m_SkeletonPose.m_AbsolutePose[i].t*rAnimLocationNext.s;
		if (rAnimLocationNext.s != 1.0f)
			params.scale = rAnimLocationNext.s;

		if (bBakeRotation)
		{
			params.pos = rAnimLocationNext.q*params.pos;
			params.q = rAnimLocationNext.q*params.q;
		}
		if (m_SkeletonPose.GetCharacterPhysics()->SetParams( &params ))
			iLast = i;
		if (params.bRecalcBBox)
			break;
	}

	// Recompute box after.
	if (iLast>=0 && !params.bRecalcBBox) 
	{
		params.bRecalcBBox = true;
		i = iLast;
		goto SetAgain;
	}

	if (bSetVel)
	{
		int iParent;
		QuatT qParent;
		IController* pController;
		float t;

		QuatT qAdditional[MAX_JOINT_AMOUNT];
		for(i=0; i<numNodes; i++)
			qAdditional[i] = m_SkeletonPose.m_RelativePose[i];			

		for(iLayer=0; iLayer<numVIRTUALLAYERS; iLayer++) if (m_SkeletonAnim.GetNumAnimsInFIFO(iLayer))
		{
			CAnimation &anim = m_SkeletonAnim.GetAnimFromFIFO(iLayer,0);
			int idGlobal = anim.m_LMG0.m_nGlobalID[0];
			int idAnim = anim.m_LMG0.m_nAnimID[0];
			GlobalAnimationHeader& animHdr = g_AnimationManager.m_arrGlobalAnimations[idGlobal];
			t = anim.m_fAnimTime + m_fOriginalDeltaTime/(animHdr.m_fEndSec - animHdr.m_fStartSec);

			for(i=0; i<numNodes; i++)	if (pController = animHdr.GetControllerByJointID(m_pModel->m_pModelSkeleton->m_arrModelJoints[i].m_nJointCRC32))//m_pModel->m_pModelSkeleton->m_arrModelJoints[i].m_arrControllersMJoint[idAnim])
				pController->GetOP(idGlobal, t, qAdditional[i].q, qAdditional[i].t);
		}

		for(i=0; i<numNodes; i++) if ((iParent=m_SkeletonPose.m_parrModelJoints[i].m_idxParent)>=0 && iParent!=i)
			qAdditional[i] =	qAdditional[iParent]*qAdditional[i];			

		t = m_fOriginalDeltaTime==0.f ? 0.f : 1.f/m_fOriginalDeltaTime;

		for(i=0; i<numNodes; i++) if (m_SkeletonPose.m_parrModelJoints[i].m_NodeID!=~0)
		{
			asv.partid = m_SkeletonPose.m_parrModelJoints[i].m_NodeID;
			asv.v = (qAdditional[i].t-m_SkeletonPose.m_AbsolutePose[i].t)*t;
			Quat dq = qAdditional[i].q*!m_SkeletonPose.m_AbsolutePose[i].q;
			float sin2 = dq.v.len();
			asv.w = sin2>0 ? dq.v*(atan2_tpl(sin2*dq.w*2,dq.w*dq.w-sin2*sin2)*t/sin2) : Vec3(0);
			m_SkeletonPose.GetCharacterPhysics()->Action(&asv);
		}
		m_bPlayedPhysAnim = 1;
	} else if (bBakeRotation && m_bPlayedPhysAnim)
	{
		asv.v.zero(); asv.w.zero();
		for(i=0; i<numNodes; i++) if ((asv.partid = m_SkeletonPose.m_parrModelJoints[i].m_NodeID)!=~0)
			m_SkeletonPose.GetCharacterPhysics()->Action(&asv);			
		m_bPlayedPhysAnim = 0;
	}

	if (m_SkeletonPose.m_ppBonePhysics)
		m_SkeletonPose.UpdateBBox(1);

#ifdef VTUNE_PROFILE 
	g_pISystem->VTunePause();
#endif

}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------

AABB CCharInstance::GetAABB() 
{ 

/*
	const char* mname = GetFilePath();
	int32 h=m_SkeletonPose.m_pModelSkeleton->IsHuman();
	if (h)
	{
		float fC1[4] = {1,0,0,1};
		g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.6f, fC1, false,"GetAABB:  Model: %s ", mname );	
		g_YLine+=16.0f;
	} */

/*
	if (m_SkeletonAnim.m_AnimationDrivenMotion)
	{
		//we have to adjust the bounding box to consider the render-offset
		AABB aabb = m_SkeletonPose.GetAABB();	
		aabb.min+=m_SkeletonPose.m_vRenderOffset;			
		aabb.max+=m_SkeletonPose.m_vRenderOffset;			
		return aabb;
	}*/

	return m_SkeletonPose.GetAABB();	
}


float CCharInstance::ComputeExtent(GeomQuery& geo, EGeomForm eForm)
{
	if (m_pModel)
	{
		if (m_pModel->m_ObjectType==CGA)
		{
			// Iterate parts.
			float fExtent = 0.f;
			geo.AllocParts(m_SkeletonPose.m_arrCGAJoints.size());
			for (int i=0; i<m_SkeletonPose.m_arrCGAJoints.size(); i++)
			{
				CCGAJoint* pJoint = &m_SkeletonPose.m_arrCGAJoints[i];
				if (pJoint->m_CGAObjectInstance)
					fExtent += geo.GetPart(i).GetExtent(pJoint->m_CGAObjectInstance.get(), eForm);
			}
			return fExtent;
		}
		else
		{
			CModelMesh* pMesh = m_pModel->GetModelMesh(m_pModel->m_nBaseLOD);
			if (pMesh)
				return pMesh->ComputeExtent(geo, eForm);
		}
	}

	// Use BB in fallback case.
	return BoxExtent(eForm, GetAABB().GetSize()*0.5f);
}

void CCharInstance::GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm)
{
	if (m_pModel)
	{
		if (m_pModel->m_ObjectType==CGA)
		{
			geo.GetExtent(this, eForm);
			int i = geo.GetRandomPart();
			CCGAJoint* pJoint = &m_SkeletonPose.m_arrCGAJoints[i];

			if (pJoint->m_CGAObjectInstance)
			{
				pJoint->m_CGAObjectInstance->GetRandomPos(ran, geo.GetPart(i), eForm);
				ran.vPos = m_SkeletonPose.m_AbsolutePose[i] * ran.vPos; 
				ran.vNorm = m_SkeletonPose.m_AbsolutePose[i] * ran.vNorm;
				return;
			}
		}
		else
		{
			if (m_pAlternativeModelForGetRandomPos)
			{
				CModelMesh* pMesh = m_pAlternativeModelForGetRandomPos->GetModelMesh(m_pAlternativeModelForGetRandomPos->m_nBaseLOD);
				if (pMesh)
				{
					pMesh->GetRandomPos(this, ran, geo, eForm);
					return;
				}
			}
			else
			{
				CModelMesh* pMesh = m_pModel->GetModelMesh(m_pModel->m_nBaseLOD);
				if (pMesh)
				{
					pMesh->GetRandomPos(this, ran, geo, eForm);
					return;
				}
			}
		}
	}

	// Use BB in fallback case.
	AABB bb = GetAABB();
	BoxRandomPos(ran, eForm, bb.GetSize()*0.5f);
	ran.vPos += bb.GetCenter();
}

void CCharInstance::SetAlternativeTargetForGetRandomPos(ICharacterInstance *target)
{
	m_pAlternativeModelForGetRandomPos = ((CSkinInstance *)target)->m_pModel;
}

//-----------------------------------------------------------------------------
//-----  Spawn decal on the character                                     -----
//-----------------------------------------------------------------------------
void CCharInstance::CreateDecal(CryEngineDecalInfo& DecalLCS)
{
	if (m_pModel->m_ObjectType==CGA)
		return;
	if (Console::GetInst().ca_UseDecals==0)
		return;
	if ( fabsf(DecalLCS.fSize) < 0.001f)
		return;

	//-----------------------------------------------------------------------------------

	m_DecalManager.CreateDecalsOnInstance(&m_arrNewSkinQuat[m_iActiveFrame][0],m_pModel,&m_Morphing,m_arrShapeDeformValues, DecalLCS, m_PhysEntityLocation );

	uint32 numAttachments = m_AttachmentManager.m_arrAttachments.size();
	for (uint32 i=0; i<numAttachments; i++)
	{
		CAttachment* pAttachment=m_AttachmentManager.m_arrAttachments[i];
		uint32 type=pAttachment->m_Type;
		if (type != CA_SKIN) 
			continue;

		IAttachmentObject* pIAttachmentObject = pAttachment->GetIAttachmentObject();	
		if (pIAttachmentObject) 
		{
			CSkinInstance* pSkinInstance = (CSkinInstance*)pIAttachmentObject->GetICharacterInstance();
			if (pSkinInstance) 
			{
				QuatTS arrNewSkinQuat[MAX_JOINT_AMOUNT+1];	//bone quaternions for skinning (entire skeleton)
				uint32 iActiveFrame		=	m_iActiveFrame;
				uint32 numRemapJoints	= pAttachment->m_arrRemapTable.size();
				for (uint32 i=0; i<numRemapJoints; i++)	
					arrNewSkinQuat[i]=m_arrNewSkinQuat[iActiveFrame][pAttachment->m_arrRemapTable[i]];
				pSkinInstance->m_DecalManager.CreateDecalsOnInstance(arrNewSkinQuat,pSkinInstance->m_pModel,&pSkinInstance->m_Morphing,m_arrShapeDeformValues, DecalLCS, m_PhysEntityLocation);
			}
		}
	}
	
	


}


void CCharInstance::OnDetach()
{
	m_pSkinAttachment=0;
	pe_params_rope pr;
	pr.pEntTiedTo[0]=pr.pEntTiedTo[1] = 0;
	m_SkeletonPose.SetAuxParams(&pr);
}


//////////////////////////////////////////////////////////////////////////
void CCharInstance::Serialize(TSerialize ser)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("CSkinInstance");
		ser.Value("fAnimSpeedScale", (float&)(m_fAnimSpeedScale));
		ser.EndGroup();

		m_SkeletonAnim.Serialize(ser);
		m_AttachmentManager.Serialize(ser);
	}
}



//////////////////////////////////////////////////////////////////////////
size_t CCharInstance::SizeOfThis (ICrySizer * pSizer)
{

	size_t SizeOfInstance	= 0;


	//--------------------------------------------------------------------
	//---        this is the size of the SkinInstance class           ----
	//--------------------------------------------------------------------

	{
		SIZER_COMPONENT_NAME(pSizer, "CAttachmentManager");
		size_t size = sizeof(CAttachmentManager) + m_AttachmentManager.SizeOfThis();
		pSizer->AddObject(&m_AttachmentManager, size);
		SizeOfInstance += size;
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "DecalManager");
		size_t size		= sizeof(CAnimDecalManager) + m_DecalManager.SizeOfThis();
		pSizer->AddObject(&m_DecalManager, size);
		SizeOfInstance += size;
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "Morphing");
		size_t size	= sizeof(CMorphing);
		size +=	sizeof(Vec3)*m_Morphing.m_morphTargetsState.capacity();
		size += sizeof(CryModEffMorph)*m_Morphing.m_arrMorphEffectors.capacity(); 
		pSizer->AddObject(&m_Morphing, size);
		SizeOfInstance += size;
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "FacialInstance");
		size_t size		= 0;
		if (m_pFacialInstance)
			size = m_pFacialInstance->SizeOfThis();
		pSizer->AddObject(m_pFacialInstance, size);
		SizeOfInstance += size;
	}


	//--------------------------------------------------------------------
	//---        this is the size of the CharInstance class           ----
	//--------------------------------------------------------------------

	{
		SIZER_COMPONENT_NAME(pSizer, "SkeletonAnim");
		size_t size	= sizeof(CSkeletonAnim)+m_SkeletonAnim.SizeOfThis(pSizer);
		pSizer->AddObject(&m_SkeletonAnim, size);
		SizeOfInstance += size;
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "SkeletonPose");
		size_t size	= sizeof(CSkeletonPose)+m_SkeletonPose.SizeOfThis(pSizer);
		pSizer->AddObject(&m_SkeletonPose, size);
		SizeOfInstance += size;
	}

	size_t SkinningQuats(0);
	{
		SIZER_COMPONENT_NAME(pSizer, "SkinningQuats");

		size_t numJoints1	= m_arrMBSkinQuat.capacity();
		size_t size1		= sizeof(QuatTS)*numJoints1; 

		size_t numJoints2	= m_arrNewSkinQuat[0].capacity();
		size_t size2		= sizeof(QuatTS)*numJoints2*MAX_MOTIONBLUR_FRAMES; 
		pSizer->AddObject(&m_arrMBSkinQuat, size2 + size1);

		SkinningQuats += size1 + size2;

		SizeOfInstance += (size1+size2);
	}






	//--------------------------------------------------------------------
	//---          evaluate the real size of CCharInstance            ----
	//--------------------------------------------------------------------

	{
		SIZER_COMPONENT_NAME(pSizer, "CCharInstance");
		size_t size		=	sizeof(CCharInstance) - sizeof(CAttachmentManager)-sizeof(CAnimDecalManager)-sizeof(CMorphing)-sizeof(CSkeletonAnim)-sizeof(CSkeletonPose);
		uint32 stringlength = m_strFilePath.capacity();
		size += stringlength;
		pSizer->AddObject(&m_fDeltaTime, size /*+ SkinningQuats*/);
		SizeOfInstance += size;
	}

	return SizeOfInstance;
};

