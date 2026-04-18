#define NO_GDI
#include "StdAfx.h"
#undef NO_GDI
#include "Cry3DEngineBase.h"
//#include <d3d8types.h>

//#ifdef WIN32
//#include <windows.h>
//#endif

#include "dds.h"
#include "LMSerializationManager.h"
#include "IEntityRenderState.h"
#include <algorithm>									// STL find
#include <set>												// STL set<>

#include <memory>

#include "ObjMan.h"
#include "Brush.h"

#define LEVELLM_PAK_NAME "LevelLM.pak"

#define SEGMENTED_SAVE

CLMSerializationManager::CLMSerializationManager(){}

CLMSerializationManager::~CLMSerializationManager()
{
	for (std::vector<RawLMData *>::iterator itLMData=m_vLightPatches.begin(); itLMData!=m_vLightPatches.end(); itLMData++)
		delete (*itLMData); 
}

RenderLMData *CLMSerializationManager::UpdateLMData( 
	const uint32 indwWidth, const uint32 indwHeight,const std::pair<int32,int32>* pGLM_IDs, const int32 nGLM_IDNumber,
	BYTE *_pColorLerp4, BYTE *_pHDRColorLerp4, BYTE *_pDomDirection3, BYTE *_pOcclusion, BYTE *_pRAE, const char* strDirPath, int32* pTextureID)
{
	std::vector<CLMTextureToObjID > _vGLM_IDs_UsingPatch;
	_vGLM_IDs_UsingPatch.resize(nGLM_IDNumber);
	for( int32 i = 0; i < nGLM_IDNumber; ++i  )
		_vGLM_IDs_UsingPatch[i]	= pGLM_IDs[i];

	std::vector<RawLMData *>::iterator itLMData;
	// for all stored lightmaps
	for(itLMData=m_vLightPatches.begin(); itLMData!=m_vLightPatches.end();)
	{
		std::vector<CLMTextureToObjID >::const_iterator itID;
		RawLMData *pCurRawLMData = *itLMData;

		// for all the given new object ids
		for (itID=_vGLM_IDs_UsingPatch.begin(); itID!=_vGLM_IDs_UsingPatch.end(); ++itID)
		{
			std::vector<CLMTextureToObjID >::iterator itRemove=pCurRawLMData->vGLM_IDs_UsingPatch.end();
			for(std::vector<CLMTextureToObjID >::iterator it=pCurRawLMData->vGLM_IDs_UsingPatch.begin();it!=pCurRawLMData->vGLM_IDs_UsingPatch.end();++it)
				if(it->ID()==(*itID).ID() && it->SubObjIdx()==(*itID).SubObjIdx())
					itRemove	=	it;

			//we cannot use std::find due to the std::pair that forwards compares just for "first"
			//			std::vector<int>::iterator itRemove=std::find(pCurRawLMData->vGLM_IDs_UsingPatch.begin(),pCurRawLMData->vGLM_IDs_UsingPatch.end(),*itID);
			if(itRemove!=pCurRawLMData->vGLM_IDs_UsingPatch.end())
				pCurRawLMData->vGLM_IDs_UsingPatch.erase(itRemove);
		}
		++itLMData;
	}

	RawLMData* pRawLMSmall = new RawLMData(indwWidth, indwHeight,_vGLM_IDs_UsingPatch);
	RawLMData* pRawLM = new RawLMData(indwWidth, indwHeight,_vGLM_IDs_UsingPatch);

	if( _pColorLerp4 != NULL )
	{

		pRawLM->initFromBMP (RawLMData::TEX_COLOR, _pColorLerp4);
		pRawLM->m_bUseLightMaps = true;
		pRawLMSmall->m_bUseLightMaps = true;
	}
	else
	{
		pRawLM->m_bUseLightMaps = false;
		pRawLMSmall->m_bUseLightMaps = false;
	}

	if( _pDomDirection3 != NULL )
	{
		pRawLM->initFromBMP (RawLMData::TEX_DOMDIR, _pDomDirection3);
		pRawLM->m_bUseDot3Maps = true;
		pRawLMSmall->m_bUseDot3Maps = true;
	}
	else
	{
		pRawLM->m_bUseDot3Maps = false; //isn't used
		pRawLMSmall->m_bUseDot3Maps = false;
	}
	if(_pHDRColorLerp4 != NULL)
	{
		pRawLM->initFromBMP (RawLMData::TEX_HDR, _pHDRColorLerp4);
		pRawLM->m_bUseHDRMaps = true;
		pRawLMSmall->m_bUseHDRMaps = true;
	}
	else
	{
		pRawLM->m_bUseHDRMaps = false;
		pRawLMSmall->m_bUseHDRMaps = false;
	}
	if(_pOcclusion != NULL)
	{
		pRawLM->initFromBMP (RawLMData::TEX_OCCLUSION, _pOcclusion);
		pRawLM->m_bUseOcclusionMaps = true;
		pRawLMSmall->m_bUseOcclusionMaps = true;
	}
	else
	{
		pRawLM->m_bUseOcclusionMaps = false;
		pRawLMSmall->m_bUseOcclusionMaps = false;
	}

	if(_pRAE != NULL)
	{
		pRawLM->initFromBMP (RawLMData::TEX_RAE, _pRAE);
		pRawLM->m_bUseRAE = true;
		pRawLMSmall->m_bUseRAE = true;
	}
	else
	{
		pRawLM->m_bUseRAE = false;
		pRawLMSmall->m_bUseRAE = false;
	}

	int32 iCurLMTexData = m_vLightPatches.size();
	m_vLightPatches.push_back(pRawLMSmall);

	//Save a dds version, after that release the used memory.
	SaveDDSFiles( strDirPath, pRawLM, iCurLMTexData );
	SAFE_DELETE( pRawLM );

	if( pTextureID )
		*pTextureID = iCurLMTexData;
	return NULL;
}

void CLMSerializationManager::AddTexCoordData(const struct TexCoord2Comp* pTexCoords, const int32 nTexCoordNumber,const int iGLM_ID_UsingTexCoord,const int iGLM_Idx_SubObj, const uint32 indwHashValue, const EntityId* pOcclIDsFirst,const EntityId* pOcclIDsSecond, const int32 nOcclIDNumber, const int8 nFirstOcclusionChannel)
{ 
	std::vector<TexCoord2Comp> vTexCoords;
	if( pTexCoords )
		for( int32 i = 0; i < nTexCoordNumber; ++i  )
			vTexCoords.push_back( pTexCoords[i] );

	std::vector<std::pair<EntityId, EntityId> > rOcclIDs;

	if( pOcclIDsFirst && pOcclIDsSecond )
	{
		for( int32 i = 0; i < nOcclIDNumber; ++i )
			rOcclIDs.push_back( std::pair<EntityId, EntityId>( pOcclIDsFirst[i], pOcclIDsSecond[i] ) );
	}
	m_vTexCoords[CLMTextureToObjID(iGLM_ID_UsingTexCoord,iGLM_Idx_SubObj)] = RawTexCoordData(vTexCoords,indwHashValue, rOcclIDs, nFirstOcclusionChannel); 
} 

void CLMSerializationManager::RescaleTexCoordData(const int iGLM_ID_UsingTexCoord,const int iGLM_Idx_SubObj, const f32 fScaleU, const f32 fScaleV )
{
	RawTexCoordData &rCurRawTexCrdData = m_vTexCoords[CLMTextureToObjID(iGLM_ID_UsingTexCoord,iGLM_Idx_SubObj)];
	std::vector<TexCoord2Comp> &vTexCoords = rCurRawTexCrdData.vTexCoords;

	std::vector<TexCoord2Comp>::iterator it;
	std::vector<TexCoord2Comp>::iterator end = vTexCoords.end();

	for( it = vTexCoords.begin(); it != end; ++it )
	{
		it->s *= fScaleU;
		it->t *= fScaleV;
	}
}


struct AutoFileCloser: Cry3DEngineBase
{
	AutoFileCloser(FILE*pFile):m_pFile(pFile){}
	~AutoFileCloser(){if (m_pFile)GetPak()->FClose(m_pFile);}
protected:
	FILE* m_pFile;
};

#ifdef WIN64
#pragma warning( push )									//AMD Port
#pragma warning( disable : 4267 )
#endif

