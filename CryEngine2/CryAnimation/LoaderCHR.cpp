////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Crytek Character Animation source code
//	
//	History:
//	28/09/2004 - Created by Ivo Herzeg <ivo@crytek.de>
//
//  Contains:
//  loading of model and animationss
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <I3DEngine.h>
#include <ICryAnimation.h>
#include <IIndexedMesh.h>
#include "VectorMap.h"
#include "CryHeaders.h"
#include "ModelMesh.h"
#include "Model.h"
#include "LoaderCHR.h"
#include "StringUtils.h"
#include "CharacterManager.h"
#include "FacialAnimation/FaceAnimation.h"
#include "FacialAnimation/FacialModel.h"
#include "FacialAnimation/FaceEffectorLibrary.h"
#include "LoaderDBA.h"
#include "AnimEventLoader.h"



//////////////////////////////////////////////////////////////////////////
// temporary solution to access the links for thin and fat models
//////////////////////////////////////////////////////////////////////////
extern CCharacterModel* model_thin;
extern CCharacterModel* model_fat;


string GetLODName(const string& file, uint32 LOD) 
{
	return LOD ? file + "_lod" + CryStringUtils::toString(LOD) + "." + CRY_CHARACTER_FILE_EXT : file+"."+CRY_CHARACTER_FILE_EXT;
}


