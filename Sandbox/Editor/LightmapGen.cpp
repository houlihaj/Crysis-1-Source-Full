////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   lightmapgen.cpp
//  Version:     v1.00
//  Created:     18/1/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"

#include "LightmapGen.h"
#include "LightmapGen.h"

#include "LightmapCompiler/RenderLight.h"

#include <I3DEngine.h>
#include "IEntitySystem.h"
#include "ITimer.h"

#include "Objects\ObjectManager.h"
#include "Objects\BrushObject.h"
#include "Objects\VoxelObject.h"

#include "GameExporter.h"
#include "GameEngine.h"

#include "Util\PakFile.h"
#include "Objects/Group.h"

#include <CryArray.h>
#include <ICryPak.h>

LMGenParam CLightmapGen::m_sParam;

//////////////////////////////////////////////////////////////////////////
class CRenderNodePred : public std::binary_function<std::pair<IRenderNode*, CBaseObject*>, std::pair<IRenderNode*, CBaseObject*>, bool>
{
public:
	bool operator () ( const std::pair<IRenderNode*, CBaseObject*>& rpIEtyRnd1,const std::pair<IRenderNode*, CBaseObject*>& rpIEtyRnd2 )
	{ 
		IRenderNode* pIEtyRnd1 = rpIEtyRnd1.first;
		IRenderNode* pIEtyRnd2 = rpIEtyRnd2.first;

		if((DWORD_PTR) pIEtyRnd1->GetEntityVisArea() < (DWORD_PTR) pIEtyRnd2->GetEntityVisArea()) return(true);
		if((DWORD_PTR) pIEtyRnd1->GetEntityVisArea() > (DWORD_PTR) pIEtyRnd2->GetEntityVisArea()) return(false);

		AABB box1 = pIEtyRnd1->GetBBox();
		AABB box2 = pIEtyRnd2->GetBBox();

		if(box1.min.x<box2.min.x) return(true);
		if(box1.min.x>box2.min.x) return(false);

		if(box1.min.y<box2.min.y) return(true);
		if(box1.min.y>box2.min.y) return(false);
		
		if(box1.min.z<box2.min.z) return(true);
		if(box1.min.z>box2.min.z) return(false);

		return(false);
	};
};

const bool CLightmapGen::Generate( ISystem *pSystem, std::vector<std::pair<IRenderNode*, CBaseObject*> >& nodes, 
	ICompilerProgress *pICompilerProgress, const ELMMode Mode, const std::set<const CBaseObject*>& vSelectionIndices )
{
	//////////////////////////////////////////////////////////////////////////
	// Put exported files in pak.
	//////////////////////////////////////////////////////////////////////////
	CString levelPath = Path::AddBackslash(GetIEditor()->GetGameEngine()->GetLevelPath());
	CString pakFilename = levelPath + "LevelLM.pak";

	if (!CFileUtil::OverwriteFile( pakFilename ))
		return false;

	if (!GetIEditor()->GetSystem()->GetIPak()->ClosePack( pakFilename ))
	{ 
		Error( "Cannot Close Pak File %s",(const char*)pakFilename );
		return false;
	}
	//////////////////////////////////////////////////////////////////////////
	IndoorBaseInterface IndInterface;
	IndInterface.m_pSystem   = pSystem;
	IndInterface.m_p3dEngine = pSystem->GetI3DEngine();
	IndInterface.m_pLog      = pSystem->GetILog();
	IndInterface.m_pRenderer = pSystem->GetIRenderer();
	IndInterface.m_pConsole  = pSystem->GetIConsole();

	CLightScene cLightScene;

	//CDLight GetIEditor()->Get3DEngine()->GetStaticLightSources();

	std::sort(nodes.begin(), nodes.end(), CRenderNodePred());

	bool bErrorsOccured = false;
	if (!cLightScene.GenerateMaps(IndInterface, m_sParam, nodes, 
		pICompilerProgress, Mode, m_pSharedData, vSelectionIndices, bErrorsOccured, false ))
	{
		Error("CLightmapGen::Generate failed");
	}
	return bErrorsOccured;
}