bool CLMSerializationManager::_Load( const char *pszFileName, struct IRenderNode ** pIGLMs, int32 nIGLMNumber, const bool cbLoadTextures) 
{
	string strDirName = PathUtil::GetParentDirectory(pszFileName);
	string lmFilename = (strDirName + "\\" LEVELLM_PAK_NAME);
	{
		// Check if lightmap file exist.
		_finddata_t fd;
		intptr_t handle = GetPak()->FindFirst( lmFilename,&fd );
		if (handle == -1)
		{
			return false;
		}
		GetPak()->FindClose( handle );
	}

	// ---------------------------------------------------------------------------------------------
	// Reconstruct lightmap data from pszFileName
	// ---------------------------------------------------------------------------------------------
	PrintMessage("Loading lightmaps ...");

	// make sure the pak file in which this LM data resides is opened
	GetPak()->OpenPack( lmFilename.c_str() );

	FileHeader sHeader;
	size_t iNumItemsRead;
	std::map<CLMTextureToObjID, RenderLMData_AutoPtr> mapLMData;			// only used if inpIGLMs!=0

	if(nIGLMNumber)
	{
		if(NULL == pIGLMs)
		{
			PrintMessage("No GLMs to load lightmaps for");
			return false;
		}
	}
	else
	{
		assert(m_vLightPatches.empty());
		assert(m_vTexCoords.empty());
	}

	// Open file
	FILE* hFile = GetPak()->FOpen(pszFileName, "rb");
	if (hFile == NULL)
	{
		PrintMessage("Could not load lightmap file");
		return false;
	}
	AutoFileCloser _AutoClose (hFile);

	// Read header
	iNumItemsRead = GetPak()->FRead(&sHeader, 1, hFile);
	// Read LM texture data
	for (UINT iCurLMTexData = 0; iCurLMTexData < sHeader.iNumLM_Pairs; ++iCurLMTexData)
	{
		LMHeader LM;

		// width, height and the number of UVs
		if (1 != GetPak()->FRead(&LM, 1, hFile))
		{
			Warning ( "%s,Cannot read LM data header #%d", pszFileName, iCurLMTexData);
			PrintMessage("Cannot read Lightmap data header ");
			return false;
		}

		std::vector<CLMTextureToObjID > vGLM_IDs_UsingPatch;				// objects that use this lightmap texture

		if( LM.numGLMs )
		{
			vGLM_IDs_UsingPatch.resize (LM.numGLMs);

			// Read IDs of GLMs which use this texture pair
			if(sHeader.iVersion<8)	//just 32bit IDs
			{
				if (!GetPak()->FRead(reinterpret_cast<int32*>(&vGLM_IDs_UsingPatch[0]), LM.numGLMs, hFile))
				{
					FileWarning (0, pszFileName, "Cannot read LM data #%d", iCurLMTexData);
					PrintMessage("Cannot read Lightmap data");
					return false;
				}
				for(int a=LM.numGLMs-1;a>-1;a--)
					vGLM_IDs_UsingPatch[a]	=	CLMTextureToObjID(reinterpret_cast<int32*>(&vGLM_IDs_UsingPatch[0])[a],0);
			}
			else	//32bit IDs and additional subobject-indices
			{
				if (!GetPak()->FRead(&vGLM_IDs_UsingPatch[0], LM.numGLMs, hFile))
				{
					FileWarning (0, pszFileName, "Cannot read LM data #%d", iCurLMTexData);
					PrintMessage("Cannot read Lightmap data");
					return false;
				}
			}
		}

		std::vector<BYTE>vDomLightDir, vColorMap;
		if(nIGLMNumber)
		{
			if( LM.numGLMs )
			{
				// Create lightmap
				RenderLMData_AutoPtr pNewLM = 
					sHeader.iVersion == 2?
					CreateLightmap(strDirName, iCurLMTexData,LM.iWidth, LM.iHeight, false/*don't load occl maps*/) :
				CreateLightmap(strDirName, iCurLMTexData,LM.iWidth, LM.iHeight, true/*load occl maps*/);

				if (!pNewLM)
				{
					PrintMessage("CreateLightmap has failed");
					return false;
				}

				//if we have any textutre...
				//if( pNewLM->GetColorLerpTex() || pNewLM->GetHDRColorLerpTex() || pNewLM->GetDomDirectionTex() || pNewLM->GetOcclusionTex() || pNewLM->GetRAETex() )
				// Instance is assigned later when we get the texture coordinates. Store a
				// reference so we can later safely release each element
				for (std::vector<CLMTextureToObjID >::iterator itID=vGLM_IDs_UsingPatch.begin(); itID!=vGLM_IDs_UsingPatch.end(); itID++)
				{
					// Shouldn't happen, probably need to fix something if. Might be wrong LM data, just
					// delete the file or ignore the assert
					assert (mapLMData.find(*itID) == mapLMData.end());

					mapLMData[*itID] = pNewLM;
				}
			}
		}
		else
		{
			RawLMData* pRawLM = new RawLMData(LM.iWidth,LM.iHeight,vGLM_IDs_UsingPatch);

			char szPostfix[8];
			if(cbLoadTextures && LM.numGLMs )
			{
				sprintf(szPostfix, "%d.dds", iCurLMTexData);
				pRawLM->initFromDDS(RawLMData::TEX_COLOR, GetPak(), strDirName + "\\c" + szPostfix);
				pRawLM->initFromDDS(RawLMData::TEX_DOMDIR, GetPak(), strDirName + "\\d" + szPostfix);
				if(sHeader.iVersion > 2)//load occlusion maps too
				{
					pRawLM->initFromDDS(RawLMData::TEX_HDR, GetPak(), strDirName + "\\r" + szPostfix);
					pRawLM->initFromDDS(RawLMData::TEX_OCCLUSION, GetPak(), strDirName + "\\a" + szPostfix);
				}
				if(sHeader.iVersion > 6)//load RAE too
					pRawLM->initFromDDS(RawLMData::TEX_RAE, GetPak(), strDirName + "\\e" + szPostfix);
			}
			m_vLightPatches.push_back(pRawLM);
		}
	}
	// Read texture coordinate data
	for (UINT iNumTexCoordSets=0; iNumTexCoordSets<sHeader.iNumTexCoordSets; iNumTexCoordSets++)
	{
		UVSetHeader *pUVh(NULL);
		if(sHeader.iVersion >= 8)
			pUVh = new UVSetHeader8();
		else
			if(sHeader.iVersion >= 6)
				pUVh = new UVSetHeader6();
			else
				if(sHeader.iVersion >= 5)
					pUVh = new UVSetHeader5();
				else
					if(sHeader.iVersion >= 4)
						pUVh = new UVSetHeader4();
					else
						if(sHeader.iVersion >= 3)
							pUVh = new UVSetHeader3();
						else
							pUVh = new UVSetHeader();//old format
		assert(pUVh);
		std::auto_ptr<UVSetHeader> uvh(pUVh);
		// Read position of GLM which uses this texture coordinate set. FIX!
		int32 nUVSetHeaderSize;
		if( sHeader.iVersion>=8 )
			nUVSetHeaderSize = sizeof(UVSetHeader8);
		else
			if( sHeader.iVersion>=6 )
				nUVSetHeaderSize = sizeof(UVSetHeader6);
			else
				if( sHeader.iVersion>=5 )
					nUVSetHeaderSize = sizeof(UVSetHeader5);
				else
					if( sHeader.iVersion>=4 )
						nUVSetHeaderSize = sizeof(UVSetHeader4);
					else
						nUVSetHeaderSize = (sHeader.iVersion>=3)?sizeof(UVSetHeader3):sizeof(UVSetHeader);

		if (1 != GetPak()->FReadRaw(pUVh, nUVSetHeaderSize, 1, hFile))
		{
			PrintMessage("Could not read texture coordinates for lightmaps");
			return false;
		}
		SwapEndian(*pUVh);
		std::vector<TexCoord2Comp> vTexCoords;
		vTexCoords.resize(pUVh->numUVs);
		if (!GetPak()->FRead(&vTexCoords[0], pUVh->numUVs, hFile))
		{
			PrintMessage("Could not read texture coordinates for lightmaps");
			return false;
		}
#ifdef _DEBUG
		for (UINT iCurTexCrd=0; iCurTexCrd<vTexCoords.size(); iCurTexCrd++)
		{
			if(	vTexCoords[iCurTexCrd].s >= 0.0f && vTexCoords[iCurTexCrd].s <= 1.0f && 
				vTexCoords[iCurTexCrd].t >= 0.0f && vTexCoords[iCurTexCrd].t <= 1.0f )
				continue;

			FileWarning(0, pszFileName, "CLMSerializationManager::_Load: Lightmap texture coordinates out of range");
			break;
		}
#endif
		// Hand out data/references to the GLMs
		if(nIGLMNumber)
		{
			int32 nGLMi;
			for( nGLMi = 0; nGLMi < nIGLMNumber; ++nGLMi )
			{
				IRenderNode *pICurGLM = pIGLMs[ nGLMi ];

				// Correct GLM for this texture coordinate set ?
				if ( pICurGLM->GetEditorObjectId() == pUVh->nIdGLM )
				{
					std::map<CLMTextureToObjID, RenderLMData_AutoPtr>::iterator itRenderLMData = mapLMData.find(CLMTextureToObjID(pUVh->nIdGLM,sHeader.iVersion >= 8?reinterpret_cast<UVSetHeader8*>(pUVh)->m_nSubObjIdx:0));

					if (itRenderLMData == mapLMData.end())
					{
						// We shouldn't have a texture coordinate set for a GLM that has no matching LM object
						break;
					}

					int8 nFirstOcclusionChannel = 0;
					int8 nSubObjIdx	=	0;

					// Copy texture coordinates and hand out a reference to the lightmap object
					if(sHeader.iVersion >= 3 ) //&& (strcmp(pICurGLM->GetEntityClassName(), "Brush") == 0))
					{
						//create vector with light ids
						std::vector<std::pair<EntityId,EntityId> > vOcclIDs;
						unsigned char ucOcclCount;

						// Ambient Occlusion datas included...
						if(sHeader.iVersion >= 4 )
						{
							if( sHeader.iVersion >= 5)
							{
								if( sHeader.iVersion >= 6)
								{
									const UVSetHeader6 *puvh = static_cast<const UVSetHeader6*>(pUVh);

									//Occlusion - the new, EntityGUID based serialization...
									ucOcclCount = puvh->ucOcclCount;
									vOcclIDs.resize(ucOcclCount);
									IEntitySystem *pEntitySystem = GetSystem()->GetIEntitySystem();
									for(int i=0; i<ucOcclCount; ++i)
									{
										vOcclIDs[i].first = pEntitySystem->FindEntityByGuid( puvh->OcclIds[i].m_nGuid );
										vOcclIDs[i].second = puvh->OcclIds[i].m_nLightId;
									}
									nFirstOcclusionChannel = puvh->m_nFirstOcclusionChannel;
									if(sHeader.iVersion>=8)
										nSubObjIdx	=	static_cast<const UVSetHeader8*>(pUVh)->m_nSubObjIdx;
								}
								else
								{
									const UVSetHeader5 *puvh = static_cast<const UVSetHeader5*>(pUVh);

									//Occlusion - the new, EntityGUID based serialization...
									ucOcclCount = puvh->ucOcclCount;
									vOcclIDs.resize(ucOcclCount);
									IEntitySystem *pEntitySystem = GetSystem()->GetIEntitySystem();
									for(int i=0; i<ucOcclCount; ++i)
									{
										vOcclIDs[i].first = pEntitySystem->FindEntityByGuid( puvh->OcclIds[i].m_nGuid );
										vOcclIDs[i].second = puvh->OcclIds[i].m_nLightId;
									}
								}
							}
							else
							{
								const UVSetHeader4 *puvh = static_cast<const UVSetHeader4*>(pUVh);

								//Occlusion
								ucOcclCount = puvh->ucOcclCount;
								vOcclIDs.resize(ucOcclCount);
								for(int i=0; i<ucOcclCount; ++i)
								{
									vOcclIDs[i].first = puvh->OcclIds[2*i];							//old EntityId based method
									vOcclIDs[i].second = puvh->OcclIds[2*i+1];
								}
							}
						}
						else
						{
							//that was an other occlusion, in version 3.
							ucOcclCount = 0;
						}
						//CBrush *pBrush = reinterpret_cast<CBrush*>(pICurGLM);//safe due to GetEntityClassName
						pICurGLM->SetLightmap (itRenderLMData->second, (float *) &vTexCoords[0], vTexCoords.size(), ucOcclCount, /*vOcclIDs,*/ nFirstOcclusionChannel,nSubObjIdx);
					}
					else
						pICurGLM->SetLightmap (itRenderLMData->second, (float *) &vTexCoords[0], vTexCoords.size() );

					// Texture coordinates are per-instance
					break;
				}
			}
		}
		else	// !inpIGLMs
		{
			if( sHeader.iVersion >= 5)
			{
				if( sHeader.iVersion >= 6)
				{
					const UVSetHeader6 *puvh = static_cast<const UVSetHeader6*>(pUVh);
					std::vector<std::pair<EntityId,EntityId> > vOcclIDs;
					vOcclIDs.resize(puvh->ucOcclCount);
					IEntitySystem *pEntitySystem = GetSystem()->GetIEntitySystem();
					for(int i=0; i<puvh->ucOcclCount; ++i)
					{
						vOcclIDs[i].first = pEntitySystem->FindEntityByGuid( puvh->OcclIds[i].m_nGuid );
						vOcclIDs[i].second = puvh->OcclIds[i].m_nLightId;
					}

					if(sHeader.iVersion>=8)
						m_vTexCoords[CLMTextureToObjID(pUVh->nIdGLM,static_cast<const UVSetHeader8*>(pUVh)->m_nSubObjIdx)] = RawTexCoordData(vTexCoords, puvh->nHashGLM,vOcclIDs,puvh->m_nFirstOcclusionChannel);
					else
						m_vTexCoords[CLMTextureToObjID(pUVh->nIdGLM,0)] = RawTexCoordData(vTexCoords, puvh->nHashGLM,vOcclIDs,puvh->m_nFirstOcclusionChannel);
				}
				else
				{
					const UVSetHeader5 *puvh = static_cast<const UVSetHeader5*>(pUVh);
					std::vector<std::pair<EntityId,EntityId> > vOcclIDs;
					vOcclIDs.resize(puvh->ucOcclCount);
					IEntitySystem *pEntitySystem = GetSystem()->GetIEntitySystem();
					for(int i=0; i<puvh->ucOcclCount; ++i)
					{
						vOcclIDs[i].first = pEntitySystem->FindEntityByGuid( puvh->OcclIds[i].m_nGuid );
						vOcclIDs[i].second = puvh->OcclIds[i].m_nLightId;
					}

					m_vTexCoords[CLMTextureToObjID(pUVh->nIdGLM,0)] = RawTexCoordData(vTexCoords, pUVh->nHashGLM, vOcclIDs, 0);
				}
			}
			else
				m_vTexCoords[CLMTextureToObjID(pUVh->nIdGLM,0)] = RawTexCoordData(vTexCoords, pUVh->nHashGLM);
		}
	}

	PrintMessage(" ... Finished loading lightmaps (%d elements)", mapLMData.size());
	return true;
}