CCharacterModel* CryCHRLoader::LoadNewCHR( const string& strGeomFileName, CharacterManager* pManager, uint32 SurpressWarning,uint32 qqloadanimations)
{

	FUNCTION_PROFILER( gEnv->pSystem, PROFILE_ANIMATION );

	uint32 s=sizeof(TFace);
	assert(s==6);

	const char* szExt = CryStringUtils::FindExtension(strGeomFileName.c_str());
	m_strGeomFileNameNoExt.assign (strGeomFileName.c_str(), *szExt?szExt-1:szExt);

	if (m_strGeomFileNameNoExt.empty()){
		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,strGeomFileName,	"Wrong character filename" );
		return 0;
	}

	//--------------------------------------------------------------------------------------------

	uint32 nLODs=0;
	std::vector<CContentCGF*> pCGF;
	pCGF.resize(1);
	bool b = true;

	uint32 baseLOD = 0;


	string filename = m_strGeomFileNameNoExt+"."+CRY_CHARACTER_FILE_EXT;

	while (b)
	{
		bool bNoWarnings = baseLOD != 0;
		pCGF[0] = g_pI3DEngine->LoadChunkFileContent(GetLODName(m_strGeomFileNameNoExt, baseLOD),bNoWarnings);
		if (pCGF[0])
		{
			b = false;
			nLODs++;
		}
		else
		{
			if (baseLOD == 0)
			{

				if (SurpressWarning==0)
				{
					g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,filename,	"Failed to Load Character file" );
				}

				return false;
			}
			else
				--baseLOD;
		}
	}

	b = true;
	while (b)
	{
		bool bNoWarnings = (baseLOD + nLODs) != 0;
		CContentCGF* p = g_pI3DEngine->LoadChunkFileContent(GetLODName(m_strGeomFileNameNoExt, baseLOD + nLODs),bNoWarnings);//m_strGeomFileNameNoExt + "_lod" + CryStringUtils::toString(nLODs) + "." + CRY_CHARACTER_FILE_EXT );
		if (p)
		{
			pCGF.push_back(p);
			nLODs++; 
		}
		else
			b =false;
	}


	static ICVar *p_e_lod_min = g_pISystem->GetIConsole()->GetCVar("e_lod_min");
	if(p_e_lod_min)
		baseLOD = p_e_lod_min->GetIVal();

	if ( baseLOD >= nLODs)
		baseLOD = nLODs - 1;


	CCharacterModel* pModel = new CCharacterModel(strGeomFileName, pManager, CHR);
	const char* fname = pModel->GetModelFilePath();

	pModel->m_nBaseLOD = baseLOD;

	// Timur Work arround for Xenon compiler December.
	//Timur, this crashes Xenon compiler: pModel->m_arrModelMeshes.resize(nLODs);
	//pModel->m_arrModelMeshes.resize(nLODs);

	{
		pModel->m_arrModelMeshes.resize(nLODs, CModelMesh());
		//pModel->m_arrModelMeshes.reserve(nLODs);
		//for (uint32 i=0; i < nLODs; i++)
		//	pModel->m_arrModelMeshes.push_back(CModelMesh());
	}


	_smart_ptr<IMaterial> m_pMaterial;

	IGeomManager *m_pPhysicalGeometryManager = g_pIPhysicalWorld ? g_pIPhysicalWorld->GetGeomManager() : NULL;
	assert(m_pPhysicalGeometryManager);


	//	std::vector<BoneID> arrExtBoneIDs;
	//	std::vector<ExtSkinVertex> arrExtVertices;

	// Create a remapping table for morph names to morph indices. This is so the morph with a given
	// name appears in the same position for all LODs (otherwise the wrong morph can get played if
	// morphs are missing in lower LODs).
	VectorMap<string, int> morphNameIndexMap;
	{
		VectorMap<string, int>::container_type morphNames;

		// Find the first model.
		CSkinningInfo* pBaseSkinningInfo = 0;
		for (uint32 lod=0; lod<nLODs; lod++)
		{
			CContentCGF* pLOD = pCGF[lod];
			CSkinningInfo* pSkinningInfo = (pLOD ? pLOD->GetSkinningInfo() : 0);
			if (pSkinningInfo)
			{
				pBaseSkinningInfo = pSkinningInfo;
				break;
			}
		}

		// Build the name map.
		for (int morphIndex = 0, morphCount = pBaseSkinningInfo->m_arrMorphTargets.size(); morphIndex < morphCount; ++morphIndex)
		{
			const string& name = pBaseSkinningInfo->m_arrMorphTargets[morphIndex]->m_strName;
			morphNames.push_back(std::make_pair(name, morphIndex));
		}
		morphNameIndexMap.SwapElementsWithVector(morphNames);
	}

	uint32 nWrongModelFormat=0;
	//	CSkinningInfo* pSkinningInfo;
	for (uint32 lod=0; lod<nLODs; lod++) 
	{
		if (pCGF[lod])
		{
			CSkinningInfo* pSkinningInfo = pCGF[lod]->GetSkinningInfo();
			if (pSkinningInfo==0) 
				return 0;

			DynArray<uint16> &m_arrExtToIntMap  = pSkinningInfo->m_arrExt2IntMap;

			//--------------------------------------------------------------------------
			//---         setup optimized indexed mesh for rendering                 ---
			//--------------------------------------------------------------------------
			uint32 numNodes = pCGF[lod]->GetNodeCount();
			CNodeCGF* pGFXNode=0;
			for(uint32 n=0; n<numNodes; n++) 
			{
				if (pCGF[lod]->GetNode(n)->type==CNodeCGF::NODE_MESH)	
				{	
					pGFXNode = pCGF[lod]->GetNode(n);	break; 
				}
			}
			if (pGFXNode==0)
			{
				g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,filename,	"Failed to Load Character file. GFXNode not found" );
				assert (0);
				return false;
			}

			CMesh* pMesh = pGFXNode->pMesh;
			if (pMesh->m_pBoneMapping==0)
			{
				g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,filename,	"Failed to Load Character file. Skeleton-Initial-Positions are missing" );
				assert (0);
				return false;
			}

			if (lod == 0 || m_pMaterial == NULL)
			{
				if (pGFXNode->pMaterial)
					m_pMaterial = g_pI3DEngine->GetMaterialManager()->LoadCGFMaterial( pGFXNode->pMaterial,PathUtil::GetPath(m_strGeomFileNameNoExt) );
				else
					m_pMaterial = g_pI3DEngine->GetMaterialManager()->GetDefaultMaterial();
			}
			// Assign loaded material to model.
			if (m_pMaterial)
				pModel->SetMaterial( m_pMaterial );


			//---------------------------------------------------------------------
			//---------------------------------------------------------------------
			//---------------------------------------------------------------------

			CModelMesh* pModelMesh = pModel->GetModelMesh(lod);
			pModelMesh->m_nLOD = lod;
			pModelMesh->m_pModel = pModel;

			//compare bone between different LODs
			if (lod >= 1) 
			{ 
				CSkinningInfo* pSkinningInfo0 = pCGF[0]->GetSkinningInfo();
				CSkinningInfo* pSkinningInfo1 = pCGF[lod]->GetSkinningInfo();
				uint32 numBones0 = pSkinningInfo0->m_arrBonesDesc.size();
				uint32 numBones1 = pSkinningInfo1->m_arrBonesDesc.size();
				if (numBones0!=numBones1) 
				{
					g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,filename,	"Failed to Load Character, Different bone amount of LOD0 and LOD%i", lod );
					assert (0);
					return false;
				}

				uint32 HasLODMismatch=0;
				for (uint32 g=0; g<numBones0; g++)
				{
					const char* name0 = pSkinningInfo0->m_arrBonesDesc[g].m_arrBoneName;
					const char* name1 = pSkinningInfo1->m_arrBonesDesc[g].m_arrBoneName;
					bool IsIdentical  = ( 0 == stricmp(name0,name1) );
					if (IsIdentical==0) 
					{
						g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,filename,	"Character LOD mismatch. The bone number %d is different. LOD0 %s  LOD%i %s",g,name0, lod, name1 );
						HasLODMismatch++;
					}
				}

				if (HasLODMismatch)
					return false;
			}




			if (lod==0) 
			{
				//-----------------------------------------------------------------
				//---                initialize bone info                       ---
				//---  in LOD 0 we initialize the bones and the morph-targets,  ---
				//---  in LOD 1 we update their physics; 
				//---  LOD 2 has no effect
				//-----------------------------------------------------------------
				uint32 numBones = pSkinningInfo->m_arrBonesDesc.size();	assert(numBones);
				if (numBones>MAX_JOINT_AMOUNT)
				{
					g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,filename,	"Model has more then %d joints", numBones );
					assert(!"Invalid amount of joints");
					return 0;
				}

				pModel->m_pModelSkeleton = new CModelSkeleton;
				pModel->m_pModelSkeleton->m_arrModelJoints.resize (numBones);

				pModel->m_arrCollisions.reserve(numBones);

				for (uint32 nBone = 0; nBone<numBones; ++nBone) 
				{
					assert(pSkinningInfo->m_arrBonesDesc[nBone].m_DefaultB2W.IsOrthonormalRH());
					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_idx							= nBone;

					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_idxParent	= -1;
					int32 offset=pSkinningInfo->m_arrBonesDesc[nBone].m_nOffsetParent;
					if (offset)
						pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_idxParent	= (int)nBone+offset;


					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_nJointCRC32	=	pSkinningInfo->m_arrBonesDesc[nBone].m_nControllerID;
					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_PhysInfo[0]			=	pSkinningInfo->m_arrBonesDesc[nBone].m_PhysInfo[0];
					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_PhysInfo[1].pPhysGeom = 0;
					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_fMass						=	pSkinningInfo->m_arrBonesDesc[nBone].m_fMass;
					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_DefaultAbsPose	=	QuatT(pSkinningInfo->m_arrBonesDesc[nBone].m_DefaultB2W);
					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_DefaultAbsPose.q.Normalize();

					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_nLimbId					=	pSkinningInfo->m_arrBonesDesc[nBone].m_nLimbId;
					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_numChildren			=	pSkinningInfo->m_arrBonesDesc[nBone].m_numChildren;
					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_nOffsetChildren	=	pSkinningInfo->m_arrBonesDesc[nBone].m_nOffsetChildren;



					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_DefaultRelPose = pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_DefaultAbsPose;
					int32 p=pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_idxParent;
					if ( p>=0 )
						pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_DefaultRelPose = pModel->m_pModelSkeleton->m_arrModelJoints[p].m_DefaultAbsPose.GetInverted() * pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_DefaultAbsPose;
					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].m_DefaultRelPose.q.Normalize();

					pModel->m_pModelSkeleton->m_arrModelJoints[nBone].SetJointName( &pSkinningInfo->m_arrBonesDesc[nBone].m_arrBoneName[0] );
				}


				//check deepness-level inside hierarchy
				for (uint32 i=0; i<numBones; i++)
				{
					int32 p = pModel->m_pModelSkeleton->m_arrModelJoints[i].m_idxParent;
					while (p >= 0) 
					{
						pModel->m_pModelSkeleton->m_arrModelJoints[i].m_numLevels++;
						p = pModel->m_pModelSkeleton->m_arrModelJoints[p].m_idxParent;
					}
				}

				//find the amount of children for every joint
				/*	for (uint32 a=0; a<numBones; a++)
				pModel->m_pModelSkeleton->m_arrModelJoints[a].m_numChilds=0;
				for (uint32 a=0; a<numBones; a++)
				{
				for (uint32 b=0; b<numBones; b++)
				{
				if (a==pModel->m_pModelSkeleton->m_arrModelJoints[b].m_idxParent)
				pModel->m_pModelSkeleton->m_arrModelJoints[a].m_numChilds++;
				}	
				}*/

			} //if(lod==0)

			//in LOD 1 we update the physics; lod 2 has no effect
			if (lod<2) 
				pModel->m_pModelSkeleton->m_arrModelJoints[0].UpdateHierarchyPhysics( pSkinningInfo->m_arrBoneEntities, lod );



			const char* RootName = pModel->m_pModelSkeleton->m_arrModelJoints[0].GetJointName();
			Matrix34 m34=pSkinningInfo->m_arrBonesDesc[0].m_DefaultB2W;
			Quat orientation = Quat(m34);
			Vec3 x=orientation.GetColumn0();
			Vec3 y=orientation.GetColumn1();
			Vec3 z=orientation.GetColumn2();
			string LODFileName = GetLODName(m_strGeomFileNameNoExt, lod);					
			if (RootName[0]=='B' && RootName[1]=='i' && RootName[2]=='p' && RootName[3]=='0' && RootName[4]=='1')
			{
				uint32 IsIdentiy0 = orientation.IsEquivalent(Quat(IDENTITY),0.001f); //this is the default Motion Builder orientation
				uint32 IsIdentiy1 = orientation.IsEquivalent(Quat(0.70710677f,Vec3(0,0,0.70710677f)),0.001f); //this is the default Character Studio orientation
				if (IsIdentiy0==0 && IsIdentiy1==0)
				{
					g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,LODFileName,	"Articulated model (BIP) has wrong orientation. Please use the CryEngine2 rules when building a model" );
					assert(!"Inverted model orientation!");
				}
			}
			else
			{
				uint32 IsIdentiy = orientation.IsEquivalent(Quat(IDENTITY),0.001f);
				if (IsIdentiy==0)
				{
					g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,LODFileName,	"Articulated model (MAX) has wrong orientation. Please use the CryEngine2 rules when building a model" );
					assert(!"Inverted model orientation!");
				}
			}

			//-----------------------------------------------------------------------------------
			//-----------------------------------------------------------------------------------
			pModel->m_arrModelMeshes[lod].m_arrPhyBoneMeshes=pSkinningInfo->m_arrPhyBoneMeshes;
			if (m_pPhysicalGeometryManager) 
			{
				// this map contains the bone geometry. The mapping is : [Chunk ID]->[Physical geometry read from that chunk]
				// the chunks from which the physical geometry is read are the bone mesh chunks.
				CCharacterModel::ChunkIdToPhysGeomMap m_mapLimbPhysGeoms;
				m_mapLimbPhysGeoms.clear();
				uint32 numPBM = pSkinningInfo->m_arrPhyBoneMeshes.size();	
				for (uint32 p=0; p<numPBM; p++) 
				{
					PhysicalProxy  pbm = pSkinningInfo->m_arrPhyBoneMeshes[p];
					uint32 numFaces = pbm.m_arrMaterials.size();
					// add a new entry to the map chunk->limb geometry, so that later it can be found by the bones
					// during the post-initialization mapping of pPhysGeom from Chunk id to the actual geometry pointer
					uint32 flags = (numFaces<=20 ? mesh_SingleBB : mesh_OBB|mesh_AABB|mesh_AABB_rotated)	| mesh_multicontact0 | mesh_approx_box | mesh_approx_sphere | mesh_approx_cylinder | mesh_approx_capsule;

					IGeometry* pPhysicalGeometry = m_pPhysicalGeometryManager->CreateMesh(&pbm.m_arrPoints[0],&pbm.m_arrIndices[0], &pbm.m_arrMaterials[0] ,0,numFaces,flags );
					assert (pPhysicalGeometry);
					assert(m_pMaterial != NULL);
					if (m_pMaterial)
					{
						// Assign custom material to physics.
						int defSurfaceIdx = pbm.m_arrMaterials.empty() ? 0 : pbm.m_arrMaterials[0] ;
						int surfaceTypesId[MAX_SUB_MATERIALS];
						memset( surfaceTypesId,0,sizeof(surfaceTypesId) );
						int numIds = m_pMaterial->FillSurfaceTypeIds(surfaceTypesId);
						m_mapLimbPhysGeoms[pbm.ChunkID] =	m_pPhysicalGeometryManager->RegisterGeometry( pPhysicalGeometry, defSurfaceIdx, &surfaceTypesId[0],numIds );
					}

					pPhysicalGeometry->Release();
				}

				if (lod==0)
				{
					pModel->m_bHasPhysics2 = false;
					// build the bone index by name and call PostInitialize for bone infos
					//pModel->m_AnimationSet.onBonesChanged();
				}		

				if (lod < 2)
				{
					// change the pPhysGeom from indices (chunk ids) to the actual physical geometry pointers
					// This modifies the given map by deleting or zeroing elements of the map that were used during the mapping
					if (pModel->PostInitBonePhysGeom (m_mapLimbPhysGeoms, lod))
					{
						pModel->UpdatePhysBonePrimitives(pSkinningInfo->m_arrBoneEntities, lod);
						pModel->m_bHasPhysics2 = true;
						pModel->onBonePhysicsChanged();
					}
				}

				// clean up the physical geometry objects that were not used
				for (CCharacterModel::ChunkIdToPhysGeomMap::iterator it = m_mapLimbPhysGeoms.begin(); it != m_mapLimbPhysGeoms.end(); ++it)
				{
					phys_geometry* pPhysGeom = it->second;
					if (pPhysGeom)
						g_pIPhysicalWorld->GetGeomManager()->UnregisterGeometry(pPhysGeom);
				}
				m_mapLimbPhysGeoms.clear();

			}



		//	const char* RootName = pModel->m_pModelSkeleton->m_arrModelJoints[0].GetJointName();
			if (RootName[0]=='B' && RootName[1]=='i' && RootName[2]=='p' && RootName[3]=='0' && RootName[4]=='1')
			{
				//This character-model is a biped and it was made in CharacterStudio. 
				//CharacterStudio is not allowing us to project the Bip01 on the ground and change its orientation; thats why we have to do it here.
				pModel->m_pModelSkeleton->m_arrModelJoints[0].m_DefaultAbsPose.SetIdentity();
				pModel->m_pModelSkeleton->m_arrModelJoints[0].m_DefaultRelPose.SetIdentity();

				QuatT abs_root	 = pModel->m_pModelSkeleton->m_arrModelJoints[0].m_DefaultAbsPose;
				QuatT abs_pelvis = pModel->m_pModelSkeleton->m_arrModelJoints[1].m_DefaultAbsPose;
				pModel->m_pModelSkeleton->m_arrModelJoints[1].m_DefaultRelPose=abs_root.GetInverted()*abs_pelvis;
			}

			//calculate inverse default pose
			uint32 numJoints = pModel->m_pModelSkeleton->m_arrModelJoints.size();
			for (uint32 i=0; i<numJoints; i++)
			{
				pModel->m_pModelSkeleton->m_arrModelJoints[i].m_DefaultAbsPoseInverse = pModel->m_pModelSkeleton->m_arrModelJoints[i].m_DefaultAbsPose.GetInverted();
				pModel->m_pModelSkeleton->m_arrModelJoints[i].m_DefaultAbsPoseInverse.q.Normalize();
			}



			//--------------------------------------------------------------------------
			//---                copy internal skin-vertices                         ---
			//--------------------------------------------------------------------------
			uint32 numIntVertices = pSkinningInfo->m_arrIntVertices.size();
			assert(numIntVertices);
			pModelMesh->m_arrIntVertices=pSkinningInfo->m_arrIntVertices;

			//--------------------------------------------------------------------------
			//---         copy internal faces (sorted by materials)                  ---
			//--------------------------------------------------------------------------
			uint32 numIntFaces = pSkinningInfo->m_arrIntFaces.size();
			assert(numIntFaces);
			pModelMesh->m_arrIntFaces = pSkinningInfo->m_arrIntFaces;

			//--------------------------------------------------------------------------
			//---                     copy Ext2Int map                               ---
			//--------------------------------------------------------------------------
			uint32 numExtVertices = pSkinningInfo->m_arrExt2IntMap.size();
			assert(numExtVertices);
			//	pModelMesh->m_arrExtToIntMap = pSkinningInfo->m_arrExt2IntMap;


			pModelMesh->m_numExtVertices=numExtVertices;



			std::vector<TFace> arrExtFaces;

			//-----------------------------------------------------------------
			//---   check if we have a thin and a fat version               ---
			//---              this is a temporary solution                 ---
			//-----------------------------------------------------------------
			if (model_thin && model_fat) 
			{	
				//overwrite the precalculated thin and fat version
				CModelMesh* pSkin_thin = model_thin->GetModelMesh(lod);
				CModelMesh* pSkin_fat	= model_fat->GetModelMesh(lod);
				if (pSkin_thin==0 || pSkin_fat==0)
				{
					g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,filename,	"Failed to Load Character file. The basemodel has thin and fat version, but the LODs don't." );
					assert (0);
					return false;
				}

				uint32 size_thin = pSkin_thin->m_arrIntVertices.size();
				uint32 size_fat  = pSkin_fat->m_arrIntVertices.size();
				if (numIntVertices!=size_thin ||  numIntVertices!=size_fat)
				{
					g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,filename,	"Failed to Load Character file. Thin, fat and basemodel have different vertex-count" );
					assert (0);
					return false;
				}

				//	assert(numIntVertices==size_thin);
				//	assert(numIntVertices==size_fat);
				for(uint32 i=0; i<numIntVertices; i++)
				{
					pModelMesh->m_arrIntVertices[i].wpos0=pSkin_thin->m_arrIntVertices[i].wpos1;
					pModelMesh->m_arrIntVertices[i].wpos2=pSkin_fat->m_arrIntVertices[i].wpos1;
				}
			}

			for (uint32 e=0; e<numExtVertices; e++ )
			{
				uint32 i=m_arrExtToIntMap[e];
				pMesh->m_pShapeDeformation[e].thin	= pModelMesh->m_arrIntVertices[i].wpos0;
				pMesh->m_pShapeDeformation[e].fat		= pModelMesh->m_arrIntVertices[i].wpos2;
				pMesh->m_pShapeDeformation[e].index	= pModelMesh->m_arrIntVertices[i].color;
			}


			IMaterial *pIMaterial = pModel->GetMaterial();	assert(pIMaterial);
			{
				//HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-HACK-
				//fix this in the exporter
				uint32 numSubsets = pMesh->m_subsets.size();
				for (uint32 i=0; i<numSubsets; i++)	
					pMesh->m_subsets[i].nNumVerts++;
			}


			//--------------------------------------------------------------------------
			//---        just for software-skinning! will be removed soon            ---
			//---          copy external faces (sorted by materials)                 ---
			//--------------------------------------------------------------------------
			uint numExtFaces = pMesh->m_nIndexCount/3;
			if (numExtFaces!=numIntFaces) 
			{
				g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,filename,	"external and internal face-amount is different. Probably caused by degenerated faces" );
				assert (!"Inconsistent face count!");
			}

			arrExtFaces.resize(numExtFaces);
			for (uint32 f=0; f<numExtFaces; f++)
			{
				arrExtFaces[f].i0 = pMesh->m_pIndices[f*3+0];
				arrExtFaces[f].i1 = pMesh->m_pIndices[f*3+1];
				arrExtFaces[f].i2 = pMesh->m_pIndices[f*3+2];
			}

			//------------------------------------------------------------------------
			//--  create external SkinBuffer and copy "splitted" internal vertices  --
			//------------------------------------------------------------------------
			ExtSkinVertex * arrExtVertices = new ExtSkinVertex[numExtVertices];//(ExtSkinVertex *)malloca(numExtVertices * sizeof(ExtSkinVertex));

			for (uint32 e=0; e<numExtVertices; e++)
			{
				arrExtVertices[e].wpos1		= pMesh->m_pPositions[e];
				arrExtVertices[e].u				=	pMesh->m_pTexCoord[e].s;
				arrExtVertices[e].v				=	pMesh->m_pTexCoord[e].t;

				arrExtVertices[e].wpos0		= pMesh->m_pShapeDeformation[e].thin;
				arrExtVertices[e].wpos2		= pMesh->m_pShapeDeformation[e].fat;
				arrExtVertices[e].color		=	pMesh->m_pShapeDeformation[e].index;

				arrExtVertices[e].binormal=	pMesh->m_pTangents[e].Binormal;
				arrExtVertices[e].tangent	=	pMesh->m_pTangents[e].Tangent;

				arrExtVertices[e].boneIDs	=	pMesh->m_pBoneMapping[e].boneIDs;	
				arrExtVertices[e].weights	=	pMesh->m_pBoneMapping[e].weights;	
			}

			std::set<int> usedBoneslist;
			std::vector<int> useRemap(pSkinningInfo->m_arrBonesDesc.size());

			// Indices in CollisionBBoxes in internal format!!!
			if (lod == baseLOD && !pSkinningInfo->m_bProperBBoxes/*&& pSkinningInfo->m_arrCollisions.empty()*/)
			{

				for (uint32 e=0; e<numExtVertices; e++)
				{
					uint32 i=m_arrExtToIntMap[e];

					uint8 idx0 = (uint8)pModelMesh->m_arrIntVertices[i].boneIDs[0];
					uint8 idx1 = (uint8)pModelMesh->m_arrIntVertices[i].boneIDs[1];
					uint8 idx2 = (uint8)pModelMesh->m_arrIntVertices[i].boneIDs[2];
					uint8 idx3 = (uint8)pModelMesh->m_arrIntVertices[i].boneIDs[3];

					usedBoneslist.insert(idx0);
					usedBoneslist.insert(idx1);
					usedBoneslist.insert(idx2);
					usedBoneslist.insert(idx3);
				}

				//if (pSkinningInfo->m_arrCollisions.empty()) 
				{
					int remap = 0;
					for (std::set<int>::iterator it = usedBoneslist.begin(), end = usedBoneslist.end(); it != end ; ++it, ++remap) 	{
						MeshCollisionInfo meshCollision;
						meshCollision.m_aABB = AABB(Vec3(VMAX),Vec3(VMIN));
						meshCollision.m_iBoneId = *it;
						pModel->m_arrCollisions.push_back(meshCollision);
						useRemap[*it] = remap;
					}
				}
			}

			//------------------------------------------------------------------------
			//---         manipulate buffer for spherical-skinning                  --
			//------------------------------------------------------------------------
			pModelMesh->m_arrExtBoneIDs.resize(numExtVertices);
			for (uint32 e=0; e<numExtVertices; e++)
			{
				uint32 i=m_arrExtToIntMap[e];

				uint8 idx0 = (uint8)pModelMesh->m_arrIntVertices[i].boneIDs[0];
				uint8 idx1 = (uint8)pModelMesh->m_arrIntVertices[i].boneIDs[1];
				uint8 idx2 = (uint8)pModelMesh->m_arrIntVertices[i].boneIDs[2];
				uint8 idx3 = (uint8)pModelMesh->m_arrIntVertices[i].boneIDs[3];
				pModelMesh->m_arrExtBoneIDs[e].idx0	= idx0;
				pModelMesh->m_arrExtBoneIDs[e].idx1	= idx1;
				pModelMesh->m_arrExtBoneIDs[e].idx2	= idx2;
				pModelMesh->m_arrExtBoneIDs[e].idx3	= idx3;
			}

			//--------------------------------------------------------------------------
			//---           copy morph-targets are just for LOD 0                    ---
			//--------------------------------------------------------------------------
			if (pSkinningInfo->m_arrMorphTargets.size() != 0)
			{
				Matrix33 z180(IDENTITY);

				if (! pSkinningInfo->m_bRotatedMorphTargets)
					z180.SetRotationZ(gf_PI);

				float fMidAxis = 0;
				// Calculate geometry vertical symetry line.
				for (uint32 e = 0; e < numExtVertices; e++)
				{
					fMidAxis += arrExtVertices[e].wpos0.x;
				}
				fMidAxis = fMidAxis / numExtVertices;

				//pModelMesh->m_arrMorphTargets = pSkinningInfo->m_arrMorphTargets;
				uint32 numMorphtargets = pSkinningInfo->m_arrMorphTargets.size();
				pModelMesh->m_morphTargets.resize(morphNameIndexMap.size()); // All lods should have the same morph layout.
				std::fill(pModelMesh->m_morphTargets.begin(), pModelMesh->m_morphTargets.end(), 0);
				for(uint32 i=0; i<numMorphtargets; i++) 
				{
					// Look up the name->index map to find out where this morph target should go in the list.
					VectorMap<string, int>::iterator itNameMapPosition = morphNameIndexMap.find(pSkinningInfo->m_arrMorphTargets[i]->m_strName);
					if (itNameMapPosition == morphNameIndexMap.end())
						continue; // If the morph target doesn't exist in the base LOD, then we don't use it.
					int morphIndex = (*itNameMapPosition).second;

					CMorphTarget *pMorphTarget = new CMorphTarget;
					pModelMesh->m_morphTargets[morphIndex] = pMorphTarget;
					pMorphTarget->m_MeshID = pSkinningInfo->m_arrMorphTargets[i]->MeshID;
					pMorphTarget->m_name = pSkinningInfo->m_arrMorphTargets[i]->m_strName;

					fMidAxis = 0;

					AABB bbox;
					bbox.Reset();
					int nVerts = (int)pSkinningInfo->m_arrMorphTargets[i]->m_arrExtMorph.size();
					assert(nVerts);
					pMorphTarget->m_vertices.resize(nVerts);
					for (int v = 0; v < nVerts; v++)
					{
						uint32 nVertexId = pSkinningInfo->m_arrMorphTargets[i]->m_arrExtMorph[v].nVertexId;
						pMorphTarget->m_vertices[v].nVertexId = nVertexId;
						pMorphTarget->m_vertices[v].m_MTVertex = z180*pSkinningInfo->m_arrMorphTargets[i]->m_arrExtMorph[v].ptVertex;

						// Calculate morph target bounding box.
						bbox.Add( arrExtVertices[nVertexId].wpos0 );
					}
					Vec3 bboxCenter = bbox.GetCenter();
					Vec3 bboxHalfSize = bbox.GetSize() * 0.5f;	

					for (int v = 0; v < nVerts; v++)
					{
						pMorphTarget->m_vertices[v].fBalanceLeft = 1;
						pMorphTarget->m_vertices[v].fBalanceRight = 1;
						assert( fabsf(bboxHalfSize.x)>0.001f );
						if (bboxHalfSize.x)
						{
							uint32 nVertexId = pMorphTarget->m_vertices[v].nVertexId;
							Vec3 point = arrExtVertices[nVertexId].wpos0;
							float balance = (point.x - bboxCenter.x) / bboxHalfSize.x;
							//pMorphTarget->m_vertices[v].fBalanceLeft = 1.0f - max( -balance,0.0f );
							//pMorphTarget->m_vertices[v].fBalanceLeft = balance;
							float fl = 1.0f - max(balance,0.0f);
							float fr = 1.0f - max(-balance,0.0f);
							fl = fl * fl * fl;
							fr = fr * fr * fr;
							pMorphTarget->m_vertices[v].fBalanceLeft = fl;
							pMorphTarget->m_vertices[v].fBalanceRight = fr;
						}
					}
				}
			}

			//////////////////////////////////////////////////////////////////////////

			// Create the RenderMesh
			if (lod >= baseLOD)
			{
				pModelMesh->m_numExtTriangles = pMesh->m_nIndexCount/3;

				pModel->m_pRenderMeshs[lod] =	g_pIRenderer->CreateRenderMesh( eBT_Static, "Character",pModel->GetModelFilePath() );
				assert(pModel->m_pRenderMeshs[lod]);

				pModel->m_pRenderMeshs[lod]->SetMaterial( pIMaterial );


				pModel->m_pRenderMeshs[lod]->SetMesh( *pMesh, 0, FSM_MORPH_TARGETS | FSM_CREATE_DEVICE_MESH, 0);

			}
			else
				pModelMesh->m_numExtTriangles = 0;


			//check which joints are used for skinning of the mesh
			//CModelMesh* pModelMesh = pInstance->m_pModel->GetModelMesh(0);
			if (pModelMesh)
			{
				//				uint32 numExtVertices = arrExtVertices.size();
				for (uint32 e=0; e<numExtVertices; e++)
				{
					uint16 idx0=pModelMesh->m_arrExtBoneIDs[e].idx0;
					uint16 idx1=pModelMesh->m_arrExtBoneIDs[e].idx1;
					uint16 idx2=pModelMesh->m_arrExtBoneIDs[e].idx2;
					uint16 idx3=pModelMesh->m_arrExtBoneIDs[e].idx3;
					pModel->m_pModelSkeleton->m_arrModelJoints[idx0].m_numVLinks++;
					pModel->m_pModelSkeleton->m_arrModelJoints[idx1].m_numVLinks++;
					pModel->m_pModelSkeleton->m_arrModelJoints[idx2].m_numVLinks++;
					pModel->m_pModelSkeleton->m_arrModelJoints[idx3].m_numVLinks++;
				}
			}

			if (lod == baseLOD) {
				if(!pSkinningInfo->m_bProperBBoxes)
				{
					//if (!pSkinningInfo->m_arrCollisions.empty())
					//	pModel->m_arrCollisions = pSkinningInfo->m_arrCollisions;

					for (int i = 0; i < pModel->m_arrCollisions.size(); ++i)
					{
						std::set<int> unique;

						for (int j = 0; j < pModel->m_arrCollisions[i].m_arrIndexes.size(); ++j)
						{
							unique.insert(pModel->m_arrCollisions[i].m_arrIndexes[j]);
						}

						DynArray<short int> tmp;
						tmp.swap(pModel->m_arrCollisions[i].m_arrIndexes);
						pModel->m_arrCollisions[i].m_arrIndexes.reserve(unique.size());

						for (uint32 j = 0; j < arrExtFaces.size(); ++j)
						{
							if ((unique.find(arrExtFaces[j].i0) != unique.end()) || (unique.find(arrExtFaces[j].i1) != unique.end()) || (unique.find(arrExtFaces[j].i2) != unique.end()))
							{
								pModel->m_arrCollisions[i].m_arrIndexes.push_back(j);
							}
						}
					}
					// we don't need base data
					DynArray<short int> tmp;
					tmp.swap(pModel->m_arrCollisions[0].m_arrIndexes);
				}
				else
				{
					pModel->m_arrCollisions = pSkinningInfo->m_arrCollisions;
				}
			}

			delete [] arrExtVertices;
		}//if (pCGF[lod])

		//---------------------------------------------------------------------------------

	} //loop over all LODs

	//---------------------------------------------------------------------------------------------------------------

	for (uint32 lod=0; lod<nLODs; lod++) 
		g_pI3DEngine->ReleaseChunkFileContent( pCGF[lod] );


	pModel->m_pModelSkeleton->SetIKJointID();













	//---------------------------------------------------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	//use vertices to calculate AABB
	CModelMesh* pModelMesh = pModel->GetModelMesh(baseLOD);
	//	uint32 numExtVertices = ;//arrExtVertices.size();
	pModel->m_ModelAABB=AABB(Vec3(VMAX),Vec3(VMIN));

	//for(uint32 i=0; i<numExtVertices; i++)
	//	pModel->m_ModelAABB.Add(arrExtVertices[i].wpos1);
	pModel->m_ModelAABB = pModel->m_arrCollisions[0].m_aABB;//


	//////////////////////////////////////////////////////////////////////////

	pModel->PostLoad();

	//---------------------------------------------------------------------------------------------------------------

	/*	uint32 match = strGeomFileName=="objects/characters/human/squad/locomanmodel2/rifleman_light.chr";
	if (match)
	{
	uint32 i=0;
	}*/


	const char* RootName = pModel->m_pModelSkeleton->m_arrModelJoints[0].GetJointName();
	uint32 IsBiped=0;
	if (RootName[0]=='B' && RootName[1]=='i' && RootName[2]=='p' && RootName[3]=='0' && RootName[4]=='1')
		IsBiped=1;

	if (g_pCharacterManager->m_IsDedicatedServer==0 || IsBiped==0)
	{
		string m_strCalFileName = m_strGeomFileNameNoExt + ".cal";
		uint32 l=loadAnimations(m_strCalFileName,pModel);
		if (l==0)	
			return NULL;
		else
		{
			pModel->m_AnimationSet.ComputeSelectionProperties();
			pModel->m_AnimationSet.VerifyLocomotionGroups();
		}

		int32 numGlobalAnims = g_AnimationManager.m_arrGlobalAnimations.size();
		assert(numGlobalAnims);
	}

	// If there is a facial model, but no expression library, we should create an empty library for it.
	// When we assign the library to the facial model it will automatically be assigned the morphs as expressions.
	{
		CFacialModel* pFacialModel = (pModel ? pModel->GetFacialModel() : 0);
		CFacialEffectorsLibrary* pEffectorsLibrary = (pFacialModel ? pFacialModel->GetFacialEffectorsLibrary() : 0);
		if (pFacialModel && !pEffectorsLibrary)
		{
			CFacialAnimation* pFacialAnimationManager = (g_pCharacterManager ? g_pCharacterManager->GetFacialAnimation() : 0);
			CFacialEffectorsLibrary* pNewLibrary = new CFacialEffectorsLibrary(pFacialAnimationManager);
			const string& filePath = pModel->GetFilePath();
			if (!filePath.empty())
			{
				string libraryFilePath = PathUtil::ReplaceExtension(filePath, ".fxl");
				pNewLibrary->SetName(libraryFilePath);
			}
			if (pFacialModel && pNewLibrary)
				pFacialModel->AssignLibrary(pNewLibrary);
		}
	}

	return pModel;
}

















