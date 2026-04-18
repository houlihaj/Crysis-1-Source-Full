////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	28/09/2004 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  stores the loaded model and animations
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <CryHeaders.h>
#include <I3DEngine.h>
#include "CryHeaders.h"
#include "Model.h"
#include "CharacterInstance.h"
#include "CharacterManager.h"
#include "StringUtils.h"
#include "FacialAnimation/FacialModel.h"

static int GetDefaultPhysMaterial()
{
	I3DEngine* pEngine = g_pI3DEngine;
	if (pEngine)
	{
		IPhysMaterialEnumerator *pMatEnum = pEngine->GetPhysMaterialEnumerator();
		if (pMatEnum)
			return pMatEnum->EnumPhysMaterial("mat_default");
	}
	return 0; // the default in case there's no physics yet
}

CCharacterModel::CCharacterModel (const string& strFileName, CharacterManager* pManager, uint32 type) : m_strFilePath (strFileName)
{
	m_IsProcessed=0;
	m_pFacialModel = 0;
	m_ObjectType=type;
	m_AnimationSet.Init();
	m_AnimationSet.m_pModel=this;

	m_bHasPhysics2 = 0;
	m_pManager=pManager;
	m_vModelOffset = Vec3(0,0,0);
	m_nDefaultGameID = GetDefaultPhysMaterial();

	// the material physical game id that will be used as default for this character
	m_pMaterial = g_pI3DEngine->GetMaterialManager()->GetDefaultMaterial();
	 
	for (int i=0; i<g_nMaxGeomLodLevels; i++)
		m_pRenderMeshs[i] = 0;

	m_pKinCharInstance = 0;
	m_nRefCounter = 0;
	m_nBaseLOD = 0;
}

CCharacterModel::~CCharacterModel()
{
	SAFE_RELEASE(m_pFacialModel);

	if (!m_SetInstances.empty())
	{
		g_pILog->LogToFile("*ERROR* ~CCharacterModel(%s): %u character instances still not deleted. Forcing deletion.", m_strFilePath.c_str(), m_SetInstances.size());
		CleanupInstances();
	}

	m_pManager->UnregisterModel(this);

	//FIXME:
	//g_AnimationManager.RemoveLoaderDBA(m_strTracksDBFilePath);

	// delete render meshes
	for (int i=0; i<g_nMaxGeomLodLevels; i++)
	{
		if (m_pRenderMeshs[i])
			g_pIRenderer->DeleteRenderMesh(m_pRenderMeshs[i]);
	}

	
}



void CCharacterModel::RegisterInstance (ICharacterInstance* pInstance)
{
	m_SetInstances.insert (pInstance);
}

void CCharacterModel::UnregisterInstance (ICharacterInstance* pInstance)
{
#ifdef _DEBUG
	if (m_SetInstances.find(pInstance) == m_SetInstances.end())
	{
		g_pILog->LogToFile("*ERROR* CCharacterModel::UnregisterInstance(0x%x): twice-deleting the character instance 0x%08X!!!", (uint32)this, (uint32)pInstance);
	}
#endif
	m_SetInstances.erase(pInstance);
}




// destroys all characters
// THis may (and should) lead to destruction and self-deregistration of this body
void CCharacterModel::CleanupInstances()
{
	// since after each instance deletion the body may be destructed itself,
	// we'll lock it for this time
	uint32 numInstances = m_SetInstances.size();
/*	for (uint32 i=0; i<numInstances; i++)
	{
		CharacterInstance* pCharacterInstance = *m_setInstances.begin();
		pCharacterInstance->Release();
	}*/


	if (!m_SetInstances.empty())
		g_pISystem->Warning (VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, 0, "CCharacterModel.CleanupInstances", "Forcing deletion of %d instances for body %s. CRASH POSSIBLE because other subsystems may have stored dangling pointer(s).", NumRefs(), m_strFilePath.c_str());

	CCharacterModel_AutoPtr pLock = this; // don't remove this line, it's for locking the body in memory untill every instance is finished.
	while (!m_SetInstances.empty())
	{
		ICharacterInstance* pCharacterInstance = *m_SetInstances.begin();
		pCharacterInstance->Release();
//		delete *m_setInstances.begin();
	}
}






