////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   statobjconstr.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: loading
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "IndexedMesh.h"

#include "CGF/CGFLoader.h"
#include "CGF/CGFSaver.h"
#include "CGF/ReadOnlyChunkFile.h"

#define MAX_TRIS_IN_LOD 300
#define GEOM_INFO_FILE_EXT "ginfo"

#define MESH_NAME_FOR_MAIN "main"

inline const char* stristr(const char* szString, const char* szSubstring)
{
	int nSuperstringLength = (int)strlen(szString);
	int nSubstringLength = (int)strlen(szSubstring);

	for (int nSubstringPos = 0; nSubstringPos <= nSuperstringLength - nSubstringLength; ++nSubstringPos)
	{
		if (strnicmp(szString+nSubstringPos, szSubstring, nSubstringLength) == 0)
			return szString+nSubstringPos;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
inline bool IsDigitChar2( char c )
{
	return (isdigit(c)||c=='.'||c=='-'||c=='+');
}

//////////////////////////////////////////////////////////////////////////
inline float ExtractFloatKeyFromString( const char *key, const char *props )
{
	const char *ptr;
	char *ptr1,buf[64];
	if (ptr=strstr(props,key)) 
	{
		for(ptr+=4;*ptr && !IsDigitChar2(*ptr);ptr++);
		for(ptr1=buf; *ptr && ptr1-buf<sizeof(buf)-1 && (IsDigitChar2(*ptr)||*ptr=='E'||*ptr=='e'); *ptr++,*ptr1++) *ptr1=*ptr;
		*ptr1 = 0;
		return (float)atof(buf);
	}
	return -1;
}


void CStatObj::Refresh(int nFlags)
{
	if(nFlags & FRO_GEOMETRY)
	{
//		bool bSpritesWasCreated = IsSpritesCreated();
		ShutDown();
		Init();
		bool bRes = LoadCGF( m_szFileName );

		LoadLowLODs(false);

		if(!bRes)
		{ // load default in case of error
			ShutDown();
			Init();
			LoadCGF("objects/default.cgf");
			m_bDefaultObject = true;
		}

		return;
	}

	if (nFlags & (FRO_TEXTURES | FRO_SHADERS))
	{
		//if (m_pMaterial)
			//m_pMaterial->Reload();

		/*
		IRenderMesh *pRM = m_pRenderMesh;

		for (int i=0; i<pRM->GetChunks()->Count(); i++)
		{
			IShader *e = (*pRM->GetChunks())[i].GetDefaultMaterial()->GetShaderItem().m_pShader;
			if (e && (*pRM->GetChunks())[i].pRE && (*pRM->GetChunks())[i].nNumIndices)
				e->Reload(nFlags);
		}
		*/
	}
}

void CStatObj::LoadLowLODs(bool bLoadLater)
{
	if (m_nLoadedLodsNum > 1)
		return;

	m_nLoadedLodsNum = 1;

	if(!GetCVars()->e_lods)
		return;

	if (m_bSubObject) // Never do this for sub objects.
		return;

	int nLoadedLods = 1;
	int nLodLevel = 1;
	_smart_ptr<CStatObj> loadedLods[MAX_STATOBJ_LODS_NUM];
	for(nLodLevel=0; nLodLevel<MAX_STATOBJ_LODS_NUM; nLodLevel++)
		loadedLods[nLodLevel] = 0;

	for(nLodLevel=1; nLodLevel<MAX_STATOBJ_LODS_NUM; nLodLevel++)
	{
		// make lod file name
		char sLodFileName[512];
		strncpy(sLodFileName, m_szFileName, sizeof(sLodFileName));
		sLodFileName[strlen(sLodFileName)-4]=0;
		strcat(sLodFileName,"_lod");
		char sLodNum[8];
		ltoa(nLodLevel,sLodNum,10);
		strcat(sLodFileName,sLodNum);
		strcat(sLodFileName, &m_szFileName[strlen(m_szFileName)-4] );

		CStatObj *pLodStatObj = m_arrpLowLODs[nLodLevel];

		// try to load
		bool bRes = false;
		if (IsValidFile(sLodFileName))
		{
			if (!pLodStatObj)
			{
				pLodStatObj = new CStatObj();
				pLodStatObj->m_bForInternalUse = true;
				pLodStatObj->m_pLod0 = this; // Must be here.
			}

			bRes = pLodStatObj->LoadCGF(sLodFileName,true);
		}

		if(!bRes)
		{
			if (m_arrpLowLODs[nLodLevel] != pLodStatObj)
				pLodStatObj->Release();
			SetLodObject( nLodLevel,0 );
			break;
		}
		else
			nLoadedLods++;

		loadedLods[nLodLevel] = pLodStatObj;

		bool bLodCompound = (pLodStatObj->GetFlags()&STATIC_OBJECT_COMPOUND) != 0;
		bool bLod0Compund = (GetFlags()&STATIC_OBJECT_COMPOUND) != 0;

		if (!bLodCompound)
		{
			SetLodObject( nLodLevel,pLodStatObj );
		}
		if (bLodCompound != bLod0Compund)
		{
			// LOD0 and LOD differ.
			FileWarning( 0,sLodFileName,"Invalid LOD%d, LOD%d have different merging property from LOD0",nLodLevel,nLodLevel );
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Put LODs into the sub objects.
	//////////////////////////////////////////////////////////////////////////
	if (nLoadedLods > 1)
	{
		for (uint i = 0; i < m_subObjects.size(); i++)
		{
			SSubObject *pSubObject = &m_subObjects[i];
			if (!pSubObject->pStatObj || pSubObject->nType != STATIC_SUB_OBJECT_MESH)
				continue;

			CStatObj *pSubStatObj = (CStatObj*)pSubObject->pStatObj;

			int nLoadedTrisCount = ((CStatObj*)pSubObject->pStatObj)->m_nLoadedTrisCount;

			for (int nLodLevel = 1; nLodLevel < nLoadedLods; nLodLevel++)
			{
				if (loadedLods[nLodLevel] != 0 && loadedLods[nLodLevel]->m_nSubObjectMeshCount > 0)
				{
					SSubObject *pLodSubObject = loadedLods[nLodLevel]->FindSubObject(pSubObject->name);
					if (pLodSubObject && pLodSubObject->pStatObj && pLodSubObject->nType == STATIC_SUB_OBJECT_MESH)
					{
						pSubStatObj->SetLodObject( nLodLevel,(CStatObj*)pLodSubObject->pStatObj );
					}
				}
			}
			if (pSubStatObj)
				pSubStatObj->CleanUnusedLods();
		}
	}

	CleanUnusedLods();
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::CleanUnusedLods()
{
	//////////////////////////////////////////////////////////////////////////
	// Free render resources for unused upper LODs.
	//////////////////////////////////////////////////////////////////////////
	if (m_nLoadedLodsNum > 1)
	{
		int nMinLod = GetMinUsableLod();
		nMinLod = clamp_tpl(nMinLod,0,(int)m_nLoadedLodsNum-1);
		for (int i = 0; i < nMinLod; i++)
		{
			CStatObj *pStatObj = (CStatObj*)GetLodObject(i);
			if (!pStatObj)
				continue;

			if (pStatObj->m_pRenderMesh)
			{
				pStatObj->SetRenderMesh(0);
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::PreloadResources(float fDist, float fTime, int dwFlags)
{
	FUNCTION_PROFILER_3DENGINE;
	if(GetRenderMesh())
		GetRenderer()->EF_PrecacheResource(GetRenderMesh(),fDist,fTime,0);

/*	if(IsSpritesCreated())
	{
		for(int i=0; i<FAR_TEX_COUNT; i++)
		{
			if(int nCompositeTexId = m_arrSpriteTexID[i])
			{
				if(int nID = nCompositeTexId & 0xffff)
					if(ITexture * pTexPic = GetRenderer()->EF_GetTextureByID(nID))
						GetRenderer()->EF_PrecacheResource(pTexPic, 0, 1.f, 0);

				if(int nID = nCompositeTexId >> 16)
					if(ITexture * pTexPic = GetRenderer()->EF_GetTextureByID(nID))
						GetRenderer()->EF_PrecacheResource(pTexPic, 0, 1.f, 0);
			}
		}

		for(int i=0; i<FAR_TEX_COUNT_60; i++)
		{
			if(int nCompositeTexId = m_arrSpriteTexID_60[i])
			{
				if(int nID = nCompositeTexId & 0xffff)
					if(ITexture * pTexPic = GetRenderer()->EF_GetTextureByID(nID))
						GetRenderer()->EF_PrecacheResource(pTexPic, 0, 1.f, 0);

				if(int nID = nCompositeTexId >> 16)
					if(ITexture * pTexPic = GetRenderer()->EF_GetTextureByID(nID))
						GetRenderer()->EF_PrecacheResource(pTexPic, 0, 1.f, 0);
			}
		}
	}*/
}

inline void TransformMesh( CMesh &mesh,Matrix34 tm )
{
	if (!mesh.m_pPositions)
		return;

	int nVerts = mesh.GetVertexCount();
	for (int i = 0; i < nVerts; i++)
	{
		mesh.m_pPositions[i] = tm.TransformPoint(mesh.m_pPositions[i]);
	}
}
/*
//////////////////////////////////////////////////////////////////////////
struct SSmartStatObjPtr
{
public:
	SSmartStatObjPtr( CStatObj *pObj ) { m_pObj = pObj; }
	~SSmartStatObjPtr()
	{
		if (m_pObj && m_pObj->m_nUsers == 0)
			delete m_pObj;
	}

private:
	CStatObj *m_pObj;
};
*/

//////////////////////////////////////////////////////////////////////////
bool CStatObj::LoadCGF( const char *filename,bool bLod,bool bForceBreakable )
{
	if (m_bSubObject) // Never execute this on the sub objects.
		return true;

	PrintComment("Loading %s", filename);
	if (!bLod)
		GetConsole()->TickProgressBar();

	m_nLoadedTrisCount = 0;
	m_szFileName = filename;
	m_szFileName.replace( '\\','/' );
	m_szFolderName = PathUtil::GetPath(m_szFileName);

	CContentCGF *pCGF = NULL;

	//////////////////////////////////////////////////////////////////////////
	// Load CGF.
	//////////////////////////////////////////////////////////////////////////
	class Listener : public ILoaderCGFListener
	{
	public:
		virtual void Warning( const char *format ) {Cry3DEngineBase::Warning("%s", format);}
		virtual void Error( const char *format ) {Cry3DEngineBase::Error("%s", format);}
	};

	// Must be in it`s own scope.
	{
		Listener listener;
		CLoaderCGF cgfLoader;
#if !defined(XENON) && !defined(PS3)
		CReadOnlyChunkFile chunkFile( bLod );
#else
		// We have to use slower version on Xenon since it cannot read unalligned data
		CChunkFile chunkFile;
#endif
		pCGF = cgfLoader.LoadCGF( filename,chunkFile, &listener );
		if (!pCGF)
		{
			// filename starts with Game/...
			//	If this is a downloaded map it won't be in the Game folder, but in %USER% instead.
			//	Try loading it from there instead.
			string newFileName = filename;
			if(stricmp(newFileName.substr(0,12).c_str(), "game/levels/") == 0)
			{
				newFileName = "%USER%/Downloads/Levels/" + newFileName.substr(12);
				pCGF = cgfLoader.LoadCGF(newFileName.c_str(), chunkFile, &listener);
			}
			
			// still no luck - give up.
			if(!pCGF)
			{
				FileWarning( 0,filename,"CGF Loading Failed: %s",cgfLoader.GetLastError() );
				return false;
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////

	CExportInfoCGF *pExportInfo = pCGF->GetExportInfo();
	CNodeCGF *pFirstMeshNode = NULL;
	m_nSubObjectMeshCount = 0;

	bool bCompiledCGF = pExportInfo->bCompiledCGF;
	bool bHasJoints = false;

	//////////////////////////////////////////////////////////////////////////
	// Find out number of meshes, and get pointer to the first found mesh.
	//////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < pCGF->GetNodeCount(); i++)
	{
		CNodeCGF *pNode = pCGF->GetNode(i);
		if (pNode->pMesh && (pNode->type == CNodeCGF::NODE_MESH))
		{
			if (m_szProperties.empty())
			{
				m_szProperties = pNode->properties; // Take properties from the first mesh node.
				m_szProperties.MakeLower();
			}
			m_nSubObjectMeshCount++;
			if (!pFirstMeshNode)
				pFirstMeshNode = pNode;
		}	
		else if (strncmp(pNode->name,"$joint",6)==0)
			bHasJoints = true;
	}

	bool bIsLod0Merged = false;
	if (bLod && m_pLod0)
	{
		// This is a log object, check if parent was merged or not.
		bIsLod0Merged = m_pLod0->m_nSubObjectMeshCount == 0;
	}

	if (pExportInfo->bMergeAllNodes || (m_nSubObjectMeshCount <= 1 && !bHasJoints && (!bLod || bIsLod0Merged)))
	{
		// If we merging all nodes, ignore sub object meshes.
		m_nSubObjectMeshCount = 0;

		if (pCGF->GetCommonMaterial())
			m_pMaterial = GetMatMan()->LoadCGFMaterial( pCGF->GetCommonMaterial(),m_szFileName );
	}

	// Common of all sub nodes bbox.
	AABB commonBBox;
	commonBBox.Reset();

	bool bHaveMeshNamedMain = false;

	//////////////////////////////////////////////////////////////////////////
	// Create StatObj from Mesh.
	//////////////////////////////////////////////////////////////////////////
	if (pExportInfo->bMergeAllNodes || m_nSubObjectMeshCount == 0)
	{
		CMesh *pMesh = NULL;
		if (pCGF->GetMergedMesh())
			pMesh = pCGF->GetMergedMesh();
		if (!pMesh && pFirstMeshNode)
			pMesh = pFirstMeshNode->pMesh;

		// Not support not merged mesh yet.
		if (!pMesh)
		{
			delete pCGF;
			return false;
		}

		// Assign mesh to this static object.
		if (!SetFromMesh( pMesh ))
		{
			delete pCGF;
			return false;
		}

		if (GetCVars()->e_mesh_simplify != 0)
			pExportInfo->bCompiledCGF = false;

		//////////////////////////////////////////////////////////////////////////
		// Physicalize merged geometry.
		//////////////////////////////////////////////////////////////////////////
		m_bHavePhysicsProxy = false;

		if (pExportInfo->bHavePhysicsProxy)
		{
			m_bHavePhysicsProxy = true;
			// Physicalize first physics proxy.
			for (int i = 0; i < pCGF->GetNodeCount(); i++)
			{
				CNodeCGF *pNode = pCGF->GetNode(i);
				if (pNode->bPhysicsProxy && pNode->pMesh)
				{
					if (pExportInfo->bCompiledCGF)
						PhysicalizeCompiled( pNode );
					else
						Physicalize( *pNode->pMesh );
					break;
				}
			}
		}
		else if (pExportInfo->bCompiledCGF)
		{
			PhysicalizeCompiled( pFirstMeshNode );
		}
		else
		{
			Physicalize( *pMesh );
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Create SubObjects.
	//////////////////////////////////////////////////////////////////////////
	if (pCGF->GetNodeCount() > 1 || m_nSubObjectMeshCount > 0)
	{
		std::vector<CNodeCGF*> nodes;
		nodes.reserve( pCGF->GetNodeCount() );

		int nNumMeshes = 0;
		int i;
		for (i = 0; i < pCGF->GetNodeCount(); i++)
		{
			CNodeCGF *pNode = pCGF->GetNode(i);

			SSubObject subObject;
			subObject.pStatObj = 0;
			subObject.bIdentityMatrix = pNode->bIdentityMatrix;
			subObject.bHidden = false;
			subObject.tm = pNode->worldTM;
			subObject.localTM = pNode->localTM;
			subObject.name = pNode->name;
			subObject.properties = pNode->properties;
			subObject.nParent = -1;
			subObject.pWeights = 0;

			if (pNode->type == CNodeCGF::NODE_MESH)
			{
				if (pExportInfo->bMergeAllNodes || m_nSubObjectMeshCount == 0) // Only add helpers, ignore meshes.
					continue;

				nNumMeshes++;
				subObject.nType = STATIC_SUB_OBJECT_MESH;

				if (stricmp(pNode->name,MESH_NAME_FOR_MAIN) == 0)
					bHaveMeshNamedMain = true;
			}
			else if (pNode->type == CNodeCGF::NODE_LIGHT)
				subObject.nType = STATIC_SUB_OBJECT_LIGHT;
			else if (pNode->type == CNodeCGF::NODE_HELPER)
			{
				switch (pNode->helperType)
				{
				case HP_POINT:
					subObject.nType = STATIC_SUB_OBJECT_POINT;
					break;
				case HP_DUMMY:
					subObject.nType = STATIC_SUB_OBJECT_DUMMY;
					subObject.helperSize = (pNode->helperSize*0.01f);
					break;
				case HP_XREF:
					subObject.nType = STATIC_SUB_OBJECT_XREF;
					break;
				case HP_CAMERA:
					subObject.nType = STATIC_SUB_OBJECT_CAMERA;
					break;
				case HP_GEOMETRY:
					{
						subObject.nType = STATIC_SUB_OBJECT_HELPER_MESH;
						subObject.bHidden = true; // Helpers are not rendered.

						if (pNode->pMesh != 0 && pNode->pMesh->GetSubSetCount() != 0 && stristr(pNode->name,"$occlusion") != 0)
						{
							CStatObj *pStatObjOwner = this;
							if (!pExportInfo->bMergeAllNodes && m_nSubObjectMeshCount > 0 && pNode->pParent)
							{
								// We are attached to some object, find it.
								for (int i = 0,num = nodes.size(); i < num; i++)
									if (nodes[i] == pNode->pParent)
									{
										pStatObjOwner = (CStatObj*)m_subObjects[i].pStatObj;
										break;
									}
							}
							if (!pStatObjOwner)
								continue;

							if (pStatObjOwner->m_pRenderMeshOcclusion)
							{
								pStatObjOwner->m_pRenderMeshOcclusion = 0;
							}

							if(!pNode->pMesh->GetIndexCount() || !pNode->pMesh->GetVertexCount())
							{
								Warning("Empty occlusion proxy found for object: %s, sub-object name is %s", pStatObjOwner->GetFilePath(), pStatObjOwner->GetGeoName());
								continue;
							}

							m_bHaveOcclusionProxy = true;
							pStatObjOwner->m_bHaveOcclusionProxy = true;
							pStatObjOwner->m_pRenderMeshOcclusion = GetRenderer()->CreateRenderMesh(eBT_Static,"OcclusionProxy",m_szFileName.c_str());
							pStatObjOwner->m_pRenderMeshOcclusion->SetMaterial( GetMatMan()->GetDefaultMaterial() );
							TransformMesh( *pNode->pMesh,pNode->localTM );
							pStatObjOwner->m_pRenderMeshOcclusion->SetMesh( *pNode->pMesh );
							continue; // Do not add this sub node.
						}
					}
					break;
				default:
					assert(0); // unknown type.
				}
			}

			// Only when multiple meshes inside.
			// If only 1 mesh inside, Do not create a separate CStatObj for it.
			if ((m_nSubObjectMeshCount > 0 && pNode->type == CNodeCGF::NODE_MESH && pNode->pMesh != NULL) || 
				(subObject.nType == STATIC_SUB_OBJECT_HELPER_MESH))
			{
				if (!pNode->pSharedMesh) // If shared mesh, then do not create static object.
				{
					// Make inner StatObj.
					CStatObj *pStatObj = new CStatObj;
					subObject.pStatObj = pStatObj;

					pStatObj->m_szFolderName = m_szFolderName;
					pStatObj->m_szFileName = m_szFileName;
					pStatObj->m_szGeomName = subObject.name;
					pStatObj->m_bSubObject = true;

					//if (strstr(m_szFileName, "fence_metal_220_220_a_lod1"))
					//{
					//	int a = 0;
					//}


					if (subObject.nType != STATIC_SUB_OBJECT_HELPER_MESH)
						pStatObj->m_pParentObject = this;
					pStatObj->m_szProperties = subObject.properties;
					pStatObj->m_szProperties.MakeLower();
					if (!pStatObj->m_szProperties.empty())
						pStatObj->ParseProperties();

					if (pNode->pMaterial)
					{
						pStatObj->m_pMaterial = GetMatMan()->LoadCGFMaterial( pNode->pMaterial,m_szFileName );
						if (!m_pMaterial || m_pMaterial->IsDefault())
							m_pMaterial = pStatObj->m_pMaterial; // take it as a general stat obj material.
					}
					if (!pStatObj->m_pMaterial)
						pStatObj->m_pMaterial = m_pMaterial;

					pStatObj->SetFromMesh( pNode->pMesh );
					if (pNode->bHasFaceMap)
						memcpy(pStatObj->m_pMapFaceToFace0=new uint16[pNode->pMesh->GetIndexCount()/3], &pNode->mapFaceToFace0[0], 
						(pNode->pMesh->GetIndexCount()/3)*sizeof(uint16));
					if (bCompiledCGF)
						pStatObj->PhysicalizeCompiled( pNode );
					else
						pStatObj->Physicalize( *pNode->pMesh );
					pStatObj->AnalizeFoliage();

					//////////////////////////////////////////////////////////////////////////
					//@TODO: Optimize this to not keep system memory copy Indexed mesh, for CGF objects.
					/*if (pStatObj->m_bHaveBreakablePhysics)
					{
					pStatObj->m_pIndexedMesh = new CIndexedMesh;
					pStatObj->m_pIndexedMesh->SetMesh( *pNode->pMesh );
					}*/
				}
				
				// Calc bbox.
				if (pNode->pMesh && pNode->type == CNodeCGF::NODE_MESH)
				{
					AABB box = pNode->pMesh->m_bbox;
					box.SetTransformedAABB( subObject.tm,box );
					commonBBox.Add( box.min );
					commonBBox.Add( box.max );
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// Check for LOD
			//////////////////////////////////////////////////////////////////////////
			if (subObject.nType == STATIC_SUB_OBJECT_HELPER_MESH)
			{
				// Check if helper object is a LOD, name must be $lod1, $lod2 etc...
				if (stristr(pNode->name,"$lod") != 0)
				{
					if (!subObject.pStatObj)
						continue;

					//if (strstr(m_szFileName, "fence_metal_220_220_a"))
					//{
					//	int a = 0;
					//}

					

					CStatObj *pLodStatObj = (CStatObj*)subObject.pStatObj;
					CStatObj *pStatObjParent = this;
					if (!pExportInfo->bMergeAllNodes && m_nSubObjectMeshCount > 0 && pNode->pParent)
					{
						// We are attached to some object, find it.
						for (int i = 0,num = nodes.size(); i < num; i++)
						{
							if (nodes[i] == pNode->pParent)
							{
								pStatObjParent = (CStatObj*)m_subObjects[i].pStatObj;
								break;
							}
						}
					}
					if (!pStatObjParent)
					{
						delete (CStatObj*)subObject.pStatObj;
						continue;
					}

					int nLodLevel = clamp_tpl( atoi(pNode->name.c_str()+4),1,MAX_STATOBJ_LODS_NUM-1 );
					if (!pStatObjParent->m_arrpLowLODs[nLodLevel])
					{
						pStatObjParent->SetLodObject( nLodLevel,pLodStatObj );
						pLodStatObj->m_nUsers--; // Because we not adding it to the sub object list
						continue;
					}
					else
					{
            FileWarning(0, m_szFolderName.c_str(),	"Duplicate LOD helper %s (%s)",pNode->name.c_str(),m_szFolderName.c_str() );
            if (pLodStatObj)
              delete pLodStatObj;
            continue;
					}
					if (subObject.pStatObj && ((CStatObj*)subObject.pStatObj)->m_nUsers == 0)
            delete (CStatObj*)subObject.pStatObj;
					continue;
				}
			}
			//////////////////////////////////////////////////////////////////////////

			if (subObject.pStatObj)
				((CStatObj*)subObject.pStatObj)->m_nUsers++; // AddRef

			m_subObjects.push_back(subObject);
			nodes.push_back(pNode);

			if (pNode->bHasFaceMap)
				m_bHasDeformationMorphs = true;
		}

		// Assign SubObject parent pointers.
		int nNumCgfNodes = (int)nodes.size();
		if (nNumCgfNodes > 0)
		{
			CNodeCGF **pNodes = &nodes[0];

			//////////////////////////////////////////////////////////////////////////
			// Move meshes to begining, Sort sub-objects so that meshes are first.
			for (i = 0; i < nNumCgfNodes; i++)
			{
				if (pNodes[i]->type != CNodeCGF::NODE_MESH)
				{
					// check if any more meshes exist.
					if (i < nNumMeshes)
					{
						// Try to find next mesh and place it here.
						for (int j = i+1; j < nNumCgfNodes; j++)
						{
							if (pNodes[j]->type == CNodeCGF::NODE_MESH)
							{
								// Swap objects at j to i.
								std::swap( pNodes[i],pNodes[j] );
								std::swap( m_subObjects[i],m_subObjects[j] );
								break;
							}
						}
					}
				}
			}
			//////////////////////////////////////////////////////////////////////////

			// Assign Shared Meshes.
			for (i = 0; i < nNumCgfNodes; i++)
			{
				if (pNodes[i]->pSharedMesh)
				{
					CNodeCGF *pSharedMesh = pNodes[i]->pSharedMesh;
					for (int j = 0; j < nNumCgfNodes; j++)
					{
						if (pNodes[j] == pSharedMesh)
						{
							CStatObj *pStatObj = (CStatObj*)m_subObjects[j].pStatObj;
							m_subObjects[i].pStatObj = pStatObj;
							if (pStatObj == this) {
								int a = 0;
								}
							if (pStatObj)
								pStatObj->m_nUsers++;
							break;
						}
					}
				}
			}

			// Assign Parent nodes.
			for (i = 0; i < nNumCgfNodes; i++)
			{
				CNodeCGF *pParentNode = pNodes[i]->pParent;
				if (pParentNode)
				{
					for (int j = 0; j < nNumCgfNodes; j++)
					{
						if (pNodes[j] == pParentNode)
						{
							m_subObjects[i].nParent = j;
							break;
						}
					}
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// Handle Main/Remain meshes used for Destroyable Objects.
			//////////////////////////////////////////////////////////////////////////
			if (bHaveMeshNamedMain)
			{
				// If have mesh named main, then mark all sub object hidden except the one called "Main".
				for (int i = 0,n = m_subObjects.size(); i < n; i++)
				{
					if (m_subObjects[i].nType == STATIC_SUB_OBJECT_MESH)
					{
						if (stricmp(m_subObjects[i].name,MESH_NAME_FOR_MAIN) == 0)
							m_subObjects[i].bHidden = false;
						else
							m_subObjects[i].bHidden = true;
					}
				}
			}
			//////////////////////////////////////////////////////////////////////////
		}
	}
	
	if (m_nSubObjectMeshCount > 0)
	{
		m_vBoxMin = commonBBox.min;
		m_vBoxMax = commonBBox.max;
		CalcRadiuses();
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Analyze folliage info.
	//////////////////////////////////////////////////////////////////////////
	if (pExportInfo->bMergeAllNodes || m_nSubObjectMeshCount == 0)
	{
		AnalizeFoliage();
	}
	//////////////////////////////////////////////////////////////////////////

	for (int i = 0; i < pCGF->GetNodeCount(); i++) if (strstr(pCGF->GetNode(i)->properties,"deformable"))
		m_nFlags |= STATIC_OBJECT_DEFORMABLE;
	if (m_nSubObjectMeshCount > 0)
		m_nFlags |= STATIC_OBJECT_COMPOUND;

	if (!bLod)
	{
		// Try load info file.
		string szInfoFile = PathUtil::ReplaceExtension(m_szFileName,GEOM_INFO_FILE_EXT);
		LoadCGF_Info(szInfoFile);
	}
	
	if (!m_szProperties.empty())
		ParseProperties();

	CPhysicalizeInfoCGF* pPi = pCGF->GetPhysiclizeInfo();
	if (pPi->nRetTets)
		m_pLattice = GetPhysicalWorld()->GetGeomManager()->CreateTetrLattice(pPi->pRetVtx,pPi->nRetVtx, pPi->pRetTets,pPi->nRetTets);

	delete pCGF;

	if (m_bHasDeformationMorphs)
	{
		int i,j;
		for(i=GetSubObjectCount()-1;i>=0;i--) if ((j=SubobjHasDeformMorph(i))>=0)
			GetSubObject(i)->pStatObj->SetDeformationMorphTarget(GetSubObject(j)->pStatObj);
	}
	
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::SetFromMesh( CMesh *pMesh )
{
	// If mesh contain faces, it must be compiled.
	if (pMesh->GetFacesCount() > 0)
	{
    Warning( "CStatObj::SetFromMesh: Stripifying geometry at loading time %s (Use resource compiler)",m_szFileName.c_str() );

		mesh_compiler::CMeshCompiler meshCompiler;
		meshCompiler.Compile( *pMesh );

		if (GetCVars()->e_mesh_simplify != 0)
			meshCompiler.ReduceMeshToLevel( *pMesh,GetCVars()->e_mesh_simplify );
	}

	if (GetCVars()->e_mesh_simplify != 0)
	{
		mesh_compiler::CMeshCompiler meshCompiler;
		meshCompiler.ReduceMeshToLevel( *pMesh,GetCVars()->e_mesh_simplify );
	}
/*
	if(pMesh->m_bbox.GetRadius()>1.f) // tessellate
	{
		mesh_compiler::CMeshCompiler meshCompiler;
		meshCompiler.Tesselate(*pMesh, 0.25f);
	}
*/
	m_vBoxMin = pMesh->m_bbox.min;
	m_vBoxMax = pMesh->m_bbox.max;

	CalcRadiuses();

	m_nLoadedTrisCount = pMesh->GetIndexCount() / 3;
	if (!m_nLoadedTrisCount)
		return 0;
	
	m_nRenderTrisCount = 0;
	m_nRenderMatIds = 0;
	//////////////////////////////////////////////////////////////////////////
	// Initialize Mesh subset material flags.
	//////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < pMesh->GetSubSetCount(); i++)
	{
		SMeshSubset &subset = pMesh->m_subsets[i];
		IMaterial *pMtl = m_pMaterial->GetSafeSubMtl(subset.nMatID);
		subset.nMatFlags = pMtl->GetFlags();
		if (subset.nPhysicalizeType==PHYS_GEOM_TYPE_NONE)
			subset.nMatFlags |= MTL_FLAG_NOPHYSICALIZE;
		if (!(subset.nMatFlags&MTL_FLAG_NODRAW) && (subset.nNumIndices > 0))
		{
			m_nRenderMatIds++;
			m_nRenderTrisCount += subset.nNumIndices/3;
		}
	}
	//////////////////////////////////////////////////////////////////////////

	if (!m_nRenderTrisCount)
  {
    //Warning( "CStatObj::SetFromMesh: 0 visible triangles found in file %s - skipped", m_szFileName.c_str() );
		return 0;
  }

	// Create renderable mesh.
	if (!GetSystem()->IsDedicated())
	{
		if (m_pRenderMesh)
		{
			GetRenderer()->DeleteRenderMesh(m_pRenderMesh);
      m_pRenderMesh = NULL;
		}
		
		if (!pMesh)
			return false;
		if (pMesh->GetSubSetCount() == 0)
			return true;

		m_pRenderMesh = GetRenderer()->CreateRenderMesh(eBT_Static,"StatObj",m_szFileName.c_str());
		m_pRenderMesh->SetMaterial( m_pMaterial );
		m_pRenderMesh->SetMesh( *pMesh, 0, FSM_CREATE_DEVICE_MESH );
		//CalcNormalCone(m_pRenderMesh);
	}

	return true;
}
/*
float ElementShadow(Vec3 v, float rSquared, Vec3 receiverNormal, Vec3 emitterNormal, float emitterArea)
{
	return (1.f - cry_sqrtf(emitterArea/3.14f/rSquared + 1.f)) *
		CLAMP(emitterNormal.Dot(v), 0, 1.f) *
		CLAMP(4.f * receiverNormal.Dot(v), 0, 1.f);
}

int CStatObj::UpdateAmbientOcclusion()
{
	if(IRenderMesh * pRM = GetRenderMesh())
	{
		// get offsets
		int nPosStride=0, nColorStride=0, nNormStride=0;
		byte *pPos      = pRM->GetPosPtr(nPosStride);
		uchar*pColor    = pRM->GetColorPtr(nColorStride);
		byte *pNorm    = pRM->GetNormalPtr(nNormStride);

		const float fTestRange = 1.f;

		static PodArray<ushort> lstHash[64][64];
		for(int i=0; i<64; i++)
			for(int j=0; j<64; j++)
				lstHash[i][j].Clear();

		for(int i=0; i<pRM->GetSysVertCount(); i++)
		{
			Vec3 & vPos    = *((Vec3*)&(pPos[i*nPosStride]));
			lstHash[uchar(vPos.x/fTestRange+32)&63][uchar(vPos.y/fTestRange+32)&63].Add(i);
		}

		static PodArray<Vec3> arrNormals; arrNormals.Clear();

		for(int i=0; i<pRM->GetSysVertCount(); i++)
		{
			Vec3 & vPos    = *((Vec3*)&pPos[nPosStride*i]);

			int hx = (vPos.x/fTestRange+32);
			int hy = (vPos.y/fTestRange+32);

			// fix normal
			Vec3 vNormal(0,0,0);
			{
				PodArray<ushort> * pList = &lstHash[uchar(hx)&63][uchar(hy)&63];
				for(int n=0; n<pList->Count(); n++)
				{
					int idx = pList->GetAt(n);
					Vec3 & vPos0    = *((Vec3*)&pPos[nPosStride*idx]);
					float fDistV = vPos0.GetDistance(vPos);
					if(fDistV<0.01f)
					{
						Vec3 & vNorm0    = *((Vec3*)&pNorm[nNormStride*idx]);
						vNormal += vNorm0;
					}
				}

				vNormal.NormalizeSafe();
			}
			
			arrNormals.Add(vNormal);
		}

		for(int i=0; i<pRM->GetSysVertCount(); i++)
		{
			Vec3 & vPos    = *((Vec3*)&pPos[nPosStride*i]);
			Vec3 & vNorm    = arrNormals[i];
			SMeshColor & uColor = *((SMeshColor*)&pColor[nColorStride*i]);

			Vec3 vPosPlus = vPos + vNorm;

			float fAmbLight = 1.f;
			{ // ambient occlusion
				float fRatioSumm = 1.f;
				float fSamplesNum = 1.f;

				int hx = (vPos.x/fTestRange+32);
				int hy = (vPos.y/fTestRange+32);

				for(int x=hx-1; x<=hx+1; x++)
					for(int y=hy-1; y<=hy+1; y++)
					{
						PodArray<ushort> * pList = &lstHash[uchar(x)&63][uchar(y)&63];
						for(int n=0; n<pList->Count(); n++)
						{
							int idx = pList->GetAt(n);
							Vec3 & vPos0    = *((Vec3*)&pPos[nPosStride*idx]);
							Vec3 & vNorm0    = arrNormals[idx];
							float fDistV = vPos0.GetDistance(vPos);
							if(fDistV<fTestRange && fDistV>0.01f)
							{
								float fDistN = (vPos0+vNorm0*fDistV*0.5f).GetDistance(vPos+vNorm*fDistV*0.5f);
								fDistV = max(fDistV,0.001f);
								float t = fDistV/fTestRange;
								float fRatioFull = min(1.f, fDistN/fDistV);
								float fRatio = (1.f-t)*fRatioFull + t;
								fRatioSumm += fRatio;
								fSamplesNum++;
							}
						}
					}

					fRatioSumm /= fSamplesNum;
					fRatioSumm = min(1.f, fRatioSumm);
					fAmbLight = pow(fRatioSumm,8);
			}

			uColor.r=uColor.g=uColor.b = uchar(255*min(1.f,max(0.f,fAmbLight)));
			uColor.a=255;
		}

		pRM->InvalidateVideoBuffer(1);
	}
	return 0;
}*/

//////////////////////////////////////////////////////////////////////////
// Save statobj to the CGF file.
bool CStatObj::SaveToCGF( const char *sFilename,IChunkFile** pOutChunkFile )
{
	CContentCGF *pCGF = new CContentCGF(sFilename);

	CChunkFile *pChunkFile = new CChunkFile;
	if (pOutChunkFile)
		*pOutChunkFile = pChunkFile;

	CMaterialCGF *pMaterialCGF = new CMaterialCGF;
	pMaterialCGF->name = m_pMaterial->GetName();
	pMaterialCGF->nFlags;  // Material flags.
	pMaterialCGF->nPhysicalizeType = PHYS_GEOM_TYPE_DEFAULT;
	pMaterialCGF->bOldMaterial = false;
	pMaterialCGF->pMatEntity = NULL;
	pMaterialCGF->nChunkId = 0;

	// Array of sub materials.
	//std::vector<CMaterialCGF*> subMaterials;

	// Add single node for merged mesh.
	CNodeCGF *pNode = new CNodeCGF;
	pNode->type = CNodeCGF::NODE_MESH;
	pNode->name = "Merged";
	pNode->localTM.SetIdentity();
	pNode->worldTM.SetIdentity();
	pNode->pos.Set(0,0,0);
	pNode->rot.SetIdentity();
	pNode->scl.Set(1,1,1);
	pNode->bIdentityMatrix = true;
	pNode->pMesh = new CMesh;
	pNode->pMesh->Copy( *GetIndexedMesh()->GetMesh() );
	pNode->pParent = 0;
	pNode->pMaterial = pMaterialCGF;
	pNode->nPhysicalizeFlags = 0;
	SavePhysicalizeData( pNode );
	// Add node to CGF contents.
	pCGF->AddNode( pNode );

	CSaverCGF cgfSaver( sFilename,*pChunkFile );
	cgfSaver.SaveContent( pCGF );

	bool bResult = true;
	if (!pOutChunkFile)
	{
		bResult = pChunkFile->Write( sFilename );
		pChunkFile->Release();
	}

	delete pCGF;

	return bResult;
}

void CStatObj::CalcNormalCone(IRenderMesh * pRM)
{
	// render tris
	PodArray<CRenderChunk> *	pChunks = pRM->GetChunks();

	int nPosStride=0;
	byte * pPos = (byte *)pRM->GetStridedPosPtr(nPosStride);

	static PodArray<Vec3> arrNormals; arrNormals.Clear();

	int nIndCount=0;
	ushort * pIndices = pRM->GetIndices(&nIndCount);

	for(int nChunkId=0; nChunkId<pChunks->Count(); nChunkId++)
	{
		CRenderChunk * pChunk = pChunks->Get(nChunkId);
		if(pChunk->m_nMatFlags & MTL_FLAG_NODRAW || !pChunk->pRE)
			continue;

		if(pChunk->m_nMatFlags & MTL_FLAG_2SIDED)
			return;		

		int nLastIndexId = pChunk->nFirstIndexId + pChunk->nNumIndices;
		for(int i=pChunk->nFirstIndexId; i<nLastIndexId; i+=3)
		{
			Vec3 & v0 = *(Vec3 *)&pPos[pIndices[i+0] * nPosStride];
			Vec3 & v1 = *(Vec3 *)&pPos[pIndices[i+1] * nPosStride];
			Vec3 & v2 = *(Vec3 *)&pPos[pIndices[i+2] * nPosStride];

			if(v0.GetSquaredDistance(v1)<0.001f || v0.GetSquaredDistance(v2)<0.001f || v1.GetSquaredDistance(v2)<0.001f)
				continue;

			Vec3 vNormal = (v0-v1).Cross(v0-v2);
			if(vNormal.GetLengthSquared()<0.001f)
				continue;

			vNormal.Normalize();
			arrNormals.Add(vNormal);
		}
	}

	m_vObjectNormal.Set(0,0,0);
	for(int i=0; i<arrNormals.Count(); i++)
		m_vObjectNormal += arrNormals[i];

	if(m_vObjectNormal.GetLength()<0.01f)
	{
		m_fObjectNormalMaxDot = -1;
		return;
	}

	m_vObjectNormal.Normalize();

	m_fObjectNormalMaxDot = 1;

	for(int i=0; i<arrNormals.Count(); i++)
	{
		float fDot = m_vObjectNormal.Dot(arrNormals[i]);
		if(fDot<m_fObjectNormalMaxDot)
			m_fObjectNormalMaxDot = fDot;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::LoadCGF_Info( const char *filename )
{
	CCryFile file;
	if (file.Open(filename,"rb",ICryPak::FOPEN_HINT_QUIET))
	{
		int nLen = file.GetLength();

		m_szProperties.resize(nLen);
		char *sAllText = (char*)&m_szProperties[0];
		file.ReadRaw( sAllText,nLen );

		// Convert properties to low case.
		m_szProperties.MakeLower();

		//m_szProperties.reserve( 2+nLen );
		//m_szProperties += '\n';
		//m_szProperties += sAllText;

		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
inline char* trim_whitespaces( char *str,char *strEnd )
{
	char *first = str;
	while (first < strEnd && (*first == ' '||*first == '\t'))
		first++;
	char *s = strEnd-1;
	while (s >= first && (*s == ' '||*s == '\t'))
		*s-- = 0;
	return first;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::ParseProperties()
{
	//m_phys_mass = ExtractFloatKeyFromString("mass",sProperties);
	//m_phys_density = ExtractFloatKeyFromString("density",sProperties);

//	int nLen = *sP
	//static DynArray<char> propertiesString;
	//propertiesString.resize(sP)

	int nLen = m_szProperties.size();
	if (nLen >= 4090)
	{
		Warning("CGF '%s' have longer then 4K geometry info file", m_szFileName.c_str());
		nLen = 4090;
	}

	char properties[4096];
	memcpy( properties,m_szProperties.c_str(),nLen );
	properties[nLen] = 0;

	char *str = properties;
	char *strEnd = str + nLen;
	while (str < strEnd)
	{
		char *line = str;
		while (str<strEnd && *str != '\n' && *str != '\r')
			str++;
		char *lineEnd = str;
		*lineEnd = 0;
		str++;
		while (str<strEnd && (*str == '\n' || *str == '\r')) // Skip all \r\n at end.
			str++;

		if (*line == '/' || *line == '#') // skip comments
		{
			continue;
		}

		if (line < lineEnd)
		{
			// Parse line.
			char *l = line;
			while (l < lineEnd && *l != '=')
				l++;
			if (l < lineEnd)
			{
				*l = 0;
				char *left = line;
				char *right = l+1;

				// remove white spaces from left and right.
				left = trim_whitespaces(left,l);
				right = trim_whitespaces(right,lineEnd);

				//////////////////////////////////////////////////////////////////////////
				if (0 == strcmp(left,"mass"))
				{
					m_phys_mass = atof(right);
				}
				else if (0 == strcmp(left,"density"))
				{
					m_phys_density = atof(right);
				}
				//////////////////////////////////////////////////////////////////////////

			}
			else
			{
				// There`s no = on the line, must be a flag.
				//////////////////////////////////////////////////////////////////////////
				if (0 == strcmp(line,"entity"))
				{
					// pickable
					m_nFlags |= STATIC_OBJECT_SPAWN_ENTITY;
				}
				else if (0 == strcmp(line,"pickable"))
				{
					m_nFlags |= STATIC_OBJECT_PICKABLE;
				}
        else if (0 == strcmp(line,"no_auto_hidepoints"))
        {
          m_nFlags |= STATIC_OBJECT_NO_AUTO_HIDEPOINTS;
        }
				//////////////////////////////////////////////////////////////////////////
			}
		}
	}
}