// loads animations for already loaded model
bool CryCHRLoader::loadAnimations( string m_strCalFileName, CCharacterModel* pModel )
{
	FUNCTION_PROFILER( gEnv->pSystem, PROFILE_ANIMATION );

	// the number of animations loaded
	uint32 numAnimAssets = 0;

	m_arrCalFiles.resize(0);
	//	m_arrCalFiles.push_back( m_strCalFileName );

	// Add a null animation to the array.
	m_arrAnimFiles.push_back( new SAnimFile(string(NULL_ANIM_FILE "/") + pModel->GetNameCStr(), "null", 0));

	ParseCALFile(m_strCalFileName,pModel);
	uint32 numCALs = m_arrCalFiles.size();
	for (uint32 i=0; i<numCALs; i++)
		CollectAnimationsInCAL(m_arrCalFiles[i],pModel);

	numAnimAssets = loadAnimationArray( pModel, m_arrAnimFiles, m_arrWildcardAnimFiles );

	// Store the list of facial animations in the model animation set.
	pModel->m_AnimationSet.GetFacialAnimations().SwapElementsWithVector(m_arrFacialAnimations);

	AnimEventLoader::loadAnimationEventDatabase(pModel, pModel->GetModelAnimEventDatabaseCStr());
	/*
	{
	//AUTO_PROFILE_SECTION(g_dTimeTest2);
	CModelMesh* pModelMesh = pModel->GetModelMesh(0);
	if (numAnimAssets==0 && !pModelMesh->m_morphTargets.size())
	{
	//AnimWarning("CryAnimation:: couldn't find any animations for the model. Standalone character will be useless.", m_strGeomFileNameNoExt.c_str());
	//return false;
	}
	}
	*/

	if (numAnimAssets>1)
		g_pILog->UpdateLoadingScreen("  %d animation-assets loaded (total assets: %d)", numAnimAssets, g_AnimationManager.NumGlobalAnimations());

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("Memstat %i", (int)(info.allocated - info.freed));
	}

	CModelMesh* pModelMesh = pModel->GetModelMesh(0);
	return  pModel->m_pModelSkeleton->m_arrModelJoints.size() || pModelMesh->m_morphTargets.size();
}


