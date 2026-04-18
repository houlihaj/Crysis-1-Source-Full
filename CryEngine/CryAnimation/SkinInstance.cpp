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
#include "CharacterInstance.h"
#include "CharacterManager.h"
#include "CryCharAnimationParams.h"
#include "CryCharMorphParams.h"
#include "IRenderAuxGeom.h"
#include "FacialAnimation/FacialInstance.h"
#include "GeomQuery.h"


// this is the count of model states created so far
uint32 g_nCharInstanceLoaded = 0;
uint32 g_nSkinInstanceLoaded = 0;
uint32 g_nCharInstanceDeleted = 0;
uint32 g_nSkinInstanceDeleted = 0;

CSkinInstance::CSkinInstance ( const string &strFileName, CCharacterModel* pModel, uint32 IsSkinAtt, CAttachment* pMasterAttachment )
{
	assert(IsSkinAtt==0xDeadBeef || IsSkinAtt==0xaaaabbbb);

	if(IsSkinAtt==0xDeadBeef)
		g_nSkinInstanceLoaded++;

	m_useDecals=0;

	m_AttachmentManager.m_pSkinInstance=0;
	m_AttachmentManager.m_pSkelInstance=0;
	m_pSkinAttachment=0; //we set this at run-time

	m_strFilePath=strFileName;
	pModel->AddRef();
	m_pModel=pModel;
	pModel->RegisterInstance (this);
	// Copy render mesh pointers from model to instance.
	uint32 nLOD = (uint32)pModel->m_arrModelMeshes.size();
	for(uint32 nLod=0; nLod<nLOD; nLod++)
		m_pRenderMeshs[nLod] = pModel->m_pRenderMeshs[nLod];

	m_nRefCounter = 0;
	m_nStillNeedsMorph = 0;
	m_bHaveEntityAttachments = false;


	m_nLodLevel=0;
	m_nLastRenderedFrameID=0xffaaffaa;
	m_RenderPass=0x55aa55aa;
	m_LastRenderedFrameID=0;
	m_nInstanceUpdateCounter=0;
	m_nMorphTargetLod = -1;
	m_UpdatedMorphTargetVBThisFrame = false;
	m_uInstanceFlags= (FLAG_DEFAULT_FLAGS);
	m_UpdateAttachmentMesh=0;

	m_bFacialAnimationEnabled = true;
	m_bForceInPlaceAnimsOnMovingTrain = false;

	m_Morphing.InitMorphing(pModel);

	m_pFacialInstance = 0;
	CFacialModel* pFacialModel = m_pModel->GetFacialModel();
	if (pFacialModel)
	{
		if (IsSkinAtt==0xDeadBeef)
		{
			CCharInstance* pIMasterInstance	=	pMasterAttachment->m_pAttachmentManager->m_pSkelInstance;
			m_pFacialInstance = new CFacialInstance(m_pModel->GetFacialModel(),this,pIMasterInstance);
			m_pFacialInstance->AddRef();
		}
		else
		{
			m_pFacialInstance = new CFacialInstance(m_pModel->GetFacialModel(),this,(CCharInstance*)this);
			m_pFacialInstance->AddRef();
		}
	}
}









//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
void CSkinInstance::UpdateAttachedObjects(const QuatT& rPhysLocationNext, IAttachmentObject::EType *pOnlyThisType, float fZoomAdjustedDistanceFromCamera, uint32 OnRender  )
{
	DEFINE_PROFILER_FUNCTION();

#ifdef VTUNE_PROFILE 
	g_pISystem->VTuneResume();
#endif

	//for this we need the absolute joints 
	m_AttachmentManager.UpdateLocationAttachments( rPhysLocationNext,pOnlyThisType );

	uint32 numAttachmnets = m_AttachmentManager.m_arrAttachments.size();
	for (uint32 i=0; i<numAttachmnets; i++) 
	{
		CAttachment* pCAttachment = m_AttachmentManager.m_arrAttachments[i];
		IAttachmentObject* pIAttachmentObject = pCAttachment->m_pIAttachmentObject;
		if (pIAttachmentObject) 
		{
			if (pOnlyThisType && *pOnlyThisType != pIAttachmentObject->GetAttachmentType())
				continue;

			uint32 type = pCAttachment->GetType();
			if (type==CA_SKIN) 
				pIAttachmentObject->UpdateAttachment( pCAttachment, rPhysLocationNext, fZoomAdjustedDistanceFromCamera, OnRender);
			else 
				pIAttachmentObject->UpdateAttachment( pCAttachment, pCAttachment->GetAttWorldAbsolute(), fZoomAdjustedDistanceFromCamera, OnRender);

			if (pIAttachmentObject->GetAttachmentType() == IAttachmentObject::eAttachment_Entity)
				m_bHaveEntityAttachments = true;
		}
	}
#ifdef VTUNE_PROFILE 
	g_pISystem->VTunePause();
#endif

}

