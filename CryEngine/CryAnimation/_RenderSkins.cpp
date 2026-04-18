////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	12/01/2005 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  start rendering 
/////////////////////////////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include <CryHeaders.h>
#include <CryAnimationScriptCommands.h>
#include <Cry_Camera.h>
#include "ModelMesh.h"
#include <I3DEngine.h>
#include <IShader.h>
#include <CryArray.h>
#include "CharacterInstance.h"
#include "CharacterManager.h"
#include "CryCharAnimationParams.h"
#include "CryCharMorphParams.h"
#include "IRenderAuxGeom.h"






//! Render object ( register render elements into renderer )
void CSkinInstance::Render(const struct SRendParams& RendParams, const QuatTS& Offset, Matrix34 *pFinalPhysLocation)
{
	if (m_pSkinAttachment==0)
		return;

	if (m_pSkinAttachment->m_Type!=CA_SKIN)
	{
		const char* name = m_pModel->GetModelFilePath();
		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,name,	"CryAnimation: this SkinInstance is no skin attachment");
		return;
	}

	CCharInstance* pMaster	=	m_pSkinAttachment->m_pAttachmentManager->m_pSkelInstance;

	if (!(pMaster->rp.m_nFlags & CS_FLAG_DRAW_MODEL))
		return;
	assert(RendParams.pMatrix);

	Matrix34 RenderMat34 = (*RendParams.pMatrix);
	Matrix34 PrevRenderMat34 = RenderMat34; 
	if (RendParams.pPrevMatrix)	PrevRenderMat34 = (*RendParams.pPrevMatrix);

	g_pAuxGeom->SetRenderFlags( e_Def3DPublicRenderflags );

	//	Set LOD
	float fLodRatioNorm, fRadiusRated;
	static ICVar *p_e_lod_ratio = g_pISystem->GetIConsole()->GetCVar("e_lod_ratio");
	if(p_e_lod_ratio && RendParams.pRenderNode && ((int)(void*)(RendParams.pRenderNode)>0))
	{
		fLodRatioNorm = RendParams.pRenderNode->GetLodRatioNormalized();
		fRadiusRated = RendParams.pRenderNode->GetBBox().GetRadius()*p_e_lod_ratio->GetIVal();
	}
	else
	{
		fLodRatioNorm = 1.f;
		fRadiusRated = pMaster->m_SkeletonPose.m_AABB.GetRadius()*p_e_lod_ratio->GetIVal();
	}

	fRadiusRated = max(0.01f, fRadiusRated);

	m_nLodLevel = (int)(RendParams.fDistance*(fLodRatioNorm*fLodRatioNorm)/(fRadiusRated));
	static ICVar *p_e_lod_min = g_pISystem->GetIConsole()->GetCVar("e_lod_min");
	if(p_e_lod_min)
		m_nLodLevel += p_e_lod_min->GetIVal();

	int32 numLODs = m_pModel->m_arrModelMeshes.size();
	if(m_nLodLevel<m_pModel->m_nBaseLOD)
		m_nLodLevel=m_pModel->m_nBaseLOD;
	if(m_nLodLevel>=numLODs)
		m_nLodLevel = numLODs-1;

	if (!(RendParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP))
		m_nMorphTargetLod = m_nLodLevel;

	assert(m_pSkinAttachment);

	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//	Render2 (RendParams, RenderMat34, rp );

	CRenderObject* pObj = g_pIRenderer->EF_GetObject(true);
	if (!pObj)
		return;

	pObj->m_nLod	=	m_nLodLevel;
	pObj->m_fSort	= RendParams.fCustomSortOffset;

	pObj->m_ObjFlags |= FOB_TRANS_MASK;

	if (RendParams.dwFObjFlags & FOB_CAMERA_SPACE)
	{
		pObj->m_ObjFlags |= FOB_CAMERA_SPACE;
	}

	//check if it should be drawn close to the player
	if ((RendParams.dwFObjFlags & FOB_NEAREST) || (pMaster->rp.m_nFlags & CS_FLAG_DRAW_NEAR) )	{ 	pObj->m_ObjFlags|=FOB_NEAREST;	}
	else pObj->m_ObjFlags&=~FOB_NEAREST;

	pObj->m_fAlpha = RendParams.fAlpha;
	pObj->m_fDistance =	RendParams.fDistance;

	pObj->m_II.m_AmbColor = RendParams.AmbientColor;

	pObj->m_ObjFlags |= RendParams.dwFObjFlags;
	SRenderObjData *pD = pObj->GetObjData(true);

	pObj->m_II.m_Matrix = RenderMat34;

  if( RendParams.pPrevMatrix && pFinalPhysLocation )
    (*pFinalPhysLocation) = RenderMat34;

  pObj->m_nMotionBlurAmount = 0;
  if( RendParams.pPrevMatrix && (Console::GetInst().m_pMotionBlur->GetIVal()> 1 || (RendParams.nMaterialLayersBlend & MTL_LAYER_BLEND_CLOAK)))
  {
    pObj->m_prevMatrix = pObj->m_II.m_Matrix;
    bool bCheckMotion = pMaster->MotionBlurMotionCheck( pObj->m_ObjFlags );
    if( bCheckMotion || !pObj->m_II.m_Matrix.IsEquivalent(PrevRenderMat34, 0.001f) )
    {
      pObj->m_prevMatrix = PrevRenderMat34;
      pObj->m_nMotionBlurAmount = 128;
    }
  }

	pObj->m_nMaterialLayers = RendParams.nMaterialLayersBlend;

  pObj->m_vBending = RendParams.vBending;

  if( RendParams.nVisionParams && !(RendParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP) && !(pObj->m_ObjFlags & FOB_VEGETATION))
    pObj->m_nVisionParams = RendParams.nVisionParams;

	if(RendParams.pTerrainTexInfo && (RendParams.dwFObjFlags & (FOB_BLEND_WITH_TERRAIN_COLOR | FOB_AMBIENT_OCCLUSION)))
	{
		pObj->m_nTextureID = RendParams.pTerrainTexInfo->nTex0;
		pObj->m_nTextureID1 = RendParams.pTerrainTexInfo->nTex1;
		pObj->m_fTempVars[0] = RendParams.pTerrainTexInfo->fTexOffsetX;
		pObj->m_fTempVars[1] = RendParams.pTerrainTexInfo->fTexOffsetY;
		pObj->m_fTempVars[2] = RendParams.pTerrainTexInfo->fTexScale;
		pObj->m_fTempVars[3] = RendParams.pTerrainTexInfo->fTerrainMinZ;
		pObj->m_fTempVars[4] = RendParams.pTerrainTexInfo->fTerrainMaxZ;
	}

  pObj->m_AlphaRef = RendParams.nAlphaRef;
	pObj->m_pShadowCasters = RendParams.pShadowMapCasters;

	assert(!pObj->m_CustomData);
	if (RendParams.pCCObjCustomData)
		pObj->m_CustomData = RendParams.pCCObjCustomData;

	/*if (!m_ShaderParams.empty())	
	{
	pObj->m_ShaderParams = &m_ShaderParams;
	}*/

	// override shader params if they exist
	/*if(RendParams.pShaderParams && !RendParams.pShaderParams->empty())
	{
	pObj->m_ShaderParams = RendParams.pShaderParams;
	}*/

	pObj->m_ObjFlags |= FOB_INSHADOW;

	pObj->m_pCharInstance = this;
	pObj->m_ObjFlags |= FOB_CHARACTER;

	pObj->m_DynLMMask = RendParams.nDLightMask;

	pD->m_pLMTCBufferO = NULL;
	pObj->m_nRAEId = 0;

	if (pObj->m_ObjFlags & FOB_NEAREST)
	{
		pObj->m_ObjFlags |= FOB_HIGHPRECISION;
		pObj->m_CustomData = NULL;    
	}

	//	AddCurrentRenderData (pObj, RendParams);
	IRenderMesh* pRenderMesh = m_pRenderMeshs[m_nLodLevel];
	if (pRenderMesh==0)
		AnimWarning("model '%s' has no render mesh", m_pModel->GetModelFilePath() );

	if (pRenderMesh)
	{       
		// MichaelS - use the instance's material if there is one, and if no override material has already been specified.
		IMaterial* pMaterial = RendParams.pMaterial;
		if (!pMaterial && this->CSkinInstance::GetMaterialOverride())
			pMaterial = this->CSkinInstance::GetMaterialOverride();

		CModelMesh* pModelMesh = m_pModel->GetModelMesh(m_nLodLevel);
		static ICVar *p_e_debug_draw = g_pISystem->GetIConsole()->GetCVar("e_debug_draw");
		if(p_e_debug_draw && p_e_debug_draw->GetIVal() > 0)
			pModelMesh->DrawDebugInfo(pMaster, m_nLodLevel, RenderMat34, p_e_debug_draw->GetIVal(), pMaterial, pObj, RendParams );

		//	float fColor[4] = {1,0,1,1};
		//	extern f32 g_YLine;
		//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"m_nLodLevel: %d  %d",RendParams.nLodLevel, pObj->m_nLod  ); g_YLine+=0x10;
		pRenderMesh->Render(RendParams,pObj,pMaterial);
		if (!(RendParams.dwFObjFlags & FOB_RENDER_INTO_SHADOWMAP))
			AddDecalsToRenderer(pMaster,m_pSkinAttachment,RenderMat34);
	}

	// draw weapon and binded objects
	m_AttachmentManager.DrawAttachments(RendParams,RenderMat34);
}