//////////////////////////////////////////////////////////////////////////
// Load a null animation for the model.
int32 CryCHRLoader::loadNullAnimation(const char* pFilePath, int nAnimID, const char* pAnimName, uint32 nAnimFlags, CCharacterModel* pModel)
{
	int32 res = pModel->m_AnimationSet.LoadCAF( pFilePath, 0.01f, nAnimID, pAnimName, nAnimFlags);

	return res;
}


//////////////////////////////////////////////////////////////////////////
// loads animations for this chr from the given cal file
// does NOT close the file (the file belongs to the calling party)
// PARAMETERS
//	 m_strGeomFileNameNoExt - the name of the cgf/cid file without extension
//   m_fCalFile             - the file opened by fopen() for the cal associated with this cgf
//   m_fScale               - the scale factor to be applied to all controllers
void CryCHRLoader::ParseCALFile (  string m_strCalFileName, CCharacterModel* pModel )
{


	for (uint32 i = 0, end = m_arrCalFiles.size(); i < end; ++i)
		if (m_arrCalFiles[i].compareNoCase(m_strCalFileName) == 0)
			return;

	FILE* fCalFile = g_pIPak->FOpen(m_strCalFileName.c_str(), "r",ICryPak::FOPEN_HINT_QUIET);
	if (fCalFile==0)
		return;

	const char* pFilePath = GetFilePath(fCalFile); 
	std::vector<string> arrCalFiles;

	// Load cal file and load animations from animations folder
	// make anim folder name
	// make search path
	string strDirName = PathUtil::GetParentDirectory(m_strGeomFileNameNoExt).c_str();
	string strAnimDirName = PathUtil::GetParentDirectory(strDirName, 2);
	if (!strAnimDirName.empty())
		strAnimDirName += "/animations";
	else
		strAnimDirName += "animations";

	// the flags applicable to the currently being loaded animation
	unsigned nAnimFlags = 0;

	if (pFilePath) 
	{
		strAnimDirName = PathUtil::ToUnixPath(pFilePath);

		while (strAnimDirName[strAnimDirName.length() - 1] == '/')
		{
			strAnimDirName.erase(strAnimDirName.length() - 1, 1);
		}
	}

	for (int i = 0; fCalFile && !g_pIPak->FEof(fCalFile); ++i)
	{
		char sBuffer[512]="";
		g_pIPak->FGets(sBuffer,512,fCalFile);
		char*szAnimName;
		char*szFileName;

		if (sBuffer && strstr(sBuffer, "on-demand")!=0)
		{
			nAnimFlags = LOAD_ON_DEMAND; // :)
		}

		if(sBuffer[0] == '/' || sBuffer[0]=='\r' || sBuffer[0]=='\n' || sBuffer[0]==0)
			continue;

		//if(sscanf(sBuffer, "%s=%s", szAnimName, szFileName) != 2)
		//	continue;
		szAnimName = strtok (sBuffer, " \t\n\r=");
		if (!szAnimName)
			continue;

		bool IsFilePath = (0 == stricmp(szAnimName,"#filepath"));
		if (IsFilePath) 
			continue;

		//if (strstr(szAnimName, "on-demand")!=0)
		//{
		//	nAnimFlags = LOAD_ON_DEMAND; // :)
		//}

		szFileName = strtok(NULL, " \t\n\r=");
		if (!szFileName || szFileName[0] == '?')
		{
			assert(0);
			continue;
		}

		if (szAnimName[0] == '/' && szAnimName[1] == '/')
			continue; // comment

		{ // skip leading '\' and replace all '\' with '/'
			while(szFileName[0]=='/' || szFileName[0]=='\\')
				++szFileName;

			for(char * p = szFileName+strlen(szFileName); p>=szFileName; p--)
				if(*p == '\\')
					*p = '/';
		}

		// process the possible directives
		if (szAnimName[0] == '$')
		{
			const char* szDirective = szAnimName + 1;

			if ( !stricmp(szDirective,"AnimationDir") || !stricmp(szDirective,"AnimDir")	|| !stricmp(szDirective,"AnimationDirectory") ||!stricmp(szDirective,"AnimDirectory") )
			{
				strAnimDirName = strDirName + "/" + szFileName;
				strAnimDirName.TrimRight('/'); // delete the trailing slashes
			}
			else if (!stricmp(szDirective, "AnimEventDatabase"))
			{
				//if (pModel->m_strAnimEventFilePath != "" && pModel->m_strAnimEventFilePath.compareNoCase(szFileName))
				//	g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,m_strCalFileName.c_str(),	"Duplicated AnimEventDatabase");

				pModel->SetModelAnimEventDatabase(szFileName);
			}
			else if (!stricmp(szDirective, "TracksDatabase"))
			{
				//if (pModel->m_strTracksDBFilePath != "" && pModel->m_strTracksDBFilePath.compareNoCase(szFileName))
				//	g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,m_strCalFileName.c_str(),	"Duplicated TracksDatabase");


				pModel->AddModelTracksDatabase(szFileName);
			}
			else if (!stricmp(szDirective, "Include"))
			{

				string calfile	=	szFileName;
				arrCalFiles.push_back( calfile );
			}
			else if (!stricmp(szDirective, "FaceLib"))
			{
				// Facial expressions library.
				_smart_ptr<IFacialEffectorsLibrary> pLib;
				//if (szFileName)

				string sFaceLibFile = szFileName;
				pLib = g_pCharacterManager->GetIFacialAnimation()->LoadEffectorsLibrary( sFaceLibFile );
				if (!pLib)
				{
					// Search in animation directory .chr file directory.
					sFaceLibFile = PathUtil::Make( strAnimDirName,szFileName );
					pLib = g_pCharacterManager->GetIFacialAnimation()->LoadEffectorsLibrary( sFaceLibFile );
				}
				if (!pLib)
				{
					// Search in .chr file directory.
					sFaceLibFile = PathUtil::Make( strDirName,szFileName );
					pLib = g_pCharacterManager->GetIFacialAnimation()->LoadEffectorsLibrary( sFaceLibFile );
				}
				if (pLib)
				{
					if (pModel->GetFacialModel())
					{
						pModel->GetFacialModel()->AssignLibrary( pLib );
					}
				}
				else
				{
					AnimWarning("Facial Effector Library %s not found (when loading %s.cal)",sFaceLibFile.c_str(),m_strGeomFileNameNoExt.c_str());
				}
			}
			else
				AnimWarning("Unknown directive '%s' in '%s' - fix .cal file", szDirective,pModel->GetModelFilePath());
			continue;
		}
	}

	g_pIPak->FClose (fCalFile);

	m_arrCalFiles.push_back(m_strCalFileName);

	for( uint32 i = 0 , end = arrCalFiles.size(); i < end; ++i)
	{
		ParseCALFile(arrCalFiles[i], pModel);
	}
}