//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
void CSkinInstance::UpdateAttachedObjectsFast(const QuatT& rPhysLocationNext, float fZoomAdjustedDistanceFromCamera, uint32 OnRender )
{
	DEFINE_PROFILER_FUNCTION();

	if (m_bHaveEntityAttachments==0)
		return;

	//const char* mname = GetFilePath();
	//float fC1[4] = {1,0,0,1};
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.6f, fC1, false,"FastAttachmentUpdate  Model: %s ",mname );	
	//g_YLine+=16.0f;
	//g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.6f, fC1, false,"pos: %f %f %f",rPhysLocationNext.t.x,rPhysLocationNext.t.y,rPhysLocationNext.t.z );	
	//g_YLine+=16.0f;

	m_AttachmentManager.UpdateLocationAttachmentsFast(rPhysLocationNext); 

	uint32 numAttachmnets = m_AttachmentManager.m_arrAttachments.size();
	for (uint32 i=0; i<numAttachmnets; i++) 
	{
		CAttachment* pCAttachment = m_AttachmentManager.m_arrAttachments[i];
		IAttachmentObject* pIAttachmentObject = pCAttachment->m_pIAttachmentObject;
		if (pIAttachmentObject) 
		{
			uint32 type = pCAttachment->GetType();
			if (type!=CA_SKIN) 
				pIAttachmentObject->UpdateAttachment( pCAttachment, pCAttachment->GetAttWorldAbsolute(), fZoomAdjustedDistanceFromCamera, OnRender);
		}
	}

}


















// Returns true if this character was created from the file the path refers to.
// If this is true, then there's no need to reload the character if you need to change its model to this one.
bool CSkinInstance::IsModelFileEqual (const char* szFileName)
{
	string strPath = szFileName;
	CryStringUtils::UnifyFilePath(strPath);
	return !stricmp(strPath.c_str(), m_pModel->GetModelFilePath());
}





void CSkinInstance::GetMemoryUsage(ICrySizer* pSizer) const
{
#if ENABLE_GET_MEMORY_USAGE
	SIZER_SUBCOMPONENT_NAME(pSizer, "Characters");
	pSizer->Add(*this);
	pSizer->AddContainer(m_AttachmentManager.m_arrAttachments);
#endif

	uint32 nLOD = m_pModel->m_arrModelMeshes.size();
	for(uint32 dwLod=0; dwLod<nLOD; dwLod++)
	{
		if(!m_pRenderMeshs[dwLod])
			continue;

		if(!m_pRenderMeshs[dwLod]->GetMaterial())
			continue;

		PodArray<CRenderChunk>* pMats = m_pRenderMeshs[dwLod]->GetChunks();

		for (uint i=0; i<(uint)pMats->Count(); i++)
		{
			CRenderChunk * pChunk = pMats->Get(i);

			if(!pChunk)
				continue;

			SShaderItem shaderItem = m_pRenderMeshs[dwLod]->GetMaterial()->GetShaderItem(pChunk->m_nMatID);
			if (!shaderItem.m_pShaderResources)
				continue;

			IRenderShaderResources *pRes = shaderItem.m_pShaderResources;

			for (int i = 0; i < EFTT_MAX; i++)
			{
				if (!pRes->GetTexture(i))
					continue;

				ITexture *pTexture = pRes->GetTexture(i)->m_Sampler.m_pITex;

				uint32 dwSize=0xffffffff;

				if(pTexture)
				{
					dwSize = pTexture->GetDeviceDataSize();

					if (i==EFTT_NORMALMAP && pRes->GetTexture(EFTT_BUMP))
						dwSize=0;			// if there is a _bump texture, we don't make use of the ddn any more, so not space is wasted for this but we still need to report the file

					pSizer->GetResourceCollector().AddResource(pTexture->GetName(),dwSize);		// used texture
				}
			}
		}
	}
}