bool CLMSerializationManager::Load(const char *pszFileName, const bool cbLoadTextures) 
{
	return(_Load(pszFileName, NULL, 0, cbLoadTextures));
}

bool CLMSerializationManager::ApplyLightmapfile(const char *pszFileName, struct IRenderNode ** pIGLMs, int32 nIGLMNumber ) 
{
	return(_Load(pszFileName,pIGLMs,nIGLMNumber));
}

//////////////////////////////////////////////////////////////////////////
uint8* MemBufferCounter=0;
uint8* MemBufferCounterLimit=(uint8*)1;
int m_numMips=0;

void SaveCompessedMipmapLevel(const void *data, size_t size, void * userData)
{
	assert(MemBufferCounter+size<=MemBufferCounterLimit);
	assert( MemBufferCounter );
	uint8* src=(uint8*)data;
	for (uint32 i=0; i<size; i++) 
	{
		*MemBufferCounter=src[i];
		MemBufferCounter++;
	}
	m_numMips++;
}

unsigned int CLMSerializationManager::InitLMUpdate(const char *pszFilePath, const bool bAppend )
{
	if( bAppend )
	{
		_Load(pszFilePath, 0, false);
	}
	else
	{
		// ---------------------------------------------------------------------------------------------
		// Store the lightmap data added in pszFileName
		// ---------------------------------------------------------------------------------------------
		string strDirName = PathUtil::GetParentDirectory(pszFilePath);
		string strPakName = strDirName + "\\" LEVELLM_PAK_NAME;
		GetPak()->ClosePack(strPakName.c_str());
		// make sure the pak file in which this LM data resides is opened
		CrySetFileAttributes(strPakName.c_str(), FILE_ATTRIBUTE_NORMAL);
		ICryArchive_AutoPtr pPak = GetPak()->OpenArchive (strPakName.c_str(), ICryArchive::FLAGS_RELATIVE_PATHS_ONLY);
		if (!pPak)
		{
			Warning( "Lightmap Serializer Failed!\r\nCannot Open pak file %s for writing.",(const char*)strPakName );
			return NSAVE_RESULT::EPAK_FILE_OPEN_FAIL;
		}

		//---------------------------------------------------------
		//delete all old files.. only the new light/occlusion maps are correct - 
		//It's important, because the file help to determinate that a brush have an light/occlusion map or not...
		//--------------------------------------------------------
		int32 nIndex = 0;
		for(;;)
		{
			char fileName[10];
			//create filename
			sprintf(fileName, "c%d.dds", nIndex);
			if(pPak->FindFile(fileName) == NULL)
				break;
			pPak->RemoveFile( fileName );
			fileName[0] = 'h';
			pPak->RemoveFile( fileName );
			++nIndex;
		}
		nIndex = 0;
		for(;;)
		{
			char fileName[10];
			//create filename
			sprintf(fileName, "a%d.dds", nIndex);
			if(pPak->FindFile(fileName) == NULL)
				break;
			pPak->RemoveFile( fileName );
			fileName[0] = 'b';
			pPak->RemoveFile( fileName );
			++nIndex;
		}

		nIndex = 0;
		for(;;)
		{
			char fileName[10];
			//create filename
			sprintf(fileName, "e%d.dds", nIndex);
			if(pPak->FindFile(fileName) == NULL)
				break;
			pPak->RemoveFile( fileName );
			++nIndex;
		}

		//close and reopen...
		GetPak()->ClosePack(strPakName.c_str());
	}
	return NSAVE_RESULT::ESUCCESS;
}