//////////////////////////////////////////////////////////////////////////
// loads animations for this chr from the given cal file
// does NOT close the file (the file belongs to the calling party)
// PARAMETERS
//	 m_strGeomFileNameNoExt - the name of the cgf/cid file without extension
//   m_fCalFile             - the file opened by fopen() for the cal associated with this cgf
//   m_fScale               - the scale factor to be applied to all controllers
void CryCHRLoader::CollectAnimationsInCAL ( string m_strCalFileName, CCharacterModel* pModel  )
{
	// Load cal file and load animations from animations folder
	// make anim folder name
	// make search path
	FILE* fCalFile = g_pIPak->FOpen(m_strCalFileName.c_str(), "r");
	if (fCalFile==0)
		return;

	const char* pFilePath = GetFilePath(fCalFile); 

	string strDirName = PathUtil::GetParentDirectory(m_strGeomFileNameNoExt).c_str();
	string strAnimDirName = PathUtil::GetParentDirectory(strDirName, 2);
	if (!strAnimDirName.empty())
		strAnimDirName += "/animations";
	else
		strAnimDirName += "animations";

	// the flags applicable to the currently being loaded animation
	unsigned nAnimFlags = 0;

	if (pFilePath) 
	{
		strAnimDirName = PathUtil::ToUnixPath(pFilePath);
		while (strAnimDirName[strAnimDirName.length() - 1] == '/')
		{
			strAnimDirName.erase(strAnimDirName.length() - 1, 1);
		}

	}

	for (int i = 0; fCalFile && !g_pIPak->FEof(fCalFile); ++i)
	{
		char sBuffer[512]="";
		g_pIPak->FGets(sBuffer,512,fCalFile);

		char*szAnimName;
		char*szFileName;

		if (sBuffer && strstr(sBuffer, "on-demand")!=0)
		{
			nAnimFlags = LOAD_ON_DEMAND; // :)
		}


		if(sBuffer[0] == '/' || sBuffer[0]=='\r' || sBuffer[0]=='\n' || sBuffer[0]==0)
			continue;

		//if(sscanf(sBuffer, "%s=%s", szAnimName, szFileName) != 2)
		//	continue;
		szAnimName = strtok (sBuffer, " \t\n\r=");
		if (!szAnimName)
			continue;



		bool IsFilePath = (0 == stricmp(szAnimName,"#filepath"));
		if (IsFilePath) 
			continue;

		szFileName = strtok(NULL, " \t\n\r=");
		if (!szFileName || szFileName[0] == '?')
		{
			assert(0);
			continue;
		}



		if (szAnimName[0] == '/' && szAnimName[1] == '/')
			continue; // comment

		{ // skip leading '\' and replace all '\' with '/'
			while(szFileName[0]=='/' || szFileName[0]=='\\')
				++szFileName;

			for(char * p = szFileName+strlen(szFileName); p>=szFileName; p--)
				if(*p == '\\')
					*p = '/';
		}

		// process the possible directives
		if (szAnimName[0] == '$')
		{
			const char* szDirective = szAnimName + 1;

			if (!stricmp(szDirective, "AnimEventDatabase"))
			{
				//if (pModel->m_strAnimEventFilePath != "" && pModel->m_strAnimEventFilePath.compareNoCase(szFileName))
				//	g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,m_strCalFileName.c_str(),	"Duplicated AnimEventDatabase");

				pModel->SetModelAnimEventDatabase(szFileName);
			}
			else if (!stricmp(szDirective, "TracksDatabase"))
			{
				//if (pModel->m_strTracksDBFilePath != "" && pModel->m_strTracksDBFilePath.compareNoCase(szFileName))
				//	g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING,	VALIDATOR_FLAG_FILE,m_strCalFileName.c_str(),	"Duplicated TracksDatabase");

				pModel->AddModelTracksDatabase(szFileName);
			}
			else if (!stricmp(szDirective, "Include"))
			{
				string calfile	=	szFileName;
				m_arrCalFiles.push_back( calfile );
			}
		}

		// is there any wildcard in the file name?
		if ( strchr(szFileName,'*') != NULL || strchr(szFileName,'?') != NULL )
		{
			m_arrWildcardAnimFiles.push_back( new SAnimFile(strAnimDirName + "/" + szFileName, szAnimName, nAnimFlags) );
		}
		else
		{
			// Check whether the filename is a facial animation, by checking the extension.
			const char* szExtension = PathUtil::GetExt(szFileName);
			if (szExtension && stricmp("fsq", szExtension) == 0)
				m_arrFacialAnimations.push_back(CAnimationSet::FacialAnimationEntry(szAnimName, strAnimDirName+"/"+szFileName));
			else
				m_arrAnimFiles.push_back( new SAnimFile(strAnimDirName+"/"+szFileName, szAnimName, nAnimFlags) );
		}
	}

	g_pIPak->FClose (fCalFile);

}


