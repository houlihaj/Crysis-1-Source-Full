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
#include "CharacterInstanceFull.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CharacterInstanceFull::CharacterInstanceFull ( const string &strFileName, CCharacterModel* pModel)
: CSkinInstance( strFileName, pModel, 0xaaaabbbb,0)
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

	
	InitInstance(pModel );


}

void CharacterInstanceFull::InitInstance(CCharacterModel* pModel )
{
	LOADING_TIME_PROFILE_SECTION(g_pISystem);

	m_pCamera=0;
	g_nCharInstanceLoaded++;
	m_nInstanceNumber = g_nCharInstanceLoaded;
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
}

void CharacterInstanceFull::Release()
{
	if (--m_nRefCounter <= 0)
	{
		const char* pFilePath = GetFilePath();
		g_pCharacterManager->ReleaseCDF(pFilePath);
		delete this;
	}
}

//////////////////////////////////////////////////////////////////////////
CharacterInstanceFull::~CharacterInstanceFull()
{
	m_SkeletonPose.DestroyPhysics();
	g_nCharInstanceLoaded--;
	g_nCharInstanceDeleted++;
}




// calculates the mask ANDed with the frame id that's used to determine whether to skin the character on this frame or not.
uint32 CharacterInstanceFull::GetUpdateFrequencyMask(const Vec3& WPos, const CCamera& rCamera)
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
void CharacterInstanceFull::SkeletonPreProcess(const QuatT &rPhysLocationCurr, const QuatTS &rAnimLocationCurr, const CCamera& rCamera, uint32 OnRender )
{
	DEFINE_PROFILER_FUNCTION();

#ifdef _DEBUG
	SCOPED_ENABLE_FLOAT_EXCEPTIONS;
#endif

	//_controlfp(0, _EM_INVALID|_EM_ZERODIVIDE | _PC_64 );

	QuatTS AnimCharLocation=QuatTS(IDENTITY);
	m_SkeletonPose.m_FootAnchor.m_vFootSlide = Vec3(ZERO);

	m_pMasterAttachment =	0;
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
			{
				pSkinInstance->m_pMasterAttachment=0; 

				if (pAttachment->m_Type!=CA_SKIN)
				{
					uint32 sdsdsd=0;
				}
			}
		}
	}


	m_pCamera=	&rCamera;

	pe_params_flags pf;
	IPhysicalEntity *pCharPhys = m_SkeletonPose.GetCharacterPhysics();
	if (m_pModel->m_ObjectType==CHR && !(pCharPhys && pCharPhys->GetType()==PE_ARTICULATED && pCharPhys->GetParams(&pf) && pf.flags & aef_recorded_physics))
	{
		m_PhysEntityLocation	=	rPhysLocationCurr;
		AnimCharLocation		=	rAnimLocationCurr;
	}
	else
	{
		m_PhysEntityLocation	=	QuatT(rAnimLocationCurr);
		AnimCharLocation	=	rAnimLocationCurr;
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
	assert(m_fDeltaTime<2.0f);

	//---------------------------------------------------------------------------------

	m_SkeletonPose.m_bFullSkeletonUpdate=0;
	int nCurrentFrameID = g_pCharacterManager->m_nUpdateCounter; //g_pIRenderer->GetFrameID(false);
	uint32 dif = nCurrentFrameID-m_LastRenderedFrameID;
	m_SkeletonPose.m_bInstanceVisible = (dif<5);

	// depending on how far the character from the player is, the bones may be updated once per several frames
	int nUFM = GetUpdateFrequencyMask(AnimCharLocation.t,rCamera);

	m_nInstanceUpdateCounter++;
	uint32 FrameMask		= m_nInstanceUpdateCounter&nUFM;
	uint32 InstanceMask = m_nInstanceNumber&nUFM;
	bool update = m_SkeletonPose.m_timeStandingUp<0 ? FrameMask==InstanceMask : true;

	if (m_SkeletonAnim.m_TrackViewExclusive)
		m_SkeletonPose.m_bFullSkeletonUpdate=1;
	if (update==0)
		m_SkeletonPose.m_bFullSkeletonUpdate=0;
	if (m_SkeletonPose.m_nForceSkeletonUpdate)
		m_SkeletonPose.m_bFullSkeletonUpdate=1;
	if (m_SkeletonPose.m_bInstanceVisible)
		m_SkeletonPose.m_bFullSkeletonUpdate=1;
	if (Console::GetInst().ca_JustRootUpdate == 1)
		m_SkeletonPose.m_bFullSkeletonUpdate=0;
	if (Console::GetInst().ca_JustRootUpdate == 2)
		m_SkeletonPose.m_bFullSkeletonUpdate=1;

	m_SkeletonAnim.m_IsAnimPlaying=0;
	m_SkeletonAnim.ProcessAnimations( m_PhysEntityLocation, AnimCharLocation, OnRender );
}