bool CLightmapGen::CreateLists( IEditor *pIEditor, const bool bInSelectionMode )
{
	bool bFoundOne = false;
	vSelection.clear();
	if( bInSelectionMode )
	{
		CSelectionGroup *pCurSel = pIEditor->GetSelection();
		UINT iNumObjects = pCurSel->GetCount();
		UINT iCurObj;

		for (iCurObj=0; iCurObj<iNumObjects; iCurObj++)
		{
			CBaseObject *pCurObject = pCurSel->GetObject(iCurObj);
			IRenderNode *pIEtyRend;	

			if( NULL == pCurObject )
				continue;

			if( pCurObject->GetType() == OBJTYPE_PREFAB )
			{
				CGroup *pGroup = (CGroup*)pCurObject;
				for(int o=0; o<pGroup->GetChildCount(); ++o)
				{
					CBaseObject *pBaseObj = pGroup->GetChild(o);
					if(pBaseObj)
					{
						if( IsLM_GLM(pBaseObj, &pIEtyRend ) )
						{
							vSelection.insert(pCurObject); 
							nodes.push_back(std::pair<IRenderNode*,CBaseObject*>(pIEtyRend,pBaseObj));
							bFoundOne = true;
						}
					}
				}
			}
			else
			{
				if( IsLM_GLM(pCurObject, &pIEtyRend ) )
				{
					vSelection.insert(pCurObject); 
					nodes.push_back(std::pair<IRenderNode*,CBaseObject*>(pIEtyRend,pCurObject));
					bFoundOne = true;
				}
			}
		} 
	}

	//now the others
	std::vector<CBaseObject*> objects;
	pIEditor->GetObjectManager()->GetObjects( objects );
	for (int i = 0; i < objects.size(); i++)
	{
		CBaseObject *obj = (CBaseObject*)objects[i];
		if(vSelection.find(obj) != vSelection.end())
			continue;
		IRenderNode *node;

		if( NULL == obj )
			continue;

		if( obj->GetType() == OBJTYPE_PREFAB )
		{
			CGroup *pGroup = (CGroup*)obj;
			for(int o=0; o<pGroup->GetChildCount(); ++o)
			{
				CBaseObject *pBaseObj = pGroup->GetChild(o);
				if(pBaseObj)
				{
					if (IsLM_GLM(pBaseObj,&node))
					{
						nodes.push_back(std::pair<IRenderNode*,CBaseObject*>(node,pBaseObj));
						bFoundOne |= !bInSelectionMode;
					}
				}
			}
		}
		else
		{
			if (IsLM_GLM(obj,&node))
			{
				nodes.push_back(std::pair<IRenderNode*,CBaseObject*>(node,obj));
				bFoundOne |= !bInSelectionMode;
			}
		}
	}

	return bFoundOne;
}


const bool CLightmapGen::GenerateSelected(IEditor *pIEditor, ICompilerProgress *pICompilerProgress)
{
	if( false == CreateLists( pIEditor, true ) )
		return true;
	const bool cbErrorsOccured = Generate(pIEditor->GetSystem(), nodes, pICompilerProgress,ELMMode_SEL, vSelection);
	if (pICompilerProgress)
		pICompilerProgress->Output("\r\nLightmap compilation finished");
	return cbErrorsOccured;
}


const bool CLightmapGen::GenerateChanged( IEditor *pIEditor, ICompilerProgress *pICompilerProgress )
{
	if( false == CreateLists( pIEditor, false ) )
		return true;
	const bool cbErrorsOccured = Generate(pIEditor->GetSystem(), nodes, pICompilerProgress, ELMMode_CHANGES, vSelection);

	// Export brush.lst
	if (pICompilerProgress)
		pICompilerProgress->Output("Full rebuild, Exporting 'brush.lst'...\r\n");
	CGameExporter cExporter(pIEditor->GetSystem());
	cExporter.ExportBrushes(pIEditor->GetGameEngine()->GetLevelPath());
	if (pICompilerProgress)
		pICompilerProgress->Output("\r\nLightmap compilation finished");
	return cbErrorsOccured;
}