void CSkinInstance::DrawDecalsBBoxes( CCharInstance* pMaster2, CAttachment* pAttachment, const Matrix34& rRenderMat34)
{

	QuatTS arrNewSkinQuat[MAX_JOINT_AMOUNT+1];	//bone quaternions for skinning (entire skeleton)
	uint32 iActiveFrame		=	pMaster2->m_iActiveFrame;
	if(pAttachment)
	{
		uint32 numRemapJoints	= pAttachment->m_arrRemapTable.size();
		for (uint32 i=0; i<numRemapJoints; i++)	arrNewSkinQuat[i]=pMaster2->m_arrNewSkinQuat[iActiveFrame][pAttachment->m_arrRemapTable[i]];
	}
	else
	{
		uint32 numJoints	= pMaster2->m_SkeletonPose.m_AbsolutePose.size();
		cryMemcpy(arrNewSkinQuat, &pMaster2->m_arrNewSkinQuat[iActiveFrame][0],	sizeof(QuatTS)*numJoints);
	}

	OBB obb;
	QuatT WorldQuat		=	QuatT( rRenderMat34 );

	uint32 count = m_pModel->m_arrCollisions.size();
	for (uint32 i=1; i<count; ++i)
	{
		int32 id	=	m_pModel->m_arrCollisions[i].m_iBoneId;
		AABB aabb	=	m_pModel->m_arrCollisions[i].m_aABB;
		QuatTS wjoint=WorldQuat*arrNewSkinQuat[id];
		obb.SetOBBfromAABB(wjoint.q,aabb);
		g_pAuxGeom->DrawOBB(obb,wjoint.t,0,RGBA8(0x0f,0x3f,0x3f,0x0f),eBBD_Extremes_Color_Encoded);
	}

}