//////////////////////////////////////////////////////////////////////////
void CharacterInstanceFull::UpdatePhysicsCGA( float fScale, const QuatTS& rAnimLocationNext )
{
	DEFINE_PROFILER_FUNCTION();

#ifdef VTUNE_PROFILE 
	g_pISystem->VTuneResume();
#endif

	if (!m_SkeletonPose.GetCharacterPhysics())
		return;

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

		for(i=0; i<numNodes; i++)
			m_SkeletonPose.m_qAdditionalRotPos[i] = m_SkeletonPose.m_RelativePose[i];

		for(iLayer=0; iLayer<numVIRTUALLAYERS; iLayer++) if (m_SkeletonAnim.GetNumAnimsInFIFO(iLayer))
		{
			CAnimation &anim = m_SkeletonAnim.GetAnimFromFIFO(iLayer,0);
			int idGlobal = anim.m_LMG0.m_nGlobalID[0];
			int idAnim = anim.m_LMG0.m_nAnimID[0];
			GlobalAnimationHeader& animHdr = g_AnimationManager.m_arrGlobalAnimations[idGlobal];
			t = anim.m_fAnimTime + m_fOriginalDeltaTime/(animHdr.m_fEndSec - animHdr.m_fStartSec);

			for(i=0; i<numNodes; i++)	if (pController = animHdr.GetControllerByJointID(m_pModel->m_pModelSkeleton->m_arrModelJoints[i].m_nJointCRC32))//m_pModel->m_pModelSkeleton->m_arrModelJoints[i].m_arrControllersMJoint[idAnim])
				pController->GetOP( idGlobal, t, m_SkeletonPose.m_qAdditionalRotPos[i].q, m_SkeletonPose.m_qAdditionalRotPos[i].t);
		}

		for(i=0; i<numNodes; i++) if ((iParent=m_SkeletonPose.m_parrModelJoints[i].m_idxParent)>=0 && iParent!=i)
			m_SkeletonPose.m_qAdditionalRotPos[i] =	m_SkeletonPose.m_qAdditionalRotPos[iParent]*m_SkeletonPose.m_qAdditionalRotPos[i];

		t = 1.0f/m_fOriginalDeltaTime;
		for(i=0; i<numNodes; i++) if (m_SkeletonPose.m_parrModelJoints[i].m_NodeID!=~0)
		{
			asv.partid = m_SkeletonPose.m_parrModelJoints[i].m_NodeID;
			asv.v = (m_SkeletonPose.m_qAdditionalRotPos[i].t-m_SkeletonPose.m_AbsolutePose[i].t)*t;
			Quat dq = m_SkeletonPose.m_qAdditionalRotPos[i].q*!m_SkeletonPose.m_AbsolutePose[i].q;
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




float CharacterInstanceFull::ComputeExtent(GeomQuery& geo, EGeomForm eForm)
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
					fExtent += geo.GetPart(i).GetExtent(pJoint->m_CGAObjectInstance, eForm);
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

void CharacterInstanceFull::GetRandomPos(RandomPos& ran, GeomQuery& geo, EGeomForm eForm)
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
			CModelMesh* pMesh = m_pModel->GetModelMesh(m_pModel->m_nBaseLOD);
			if (pMesh)
			{
				pMesh->GetRandomPos(this, ran, geo, eForm);
				return;
			}
		}
	}

	// Use BB in fallback case.
	AABB bb = GetAABB();
	BoxRandomPos(ran, eForm, bb.GetSize()*0.5f);
	ran.vPos += bb.GetCenter();
}


// Spawn decal on the character
void CharacterInstanceFull::CreateDecal(CryEngineDecalInfo& DecalLCS)
{
	if (Console::GetInst().ca_EnableDecals && DecalLCS.fSize > 0.001f)
	{
		CreateDecalsOnInstance(DecalLCS, m_PhysEntityLocation );

		/*	uint32 numAttachments = m_AttachmentManager.m_arrAttachments.size();
		for (AttachArray::iterator it = m_AttachmentManager.m_arrAttachments.begin(); it != m_AttachmentManager.m_arrAttachments.end(); ++it)
		{
		CAttachment* pAttachment	= (*it);
		if (pAttachment->m_Type!=CA_SKIN) 
		continue;

		IAttachmentObject* pIAttachmentObject = pAttachment->GetIAttachmentObject();	
		if (pIAttachmentObject) 
		{
		CSkinInstance* pSkinCharInstance = (CSkinInstance*)pIAttachmentObject->GetICharacterInstance();
		if (pSkinCharInstance) 
		pSkinCharInstance->CreateDecalsOnInstance(DecalLCS, m_PhysEntityLocation);
		}
		}*/

	}
}