bool CLightmapGen::IsLM_GLM(CBaseObject* pObject, struct IRenderNode **pIEtyRend)
{
	if( NULL == pObject || NULL == pIEtyRend )
		return false;
/*
	if(pRenderNode->m_pVisArea == NULL && !(pRenderNode->GetRndFlags() & ERF_HIDDEN))
GetFlags() & STATIC_OBJECT_HIDDEN
if(crObjPos.x < 0.f || crObjPos.y < 0.f || crObjPos.x >= (float)cWidthUnits || crObjPos.y >= (float)cWidthUnits)
return true;//off terrain
	m_pHeightMap = GetIEditor()->GetHeightmap();
	if(!m_pHeightMap)
		return false;
	m_UnitSize = m_pHeightMap->GetUnitSize();

	const uint32 cWidth = m_pHeightMap->GetWidth();
	const uint32 cWidthUnits = m_UnitSize * cWidth;
*/
	ObjectType CurType = pObject->GetType();
	if( CurType == OBJTYPE_BRUSH )
	{
		const CBrushObject *pBrushObj = (CBrushObject *) pObject;
		*pIEtyRend = pBrushObj->GetEngineNode();
	}
	else
		if( CurType == OBJTYPE_VOXEL )
		{
			const CVoxelObject *pVoxelObj = (CVoxelObject *) pObject;
			*pIEtyRend = pVoxelObj->GetEngineNode();
		}
		else
			return false;

	if( NULL == (*pIEtyRend) )
		return false;

	if ((*pIEtyRend)->GetRndFlags() & ERF_USERAMMAPS)
		return true;
	if ((*pIEtyRend)->GetRndFlags() & ERF_CASTSHADOWINTORAMMAP)
		return true;
	return false;
}

const bool CLightmapGen::GenerateAll(IEditor *pIEditor, ICompilerProgress *pICompilerProgress)
{
	if( false == CreateLists( pIEditor, false ) )
		return true;
	const bool cbErrorsOccured = Generate(pIEditor->GetSystem(), nodes, pICompilerProgress,ELMMode_ALL, vSelection);

	// Export brush.lst
	if (pICompilerProgress)
		pICompilerProgress->Output("Full rebuild, Exporting 'brush.lst'...\r\n");
	CGameExporter cExporter(pIEditor->GetSystem());
	cExporter.ExportBrushes(pIEditor->GetGameEngine()->GetLevelPath());
	if (pICompilerProgress)
		pICompilerProgress->Output("\r\nLightmap compilation finished");
	return cbErrorsOccured;
}


const bool CLightmapGen::GenerateLightList(IEditor *pIEditor, ICompilerProgress *pICompilerProgress)
{
	std::vector<std::pair<IRenderNode*,CBaseObject*> > nodes;
	UINT i;

	std::vector<CBaseObject*> objects;
	pIEditor->GetObjectManager()->GetObjects( objects );
	for (i = 0; i < objects.size(); i++)
	{
		CBaseObject *pObject = (CBaseObject*)objects[i];
		IRenderNode *pIEtyRend;

		if( NULL == pObject )
			continue;

		ObjectType CurType = pObject->GetType();
		if( CurType == OBJTYPE_BRUSH )
		{
			const CBrushObject *pBrushObj = (CBrushObject *) pObject;
			pIEtyRend = pBrushObj->GetEngineNode();
		}
		else
			continue;

		if( NULL == pIEtyRend )
			continue;

		nodes.push_back(std::pair<IRenderNode*,CBaseObject*>(pIEtyRend,pObject));
	}
	std::set<const CBaseObject*> vSelectionIndices;
	const bool cbErrorsOccured = true;

	ISystem *pSystem = pIEditor->GetSystem();
	//////////////////////////////////////////////////////////////////////////
	// Put exported files in pak.
	//////////////////////////////////////////////////////////////////////////
	CString levelPath = Path::AddBackslash(GetIEditor()->GetGameEngine()->GetLevelPath());
	CString pakFilename = levelPath + "LevelLM.pak";

	if (!CFileUtil::OverwriteFile( pakFilename ))
		return false;

	if (!GetIEditor()->GetSystem()->GetIPak()->ClosePack( pakFilename ))
	{ 
		Error( "Cannot Close Pak File %s",(const char*)pakFilename );
		return false;
	}
	//////////////////////////////////////////////////////////////////////////
	IndoorBaseInterface IndInterface;
	IndInterface.m_pSystem   = pSystem;
	IndInterface.m_p3dEngine = pSystem->GetI3DEngine();
	IndInterface.m_pLog      = pSystem->GetILog();
	IndInterface.m_pRenderer = pSystem->GetIRenderer();
	IndInterface.m_pConsole  = pSystem->GetIConsole();

	CLightScene cLightScene;

	//CDLight GetIEditor()->Get3DEngine()->GetStaticLightSources();

	std::sort(nodes.begin(), nodes.end(), CRenderNodePred());

	bool bErrorsOccured = false;
	if (!cLightScene.GenerateFalseLightList(IndInterface, m_sParam, nodes, 
		pICompilerProgress, ELMMode_ALL, m_pSharedData, vSelectionIndices, bErrorsOccured, false ))
	{
		Error("CLightmapGen::Generate light list failed!");
	}

	// Export brush.lst
	if (pICompilerProgress)
		pICompilerProgress->Output("Full rebuild, Exporting 'brush.lst'...\r\n");
	CGameExporter cExporter(pIEditor->GetSystem());
	cExporter.ExportBrushes(pIEditor->GetGameEngine()->GetLevelPath());
	if (pICompilerProgress)
		pICompilerProgress->Output("\r\nLightList compilation finished");
	return cbErrorsOccured;
}