unsigned int CLMSerializationManager::SaveDDSFiles(const char *pszFilePath, RawLMData *pCurRawLMData, const int32 nID )
{
	// ---------------------------------------------------------------------------------------------
	// Store the lightmap data added in pszFileName
	// ---------------------------------------------------------------------------------------------
	string strDirName = PathUtil::GetParentDirectory(pszFilePath);
	string strPakName = strDirName + "\\" LEVELLM_PAK_NAME;
	GetPak()->ClosePack(strPakName.c_str());
	// make sure the pak file in which this LM data resides is opened
	CrySetFileAttributes(strPakName.c_str(), FILE_ATTRIBUTE_NORMAL);
	ICryArchive_AutoPtr pPak = GetPak()->OpenArchive (strPakName.c_str(), ICryArchive::FLAGS_RELATIVE_PATHS_ONLY);
	if (!pPak)
		return NSAVE_RESULT::EPAK_FILE_OPEN_FAIL;

	char szName[64];
	sprintf(szName, "x%d.dds", nID );

	if(pCurRawLMData->m_bUseLightMaps)
	{
		uint32 size	= pCurRawLMData->m_ColorLerp4.GetSize();
		uint8* pSr0	= (uint8*)pCurRawLMData->m_ColorLerp4.GetData();
		uint8* pSr1	= (uint8*)pCurRawLMData->m_DomDirection3.GetData();
		CRY_DDS_HEADER* pDDS=(CRY_DDS_HEADER*)(pSr0+4);
		uint32 width	=	pDDS->dwWidth;
		uint32 height	=	pDDS->dwHeight;

		CRY_DDS_HEADER ddsh;

		if (0) 
		{//save uncompressed version
			szName[0] = 'c';
			pPak->UpdateFile (szName, pCurRawLMData->m_ColorLerp4.GetData(), pCurRawLMData->m_ColorLerp4.GetSize(), ICryArchive::METHOD_COMPRESS, ICryArchive::LEVEL_BEST);

		} else 
		{
			//-------------------------------------------------------------------------------
			//---- create DXT3-image for color-map ------------------------------------------
			//-------------------------------------------------------------------------------
			uint8* pColorMaps32	= (uint8*)pCurRawLMData->m_ColorLerp4.GetData();;
			uint8* pColorDXT3=(uint8*)malloc(size);
			MemBufferCounter = pColorDXT3;
			MemBufferCounterLimit	=	MemBufferCounter+size;
			m_numMips=0;

			memset( (void*)(&ddsh), 0, sizeof(ddsh) );
			*MemBufferCounter='D'; MemBufferCounter++;
			*MemBufferCounter='D'; MemBufferCounter++;
			*MemBufferCounter='S'; MemBufferCounter++;
			*MemBufferCounter=' '; MemBufferCounter++;
			ddsh.dwSize = sizeof(CRY_DDS_HEADER);
			ddsh.dwWidth	= width;
			ddsh.dwHeight = height;
			ddsh.dwMipMapCount = 0;
			ddsh.ddspf = DDSPF_DXT1;
			ddsh.dwHeaderFlags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP;
			ddsh.dwSurfaceFlags = DDS_SURFACE_FLAGS_TEXTURE | DDS_SURFACE_FLAGS_MIPMAP;

			uint8* hsrcDXT3=(uint8*)(&ddsh);
			for (uint32 i=0; i<sizeof(ddsh); i++) {
				*MemBufferCounter=hsrcDXT3[i];	MemBufferCounter++;
			}

			if(!GetSystem()->GetIRenderer()->DXTCompress( 
				&pColorMaps32[0x80], 
				width, 
				height, 
				eTF_DXT3,										//DXT-format
				false,											//use hardware
				0,												//mipmaps on/off
				4,												//bytes per pixel 
				Vec3(0,0,0),							// default luminance weighting
				SaveCompessedMipmapLevel	//callback
				))	return NSAVE_RESULT::EDXT_COMPRESS_FAIL;

			CRY_DDS_HEADER* pDXT3=(CRY_DDS_HEADER*)(pColorDXT3+4);
			pDXT3->dwMipMapCount = m_numMips;
			if (m_numMips==1) pDXT3->dwMipMapCount = 0;

			szName[0] = 'c';
			uint32 dxt3size=MemBufferCounter-pColorDXT3;
			pPak->UpdateFile (szName, pColorDXT3, dxt3size, ICryArchive::METHOD_COMPRESS, ICryArchive::LEVEL_BEST);

			szName[0] = 'h';
			pPak->UpdateFile (szName, pCurRawLMData->m_ColorLerp4.GetData(), pCurRawLMData->m_ColorLerp4.GetSize(), ICryArchive::METHOD_COMPRESS, ICryArchive::LEVEL_BEST);

			free(pColorDXT3);
		}
	}//m_bUseLightMaps

	//-------------------------------------------------------------------------------------------
	if(pCurRawLMData->m_bUseOcclusionMaps)
	{
		uint32 size	= pCurRawLMData->m_Occlusion.GetSize();
		uint8* pSr0	= (uint8*)pCurRawLMData->m_Occlusion.GetData();
		CRY_DDS_HEADER* pDDS=(CRY_DDS_HEADER*)(pSr0+4);
		uint32 width	=	pDDS->dwWidth;
		uint32 height	=	pDDS->dwHeight;
		CRY_DDS_HEADER ddsh;

		//-------------------------------------------------------------------------------
		//---- create 16Bit-image for ambient occlusion-map ------------------------------------------
		//-------------------------------------------------------------------------------
		uint8* pOcclMaps32	= (uint8*)pCurRawLMData->m_Occlusion.GetData();;
		uint8* pOccl16Bit =(uint8*)malloc(size);
		MemBufferCounter = pOccl16Bit;
		MemBufferCounterLimit	=	MemBufferCounter+size;
		m_numMips=0;

		memset( (void*)(&ddsh), 0, sizeof(ddsh) );
		*MemBufferCounter='D'; MemBufferCounter++;
		*MemBufferCounter='D'; MemBufferCounter++;
		*MemBufferCounter='S'; MemBufferCounter++;
		*MemBufferCounter=' '; MemBufferCounter++;
		ddsh.dwSize = sizeof(CRY_DDS_HEADER);
		ddsh.dwWidth	= width;
		ddsh.dwHeight = height;
		ddsh.dwMipMapCount = 0;
		ddsh.ddspf = DDSPF_A4R4G4B4;
		ddsh.dwHeaderFlags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP;
		ddsh.dwSurfaceFlags = DDS_SURFACE_FLAGS_TEXTURE | DDS_SURFACE_FLAGS_MIPMAP;

		uint8* hsrc16Bit=(uint8*)(&ddsh);
		for (uint32 i=0; i<sizeof(ddsh); i++) {
			*MemBufferCounter=hsrc16Bit[i];	MemBufferCounter++;
		}

		if(!GetSystem()->GetIRenderer()->DXTCompress( 
			&pOcclMaps32[0x80], 
			width, 
			height, 
			eTF_A4R4G4B4,										//DXT-format
			false,											//use hardware
			0,												//mipmaps on/off
			4,												//bytes per pixel 
			Vec3(0,0,0),							// default luminance weighting
			SaveCompessedMipmapLevel	//callback
			))	return NSAVE_RESULT::EDXT_COMPRESS_FAIL;

		CRY_DDS_HEADER* p16Bit=(CRY_DDS_HEADER*)(pOccl16Bit+4);
		p16Bit->dwMipMapCount = m_numMips;
		if (m_numMips==1) p16Bit->dwMipMapCount = 0;

		szName[0] = 'a';
		uint32 n16Bitsize=MemBufferCounter-pOccl16Bit;
		pPak->UpdateFile (szName, pOccl16Bit, n16Bitsize, ICryArchive::METHOD_COMPRESS, ICryArchive::LEVEL_BEST);

		szName[0] = 'b';
		pPak->UpdateFile (szName, pCurRawLMData->m_Occlusion.GetData(), pCurRawLMData->m_Occlusion.GetSize(), ICryArchive::METHOD_COMPRESS, ICryArchive::LEVEL_BEST);

		free(pOccl16Bit);
	}

	//-------------------------------------------------------------------------------------------
	if(pCurRawLMData->m_bUseRAE)
	{
		uint32 size	= pCurRawLMData->m_RAE.GetSize();
		uint8* pSr0	= (uint8*)pCurRawLMData->m_RAE.GetData();
		CRY_DDS_HEADER* pDDS=(CRY_DDS_HEADER*)(pSr0+4);
		uint32 width	=	pDDS->dwWidth;
		uint32 height	=	pDDS->dwHeight;
		CRY_DDS_HEADER ddsh;

		uint8* pRAEMaps32	= (uint8*)pCurRawLMData->m_RAE.GetData();;
		uint8* pRAE8Bit =(uint8*)malloc(size+sizeof(ddsh)+4 );			// DDS header + magic word
		MemBufferCounter = pRAE8Bit;
		MemBufferCounterLimit	=	MemBufferCounter+size;
		m_numMips=0;

		memset( (void*)(&ddsh), 0, sizeof(ddsh) );
		*MemBufferCounter='D'; MemBufferCounter++;
		*MemBufferCounter='D'; MemBufferCounter++;
		*MemBufferCounter='S'; MemBufferCounter++;
		*MemBufferCounter=' '; MemBufferCounter++;
		ddsh.dwSize = sizeof(CRY_DDS_HEADER);
		ddsh.dwWidth	= width;
		ddsh.dwHeight = height;
		ddsh.dwMipMapCount = 0;
//		ddsh.ddspf = DDSPF_L8;
		ddsh.ddspf = DDSPF_DXT1;
		ddsh.dwHeaderFlags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP;
		ddsh.dwSurfaceFlags = DDS_SURFACE_FLAGS_TEXTURE | DDS_SURFACE_FLAGS_MIPMAP;

		uint8* hsrc16Bit=(uint8*)(&ddsh);
		for (uint32 i=0; i<sizeof(ddsh); i++) {
			*MemBufferCounter=hsrc16Bit[i];	MemBufferCounter++;
		}

/*		uint16* pPixelMinMax0	=	new uint16[width*height/16];
		uint16* pPixelMinMax1	=	new uint16[width*height/16];

		uint32* pPixel	=	reinterpret_cast<uint32*>(&pRAEMaps32[0x80]); 
		for(uint32 y=0;y<height;y+=4)
			for(uint32 x=0;x<width;x+=4)
			{
				int Min,Max;
				Min=Max=pPixel[x+y*width];
				for(uint32 yy=0;yy<4;yy++)
					for(uint32 xx=0;xx<4;xx++)
					{
						if(pPixel[x+xx+(y+yy)*width]<Min)
							Min=pPixel[x+xx+(y+yy)*width];
						else
						if(pPixel[x+xx+(y+yy)*width]>Max)
							Max=pPixel[x+xx+(y+yy)*width];
					}
				Min&=0xff00;
				Max&=0xff00;
				Min>>=8+3;//simulate 5bit per chan
				Min=(Min<<3)|(Min>>2);//with decompression
				Max>>=8+3;//simulate 5bit per chan
				Max=(Max<<3)|(Max>>2);//with decompression
				if(Max==Min)
				{
					Max|=1;
					Min&=0xfe;
				}
				pPixelMinMax0[x/4+y/4*width/4]=Min|(Max<<8);
			}
		const int TRESHOLD	=	12;
		for(int32 Rounds=0;Rounds<64;Rounds++)
		{
			for(int32 y=0;y<height/4;y++)
				for(int32 x=0;x<width/4;x++)
				{
					int Min,Max,C;
					Min=pPixelMinMax0[y*width/4+x]&0xff;
					Max=pPixelMinMax0[y*width/4+x]>>8;
					for(int32 yy=-1;yy<2;yy++)
						for(int32 xx=-1;xx<2;xx++)
						{
							C=pPixelMinMax0[min(max(x+xx,0),(int)width/4-1)+min(max(y+yy,0),(int)height/4-1)*width/4];
							if((C&0xff)<Min)
								Min=C&0xff;
							if((C>>8)>Max)
								Max=C>>8;
						}
					C=pPixelMinMax0[y*width/4+x];
					if((C&0xff)>Min+TRESHOLD)
						Min=(C&0xff)-TRESHOLD;
					else
						Min=C&0xff;
					if((C>>8)<Max-TRESHOLD)
						Max=(C>>8)+TRESHOLD;
					else
						Max=C>>8;

					pPixelMinMax1[y*width/4+x]=Min|(Max<<8);
				}

			uint16* pSwap=pPixelMinMax0;
			pPixelMinMax0=pPixelMinMax1;
			pPixelMinMax1=pSwap;
		}



		for(int32 y=0;y<height;y+=4)
			for(int32 x=0;x<width;x+=4)
			{
				int Min,Max;
				Min=Max=pPixelMinMax0[x/4+y/4*width/4];
				Min&=0xff;
				Max>>=8;
				for(int32 yy=0;yy<4;yy++)
					for(int32 xx=0;xx<4;xx++)
					{
						uint32& Pixel=pPixel[x+xx+(y+yy)*width];
						f32 ScaledIntensity=static_cast<f32>(((Pixel>>8)&0xff)-Min);
						ScaledIntensity*=255.f/static_cast<f32>(Max-Min);
						Pixel=0xff000000;
						Pixel|=max(min(255,static_cast<int>(ScaledIntensity)),0)<<8;
						Pixel|=Min|(Max<<16);
					}
			}

		delete[] pPixelMinMax0;
		delete[] pPixelMinMax1;*/

		if(!GetSystem()->GetIRenderer()->DXTCompress( 
			&pRAEMaps32[0x80], 
			width, 
			height, 
			eTF_DXT1,									//DXT-format
			false,											//use hardware
			0,												//mipmaps on/off
			4,												//bytes per pixel 
			Vec3(1,1,1),							// not default luminance weighting, cause we average the gray that is saved in r g b
			SaveCompessedMipmapLevel	//callback
			))	return NSAVE_RESULT::EDXT_COMPRESS_FAIL;

		CRY_DDS_HEADER* p16Bit=(CRY_DDS_HEADER*)(pRAE8Bit+4);
		p16Bit->dwMipMapCount = m_numMips;
		if (m_numMips==1) p16Bit->dwMipMapCount = 0;

		szName[0] = 'e';
		uint32 n16Bitsize=MemBufferCounter-pRAE8Bit;
		pPak->UpdateFile (szName, pRAE8Bit, n16Bitsize, ICryArchive::METHOD_COMPRESS, ICryArchive::LEVEL_BEST);

//		szName[0] = 'f';
//		pPak->UpdateFile (szName, pCurRawLMData->m_RAE.GetData(), pCurRawLMData->m_RAE.GetSize(), ICryArchive::METHOD_COMPRESS, ICryArchive::LEVEL_BEST);

		free(pRAE8Bit);
	}

	if(pCurRawLMData->m_bUseDot3Maps)
	{
		szName[0] = 'd';
		pPak->UpdateFile (szName, pCurRawLMData->m_DomDirection3.GetData(), pCurRawLMData->m_DomDirection3.GetSize(), ICryArchive::METHOD_COMPRESS, ICryArchive::LEVEL_BEST);
	}

	if(pCurRawLMData->m_bUseHDRMaps)
	{
		szName[0] = 'r';
		pPak->UpdateFile (szName, pCurRawLMData->m_HDRColorLerp4.GetData(), pCurRawLMData->m_HDRColorLerp4.GetSize(), ICryArchive::METHOD_COMPRESS, ICryArchive::LEVEL_BEST);
	}

	return NSAVE_RESULT::ESUCCESS;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
// Load the falselight mapping for objects - which affected by not usable lights
//----------------------------------------------------------------------------------------------------------------------------------------------------------
bool CLMSerializationManager::LoadFalseLight(const char *pszFileName, struct IRenderNode ** pIGLMs, int32 nIGLMNumber)
{
/*	if( 0 == nIGLMNumber )
		return true;

	if( NULL == pIGLMs )
	{
		PrintMessage("No GLMs to load falselights for");
		return false;
	}

	string strDirName = PathUtil::GetParentDirectory(pszFileName);
	string lmFilename = (strDirName + "\\" LEVELLM_PAK_NAME);
	{
		// Check if lightmap file exist.
		_finddata_t fd;
		intptr_t handle = GetPak()->FindFirst( lmFilename,&fd );
		if (handle == -1)
		{
			return false;
		}
		GetPak()->FindClose( handle );
	}

	PrintMessage("Loading falselights ...");

	// make sure the pak file in which this LM data resides is opened
	GetPak()->OpenPack( lmFilename.c_str() );

	// Open file
	FILE* hFile = GetPak()->FOpen(pszFileName, "rb");
	if (hFile == NULL)
	{
		PrintMessage("Could not load falselight file");
		return false;
	}
	AutoFileCloser _AutoClose (hFile);

	IEntitySystem *pEntitySystem = GetSystem()->GetIEntitySystem();
	SEntitySlotInfo slotInfo;
	size_t iNumItemsRead;
	LMFalseLightFileHeader sHeader;
	// Read header
	iNumItemsRead = GetPak()->FRead(&sHeader, 1, hFile);

	EntityGUID nGUIDs[32];
	for (UINT i = 0; i < sHeader.iNumObjects; ++i )
	{
		uint32 nObjectID;
		iNumItemsRead = GetPak()->FRead(&nObjectID, 1, hFile);
		
		int32 nGLMi;
		for( nGLMi = 0; nGLMi < nIGLMNumber; ++nGLMi )
		{
			IRenderNode *pICurGLM = pIGLMs[ nGLMi ];

			// Correct GLM for this set
			if ( pICurGLM->GetEditorObjectId() == nObjectID )
			{
				int32  nLightNum;
				iNumItemsRead = GetPak()->FRead(&nLightNum, 1, hFile);
				iNumItemsRead = GetPak()->FReadRaw(nGUIDs, sizeof(EntityGUID)*nLightNum, 1, hFile);

				//Attach..
				SAFE_DELETE( pICurGLM->m_pFalseLights );
				pICurGLM->m_pFalseLights = new PodArray<struct ILightSource*>;
				if( NULL == pICurGLM->m_pFalseLights )
					continue;

				for( uint32 i = 0; i < nLightNum; ++i )
				{
					IEntity* pEnt = pEntitySystem->GetEntity( pEntitySystem->FindEntityByGuid( nGUIDs[i] ) );
					if (pEnt)
					{
						//clear
						slotInfo.pLight = NULL;

						//slot 1 is for light, but try to search if not found
						if( false == pEnt->GetSlotInfo(1, slotInfo) || NULL == slotInfo.pLight )
							if( false == pEnt->GetSlotInfo(0, slotInfo) )
							{
								assert(0);
							}

							ILightSource *pLightSource = (ILightSource *)slotInfo.pLight;
							if( pLightSource )
								pICurGLM->m_pFalseLights->Add( pLightSource->GetLightProperties().m_pOwner );
					}
				}
			}
		}
	}

	PrintMessage("Finished loading falselights ...");
	*/
	return true;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
// Save the falselight mapping
//----------------------------------------------------------------------------------------------------------------------------------------------------------
unsigned int CLMSerializationManager::SaveFalseLight(const char *pszFilePath, const int nObjectNumber,const struct sFalseLightSerializationStructure* pFalseLightList )
{
	string strDirName = PathUtil::GetParentDirectory(pszFilePath);
	const char* pFileName = pszFilePath + (strDirName.empty()?0:strDirName.length()+1);
	string strPakName = strDirName + "\\" LEVELLM_PAK_NAME;
	GetPak()->ClosePack(strPakName.c_str());
	// make sure the pak file in which this LM data resides is opened
	CrySetFileAttributes(strPakName.c_str(), FILE_ATTRIBUTE_NORMAL);
	ICryArchive_AutoPtr pPak = GetPak()->OpenArchive (strPakName.c_str(), ICryArchive::FLAGS_RELATIVE_PATHS_ONLY);
	if (!pPak)
		return NSAVE_RESULT::EPAK_FILE_OPEN_FAIL;

	LMFalseLightFileHeader sHeader;
	sHeader.iNumObjects = nObjectNumber;

	if (nObjectNumber == 0)
	{
		pPak->RemoveFile (pFileName);
		return NSAVE_RESULT::ESUCCESS;
	}

	CTempFile fMem;
	fMem.Write (sHeader);

	IEntitySystem *pEntitySystem = GetSystem()->GetIEntitySystem();

	for( int i = 0; i < nObjectNumber; ++i )
	{
		fMem.Write (pFalseLightList[i].m_nObjectID);

		int32 nLightNum = 0;
		for( int j = 0; j < pFalseLightList[i].m_nLightNumber; ++j )
		{
			IEntity *pEntity = pEntitySystem->GetEntity( pFalseLightList[i].m_Lights[j] );
			if(pEntity )
				++nLightNum;
		}
		fMem.Write(nLightNum);
		for( int32 j = 0; j < nLightNum; ++j )
		{
			IEntity *pEntity = pEntitySystem->GetEntity( pFalseLightList[i].m_Lights[j] );
			if(pEntity )
			{
				EntityGUID nGUID = pEntity->GetGuid();
				fMem.Write (nGUID);
			}
		}
	}

	if(0 == pPak->UpdateFile(pFileName, fMem.GetData(), fMem.GetSize()))
		return NSAVE_RESULT::ESUCCESS;
	else
		return NSAVE_RESULT::EPAK_FILE_UPDATE_FAIL;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------------------------------------------
unsigned int CLMSerializationManager::Save(const char *pszFilePath, LMGenParam rParam, const bool cbAppend )
{ 
	// ---------------------------------------------------------------------------------------------
	// Store the lightmap data added in pszFileName
	// ---------------------------------------------------------------------------------------------
	string strDirName = PathUtil::GetParentDirectory(pszFilePath);
	const char* pFileName = pszFilePath + (strDirName.empty()?0:strDirName.length()+1);
	string strPakName = strDirName + "\\" LEVELLM_PAK_NAME;
	GetPak()->ClosePack(strPakName.c_str());
	// make sure the pak file in which this LM data resides is opened
	CrySetFileAttributes(strPakName.c_str(), FILE_ATTRIBUTE_NORMAL);
	ICryArchive_AutoPtr pPak = GetPak()->OpenArchive (strPakName.c_str(), ICryArchive::FLAGS_RELATIVE_PATHS_ONLY);
	if (!pPak)
		return NSAVE_RESULT::EPAK_FILE_OPEN_FAIL;

	FileHeader sHeader;
	std::map<CLMTextureToObjID,RawTexCoordData>::iterator itTexCrdData;
	RawLMData *pCurRawLMData = NULL;
	RawTexCoordData *pCurRawTexCrdData = NULL;

	// Write header
	sHeader.iNumLM_Pairs = m_vLightPatches.size();
	sHeader.iNumTexCoordSets = m_vTexCoords.size();

	int nSize = sizeof(sHeader)+sizeof(LMHeader)*sHeader.iNumLM_Pairs;
	for (UINT iCurLMTexData = 0; iCurLMTexData < sHeader.iNumLM_Pairs; ++iCurLMTexData)
	{
		pCurRawLMData = m_vLightPatches[iCurLMTexData];
		nSize += pCurRawLMData->vGLM_IDs_UsingPatch.size()*sizeof(CLMTextureToObjID);
	}

	for (itTexCrdData=m_vTexCoords.begin(); itTexCrdData!=m_vTexCoords.end(); itTexCrdData++)
	{
		pCurRawTexCrdData = &((*itTexCrdData).second);
		nSize += sizeof(UVSetHeader8) + sizeof(TexCoord2Comp)*pCurRawTexCrdData->vTexCoords.size();
	}

	PrintMessage("RAM Serializer file size: %d byte\n", nSize);

#ifdef SEGMENTED_SAVE
	{
		pPak->StartContinuousFileUpdate( pFileName, nSize );
		CTempFile fMem;
		fMem.Write(sHeader);
		if(0 != pPak->UpdateFileContinuousSegment(pFileName, nSize, fMem.GetData(), fMem.GetSize() ) )
			return NSAVE_RESULT::EPAK_FILE_UPDATE_FAIL;
	}
#else
	CTempFile fMem(nSize);
	fMem.Write(sHeader);
#endif
	// Write LM texture data
	for (UINT iCurLMTexData = 0; iCurLMTexData < sHeader.iNumLM_Pairs; ++iCurLMTexData)
	{
#ifdef SEGMENTED_SAVE
		CTempFile fMem;
#endif

		pCurRawLMData = m_vLightPatches[iCurLMTexData];
		LMHeader LM;
		LM.iHeight = pCurRawLMData->m_dwHeight;
		LM.iWidth  = pCurRawLMData->m_dwWidth;
		LM.numGLMs = pCurRawLMData->vGLM_IDs_UsingPatch.size();
		fMem.Write(LM);

		if( LM.numGLMs )
			fMem.Write(&pCurRawLMData->vGLM_IDs_UsingPatch[0], LM.numGLMs);

#ifdef SEGMENTED_SAVE
		if(0 != pPak->UpdateFileContinuousSegment(pFileName, nSize, fMem.GetData(), fMem.GetSize() ) )
			return NSAVE_RESULT::EPAK_FILE_UPDATE_FAIL;
#endif
	}

	// Write texture coordinate data
	std::map<std::pair<int,int>, int> DebugMap;
	for (itTexCrdData=m_vTexCoords.begin(); itTexCrdData!=m_vTexCoords.end(); itTexCrdData++)
	{
#ifdef SEGMENTED_SAVE
		CTempFile fMem;
#endif

		UVSetHeader8 uvh;
		pCurRawTexCrdData = &((*itTexCrdData).second);
		uvh.nIdGLM				= (*itTexCrdData).first.ID();
		uvh.m_nSubObjIdx	=	(*itTexCrdData).first.SubObjIdx();
		assert(DebugMap.find(std::pair<int,int>(uvh.nIdGLM,uvh.m_nSubObjIdx)) == DebugMap.end());
		DebugMap[std::pair<int,int>(uvh.nIdGLM,uvh.m_nSubObjIdx)]	=	0;
		uvh.nHashGLM = pCurRawTexCrdData->m_dwHashValue;
		uvh.numUVs   = pCurRawTexCrdData->vTexCoords.size();
		uvh.ucOcclCount = pCurRawTexCrdData->vOcclIDs.size();
		uvh.m_nFirstOcclusionChannel = pCurRawTexCrdData->m_nFirstOcclusionChannel;

		IEntitySystem *pEntitySystem = GetSystem()->GetIEntitySystem();
		for(int i=0; i<uvh.ucOcclCount; ++i)
		{
			IEntity *pEntity = pEntitySystem->GetEntity( pCurRawTexCrdData->vOcclIDs[i].first );
			assert(pEntity);
			if(pEntity )
			{
				uvh.OcclIds[i].m_nGuid = pEntity->GetGuid();//real entity id
				uvh.OcclIds[i].m_nLightId = pCurRawTexCrdData->vOcclIDs[i].second;//light index in statlights
			}
			else
			{
				uvh.OcclIds[i].m_nGuid = 0;
				uvh.OcclIds[i].m_nLightId = 0;
			}
		}

		fMem.Write (uvh);
		fMem.Write(&pCurRawTexCrdData->vTexCoords[0], uvh.numUVs);

#ifdef SEGMENTED_SAVE
		if(0 != pPak->UpdateFileContinuousSegment(pFileName, nSize, fMem.GetData(), fMem.GetSize() ) )
			return NSAVE_RESULT::EPAK_FILE_UPDATE_FAIL;
#endif

	}

#ifdef SEGMENTED_SAVE
	return NSAVE_RESULT::ESUCCESS;
#else
	if(0 == pPak->UpdateFile(pFileName, fMem.GetData(), fMem.GetSize()))
		return NSAVE_RESULT::ESUCCESS;
	else
		return NSAVE_RESULT::EPAK_FILE_UPDATE_FAIL;
#endif
}

#ifdef WIN64
#pragma warning( pop )									//AMD Port
#endif

uint32 CLMSerializationManager::GetHashValue( const std::pair<int32,int32> iniGLM_ID_UsingTexCoord ) const
{
	std::map<CLMTextureToObjID,RawTexCoordData>::const_iterator itTexCrdData;

	for (itTexCrdData=m_vTexCoords.begin(); itTexCrdData!=m_vTexCoords.end(); itTexCrdData++)
	{
		const CLMTextureToObjID& iGLM_ID_UsingTexCoord=(*itTexCrdData).first;

		if(iGLM_ID_UsingTexCoord==iniGLM_ID_UsingTexCoord)
		{
			const RawTexCoordData &rCurRawTexCrdData = (*itTexCrdData).second;

			return(rCurRawTexCrdData.m_dwHashValue);
		}
	}	
	return(0x12341234);		// object not found
}

RenderLMData * CLMSerializationManager::CreateLightmap(const string& strDirPath, int nItem, UINT iWidth, UINT iHeight, const bool cbLoadHDRMaps, const bool cbLoadOcclMaps)
{
	// ---------------------------------------------------------------------------------------------
	// Create a DOT3 Lightmap object
	// ---------------------------------------------------------------------------------------------

	IRenderer *pIRenderer = GetSystem()->GetIRenderer();
	int /*iColorLerpTex = 0, iHDRColorLerpTex = 0, iDomDirectionTex = 0, iOcclusionTex = 0,*/ iRAETex = 0;

	char szPostfix[8];
	sprintf(szPostfix, "%d.dds", nItem);

		ITexture *pRAETexture;
		pRAETexture = pIRenderer->EF_LoadTexture((strDirPath + "\\e" + szPostfix).c_str(), FT_TEX_LM | FT_DONT_ANISO | FT_STATE_CLAMP, eTT_2D);
		if (pRAETexture && pRAETexture->IsTextureLoaded())
			iRAETex = pRAETexture->GetTextureID();
		else
		{
			SAFE_RELEASE(pRAETexture);
			CryWarning(VALIDATOR_MODULE_3DENGINE,VALIDATOR_ERROR, "Error: Could not load RAM map (%s)",(strDirPath + "\\e" + szPostfix).c_str());
		}

	return new RenderLMData(pIRenderer, /*iColorLerpTex, iHDRColorLerpTex, iDomDirectionTex, iOcclusionTex,*/ iRAETex);
}

typedef LMStatLightFileHeader LightFileHeader;

bool CLMSerializationManager::ExportDLights(const char *pszFilePath, const CDLight **ppLights, UINT iNumLights, bool bNewZip) const
{
	// ---------------------------------------------------------------------------------------------
	// Serialize dynamic lights into a file
	// ---------------------------------------------------------------------------------------------
	string strDirName = PathUtil::GetParentDirectory(pszFilePath);
	const char* pFileName = pszFilePath + (strDirName.empty()?0:strDirName.length()+1);
	string strPakName = strDirName + "\\" LEVELLM_PAK_NAME;
	GetPak()->ClosePack(strPakName.c_str());
	// make sure the pak file in which this LM data resides is opened
	CrySetFileAttributes(strPakName.c_str(), FILE_ATTRIBUTE_NORMAL);
	if (!bNewZip)
		GetPak()->ClosePack( strPakName.c_str() );
	ICryArchive_AutoPtr pPak = GetPak()->OpenArchive (strPakName.c_str(), ICryArchive::FLAGS_RELATIVE_PATHS_ONLY|(bNewZip?ICryArchive::FLAGS_CREATE_NEW:0));
	if (!pPak)
		return false;

	LightFileHeader sHeader;
	UINT iCurLight;
	CTempFile fMem;

	if (iNumLights == 0)
	{
		pPak->RemoveFile (pFileName);
		return true;
	}

//	assert(!IsBadReadPtr(ppLights, sizeof(CDLight *) * iNumLights));

	sHeader.iNumDLights = iNumLights;
	fMem.Write (sHeader);

	for (iCurLight=0; iCurLight<iNumLights; iCurLight++)
	{
		fMem.Write(*(ppLights[iCurLight]));

		int nFlags = ppLights[iCurLight]->m_pLightImage ? ppLights[iCurLight]->m_pLightImage->GetFlags() : 0;
		fMem.Write (nFlags);

		ITexture* pLightImg = ppLights[iCurLight]->m_pLightImage;
		WriteString(pLightImg? pLightImg->GetName() : "" , fMem);

		IShader* pShader = ppLights[iCurLight]->m_Shader.m_pShader;
		WriteString(pShader ? pShader->GetName() : "", fMem);
	}
	return 0 == pPak->UpdateFile (pFileName, fMem.GetData(), fMem.GetSize());
}

bool CLMSerializationManager::LoadDLights(const char *pszFileName, CDLight **&ppLightsOut, UINT &iNumLightsOut) const
{
	return true;
}

// writes the mips to the given DDS
// gets the highest level MIP from the dds itself (to the moment there must be the 
// MIP written to the end of dds file). Returns the number of mips generated
// (including already given highest-level mip)
unsigned GenerateDDSMips( CTempFile& dds, int wdt, int hgt)
{
	int wd;
	int i, j;
	int num;

	// the size of all MIPs including the highest-level (given) one
	unsigned numBytes = 0;
	int w = wdt;
	int h = hgt;
	while (w || h)
	{
		if (w == 0) w = 1;
		if (h == 0) h = 1;
		numBytes += w * h * 4;
		w >>= 1;
		h >>= 1;
	}
	// the source mip offset: will be changed as the mips are generated sequentially
	int nOffsetSrcMip = dds.GetSize() - wdt*hgt*4;
	if (nOffsetSrcMip < 0)
		return 0;
	dds.SetSize(nOffsetSrcMip + numBytes);
	
	num = 1;                 // number of mips
	byte* src = (byte*)dds.GetData() + nOffsetSrcMip;
	byte* dst = src + wdt*hgt*4; // pointer to next mip
	wd = wdt<<2;			     // width of one row of the source mip
	wdt >>= 1;						 // next mip dimension
	hgt >>= 1;
	while (wdt || hgt)
	{
		if (wdt < 1)
			wdt = 1;
		if (hgt < 1)
			hgt = 1;
		byte* src1 = src;
		byte* dst1 = dst;
		for (i=0; i<hgt; i++)	 // for all rows in the NEW mip
		{
			byte *src2 = src1;	 // running pointer to the previous mip's 2x2 block
			for (j=0; j<wdt; j++)
			{
				assert (dst1 < (byte*)dds.GetData() + dds.GetSize());
				dst1[0] = (src2[0]+src2[4]+src2[wd]+src2[wd+4])>>2;
				dst1[1] = (src2[1]+src2[5]+src2[wd+1]+src2[wd+5])>>2;
				dst1[2] = (src2[2]+src2[6]+src2[wd+2]+src2[wd+6])>>2;
				dst1[3] = (src2[3]+src2[7]+src2[wd+3]+src2[wd+7])>>2;
				dst1 += 4;         // run the target pointer to the next pixel
				src2 += 8;        // run the source pointer to the next 2x2 block
			}
			src1 += wd<<1;
		}
		src = dst;
		dst = dst1;
		num++;
		wd = wdt<<2;			     // width of one row of the source mip
		wdt >>= 1;						 // next mip dimension
		hgt >>= 1;
	}
	assert (dst == (byte*)dds.GetData() + dds.GetSize());
	return num;
}


static CTempFile* g_pDDSTarget;
static unsigned int g_numMips;
static bool DDSCompressCallback(void * data, int miplevel, uint32 size)
{
	g_pDDSTarget->WriteData(data, size);
	return false;
}

// initializes from raw bitmaps
void CLMSerializationManager::RawLMData::initFromBMP (BitmapEnum t, const void* pSource)
{
	CTempFile* pBMP;
	switch (t) 
	{
		case TEX_COLOR:		pBMP = &m_ColorLerp4; break;
		case TEX_DOMDIR:	pBMP = &m_DomDirection3; break;
		case TEX_HDR:		  pBMP = &m_HDRColorLerp4; break;
		case TEX_OCCLUSION: pBMP = &m_Occlusion; break;
		case TEX_RAE: pBMP = &m_RAE; break;
		default: return;
	}

	int nBytesPerPixel = 4;
	int nBytesReservedPerPixel = 4;
	switch(t)
	{
		case TEX_COLOR:		nBytesPerPixel = 4; nBytesReservedPerPixel = 4; break;
#ifdef USE_DOT3_ALPHA
		case TEX_DOMDIR:	nBytesPerPixel = 4; nBytesReservedPerPixel = 4; break;
#else
		case TEX_DOMDIR:	nBytesPerPixel = 3; nBytesReservedPerPixel = 4; break;
#endif
		case TEX_HDR: 		nBytesPerPixel = 4; nBytesReservedPerPixel = 4; break;
		case TEX_OCCLUSION: nBytesPerPixel = 4; nBytesReservedPerPixel = 4; break;
		case TEX_RAE: nBytesPerPixel = 4; nBytesReservedPerPixel = 4; break;
	}

	pBMP->Clear();
	pBMP->Reserve (sizeof(uint32) + sizeof(CRY_DDS_HEADER) + m_dwWidth*m_dwHeight*nBytesReservedPerPixel);

	uint32 dwMagic = MAKEFOURCC('D','D','S',' ');
	pBMP->Write( dwMagic);

	CRY_DDS_HEADER ddsh;
	ZeroStruct(ddsh);
	ddsh.dwSize = sizeof(CRY_DDS_HEADER);
	ddsh.dwWidth = m_dwWidth;
	ddsh.dwHeight = m_dwHeight;
	if(t == TEX_DOMDIR || t == TEX_COLOR || t == TEX_HDR || t == TEX_OCCLUSION || t == TEX_RAE)
	{
		ddsh.dwHeaderFlags = DDS_HEADER_FLAGS_TEXTURE | DDS_HEADER_FLAGS_MIPMAP;
		ddsh.dwSurfaceFlags = DDS_SURFACE_FLAGS_TEXTURE | DDS_SURFACE_FLAGS_MIPMAP;
		ddsh.dwMipMapCount = 1;
	}
	else
	{
		ddsh.dwHeaderFlags = DDS_HEADER_FLAGS_TEXTURE;
		ddsh.dwSurfaceFlags = DDS_SURFACE_FLAGS_TEXTURE;
		ddsh.dwMipMapCount = 0;
	}
	unsigned nOffsetDDSH = pBMP->GetSize();

	if(t == TEX_DOMDIR || t == TEX_COLOR || t == TEX_HDR || t == TEX_OCCLUSION || t == TEX_RAE)
		ddsh.ddspf = DDSPF_A8R8G8B8;
	else
		ddsh.ddspf = DDSPF_A4R4G4B4;

	pBMP->Write (ddsh);
	if(t == TEX_DOMDIR || t == TEX_COLOR || t == TEX_HDR || t == TEX_OCCLUSION || t == TEX_RAE)
	{
		for (unsigned i = 0; i < m_dwWidth*m_dwHeight; ++i)
		{
			const char* pSrc = ((const char*)pSource)+ i * nBytesPerPixel;
			pBMP->Write<char>(pSrc[0]);
			pBMP->Write<char>(pSrc[1]);
			pBMP->Write<char>(pSrc[2]);
			pBMP->Write<char>((nBytesPerPixel == 4) ? pSrc[3] : (char)0xFF);
		}
		unsigned numMips = GenerateDDSMips(*pBMP, m_dwWidth, m_dwHeight);
		((CRY_DDS_HEADER*)((byte*)pBMP->GetData() + nOffsetDDSH))->dwMipMapCount = numMips;
	}
	else
	{
		for (unsigned i = 0; i < m_dwWidth*m_dwHeight; ++i)
		{
			const char* pSrc = ((const char*)pSource)+ i * nBytesPerPixel;//data already preswizzled into BGRA each 4 bit
			pBMP->Write<char>(pSrc[0]);
			pBMP->Write<char>(pSrc[1]);
		}
	}
}


// initializes from files
bool CLMSerializationManager::RawLMData::initFromDDS (BitmapEnum t, ICryPak* pPak, const string& szFileName)
{
	FILE* f = pPak->FOpen (szFileName.c_str(), "rb");
	if (!f)
		return false;

	AutoFileCloser _AC(f);
	if (pPak->FSeek(f, 0, SEEK_END))
		return false;
	long nLength = pPak->FTell(f);
	if (nLength <= 0)
		return false;

	if (pPak->FSeek(f, 0, SEEK_SET))
		return false;

	CTempFile* pBMP;
	switch (t)
	{
		case TEX_COLOR:		pBMP = &m_ColorLerp4;		break;
		case TEX_DOMDIR:	pBMP = &m_DomDirection3;	break;
		case TEX_HDR: 		pBMP = &m_HDRColorLerp4;			break;
		case TEX_OCCLUSION: 		pBMP = &m_Occlusion;			break;
		case TEX_RAE: 		pBMP = &m_RAE;			break;
		default: return false;
	}
	pBMP->Init(nLength);
	if (pPak->FReadRaw(pBMP->GetData(), nLength, 1, f) != 1)
	{
		pBMP->Init(0);
		return false;
	}
	return true;
}