char sBuffer[512]="";

const char* CryCHRLoader::GetFilePath( FILE* fCalFile ) 
{
	char*szFileName = 0;

	if (fCalFile) 
	{
		g_pIPak->FGets(sBuffer,512,fCalFile);
		/*
		for (uint32 i=0; i<512; i++) 
		{
		if (sBuffer[i]=='#') 
		{	
		const char* p=&sBuffer[i];
		if ( p[1]=='f' && p[2]=='i' && p[3]=='l' && p[4]=='e' && p[5]=='p' && p[6]=='a' && p[7]=='t' && p[8]=='h') 		
		{
		const char* q=&p[i+9];
		for (uint32 f=0; f<256; f++) 
		{
		if (q[f]=='=') 
		{	
		const char* h=&q[f+1];
		for (uint32 w=0; w<256; w++) 
		{
		if (h[w]!=0x20) 
		{	
		char* name=(char*)&h[w];
		//find end of name
		for (uint32 j=0; j<256; j++) 
		{
		if (name[j]==0x20 || name[j]==0x0d || name[j]==0x0a) 
		{
		g_pIPak->FSeek(fCalFile,0,SEEK_SET);
		name[j]=0;
		return &h[w];
		}
		}
		}
		}
		}
		}
		}
		}
		}
		*/

		char*szAnimName;

		szAnimName = strtok (sBuffer, " \t\n\r=");
		if (szAnimName)
		{
			bool IsFilePath = (0 == stricmp(szAnimName,"#filepath"));
			if (IsFilePath) 
			{
				szFileName = strtok(NULL, " \t\n\r=");
				if (szFileName[0] == '?')
				{
					szFileName = 0;
				}
			}
		}			
		g_pIPak->FSeek(fCalFile,0,SEEK_SET);
	}

	return szFileName;
}