uint32 CSkinInstance::GetSkeletonPose( int nLOD, const Matrix34& rRenderMat34, QuatTS*& pBoneQuatsL, QuatTS*& pBoneQuatsS, QuatTS*& pMBBoneQuatsL, QuatTS*& pMBBoneQuatsS, Vec4 shapeDeformationData[], uint32 &HWSkinningFlags, uint8*& pRemapTable )
{

#ifdef DEFINE_PROFILER_FUNCTION
	DEFINE_PROFILER_FUNCTION();
#endif
//	static QuatTS g_arrNewSkinQuat[256];	//bone quaternions for skinning (entire skeleton)
//	static QuatTS g_arrMBSkinQuat[256];	//bone quaternions for skinning (entire skeleton)

	if (m_pSkinAttachment->m_Type!=CA_SKIN)
		CryError("CryAnimation: SkinInstance is no skin attachment");

	bool bUseMotionBlur = (HWSkinningFlags&eHWS_MotionBlured)>0;
	uint32 nCloseEnough = (HWSkinningFlags&eHWS_MorphTarget);
	HWSkinningFlags = eHWS_ShapeDeform;



	//This is a slave character! take bones of master character	
	//Get the instance of the master-character
	CCharInstance* pMaster	=	m_pSkinAttachment->m_pAttachmentManager->m_pSkelInstance;
	if (pMaster==0)
		CryError("CryAnimation: pMaster is zero");

	int nCurrentFrameID = g_pCharacterManager->m_nUpdateCounter; //g_pIRenderer->GetFrameID(false);
	if (m_nLastRenderedFrameID!=nCurrentFrameID) 
	{
		m_nLastRenderedFrameID=nCurrentFrameID; 
		m_RenderPass=0;
		m_UpdatedMorphTargetVBThisFrame = false;
 
		if (m_nStillNeedsMorph > 0)
			m_nStillNeedsMorph--;
	} 
	else 
	{
		m_RenderPass++;
	}
	assert(m_RenderPass!=0x55aa55aa);

	CModelMesh* pModelMesh = m_pModel->GetModelMesh(nLOD);

	uint32 numRemapJoints	= m_pSkinAttachment->m_arrRemapTable.size();
	{
		pBoneQuatsS		=	0;
		pBoneQuatsL		=	0; 
		pMBBoneQuatsL	=	0;
		pMBBoneQuatsS	=	0;

		uint32 iActiveFrame		=	pMaster->m_iActiveFrame;
		pRemapTable = &m_pSkinAttachment->m_arrRemapTable[0];

		//for (uint32 i=0; i<numRemapJoints; i++)
		//{
		//	uint32 r=m_pSkinAttachment->m_arrRemapTable[i];
		//	g_arrNewSkinQuat[i] = pMaster->m_arrNewSkinQuat[iActiveFrame][r];
		//}
		if (Console::GetInst().ca_SphericalSkinning) 
			pBoneQuatsS		=	&pMaster->m_arrNewSkinQuat[iActiveFrame][0];
		else
			pBoneQuatsL		=	&pMaster->m_arrNewSkinQuat[iActiveFrame][0]; 


		if (bUseMotionBlur) 
		{
			pMaster->UpdatePreviousFrameBones(0);
			if (pMaster->m_arrMBSkinQuat.size()==0)
				CryError("CryAnimation: m_arrMBSkinQuat is zero");

			//for (uint32 i=0; i<numRemapJoints; i++)
			//{
			//	uint32 r=m_pSkinAttachment->m_arrRemapTable[i];
			//	g_arrMBSkinQuat[i] = pMaster->m_arrMBSkinQuat[r];
			//}
			if (Console::GetInst().ca_SphericalSkinning) 
				pMBBoneQuatsS	=	&pMaster->m_arrMBSkinQuat[0];
			else
				pMBBoneQuatsL	=	&pMaster->m_arrMBSkinQuat[0];
		}

	}

	AddMorphTargetsToVertexBuffer(nLOD,nCloseEnough,HWSkinningFlags);

	//-----------------------------------------------------------------------
	//----    Create 8 Vec4 vectors that contain the blending values     ----
	//---                    to blend between 3 models                    ---
	//-----------------------------------------------------------------------
	uint32 rm=pMaster->GetResetMode();
	f32 MorphArray[8] = { 0,0,0,0,0,0,0,0};
	if (rm==0) 
	{ 
		f32* pMorphValues = pMaster->GetShapeDeformArray();
		MorphArray[0]=pMorphValues[0];
		MorphArray[1]=pMorphValues[1];
		MorphArray[2]=pMorphValues[2];
		MorphArray[3]=pMorphValues[3];
		MorphArray[4]=pMorphValues[4];
		MorphArray[5]=pMorphValues[5];
		MorphArray[6]=pMorphValues[6];
		MorphArray[7]=pMorphValues[7];
	}

	for (uint32 i=0; i<8; i++) 
	{
		if (MorphArray[i]<0.0f) 
			shapeDeformationData[i]	=	Vec4( 1-(MorphArray[i]+1), MorphArray[i]+1,	0, 0 );
		else
			shapeDeformationData[i]	=	Vec4( 0, 1-MorphArray[i], MorphArray[i],	0);
	}


	//------------------------------------------------------------------
	//---       render debug-output                                  ---
	//------------------------------------------------------------------
	if (m_RenderPass==0) 
	{

	//	float fColor[4] = {0,1,0,1};
	//	g_pIRenderer->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"CSkinInstance: %s ",m_pModel->GetFilePath() );	
	//	g_YLine+=16.0f;


	//	if (Console::GetInst().ca_DrawMotionBlurTest)
	//		MotionBlurTest(0,rRenderMat34);

		if (Console::GetInst().ca_DrawDecalsBBoxes)
			DrawDecalsBBoxes(pMaster,m_pSkinAttachment, rRenderMat34 );

		if (Console::GetInst().ca_DrawWireframe) 
		{

			QuatTS arrNewSkinQuat[MAX_JOINT_AMOUNT+1];	//bone quaternions for skinning (entire skeleton)
			uint32 iActiveFrame		=	pMaster->m_iActiveFrame;
			uint32 numRemapJoints	= m_pSkinAttachment->m_arrRemapTable.size();
			for (uint32 i=0; i<numRemapJoints; i++)	arrNewSkinQuat[i]=pMaster->m_arrNewSkinQuat[iActiveFrame][m_pSkinAttachment->m_arrRemapTable[i]];
			pModelMesh->DrawWireframe(&m_Morphing,arrNewSkinQuat,pMaster->GetShapeDeformArray(),pMaster->GetResetMode(),nLOD,rRenderMat34);
		}
		uint32 tang=0;
		tang|=Console::GetInst().ca_DrawTangents; 
		tang|=Console::GetInst().ca_DrawBinormals; 
		tang|=Console::GetInst().ca_DrawNormals; 
		if (tang) 
		{
			
			pModelMesh->SkinningExtSW(pMaster,m_pSkinAttachment,nLOD);

			if (Console::GetInst().ca_DrawTangents) 
				pModelMesh->DrawTangents(rRenderMat34);
			if (Console::GetInst().ca_DrawBinormals) 
				pModelMesh->DrawBinormals(rRenderMat34);
			if (Console::GetInst().ca_DrawNormals) 
				pModelMesh->DrawNormals(rRenderMat34);
		}

	}

	return numRemapJoints;//
}