const bool CLightmapGen::SetupLightmapQuality( ISystem *pSystem, std::vector<std::pair<IRenderNode*, CBaseObject*> >& nodes, 
																	ICompilerProgress *pICompilerProgress, const ELMMode Mode, const std::set<const CBaseObject*>& vSelectionIndices )
{
	//////////////////////////////////////////////////////////////////////////
	IndoorBaseInterface IndInterface;
	IndInterface.m_pSystem   = pSystem;
	IndInterface.m_p3dEngine = pSystem->GetI3DEngine();
	IndInterface.m_pLog      = pSystem->GetILog();
	IndInterface.m_pRenderer = pSystem->GetIRenderer();
	IndInterface.m_pConsole  = pSystem->GetIConsole();

	CLightScene cLightScene;

	std::sort(nodes.begin(), nodes.end(), CRenderNodePred());

	bool bErrorsOccured = false;
	if (!cLightScene.SetupLightmapQuality(IndInterface, m_sParam, nodes, 
		pICompilerProgress, Mode, m_pSharedData, vSelectionIndices, bErrorsOccured))
	{
		Error("CLightmapGen::SetupLightmapQuality failed");
	}
	return bErrorsOccured;
}

const bool CLightmapGen::SetupLightmapQualityAll(IEditor *pIEditor, ICompilerProgress *pICompilerProgress)
{
	if( false == CreateLists( pIEditor, false ) )
		return true;
	const bool cbErrorsOccured = SetupLightmapQuality(pIEditor->GetSystem(), nodes, pICompilerProgress,ELMMode_ALL, vSelection);

	if (pICompilerProgress)
		pICompilerProgress->Output("\r\nLightmapQuality setup finished");
	return cbErrorsOccured;
}

const bool CLightmapGen::SetupLightmapQualitySelected(IEditor *pIEditor, ICompilerProgress *pICompilerProgress)
{
	if( false == CreateLists( pIEditor, true ) )
		return true;
	const bool cbErrorsOccured = SetupLightmapQuality(pIEditor->GetSystem(), nodes, pICompilerProgress,ELMMode_SEL, vSelection);
	if (pICompilerProgress)
		pICompilerProgress->Output("\r\nLightmapQuality compilation finished");
	return cbErrorsOccured;
}



const bool CLightmapGen::ShowTexelDensity( ISystem *pSystem, std::vector<std::pair<IRenderNode*, CBaseObject*> >& nodes, 
																							ICompilerProgress *pICompilerProgress, const ELMMode Mode, const std::set<const CBaseObject*>& vSelectionIndices )
{
	//////////////////////////////////////////////////////////////////////////
	IndoorBaseInterface IndInterface;
	IndInterface.m_pSystem   = pSystem;
	IndInterface.m_p3dEngine = pSystem->GetI3DEngine();
	IndInterface.m_pLog      = pSystem->GetILog();
	IndInterface.m_pRenderer = pSystem->GetIRenderer();
	IndInterface.m_pConsole  = pSystem->GetIConsole();

	CLightScene cLightScene;

	std::sort(nodes.begin(), nodes.end(), CRenderNodePred());

	bool bErrorsOccured = false;
	if (!cLightScene.GenerateMaps(IndInterface, m_sParam, nodes, 
		pICompilerProgress, Mode, m_pSharedData, vSelectionIndices, bErrorsOccured, true ))
	{
		Error("CLightmapGen::ShowTexelDensity failed");
	}
	return bErrorsOccured;
}