struct FindByAliasName
{
	const string& _name;
	FindByAliasName(const string& name) : _name(name) {}
	bool operator () (const SAnimFile* animFile)
	{
		return _name.compareNoCase(animFile->strAnimName) == 0;
	}
	bool operator () (const CAnimationSet::FacialAnimationEntry& facialAnimEntry)
	{
		return _name.compareNoCase(facialAnimEntry.name) == 0;
	}
};

#include "LoaderDBA.h"

// loads the animations from the array: pre-allocates the necessary controller arrays
// the 0th animation is the default animation
uint32 CryCHRLoader::loadAnimationArray( CCharacterModel* pModel, TAnimFilesVec& arrAnimFiles, const TAnimFilesVec& arrWildcardAnimFiles )
{
	uint32 nAnimID = 0;
	uint32 numAnims = arrAnimFiles.size();
	if (numAnims==0)
		return 0;

	int nNullLen = strlen(NULL_ANIM_FILE);

	//	CLoaderDBA * pLoader = 0;
	std::vector< CLoaderDBA* > dbaLoaders;
	if (Console::GetInst().ca_LoadDatabase > 0 && pModel->m_arrTracksDBFilePath.size())
	{
		// Loading or retrieving 
		g_AnimationManager.LoadDBA(pModel->m_arrTracksDBFilePath, &dbaLoaders);
	}

	// expand wildcards
	for ( uint32 i = 0; i < m_arrWildcardAnimFiles.size(); ++i )
	{
		const char* szFileName = m_arrWildcardAnimFiles[i]->strFileName.c_str();
		const char* szFile = PathUtil::GetFile(szFileName);
		const char* szExt = PathUtil::GetExt(szFileName);

		char szAnimName[512]; strcpy( szAnimName, m_arrWildcardAnimFiles[i]->strAnimName.c_str() );
		uint32 nAnimFlags = m_arrWildcardAnimFiles[i]->nAnimFlags;

		const char* firstWildcard = strchr(szFileName,'*');
		if ( const char* qm = strchr(szFileName,'?') )
			if ( firstWildcard == NULL || firstWildcard > qm )
				firstWildcard = qm;
		int iFirstWildcard = firstWildcard - szFileName;
		int iPathLength = szFile - szFileName;

		// conversion to offset from beginning of name
		int offset = firstWildcard - szFile;
		if ( offset < 0 )
		{
			AnimWarning("Wildcards in animation file path are not supported - ignoring .CAL entry \"%s = %s\"", m_arrWildcardAnimFiles[i]->strAnimName.c_str(), szFileName );
			offset = 0;
		}

		string filepath = PathUtil::GetParentDirectory(szFileName);
		char* starPos = strchr( szAnimName, '*' );
		if ( starPos )
			*starPos++ = 0;

		// insert all animations found in the file system
		_finddata_t fd;
		intptr_t handle = g_pIPak->FindFirst( szFileName, &fd, ICryPak::FLAGS_NO_LOWCASE );
		if ( handle != -1 )
		{
			do
			{
				string name = starPos == NULL ? szAnimName : string(szAnimName) + PathUtil::GetFileName(fd.name+offset) + starPos;
				FindByAliasName pd(name);
				
				// Check whether the filename is a facial animation, by checking the extension.
				const char* szExtension = PathUtil::GetExt(fd.name);
				if ( szExtension && stricmp("fsq", szExtension) == 0 )
				{
					// insert unique
					if ( std::find_if(m_arrFacialAnimations.begin(), m_arrFacialAnimations.end(), pd) == m_arrFacialAnimations.end() )
						m_arrFacialAnimations.push_back( CAnimationSet::FacialAnimationEntry(name.c_str(), filepath+"/"+fd.name) );
				}
				else
				{
					// insert unique
					if ( std::find_if(arrAnimFiles.begin(), arrAnimFiles.end(), pd) == arrAnimFiles.end() )
						arrAnimFiles.push_back( new SAnimFile(filepath+"/"+fd.name, name.c_str(), nAnimFlags) );
				}
			} while ( g_pIPak->FindNext( handle, &fd ) >= 0 );

			g_pIPak->FindClose( handle );
		}

		// insert all animations found in the DBA files
		for ( uint32 j = 0; j < dbaLoaders.size(); ++j )
		{
			CLoaderDBA* pLoader = dbaLoaders[j];
			TNamesVector::iterator it, itEnd = pLoader->m_pDatabaseInfo->m_AnimationNames.end();
			for ( it = pLoader->m_pDatabaseInfo->m_AnimationNames.begin(); it != itEnd; ++it )
			{
				const char* currentFile = *it;
				const char* file = PathUtil::GetFile( currentFile );
				const char* ext = PathUtil::GetExt( currentFile );
				if ( strnicmp(szFileName, currentFile, iPathLength) == 0 && PathUtil::MatchWildcard(file, szFile) )
				{
					string name = starPos == NULL ? szAnimName : string(szAnimName) + string(currentFile+iFirstWildcard, ext-currentFile-1-iFirstWildcard) + starPos;
					FindByAliasName pd(name);

					// Check whether the filename is a facial animation, by checking the extension.
					if ( ext && stricmp("fsq", ext) == 0 )
					{
						// insert unique
						if ( std::find_if(m_arrFacialAnimations.begin(), m_arrFacialAnimations.end(), pd) == m_arrFacialAnimations.end() )
							m_arrFacialAnimations.push_back( CAnimationSet::FacialAnimationEntry(name.c_str(), currentFile) );
					}
					else
					{
						// insert unique
						if ( std::find_if(arrAnimFiles.begin(), arrAnimFiles.end(), pd) == arrAnimFiles.end() )
							arrAnimFiles.push_back( new SAnimFile(currentFile, name.c_str(), nAnimFlags) );
					}
				}
			}
		}
	}

	numAnims = arrAnimFiles.size();
	pModel->m_AnimationSet.prepareLoadCAFs ( numAnims );
	for (uint32 i=0; i<numAnims; i++)
	{
		const char* pFilePath = arrAnimFiles[i]->strFileName.c_str();
		const char* pAnimName = arrAnimFiles[i]->strAnimName.c_str();

		const char* fileExt = PathUtil::GetExt(pFilePath);
		bool IsNull = (strncmp(pFilePath, NULL_ANIM_FILE, nNullLen) == 0);
		bool IsCAF = (0 == stricmp(fileExt,"caf"));
		bool IsLMG = (0 == stricmp(fileExt,"lmg"));
		bool IsCPM = (0 == stricmp(fileExt,"cpm"));

		int32 rel=0;
		if (IsNull)
			rel = loadNullAnimation(pFilePath, nAnimID, pAnimName, arrAnimFiles[i]->nAnimFlags, pModel);
		else if (IsCAF)
			rel = pModel->m_AnimationSet.LoadCAF( pFilePath, 0.01f, nAnimID, pAnimName, arrAnimFiles[i]->nAnimFlags);
		else if (IsLMG)
			rel = pModel->m_AnimationSet.LoadLMG( pFilePath, 0.01f, nAnimID, pAnimName, arrAnimFiles[i]->nAnimFlags );

		if (rel >= 0)
			nAnimID++;
		else
			AnimWarning("Animation (caf) file \"%s\" could not be read (it's an animation of \"%s.%s\")", arrAnimFiles[i]->strFileName.c_str(), m_strGeomFileNameNoExt.c_str(),CRY_CHARACTER_FILE_EXT );
	}
	return nAnimID;
}




// cleans up the resources allocated during load
void CryCHRLoader::clear()
{
	m_strGeomFileNameNoExt = "";

	// delete all structures
	for (uint32 i = 0, size = m_arrAnimFiles.size(); i < size; ++i)
	{
		delete m_arrAnimFiles[i];
	}
	m_arrAnimFiles.clear();

	for (uint32 i = 0, size = m_arrWildcardAnimFiles.size(); i < size; ++i)
	{
		delete m_arrWildcardAnimFiles[i];
	}
	m_arrWildcardAnimFiles.clear();

	m_arrCalFiles.clear();

	CAnimationSet::FacialAnimationSet::container_type().swap(m_arrFacialAnimations);
}