void CharacterInstanceFull::OnDetach()
{
	m_pMasterAttachment=0;
	pe_params_rope pr;
	pr.pEntTiedTo[0]=pr.pEntTiedTo[1] = 0;
	m_SkeletonPose.SetAuxParams(&pr);
}


//////////////////////////////////////////////////////////////////////////
void CharacterInstanceFull::Serialize(TSerialize ser)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		//if(ser.IsReading())
		//	InitInstance();
		ser.BeginGroup("CSkinInstance");
		ser.Value("fAnimSpeedScale", (float&)(m_fAnimSpeedScale));
		ser.EndGroup();

		m_SkeletonAnim.Serialize(ser);
		m_AttachmentManager.Serialize(ser);
	}
}


//////////////////////////////////////////////////////////////////////////
size_t CharacterInstanceFull::SizeOfThis (ICrySizer * pSizer)
{

	size_t SizeOfInstance	= 0;

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "SkeletonAnim");
		size_t size	= m_SkeletonAnim.SizeOfThis(pSizer);
		pSizer->AddObject(&m_SkeletonAnim, size);
		SizeOfInstance += size;
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "SkeletonPose");
		size_t size	= m_SkeletonPose.SizeOfThis(pSizer);
		pSizer->AddObject(&m_SkeletonAnim, size);
		SizeOfInstance += size;
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "SkinningQuats");

		size_t numJoints1	= m_arrMBSkinQuat.capacity();
		size_t size1		= sizeof(QuatTS)*numJoints1; 
	//	pSizer->AddObject(&m_arrMBSkinQuat[0], size1);

		size_t numJoints2	= m_arrNewSkinQuat[0].capacity();
		size_t size2		= sizeof(QuatTS)*numJoints2*MAX_MOTIONBLUR_FRAMES; 
		pSizer->AddObject(&m_arrMBSkinQuat[0], size2);

		SizeOfInstance += (size1+size2);
	}



	//--------------------------------------------------------------------

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CAttachmentManager");
		size_t size = m_AttachmentManager.SizeOfThis();
		pSizer->AddObject(this, size);
		SizeOfInstance += size;
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "MTStateSize");
		size_t size	=	sizeof(Vec3)*m_Morphing.m_morphTargetsState.capacity();
		pSizer->AddObject(&m_Morphing.m_morphTargetsState[0], size);
		SizeOfInstance += size;
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "MESize");
		size_t size	= sizeof(CMorphing)+sizeof(CryModEffMorph)*m_Morphing.m_arrMorphEffectors.capacity(); 
		pSizer->AddObject(&m_Morphing.m_arrMorphEffectors[0], size);
		SizeOfInstance += size;
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "DecalMemAlloc");
		size_t size		= m_DecalManager.SizeOfThis();
		pSizer->AddObject(&m_DecalManager, size);

		size	+= m_arrFirstHit.size()*sizeof(Vec3);
		pSizer->AddObject(&m_arrFirstHit[0], size);

		SizeOfInstance += size;
	}



	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "FacialInstance");
		size_t size		= 0;
		if (m_pFacialInstance)
			size = m_pFacialInstance->SizeOfThis();
		pSizer->AddObject(m_pFacialInstance, size);
		SizeOfInstance += size;
	}


	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "SkinningQuats");

		size_t numJoints1	= m_arrMBSkinQuat.capacity();
		size_t size1		= sizeof(QuatTS)*numJoints1; 
		pSizer->AddObject(&m_arrMBSkinQuat[0], size1);

		size_t numJoints2	= m_arrNewSkinQuat[0].capacity();
		size_t size2		= sizeof(QuatTS)*numJoints2*MAX_MOTIONBLUR_FRAMES; 
		pSizer->AddObject(&m_arrMBSkinQuat[0], size2);

		SizeOfInstance += (size1+size2);
	}



	{
		size_t size		=	sizeof(CSkinInstance)-sizeof(CAttachmentManager)-sizeof(CAnimDecalManager)-sizeof(CMorphing);
		size		-=	(sizeof(CSkeletonPose)+sizeof(CSkeletonPose));
		uint32 stringlength = m_strFilePath.capacity();
		size += stringlength;
		SIZER_SUBCOMPONENT_NAME(pSizer, "CSkinInstance");
		pSizer->AddObject(&m_AttachmentManager, size);
		SizeOfInstance += size;
	}

	return SizeOfInstance;
};