const bool CLightmapGen::ShowTexelDensityAll(IEditor *pIEditor, ICompilerProgress *pICompilerProgress)
{
	if( false == CreateLists( pIEditor, false ) )
		return true;
	const bool cbErrorsOccured = ShowTexelDensity(pIEditor->GetSystem(), nodes, pICompilerProgress,ELMMode_ALL, vSelection);

	// Export brush.lst
	if (pICompilerProgress)
		pICompilerProgress->Output("Full rebuild, Exporting 'brush.lst'...\r\n");

	CGameExporter cExporter(pIEditor->GetSystem());
	cExporter.ExportBrushes(pIEditor->GetGameEngine()->GetLevelPath());

	if (pICompilerProgress)
		pICompilerProgress->Output("\r\nShowTexelDensity setup finished");

	return cbErrorsOccured;
}

const bool CLightmapGen::ShowTexelDensitySelected(IEditor *pIEditor, ICompilerProgress *pICompilerProgress)
{
	if( false == CreateLists( pIEditor, true ) )
		return true;
	const bool cbErrorsOccured = ShowTexelDensity(pIEditor->GetSystem(), nodes, pICompilerProgress,ELMMode_SEL, vSelection);
	if (pICompilerProgress)
		pICompilerProgress->Output("\r\nShowTexelDensity compilation finished");
	return cbErrorsOccured;
}

const bool CLightmapGen::GenerateLightVolume(IEditor *pIEditor, ICompilerProgress *pICompilerProgress)
{
	CreateLists( pIEditor, false );

	//////////////////////////////////////////////////////////////////////////
	IndoorBaseInterface IndInterface;
	ISystem *pSystem = pIEditor->GetSystem();
	IndInterface.m_pSystem   = pSystem;
	IndInterface.m_p3dEngine = pSystem->GetI3DEngine();
	IndInterface.m_pLog      = pSystem->GetILog();
	IndInterface.m_pRenderer = pSystem->GetIRenderer();
	IndInterface.m_pConsole  = pSystem->GetIConsole();

	CLightScene cLightScene;
	std::sort(nodes.begin(), nodes.end(), CRenderNodePred());

	bool bErrorsOccured = false;
	if (!cLightScene.GenerateLightVolume(IndInterface, m_sParam, nodes, pICompilerProgress, bErrorsOccured ) )
	{
		Error("CLightmapGen::GenerateLightVolume failed");
	}

	if( pICompilerProgress )
				pICompilerProgress->Output("\r\nGenerateLightVolume finished");
	return bErrorsOccured;
}