void CSkinInstance::DeleteDecals()
{
	m_DecalManager.clear();
}









//////////////////////////////////////////////////////////////////////////
IFacialInstance* CSkinInstance::GetFacialInstance()
{
	if (m_pFacialInstance)
		return m_pFacialInstance;

	// Check all skin attachments if they have facial animation.
	uint32 numAttachmnets = m_AttachmentManager.m_arrAttachments.size();
	for (uint32 i=0; i<numAttachmnets; i++) 
	{
		CAttachment* pAttachment = m_AttachmentManager.m_arrAttachments[i];
		IAttachmentObject* pIAttachmentObject = pAttachment->m_pIAttachmentObject;
		if (pIAttachmentObject) 
		{
			ICharacterInstance *pAttachedCharacter = pIAttachmentObject->GetICharacterInstance();
			if (pAttachedCharacter)
			{
				IFacialInstance *pFacialInstance = pAttachedCharacter->GetFacialInstance();
				if (pFacialInstance)
					return pFacialInstance;
			}
		}
	}
	return NULL;
};

//////////////////////////////////////////////////////////////////////////
void CSkinInstance::LipSyncWithSound( uint32 nSoundId, bool bStop )
{
	IFacialInstance *pFacialInstance = GetFacialInstance();
	if (pFacialInstance)
	{
		pFacialInstance->LipSyncWithSound( nSoundId, bStop );
	}
}