CMorphTarget* CCharacterModel::getMorphSkin (unsigned nLOD, int nMorphTargetId)
{
	/*
	if (nLOD==0  &&  nMorphTargetId>=0  &&  nMorphTargetId<(int)GetModelMesh(0)->m_morphTargets.size() )
		return GetModelMesh(0)->m_morphTargets[nMorphTargetId];
	else
	{
		return 0;
	}
	*/

	if (nMorphTargetId>=0  &&  nMorphTargetId<(int)GetModelMesh(nLOD)->m_morphTargets.size() )
		return GetModelMesh(nLOD)->m_morphTargets[nMorphTargetId];
	else
	{
		return 0;
	}
}

const Vec3& CCharacterModel::getModelOffset()const
{
	return m_vModelOffset;
}



// sets the memory for the given vector, compatible with the STL vector by value_type, size() and &[0]
#define MEMSET_VECTOR(arr,value) memset (&((arr)[0]),value,sizeof(arr[0])*arr.size())


//////////////////////////////////////////////////////////////////////////
void CCharacterModel::PostLoad()
{
	CModelMesh *pModelMesh = GetModelMesh(0);
	if (pModelMesh)
	{
		if (pModelMesh->m_morphTargets.size() != 0)
		{
			m_pFacialModel = new CFacialModel( this );
			m_pFacialModel->AddRef();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCharacterModel::UpdatePhysBonePrimitives(DynArray<BONE_ENTITY> arrBoneEntities, int iLOD)
{



	CModelJoint *pBone;
	mesh_data *pmesh;
	IGeometry *pMeshGeom;
	std::map<unsigned, CModelJoint*> mapCtrlId;
	IGeomManager *pGeoman = g_pIPhysicalWorld->GetGeomManager();
	m_pModelSkeleton->m_arrModelJoints[0].AddHierarchyToControllerIdMap(mapCtrlId);

	for (int i=arrBoneEntities.size()-1; i>=0; i--)
		if (arrBoneEntities[i].prop[0] && (pBone = stl::find_in_map(mapCtrlId, arrBoneEntities[i].ControllerID, 0)) && 
				pBone->m_PhysInfo[iLOD].pPhysGeom && pBone->m_PhysInfo[iLOD].pPhysGeom->pGeom->GetType()==GEOM_TRIMESH)
		{
			int flags = mesh_SingleBB;
			if (CryStringUtils::strnstr(arrBoneEntities[i].prop,"box", sizeof(arrBoneEntities[i].prop)))
				flags |= mesh_approx_box;
			else if (CryStringUtils::strnstr(arrBoneEntities[i].prop,"cylinder", sizeof(arrBoneEntities[i].prop)))
				flags |= mesh_approx_cylinder;
			else if (CryStringUtils::strnstr(arrBoneEntities[i].prop,"capsule", sizeof(arrBoneEntities[i].prop)))
				flags |= mesh_approx_capsule;
			else if (CryStringUtils::strnstr(arrBoneEntities[i].prop,"sphere", sizeof(arrBoneEntities[i].prop)))
				flags |= mesh_approx_sphere;
			else continue;
			pmesh = (mesh_data*)(pMeshGeom=pBone->m_PhysInfo[iLOD].pPhysGeom->pGeom)->GetData();
			pBone->m_PhysInfo[iLOD].pPhysGeom->pGeom = pGeoman->CreateMesh(pmesh->pVertices,
				strided_pointer<uint16>((uint16*)pmesh->pIndices,sizeof(pmesh->pIndices[0])), pmesh->pMats, 0,pmesh->nTris,flags, 1.0f);
			pMeshGeom->Release();
		}
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CCharacterModel::GetMaterial()
{
	if (this->pCGA_Object)
		return this->pCGA_Object->GetMaterial();

	return m_pMaterial; 
}

//////////////////////////////////////////////////////////////////////////
uint32 CCharacterModel::GetTextureMemoryUsage( ICrySizer *pSizer )
{
	uint32 nSize = 0;
	if (pSizer)
	{
		for (int i=0; i<g_nMaxGeomLodLevels; i++)
		{
			if (m_pRenderMeshs[i])
			{
				nSize += (uint32)m_pRenderMeshs[i]->GetTextureMemoryUsage( m_pMaterial,pSizer );
			}
		}
	}
	else
	{
		if (m_pRenderMeshs[0])
			nSize = (uint32)m_pRenderMeshs[0]->GetTextureMemoryUsage( m_pMaterial );
	}
	return nSize;
}

//////////////////////////////////////////////////////////////////////////
uint32 CCharacterModel::GetMeshMemoryUsage( ICrySizer *pSizer)
{
	uint32 nSize = 0;

	for (int i=0; i<g_nMaxGeomLodLevels; i++)
	{
		if (m_pRenderMeshs[i])
		{
			nSize += m_pRenderMeshs[i]->GetMemoryUsage(0, IRenderMesh::MEM_USAGE_COMBINED);
		}
	}

	nSize += SizeOfThis(pSizer);

	return nSize;
}

uint32 CCharacterModel::SizeOfThis(ICrySizer *pSizer)
{
	uint32 nSize = sizeof(CCharacterModel) + m_SetInstances.size() * 4;

	{
		SIZER_COMPONENT_NAME(pSizer, "Models size");

		for(uint32 i =0, end = m_arrModelMeshes.size(); i < end; ++i)
		{
			nSize += m_arrModelMeshes[i].SizeOfThis(pSizer) + sizeof(CModelMesh);
		}

		pSizer->AddObject(&m_arrModelMeshes, nSize);
	}


	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "Joints");
		nSize += m_pModelSkeleton->SizeOfThis();
		pSizer->AddObject(&m_pModelSkeleton, nSize);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "AnimationSets");
		nSize += m_AnimationSet.SizeOfThis();
		pSizer->AddObject(&m_AnimationSet, m_AnimationSet.SizeOfThis());
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "FacialModel");
		if (m_pFacialModel)
		{
			nSize += m_pFacialModel->SizeOfThis();
			pSizer->AddObject(&m_pFacialModel, m_pFacialModel->SizeOfThis());
		}
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CollisionInfo");

		uint32 coll(0);
		for(uint32 i =0, end = m_arrCollisions.size(); i < end; ++i)
		{

			coll += sizeof(MeshCollisionInfo);
			coll += m_arrCollisions[i].m_arrIndexes.capacity() * sizeof(short int);
		}

		nSize += coll;
		pSizer->AddObject(&m_arrCollisions, coll);
	}


	return nSize;
}

//////////////////////////////////////////////////////////////////////////
IRenderMesh* CCharacterModel::GetRenderMesh( int nLod )
{
	if (nLod == -1)
		nLod = m_nBaseLOD;

	if (nLod < 0 || nLod >= g_nMaxGeomLodLevels)
		return 0;
	return m_pRenderMeshs[nLod];

}

//////////////////////////////////////////////////////////////////////////
uint32 CCharacterModel::GetNumLods()
{
	uint32 nLods = 0;
	for (int i=0; i<g_nMaxGeomLodLevels; i++)
	{
		if (m_pRenderMeshs[i])
			nLods++;
	}
	return nLods;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void CCharacterModel::AddModelTracksDatabase(const string& sAnimTracksDatabasePath) 
{
	string tmp(sAnimTracksDatabasePath);
	CryStringUtils::UnifyFilePath(tmp);
	for (int i = 0, end = m_arrTracksDBFilePath.size(); i < end; ++i)
	{
		if (m_arrTracksDBFilePath[i].compareNoCase(tmp) == 0)
			return;
	}

	m_arrTracksDBFilePath.push_back(tmp);
}