void CLightmapGen::Serialize(CXmlArchive xmlAr)
{
	// Serialize lightmap compiler settings

	//Get scene context singleton
	CSceneContext& sceneCtx = CSceneContext::GetInstance();

	if (xmlAr.bLoading)
	{
		XmlNodeRef xmlLMCompiler = xmlAr.root->findChild("LMCompiler");

		if (!xmlLMCompiler)
			return;

		xmlLMCompiler->getAttr("UseSuperSampling", sceneCtx.m_bUseSuperSampling);
		xmlLMCompiler->getAttr("MaxDiffuseDepth", sceneCtx.m_nMaxDiffuseDepth);
		xmlLMCompiler->getAttr("MaxSpecularDepth", sceneCtx.m_nMaxSpecularDepth);
		xmlLMCompiler->getAttr("numEmitPhotons", sceneCtx.m_numEmitPhotons);
		xmlLMCompiler->getAttr("PhotonEstimator", sceneCtx.m_nPhotonEstimator);
		xmlLMCompiler->getAttr("MinPhotonSearchRadius", sceneCtx.m_fMinPhotonSearchRadius);
		xmlLMCompiler->getAttr("MaxPhotonSearchRadius", sceneCtx.m_fMaxPhotonSearchRadius);
		xmlLMCompiler->getAttr("GridJitterBias", sceneCtx.m_fGridJitterBias);
		xmlLMCompiler->getAttr("numIndirectSamples", sceneCtx.m_numIndirectSamples);
		xmlLMCompiler->getAttr("MaxBrightness", sceneCtx.m_fMaxBrightness);
		xmlLMCompiler->getAttr("FinalRegatharing", sceneCtx.m_bFinalRegatharing);
		xmlLMCompiler->getAttr("UseSunLight", sceneCtx.m_bUseSunLight);
		xmlLMCompiler->getAttr("numSunDirectSamples", sceneCtx.m_numSunDirectSamples);
		xmlLMCompiler->getAttr("numSunPhotons", sceneCtx.m_numSunPhotons);
		xmlLMCompiler->getAttr("numDirectSamples", sceneCtx.m_numDirectSamples);

		xmlLMCompiler->getAttr("LightmapPageWidth", sceneCtx.m_nLightmapPageWidth);
		sceneCtx.m_nLightmapPageHeight = sceneCtx.m_nLightmapPageWidth; //same value
		xmlLMCompiler->getAttr("LumenPerUnitU", sceneCtx.m_fLumenPerUnitU);
		sceneCtx.m_fLumenPerUnitV = sceneCtx.m_fLumenPerUnitU; //same value

		xmlLMCompiler->getAttr("eLightmapMode", sceneCtx.m_eLightmapMode);

		xmlLMCompiler->getAttr("MakeRAE", sceneCtx.m_bMakeRAE);
		xmlLMCompiler->getAttr("MakeOcclMap", sceneCtx.m_bMakeOcclMap);
		xmlLMCompiler->getAttr("MakeLightMap", sceneCtx.m_bMakeLightMap);
		xmlLMCompiler->getAttr("MakeDistributedMap", sceneCtx.m_bDistributedMap);

		xmlLMCompiler->getAttr("numAmbientOcclusionSamples", sceneCtx.m_numAmbientOcclusionSamples );
		xmlLMCompiler->getAttr("fMinAmbientOcclusionSearchRadius", sceneCtx.m_fMinAmbientOcclusionSearchRadius );
		xmlLMCompiler->getAttr("fMaxAmbientOcclusionSearchRadius", sceneCtx.m_fMaxAmbientOcclusionSearchRadius );
	}
	else
	{
		XmlNodeRef xmlLMCompiler = xmlAr.root->newChild("LMCompiler");

		xmlLMCompiler->setAttr("UseSuperSampling", sceneCtx.m_bUseSuperSampling);
		xmlLMCompiler->setAttr("MaxDiffuseDepth", sceneCtx.m_nMaxDiffuseDepth);
		xmlLMCompiler->setAttr("MaxSpecularDepth", sceneCtx.m_nMaxSpecularDepth);
		xmlLMCompiler->setAttr("numEmitPhotons", sceneCtx.m_numEmitPhotons);
		xmlLMCompiler->setAttr("PhotonEstimator", sceneCtx.m_nPhotonEstimator);
		xmlLMCompiler->setAttr("MinPhotonSearchRadius", sceneCtx.m_fMinPhotonSearchRadius);
		xmlLMCompiler->setAttr("MaxPhotonSearchRadius", sceneCtx.m_fMaxPhotonSearchRadius);
		xmlLMCompiler->setAttr("GridJitterBias", sceneCtx.m_fGridJitterBias);
		xmlLMCompiler->setAttr("numIndirectSamples", sceneCtx.m_numIndirectSamples);
		xmlLMCompiler->setAttr("MaxBrightness", sceneCtx.m_fMaxBrightness);
		xmlLMCompiler->setAttr("FinalRegatharing", sceneCtx.m_bFinalRegatharing);
		xmlLMCompiler->setAttr("UseSunLight", sceneCtx.m_bUseSunLight);
		xmlLMCompiler->setAttr("numSunDirectSamples", sceneCtx.m_numSunDirectSamples);
		xmlLMCompiler->setAttr("numSunPhotons", sceneCtx.m_numSunPhotons);
		xmlLMCompiler->setAttr("numDirectSamples", sceneCtx.m_numDirectSamples);

		xmlLMCompiler->setAttr("LightmapPageWidth", sceneCtx.m_nLightmapPageWidth);
		xmlLMCompiler->setAttr("LumenPerUnitU", sceneCtx.m_fLumenPerUnitU);

		xmlLMCompiler->setAttr("eLightmapMode", sceneCtx.m_eLightmapMode);

		xmlLMCompiler->setAttr("MakeRAE", sceneCtx.m_bMakeRAE);
		xmlLMCompiler->setAttr("MakeOcclMap", sceneCtx.m_bMakeOcclMap);
		xmlLMCompiler->setAttr("MakeLightMap", sceneCtx.m_bMakeLightMap);
		xmlLMCompiler->setAttr("MakeDistributedMap", sceneCtx.m_bDistributedMap);

		xmlLMCompiler->setAttr("numAmbientOcclusionSamples", sceneCtx.m_numAmbientOcclusionSamples );
		xmlLMCompiler->setAttr("fMinAmbientOcclusionSearchRadius", sceneCtx.m_fMinAmbientOcclusionSearchRadius );
		xmlLMCompiler->setAttr("fMaxAmbientOcclusionSearchRadius", sceneCtx.m_fMaxAmbientOcclusionSearchRadius );
	}
}