//////////////////////////////////////////////////////////////////////////
void CSkinInstance::EnableFacialAnimation( bool bEnable )
{
	m_bFacialAnimationEnabled = bEnable;

	for (int attachmentIndex = 0, end = int(m_AttachmentManager.m_arrAttachments.size()); attachmentIndex < end; ++attachmentIndex)
	{
		CAttachment* pAttachment = m_AttachmentManager.m_arrAttachments[attachmentIndex];
		IAttachmentObject* pAttachmentObject = (pAttachment ? pAttachment->GetIAttachmentObject() : 0);
		ICharacterInstance* pAttachmentCharacter = (pAttachmentObject ? pAttachmentObject->GetICharacterInstance() : 0);
		if (pAttachmentCharacter)
			pAttachmentCharacter->EnableFacialAnimation(bEnable);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSkinInstance::EnableProceduralFacialAnimation( bool bEnable )
{
	IFacialInstance *pInst = GetFacialInstance();
	if (pInst)
		pInst->EnableProceduralFacialAnimation( bEnable );
}




void CSkinInstance::ReleaseTemporaryResources()
{
	if (m_Morphing.m_morphTargetsState.capacity())
	{
		std::vector<Vec3> dummy;
		m_Morphing.m_morphTargetsState.swap(dummy);
	}

	int end = int(m_AttachmentManager.m_arrAttachments.size());
	for (int attachmentIndex = 0; attachmentIndex < end; ++attachmentIndex)
	{
		CAttachment* pAttachment = m_AttachmentManager.m_arrAttachments[attachmentIndex];
		IAttachmentObject* pAttachmentObject = (pAttachment ? pAttachment->GetIAttachmentObject() : 0);
		CSkinInstance* pAttachedCharacter = (CSkinInstance*)(pAttachmentObject ? pAttachmentObject->GetICharacterInstance() : 0);
		if (pAttachedCharacter)
			pAttachedCharacter->ReleaseTemporaryResources();
	}
}



void CSkinInstance::Release()
{
	if (--m_nRefCounter <= 0)
	{
		const char* pFilePath = GetFilePath();
		g_pCharacterManager->ReleaseCDF(pFilePath);
		delete this;
	}
}

//////////////////////////////////////////////////////////////////////////
CSkinInstance::~CSkinInstance()
{ 
	SAFE_RELEASE(m_pFacialInstance);
	//////////////////////////////////////////////////////////////////////////
	// Check if character body locked for the next level.
	//////////////////////////////////////////////////////////////////////////
	if (m_pModel != 0 && IsResourceLocked(m_pModel->GetModelFilePath()))
	{
		g_pCharacterManager->LockModel( m_pModel );
	}

	assert(m_nRefCounter==0);

	if(m_pSkinAttachment)
	{
		g_nSkinInstanceLoaded--;
		g_nSkinInstanceDeleted++;
	}
	// the submeshes that are not the default model submeshes lock their corresponding
	// CCharacterModels. The default model can't lock its parent CCharacterModel, because it will
	// be circular dependency. Owner ship is as follows: States->CCharacterModel->Default State
	m_pModel->Release();
	m_AttachmentManager.RemoveAllAttachments();
	m_pModel->UnregisterInstance(this);  
}





//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------

void CSkinInstance::ProcessSkinAttachment(const QuatT &rPhysLocationNext,const QuatTS &rAnimLocationNext, IAttachment* pIAttachment, float fZoomAdjustedDistanceFromCamera, uint32 OnRender )
{

	//this is a skin attachment
	assert(m_pSkinAttachment==0);
	assert(pIAttachment); 
	assert(pIAttachment->GetType()==CA_SKIN); 

	const char* name = m_pModel->GetModelFilePath();
	if (pIAttachment==0) 
		CryError("expecting pointer to skin-attachment for model: '%s'", name );

	m_pSkinAttachment  =	(CAttachment*)pIAttachment;

	//Get the instance of the master-character
	CCharInstance* pMaster	=	m_pSkinAttachment->m_pAttachmentManager->m_pSkelInstance;
	f32 fDeltaTime = pMaster->m_fDeltaTime;
	if (m_bFacialAnimationEnabled)
		m_Morphing.UpdateMorphEffectors(m_pFacialInstance, fDeltaTime, rAnimLocationNext, fZoomAdjustedDistanceFromCamera);
	else
		m_Morphing.UpdateMorphEffectors(0,fDeltaTime, rAnimLocationNext, fZoomAdjustedDistanceFromCamera);

	uint32 numAttachmnets=m_AttachmentManager.GetAttachmentCount();
	if (numAttachmnets)
		UpdateAttachedObjects(rPhysLocationNext,0,fZoomAdjustedDistanceFromCamera,OnRender);

}



AABB CSkinInstance::GetAABB() 
{ 
	if (m_pSkinAttachment==0)
		return AABB(ZERO,ZERO);

	CCharInstance* pMaster	=	m_pSkinAttachment->m_pAttachmentManager->m_pSkelInstance;
	return pMaster->m_SkeletonPose.GetAABB();	
}








//////////////////////////////////////////////////////////////////////////
size_t CSkinInstance::SizeOfThis (ICrySizer * pSizer)
{

	size_t SizeOfInstance	= 0;
	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CAttachmentManager");
		size_t size = sizeof(CAttachmentManager)+m_AttachmentManager.SizeOfThis();
		pSizer->AddObject(&m_AttachmentManager, size);
		SizeOfInstance += size;
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "DecalManager");
		size_t size		= sizeof(CAnimDecalManager)+m_DecalManager.SizeOfThis();
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
		SIZER_SUBCOMPONENT_NAME(pSizer, "FacialInstance");
		size_t size		= 0;
		if (m_pFacialInstance)
			size = m_pFacialInstance->SizeOfThis();
		pSizer->AddObject(m_pFacialInstance, size);
		SizeOfInstance += size;
	}




	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CSkinInstance");
		size_t size		=	sizeof(CSkinInstance)-sizeof(CAttachmentManager)-sizeof(CAnimDecalManager)-sizeof(CMorphing);
		uint32 stringlength = m_strFilePath.capacity();
		size += stringlength;
		pSizer->AddObject(&m_uInstanceFlags, size);
		SizeOfInstance += size;
	}


	return SizeOfInstance;
};