//----------------------------------------------------------------------------
void CryCHRLoader::VertexCleanup(CCharacterModel* pModel,CModelMesh* pModelMesh, uint32 i, uint32 numExtVertices,DynArray<uint16>& m_arrExtToIntMap, ExtSkinVertex * arrExtVertices) //std::vector<ExtSkinVertex>& arrExtVertices)
{

	uint16 qidx0 = pModelMesh->m_arrIntVertices[i].boneIDs[0];
	uint16 qidx1 = pModelMesh->m_arrIntVertices[i].boneIDs[1];
	uint16 qidx2 = pModelMesh->m_arrIntVertices[i].boneIDs[2];
	uint16 qidx3 = pModelMesh->m_arrIntVertices[i].boneIDs[3];
	uint16 qlevel0=pModel->m_pModelSkeleton->m_arrModelJoints[qidx0].m_numLevels;
	uint16 qlevel1=pModel->m_pModelSkeleton->m_arrModelJoints[qidx1].m_numLevels;
	uint16 qlevel2=pModel->m_pModelSkeleton->m_arrModelJoints[qidx2].m_numLevels;
	uint16 qlevel3=pModel->m_pModelSkeleton->m_arrModelJoints[qidx3].m_numLevels;

	/*
	pModelMesh->m_arrIntVertices[i].weights[1]=0;
	pModelMesh->m_arrIntVertices[i].weights[2]=0;
	pModelMesh->m_arrIntVertices[i].weights[3]=0;
	pModelMesh->m_arrIntVertices[i].boneIDs[1]=0;
	pModelMesh->m_arrIntVertices[i].boneIDs[2]=0;
	pModelMesh->m_arrIntVertices[i].boneIDs[3]=0;
	for (uint32 e=0; e<numExtVertices; e++)
	{
	uint32 icount=m_arrExtToIntMap[e];
	if (icount==i)
	{
	pModelMesh->m_arrExtVertices[e].boneIDs[1] = 0;
	pModelMesh->m_arrExtVertices[e].boneIDs[2] = 0;
	pModelMesh->m_arrExtVertices[e].boneIDs[3] = 0;
	pModelMesh->m_arrExtVertices[e].weights[1] = 0;
	pModelMesh->m_arrExtVertices[e].weights[2] = 0;
	pModelMesh->m_arrExtVertices[e].weights[3] = 0;
	}
	}*/


	uint32 numBonesPerVertex=0;
	numBonesPerVertex += pModelMesh->m_arrIntVertices[i].weights[0]!=0;
	numBonesPerVertex += pModelMesh->m_arrIntVertices[i].weights[1]!=0;
	numBonesPerVertex += pModelMesh->m_arrIntVertices[i].weights[2]!=0;
	numBonesPerVertex += pModelMesh->m_arrIntVertices[i].weights[3]!=0;

	if (numBonesPerVertex>1)
	{

		int32 qmaxdist=qlevel0-qlevel1;
		if (qmaxdist<0)
			qmaxdist = -qmaxdist;

		//check if both bones are on the same level or more then one level apart
		if (qmaxdist==0 || qmaxdist>1)
		{
			//yes, then keep the most important, delete the other one
			pModelMesh->m_arrIntVertices[i].boneIDs[1] = pModelMesh->m_arrIntVertices[i].boneIDs[2];
			pModelMesh->m_arrIntVertices[i].boneIDs[2] = pModelMesh->m_arrIntVertices[i].boneIDs[3];
			pModelMesh->m_arrIntVertices[i].boneIDs[3] = 0;
			pModelMesh->m_arrIntVertices[i].weights[1] = pModelMesh->m_arrIntVertices[i].weights[2];
			pModelMesh->m_arrIntVertices[i].weights[2] = pModelMesh->m_arrIntVertices[i].weights[3];
			pModelMesh->m_arrIntVertices[i].weights[3] = 0;

			for (uint32 e=0; e<numExtVertices; e++)
			{
				uint32 icount=m_arrExtToIntMap[e];
				if (icount==i)
				{
					arrExtVertices[e].boneIDs[1] = arrExtVertices[e].boneIDs[2];
					arrExtVertices[e].boneIDs[2] = arrExtVertices[e].boneIDs[3];
					arrExtVertices[e].boneIDs[3] = 0;
					arrExtVertices[e].weights[1] = arrExtVertices[e].weights[2];
					arrExtVertices[e].weights[2] = arrExtVertices[e].weights[3];
					arrExtVertices[e].weights[3] = 0;
				}
			}
			return;
		}
	}


	if (numBonesPerVertex>2)
	{
		int32 qmaxdist=qlevel1-qlevel2;
		if (qmaxdist<0)
			qmaxdist = -qmaxdist;

		//check if both bones are on the same level
		if (qmaxdist==0 || qmaxdist>1)
		{
			//yes, then keep the most important, delete the other
			pModelMesh->m_arrIntVertices[i].boneIDs[2] = pModelMesh->m_arrIntVertices[i].boneIDs[3];
			pModelMesh->m_arrIntVertices[i].boneIDs[3] = 0;
			pModelMesh->m_arrIntVertices[i].weights[2] = pModelMesh->m_arrIntVertices[i].weights[3];
			pModelMesh->m_arrIntVertices[i].weights[3] = 0;
			for (uint32 e=0; e<numExtVertices; e++)
			{
				uint32 icount=m_arrExtToIntMap[e];
				if (icount==i)
				{
					arrExtVertices[e].boneIDs[2] = arrExtVertices[e].boneIDs[3];
					arrExtVertices[e].boneIDs[3] = 0;
					arrExtVertices[e].weights[2] = arrExtVertices[e].weights[3];
					arrExtVertices[e].weights[3] = 0;
				}
			}
			return;
		}



		qmaxdist=qlevel0-qlevel2;
		if (qmaxdist<0)
			qmaxdist = -qmaxdist;

		//check if both bones are on the same level
		if (qmaxdist==0 || qmaxdist>1)
		{
			//yes, then keep the most important, delete the other
			pModelMesh->m_arrIntVertices[i].weights[2] = pModelMesh->m_arrIntVertices[i].weights[3];
			pModelMesh->m_arrIntVertices[i].weights[3] = 0;
			pModelMesh->m_arrIntVertices[i].boneIDs[2] = pModelMesh->m_arrIntVertices[i].boneIDs[3];
			pModelMesh->m_arrIntVertices[i].boneIDs[3] = 0;
			for (uint32 e=0; e<numExtVertices; e++)
			{
				uint32 icount=m_arrExtToIntMap[e];
				if (icount==i)
				{
					arrExtVertices[e].boneIDs[2] = arrExtVertices[e].boneIDs[3];
					arrExtVertices[e].boneIDs[3] = 0;
					arrExtVertices[e].weights[2] = arrExtVertices[e].weights[3];
					arrExtVertices[e].weights[3] = 0;
				}
			}
			return;
		}

	}

	return;
}


void CryCHRLoader::ChildParentRelation(CCharacterModel* pModel,CModelMesh* pModelMesh, uint32 i, uint32 numExtVertices,DynArray<uint16>& m_arrExtToIntMap,ExtSkinVertex * arrExtVertices) // std::vector<ExtSkinVertex>& arrExtVertices)
{
	uint32 numBonesPerVertex=0;
	numBonesPerVertex += pModelMesh->m_arrIntVertices[i].weights[0]!=0;
	numBonesPerVertex += pModelMesh->m_arrIntVertices[i].weights[1]!=0;
	numBonesPerVertex += pModelMesh->m_arrIntVertices[i].weights[2]!=0;
	numBonesPerVertex += pModelMesh->m_arrIntVertices[i].weights[3]!=0;

	if (numBonesPerVertex==2)
	{
		uint16 qidx0 = pModelMesh->m_arrIntVertices[i].boneIDs[0];
		uint16 qidx1 = pModelMesh->m_arrIntVertices[i].boneIDs[1];

		uint16 pidx0=pModel->m_pModelSkeleton->m_arrModelJoints[qidx0].m_idxParent;
		uint16 pidx1=pModel->m_pModelSkeleton->m_arrModelJoints[qidx1].m_idxParent;

		uint32 IsChildOf0 = (qidx0==pidx1);
		uint32 IsChildOf1 = (qidx1==pidx0);
		uint32 res = ( IsChildOf0|IsChildOf1);

		if (res==0)
		{
			//yes, then keep the most important, delete the other one
			pModelMesh->m_arrIntVertices[i].boneIDs[1] = pModelMesh->m_arrIntVertices[i].boneIDs[2];
			pModelMesh->m_arrIntVertices[i].boneIDs[2] = pModelMesh->m_arrIntVertices[i].boneIDs[3];
			pModelMesh->m_arrIntVertices[i].boneIDs[3] = 0;
			pModelMesh->m_arrIntVertices[i].weights[1] = pModelMesh->m_arrIntVertices[i].weights[2];
			pModelMesh->m_arrIntVertices[i].weights[2] = pModelMesh->m_arrIntVertices[i].weights[3];
			pModelMesh->m_arrIntVertices[i].weights[3] = 0;

			for (uint32 e=0; e<numExtVertices; e++)
			{
				uint32 icount=m_arrExtToIntMap[e];
				if (icount==i)
				{
					arrExtVertices[e].boneIDs[1] = arrExtVertices[e].boneIDs[2];
					arrExtVertices[e].boneIDs[2] = arrExtVertices[e].boneIDs[3];
					arrExtVertices[e].boneIDs[3] = 0;
					arrExtVertices[e].weights[1] = arrExtVertices[e].weights[2];
					arrExtVertices[e].weights[2] = arrExtVertices[e].weights[3];
					arrExtVertices[e].weights[3] = 0;
				}
			}
		}
	}
	return;
}
