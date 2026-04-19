////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   LoaderDBA.cpp
//  Version:     v1.00
//  Created:     31/08/2006 by Alexey Medvedev.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <StringUtils.h>
#include "CryPath.h"

#include "ControllerPQ.h"
#include "ControllerOpt.h"
#include <CGFContent.h>
#include "LoaderDBA.h"
#include "crc32.h"


CLoaderDBA::CLoaderDBA()
{
	m_pDatabaseInfo = 0;
	m_pChunkFile = 0;
	m_iRefCounter = 0;
}

CLoaderDBA::~CLoaderDBA()
{
	if (m_pDatabaseInfo)
		delete m_pDatabaseInfo;

	if (m_pChunkFile)
		m_pChunkFile->Release();
}

CInternalDatabaseInfo* CLoaderDBA::LoadDBA(const char *filename)
{
	m_pChunkFile = g_pI3DEngine->CreateChunkFile(true);
	//FIXME:
	CInternalDatabaseInfo* pDB = LoadDBA( filename, m_pChunkFile);
	m_pChunkFile->Release();
	m_pChunkFile = 0;

	return pDB;
}

CInternalDatabaseInfo* CLoaderDBA::LoadDBA( const char *filename,IChunkFile * pChunkFile)
{
	string strPath = filename;
	CryStringUtils::UnifyFilePath(strPath);
	string fileExt = PathUtil::GetExt(strPath);

	m_Filename = filename;
	m_pChunkFile = pChunkFile;

	if (!pChunkFile->Read( filename ))
	{
		m_LastError = pChunkFile->GetLastError();
		return 0;
	}

	// Load mesh from chunk file.
	if (pChunkFile->GetFileHeader().Version != GeomFileVersion) 
	{
		m_LastError.Format("Bad DBA file version: %s", m_Filename.c_str() );
		return 0;
	}

	if (pChunkFile->GetFileHeader().FileType != FileType_Geom) 
	{
		if (pChunkFile->GetFileHeader().FileType != FileType_Anim) 
		{
			m_LastError.Format("Illegal File Type for .dba file: %s", m_Filename.c_str() );
			return 0;
		}
	}

	m_pDatabaseInfo = new CInternalDatabaseInfo;

	if (!LoadChunks())
	{
		delete m_pDatabaseInfo;
		m_pDatabaseInfo = 0;

	} 

	return m_pDatabaseInfo;

}

bool CLoaderDBA::LoadChunks()
{
	uint32 numChunck = m_pChunkFile->NumChunks();

	for (uint32 i=0; i<numChunck; i++)
	{

		const CHUNK_HEADER &hdr = m_pChunkFile->GetChunkHeader(i);

		switch (hdr.ChunkType)
		{
			//---------------------------------------------------------
			//---       chunks for CGA-objects  -----------------------
			//---------------------------------------------------------

		case ChunkType_Controller:
			if (!ReadControllers( m_pChunkFile->GetChunk(i)) )
				return false;
			break;
		}
	}

	for (uint32 i = 0, end = m_pDatabaseInfo->m_arrStartDirs.size() ; i < end; ++i)
	{
		m_pDatabaseInfo->m_Headers[i]->m_StartPosition = m_pDatabaseInfo->m_arrStartDirs[i];
	}

	m_pDatabaseInfo->m_arrStartDirs.clear();

	return true;
}

bool CLoaderDBA::ReadController900 (IChunkFile::ChunkDesc *pChunkDesc  )
{
	CONTROLLER_CHUNK_DESC_0900* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0900*)pChunkDesc->data;
	SwapEndian(*pCtrlChunk);

	m_pDatabaseInfo->m_iTotalControllers = 0;
	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController. Init. Memstat %i", (int)(info.allocated - info.freed));
	}

	m_pDatabaseInfo->m_AnimationNames.resize(pCtrlChunk->numAnims);
	m_pDatabaseInfo->m_Headers.resize(pCtrlChunk->numAnims);

	m_pDatabaseInfo->m_arrKeyTimes.resize(pCtrlChunk->numKeyTime);
	m_pDatabaseInfo->m_arrPositionTracks.resize(pCtrlChunk->numKeyPos);
	m_pDatabaseInfo->m_arrRotationTracks.resize(pCtrlChunk->numKeyRot);

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController. TracksResize. Memstat %i", (int)(info.allocated - info.freed));
	}

	std::vector<uint16> sizesKeyTimes;
	std::vector<uint8> formatKeyTimes;

	char *pData = (char*)(pCtrlChunk+1);

	uint32 keyTime = pCtrlChunk->numKeyTime;

	sizesKeyTimes.resize(keyTime);
	formatKeyTimes.resize(keyTime);

	// Key times info!
	memcpy(&sizesKeyTimes[0], pData, sizesKeyTimes.size() * sizeof(uint16));
	SwapEndian(&sizesKeyTimes[0], sizesKeyTimes.size());
	pData += keyTime * sizeof(uint16);

	memcpy(&formatKeyTimes[0], pData, formatKeyTimes.size() * sizeof(uint8));
	pData += keyTime * sizeof(uint8);
	// copy raw data


	//	char * pStat(pData);

	m_pDatabaseInfo->m_arrKeyTimesArray.resize(eBitset + 1);

	int stat[eBitset + 1];
	memset(stat, 0, sizeof(stat));

	for(uint32 k = 0; k < pCtrlChunk->numKeyTime; ++k)
	{
		++stat[formatKeyTimes[k]];
	}

	for (uint32 k =0; k <= eBitset; ++k)
	{
		if (stat[k] > 0)
			m_pDatabaseInfo->m_arrKeyTimesArray[k] = ControllerHelper::GetKeyTimesControllerPtrArray(k, stat[k]);
		else
			m_pDatabaseInfo->m_arrKeyTimesArray[k] = 0;
	}

	memset(stat, 0, sizeof(stat));

	for(uint32 k = 0; k < pCtrlChunk->numKeyTime; ++k)
	{
		//IKeyTimesInformation * pKeys = m_pDatabaseInfo->m_arrKeyTimesArray[formatKeyTimes[k]];
		m_pDatabaseInfo->m_arrKeyTimes[k] =  ControllerHelper::GetKeyTimesPtrFromArray(m_pDatabaseInfo->m_arrKeyTimesArray[formatKeyTimes[k]], formatKeyTimes[k], stat[formatKeyTimes[k]]++);//formatKeyTimes[k]->operator[](stat[formatKeyTimes[k]]++);//ControllerHelper::GetKeyTimesControllerPtr(formatKeyTimes[k]);
		m_pDatabaseInfo->m_arrKeyTimes[k]->ResizeKeyTime(sizesKeyTimes[k]);

		memcpy(m_pDatabaseInfo->m_arrKeyTimes[k]->GetData(), pData, m_pDatabaseInfo->m_arrKeyTimes[k]->GetDataRawSize());

		if (formatKeyTimes[k] == eF32)
			SwapEndian((f32 *)m_pDatabaseInfo->m_arrKeyTimes[k]->GetData(), m_pDatabaseInfo->m_arrKeyTimes[k]->GetDataRawSize()/sizeof(f32));
		else
			if (formatKeyTimes[k] == eUINT16)
				SwapEndian((uint16 *)m_pDatabaseInfo->m_arrKeyTimes[k]->GetData(), m_pDatabaseInfo->m_arrKeyTimes[k]->GetDataRawSize()/sizeof(uint16));

		pData += m_pDatabaseInfo->m_arrKeyTimes[k]->GetDataRawSize();
	}

	// test!!!
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//IKeyTimesInformation * ppp = ControllerHelper::GetKeyTimesControllerPtr(eBitset);
	//		uint32 keeeys = 0;
	//		uint32 keeeyssize = 0;
	//
	//		std::vector<uint16> vSpecial;
	//
	//
	//		{
	//			for(uint32 k = 0; k < pCtrlChunk->numKeyTime; ++k)
	//			{
	//
	//				/*
	//				if (m_pDatabaseInfo->m_arrKeyTimes[k]->GetNumKeys() == (m_pDatabaseInfo->m_arrKeyTimes[k]->GetKeyValueFloat(m_pDatabaseInfo->m_arrKeyTimes[k]->GetNumKeys()-1) - m_pDatabaseInfo->m_arrKeyTimes[k]->GetKeyValueFloat(0) + 1))
	//				{
	//				vSpecial.push_back(k);
	//				++keeeys;
	//				keeeyssize += m_pDatabaseInfo->m_arrKeyTimes[k]->GetDataRawSize();
	//
	//				f32 start = m_pDatabaseInfo->m_arrKeyTimes[k]->GetKeyValueFloat(0);
	//				f32 stop = m_pDatabaseInfo->m_arrKeyTimes[k]->GetKeyValueFloat(m_pDatabaseInfo->m_arrKeyTimes[k]->GetNumKeys()-1);
	//
	//				if (m_pDatabaseInfo->m_arrKeyTimes[k]->GetFormat() == eF32)
	//				m_pDatabaseInfo->m_arrKeyTimes[k] = ControllerHelper::GetKeyTimesControllerPtr(eF32StartStop);
	//				else
	//				if (m_pDatabaseInfo->m_arrKeyTimes[k]->GetFormat() == eUINT16)
	//				m_pDatabaseInfo->m_arrKeyTimes[k] = ControllerHelper::GetKeyTimesControllerPtr(eUINT16StartStop);
	//				else
	//				if (m_pDatabaseInfo->m_arrKeyTimes[k]->GetFormat() == eByte)
	//				m_pDatabaseInfo->m_arrKeyTimes[k] = ControllerHelper::GetKeyTimesControllerPtr(eByteStartStop);
	//
	//				m_pDatabaseInfo->m_arrKeyTimes[k]->AddKeyTime(start);
	//				m_pDatabaseInfo->m_arrKeyTimes[k]->AddKeyTime(stop);
	//
	//				}
	//				*/
	//
	//
	//				if (m_pDatabaseInfo->m_arrKeyTimes[k]->GetFormat() == eByte)
	//				{
	//
	//					vSpecial.push_back(k);
	//					++keeeys;
	//					keeeyssize += m_pDatabaseInfo->m_arrKeyTimes[k]->GetDataRawSize();
	//
	//					TArray<uint16> data;
	//					byte start = m_pDatabaseInfo->m_arrKeyTimes[k]->GetKeyValueFloat(0);
	//					byte stop = m_pDatabaseInfo->m_arrKeyTimes[k]->GetKeyValueFloat(m_pDatabaseInfo->m_arrKeyTimes[k]->GetNumKeys()-1);
	//
	//					data.push_back(start);
	//					data.push_back(stop);
	//					data.push_back(m_pDatabaseInfo->m_arrKeyTimes[k]->GetNumKeys());
	//
	//					if (start == 0 && stop == 60)
	//					{
	//						int a = 0;
	//					}
	//
	//					uint16 currentWord(0);
	//					uint16 currentTime(0);
	//					//bool bLast(false);
	//					int j(1), i(0);
	//					for (i =0, j =0; i < stop - start + 1/*m_pDatabaseInfo->m_arrKeyTimes[k]->GetNumKeys()*/; ++i, ++j)
	//					{
	//						if (j == 16)
	//						{
	//							data.push_back(currentWord);
	//							currentWord = 0;
	//							//							bLast = true;
	//							j = 0;
	//						}
	//
	//						uint8 curData = -1;
	//						if (currentTime < m_pDatabaseInfo->m_arrKeyTimes[k]->GetNumKeys())
	//							curData = *(m_pDatabaseInfo->m_arrKeyTimes[k]->GetData() + currentTime) - start;
	//
	//						uint16 val(0);
	//						if ( i == curData)
	//						{
	//							val = 1 << j;
	//							++currentTime;
	//						}
	//
	//						if (i == start || i == stop)
	//						{
	//							val = 1 << j;
	//						}
	//
	//						currentWord += val;
	////						bLast = false;
	//					}
	//
	//					if (j)
	//					{
	//						data.push_back(currentWord);
	//					}
	//
	//					m_pDatabaseInfo->m_arrKeyTimes[k] = ControllerHelper::GetKeyTimesControllerPtr(eBitset);
	//					m_pDatabaseInfo->m_arrKeyTimes[k]->ResizeKeyTime(data.size());
	//					//std::copy()
	//					memcpy(m_pDatabaseInfo->m_arrKeyTimes[k]->GetData(), &data[0], m_pDatabaseInfo->m_arrKeyTimes[k]->GetDataRawSize());
	//
	//				}
	//			}
	//		}
	//
	//
	//#ifdef LOADING_STATISTICS
	//		//char tmp[4096];
	//		//		CryModuleMemoryInfo info;
	//		CryGetMemoryInfoForModule(&info);
	//		g_pILog->UpdateLoadingScreen("ReadController. Stepped KeyTimes %i. Saved bytes %i. Total keytimes %i", keeeys, keeeyssize, pCtrlChunk->numKeyTime );
	//#endif
	//

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController. KeyTimes. Memstat %i", (int)(info.allocated - info.freed));
	}

	uint32 keyPos = pCtrlChunk->numKeyPos;

	//pos sizes
	const uint16 *pSizesPos = (uint16*)pData;
	SwapEndian((uint16 *)pSizesPos, keyPos);
	pData += keyPos * sizeof(uint16);
	//pos formats
	const uint8 * pFormatPos = (uint8*)pData;
	pData += keyPos * sizeof(uint8);
	// copy raw data

	m_pDatabaseInfo->m_arrPositionTracksArray.resize(eAutomaticQuat + 1);

	int posStat[eAutomaticQuat + 1];
	memset(posStat, 0, sizeof(posStat));
	for(uint32 p = 0; p < keyPos; ++p)
	{	
		++posStat[pFormatPos[p]];
	}

	for (uint32 k =0; k <= eAutomaticQuat; ++k)
	{
		if (posStat[k] > 0)
			m_pDatabaseInfo->m_arrPositionTracksArray[k] = ControllerHelper::GetPositionControllerPtrArray(k, posStat[k]);
		else
			m_pDatabaseInfo->m_arrPositionTracksArray[k] = 0;
		//			m_arrPositionTracksArrayStat[k] = posStat[k];
	}

	memset(posStat, 0, sizeof(posStat));

	for(uint32 p = 0; p < keyPos; ++p)
	{
		m_pDatabaseInfo->m_arrPositionTracks[p] = ControllerHelper::GetPositionPtrFromArray(m_pDatabaseInfo->m_arrPositionTracksArray[pFormatPos[p]], pFormatPos[p], posStat[pFormatPos[p]]++);
		m_pDatabaseInfo->m_arrPositionTracks[p]->Resize(pSizesPos[p]);

		memcpy(m_pDatabaseInfo->m_arrPositionTracks[p]->GetData(), pData, m_pDatabaseInfo->m_arrPositionTracks[p]->GetDataRawSize());
		SwapEndian((f32 *)m_pDatabaseInfo->m_arrPositionTracks[p]->GetData(), m_pDatabaseInfo->m_arrPositionTracks[p]->GetDataRawSize()/sizeof(f32));
		pData += m_pDatabaseInfo->m_arrPositionTracks[p]->GetDataRawSize();
	}

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController. Positions. Memstat %i", (int)(info.allocated - info.freed));
	}

	uint32 keyRot = pCtrlChunk->numKeyRot;

	// rot sizes
	const uint16 * pSizesRot = (uint16*)pData;
	SwapEndian((uint16 *)pSizesRot, keyRot);
	pData += keyRot * sizeof(uint16);
	//rot formats
	const uint8 * pFormatRot = (uint8*)pData;
	pData += keyRot * sizeof(uint8);

	m_pDatabaseInfo->m_arrRotationTracksArray.resize(eAutomaticQuat + 1);

	int rotStat[eAutomaticQuat + 1];
	memset(rotStat, 0, sizeof(rotStat));

	for(uint32 p = 0; p < keyRot; ++p)
	{	
		++rotStat[pFormatRot[p]];
	}

	for (uint32 k =0; k <= eAutomaticQuat; ++k)
	{
		if (rotStat[k] > 0)
			m_pDatabaseInfo->m_arrRotationTracksArray[k] = ControllerHelper::GetRotationControllerPtrArray(k, rotStat[k]);
		else
			m_pDatabaseInfo->m_arrRotationTracksArray[k] = 0;
	}

	memset(rotStat, 0, sizeof(rotStat));

	// copy raw data
	for(uint32 r = 0; r < keyRot; ++r)
	{
		m_pDatabaseInfo->m_arrRotationTracks[r] = ControllerHelper::GetRotationPtrFromArray(m_pDatabaseInfo->m_arrRotationTracksArray[pFormatRot[r]], pFormatRot[r], rotStat[pFormatRot[r]]++);
		//ControllerHelper::GetRotationControllerPtr(pFormatRot[r]);
		m_pDatabaseInfo->m_arrRotationTracks[r]->Resize(pSizesRot[r]);

		memcpy(m_pDatabaseInfo->m_arrRotationTracks[r]->GetData(), pData, m_pDatabaseInfo->m_arrRotationTracks[r]->GetDataRawSize());
#if defined(XENON) || defined(PS3)
		if (pFormatRot[r] == eNoCompress)
			SwapEndian((Quat *)m_pDatabaseInfo->m_arrRotationTracks[r]->GetData(), m_pDatabaseInfo->m_arrRotationTracks[r]->GetDataRawSize()/ sizeof(Quat));
		else
			if (pFormatRot[r] == eSmallTree48BitQuat)
			{

				int n = m_pDatabaseInfo->m_arrRotationTracks[r]->GetDataRawSize() / 6;
				char *pSrc = m_pDatabaseInfo->m_arrRotationTracks[r]->GetData();
				for (int i=0; i<n; i++)
				{
					std::swap(pSrc[0], pSrc[1]);
					std::swap(pSrc[2], pSrc[3]);
					std::swap(pSrc[4], pSrc[5]);
					pSrc += 6;
				}
			}
			else
				if (pFormatRot[r] == eSmallTree64BitQuat || pFormatRot[r] == eSmallTree64BitExtQuat)
				{

					int n = m_pDatabaseInfo->m_arrRotationTracks[r]->GetDataRawSize() / 8;
					char *pSrc = m_pDatabaseInfo->m_arrRotationTracks[r]->GetData();
					for (int i=0; i<n; i++)
					{
						std::swap(pSrc[0], pSrc[1]);
						std::swap(pSrc[2], pSrc[3]);
						std::swap(pSrc[4], pSrc[5]);
						std::swap(pSrc[6], pSrc[7]);
						pSrc += 8;
					}
				}
#endif
		pData += m_pDatabaseInfo->m_arrRotationTracks[r]->GetDataRawSize();
	}
	if (Console::GetInst().ca_MemoryUsageLog)
	{

		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController. Rotations. Memstat %i", (int)(info.allocated - info.freed));
	}


	uint32 totalAnims = pCtrlChunk->numAnims;

	uint32 posControllers = 0, rotControllers = 0, totalControllers =0;

	char * pSave (pData);
	// statistics
	for (uint32 i = 0; i < totalAnims; ++i)
	{

		uint16 strSize;// = pData;
		memcpy(&strSize, pData, sizeof(uint16));
		SwapEndian(strSize);
		pData += sizeof(uint16);
		pData += strSize;
		pData += sizeof(CStoredSkinningInfo);

		// footplants
		uint16 footplans;// = m_arrAnimations[i].m_FootPlantBits.size();
		memcpy(&footplans, pData, sizeof(uint16));
		SwapEndian(footplans);
		pData += sizeof(uint16);

		if (footplans)
		{
			pData += footplans;
		}

		// 

		uint16 controllerInfo;

		memcpy(&controllerInfo, pData, sizeof(uint16));
		SwapEndian(controllerInfo);
		pData += sizeof(uint16);

		//DynArray<CControllerInfo> controllers;
		//controllers.resize(controllerInfo);

		totalControllers += controllerInfo;
		if (controllerInfo == 0)
			continue;


		DynArray<CControllerInfo> controllers;
		controllers.resize(controllerInfo);

		if (controllerInfo == 0)
			continue;

		memcpy(&(controllers[0]), pData, controllerInfo * sizeof(CControllerInfo));
		SwapEndian(&(controllers[0]), controllerInfo);
		pData += controllerInfo * sizeof(CControllerInfo);

		// counting statistics for position\rotation tracks
		for (uint32  t = 0; t < controllerInfo; ++t)
		{
			uint32 p = controllers[t].m_nPosTrack;
			uint32 r = controllers[t].m_nRotTrack;
			uint32 pk = controllers[t].m_nPosKeyTimeTrack;
			uint32 rk = controllers[t].m_nRotKeyTimeTrack;

			if (r != -1 && rk != -1)
			{
				++rotControllers;
			}

			if (p != -1 && pk != -1)
			{
				++posControllers;
			}

		}
	}

	pData = pSave;

	//		m_pDatabaseInfo->m_pControllers = new CControllerInplace[totalControllers];
	//		m_pDatabaseInfo->m_pRots =  new RotationTrackInformationInplace[rotControllers];// pData;
	//		m_pDatabaseInfo->m_pPoss = new PositionTrackInformationInplace[posControllers];//pData;

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		char tmp[256];
		sprintf(tmp, "ReadController. ControllersResize. Controllers %i sizeof %i,  rots %i sizeof %i, poss %i sizeof %i. Memstat %i", totalControllers, sizeof(CControllerInplace), rotControllers, sizeof(RotationTrackInformationInplace),  posControllers, sizeof(PositionTrackInformationInplace), (int)(info.allocated - info.freed));
		g_pILog->UpdateLoadingScreen(tmp);
	}

	uint32 rc =0, pc = 0, cc = 0;

	for (uint32 i = 0; i < totalAnims; ++i)
	{

		uint16 strSize;// = pData;
		memcpy(&strSize, pData, sizeof(uint16));
		SwapEndian(strSize);
		pData += sizeof(uint16);
		char tmp[1024];

		memset(tmp,0,1024);
		memcpy(tmp, pData, strSize);

		m_pDatabaseInfo->m_AnimationNames[i] = tmp;

		pData += strSize;


		// m_pDatabaseInfo->m_AnimationCRCMap.insert(std::make_pair<uint32, uint32>(g_pCrc32Gen->GetCRC32(tmp), i));  // as found
		m_pDatabaseInfo->m_AnimationCRCMap.insert(std::pair<uint32, uint32>(g_pCrc32Gen->GetCRC32(tmp), i));

		CStoredSkinningInfo info;

		memcpy(&info, pData, sizeof(info));
		SwapEndian(info);
		pData += sizeof(info);


		// fill 
		m_pDatabaseInfo->m_Headers[i] = new CCommonSkinningInfo;
		m_pDatabaseInfo->m_Headers[i]->m_nTicksPerFrame = info.m_nTicksPerFrame;
		m_pDatabaseInfo->m_Headers[i]->m_secsPerTick = info.m_secsPerTick;
		m_pDatabaseInfo->m_Headers[i]->m_nStart = info.m_nStart;
		m_pDatabaseInfo->m_Headers[i]->m_nEnd = info.m_nEnd;
		m_pDatabaseInfo->m_Headers[i]->m_fSpeed = info.m_Speed;
		m_pDatabaseInfo->m_Headers[i]->m_fDistance = info.m_Distance;
		m_pDatabaseInfo->m_Headers[i]->m_fSlope = info.m_Slope;
		m_pDatabaseInfo->m_Headers[i]->m_Looped = info.m_Looped;
		m_pDatabaseInfo->m_Headers[i]->m_LHeelStart = info.m_LHeelStart,
		m_pDatabaseInfo->m_Headers[i]->m_LHeelEnd = info.m_LHeelEnd;
		m_pDatabaseInfo->m_Headers[i]->m_LToe0Start = info.m_LToe0Start;
		m_pDatabaseInfo->m_Headers[i]->m_LToe0End = info.m_LToe0End;
		m_pDatabaseInfo->m_Headers[i]->m_RHeelStart = info.m_RHeelStart;
		m_pDatabaseInfo->m_Headers[i]->m_RHeelEnd = info.m_RHeelEnd;
		m_pDatabaseInfo->m_Headers[i]->m_RToe0Start = info.m_RToe0Start;
		m_pDatabaseInfo->m_Headers[i]->m_RToe0End = info.m_RToe0End;
		m_pDatabaseInfo->m_Headers[i]->m_MoveDirectionInfo = info.m_MoveDirection; // raw storage

		// footplants
		uint16 footplans;// = m_arrAnimations[i].m_FootPlantBits.size();
		memcpy(&footplans, pData, sizeof(uint16));
		SwapEndian(footplans);
		pData += sizeof(uint16);

		m_pDatabaseInfo->m_Headers[i]->m_FootPlantBits.resize(footplans);
		if (footplans)
		{
			memcpy(&(m_pDatabaseInfo->m_Headers[i]->m_FootPlantBits[0]), pData, footplans);
			pData += footplans;
		}

		// 
		uint16 controllerInfo;// = m_arrAnimations[i].m_arrControlerInfo.size();
		memcpy(&controllerInfo, pData, sizeof(uint16));
		SwapEndian(controllerInfo);
		pData += sizeof(uint16);

		DynArray<CControllerInfo> controllers;
		controllers.resize(controllerInfo);

		if (controllerInfo == 0)
			continue;

		memcpy(&(controllers[0]), pData, controllerInfo * sizeof(CControllerInfo));
		SwapEndian(&(controllers[0]), controllerInfo);
		pData += controllerInfo * sizeof(CControllerInfo);

		m_pDatabaseInfo->m_Headers[i]->m_pControllers.resize(controllerInfo);
		m_pDatabaseInfo->m_Headers[i]->m_arrControllerId.resize(controllerInfo);


		// counting statistics for position\rotation tracks

		//uint32 posControllers = 0, rotControllers = 0;

		//for (uint32  t = 0; t < controllerInfo; ++t)
		//{
		//	uint32 p = controllers[t].m_nPosTrack;
		//	uint32 r = controllers[t].m_nRotTrack;
		//	uint32 pk = controllers[t].m_nPosKeyTimeTrack;
		//	uint32 rk = controllers[t].m_nRotKeyTimeTrack;

		//	if (r != -1 && rk != -1)
		//	{
		//		++rotControllers;
		//	}

		//	if (p != -1 && pk != -1)
		//	{
		//		++posControllers;
		//	}

		//}
		//CController * pControllers = new CController[controllerInfo];
		//RotationTrackInformation * pRots =  new RotationTrackInformation[rotControllers];// pData;
		//PositionTrackInformation * pPoss = new PositionTrackInformation[posControllers];//pData;

		bool b = true;
		m_pDatabaseInfo->m_iTotalControllers += controllerInfo;
		for (uint32  t = 0 ; t < controllerInfo; ++t)
		{
			uint32 p = controllers[t].m_nPosTrack;
			uint32 r = controllers[t].m_nRotTrack;
			uint32 pk = controllers[t].m_nPosKeyTimeTrack;
			uint32 rk = controllers[t].m_nRotKeyTimeTrack;
			uint32 id = controllers[t].m_nControllerID;


			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			//if (b && (std::find(vSpecial.begin(), vSpecial.end(), pk) != vSpecial.end() || std::find(vSpecial.begin(), vSpecial.end(), rk) != vSpecial.end()))
			//{
			//	g_pILog->UpdateLoadingScreen("Stepped KeyTimes in %s", tmp );			
			//	b = false;
			//}

			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			CController_AutoPtr pController(/*&m_pDatabaseInfo->m_pControllers[cc++]);//*/new CController);
			TrackInformationPtr pRotation;
			PositionInformationPtr pPosition;


			if (r != -1 && rk != -1)
			{
				pRotation =  TrackInformationPtr(new RotationTrackInformation/*&m_pDatabaseInfo->m_pRots[rc++]*/);			
				pRotation->SetKeyTimesInformation(m_pDatabaseInfo->m_arrKeyTimes[rk]);
				pRotation->SetRotationStorage(m_pDatabaseInfo->m_arrRotationTracks[r]);
				pController->SetRotationController(pRotation);
			}

			if (p != -1 && pk != -1)
			{
				pPosition = PositionInformationPtr(new PositionTrackInformation/*&m_pDatabaseInfo->m_pPoss[pc++]*/);
				pPosition->SetKeyTimesInformation(m_pDatabaseInfo->m_arrKeyTimes[pk]);
				pPosition->SetPositionStorage(m_pDatabaseInfo->m_arrPositionTracks[p]);
				pController->SetPositionController(pPosition);
			}

			pController->m_nControllerId = id;
			m_pDatabaseInfo->m_Headers[i]->m_pControllers[t] = pController;
			m_pDatabaseInfo->m_Headers[i]->m_arrControllerId[t] = id;
		}
	}

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController. Finished. Memstat %i", (int)(info.allocated - info.freed));
	}

	//m_pDatabaseInfo->Clear();


	return true;
}

const CCommonSkinningInfo * CLoaderDBA::GetSkinningInfo(const char * filename) const
{
	//int num = m_pDatabaseInfo->m_AnimationNames.GetValue(filename);
	uint32 crc =  g_pCrc32Gen->GetCRC32(filename);
	std::map<uint32, uint32>::iterator it = m_pDatabaseInfo->m_AnimationCRCMap.find(crc);
	if ( it == m_pDatabaseInfo->m_AnimationCRCMap.end())
		return 0;

	return m_pDatabaseInfo->m_Headers[it->second];
}

bool CLoaderDBA::RemoveSkinningInfo(const char * filename)
{
	//return;
	//int num = m_pDatabaseInfo->m_AnimationNames.GetValue(filename);
	uint32 crc =  g_pCrc32Gen->GetCRC32(filename);
	std::map<uint32, uint32>::iterator it = m_pDatabaseInfo->m_AnimationCRCMap.find(crc);
	if ( it == m_pDatabaseInfo->m_AnimationCRCMap.end())
		return false;

	uint32 offset = it->second;
	THeaderVector::iterator itv(m_pDatabaseInfo->m_Headers.begin());
	std::advance(itv, offset);

	delete *itv;
	*itv = 0;
	//	m_pDatabaseInfo->m_Headers.erase(itv);

	m_pDatabaseInfo->m_AnimationCRCMap.erase(it);

	return true;
}

const size_t CLoaderDBA::SizeOfThis() const
{
	size_t nSize(0);


	for (TNamesVector::iterator it = m_pDatabaseInfo->m_AnimationNames.begin(), 
		end = m_pDatabaseInfo->m_AnimationNames.end(); it != end; ++it)
	{
		nSize += it->length();
	}

	nSize += m_pDatabaseInfo->m_AnimationNames.size() * sizeof(string);

	nSize += m_pDatabaseInfo->m_AnimationCRCMap.size() * sizeof(std::pair<uint32, uint32>);

	nSize += m_pDatabaseInfo->m_Headers.size() * sizeof(CCommonSkinningInfo * );

	nSize += m_pDatabaseInfo->m_arrKeyTimesArray.size() * m_pDatabaseInfo->m_arrKeyTimes.size() * sizeof(IKeyTimesInformation);

	nSize += m_pDatabaseInfo->m_arrRotationTracksArray.size() * m_pDatabaseInfo->m_arrPositionTracks.size() * sizeof(ITrackRotationStorage);
	nSize += m_pDatabaseInfo->m_arrPositionTracksArray.size() * m_pDatabaseInfo->m_arrRotationTracks.size() * sizeof(ITrackPositionStorage);

	for (int32 i = 0; i < m_pDatabaseInfo->m_arrKeyTimes.size(); ++i)
	{
		nSize += m_pDatabaseInfo->m_arrKeyTimes[i]->GetDataRawSize();
	}

	nSize += m_pDatabaseInfo->m_arrKeyTimes.size() * sizeof(DynArray<KeyTimesInformationPtr>);

	for (int32 i = 0; i < m_pDatabaseInfo->m_arrRotationTracks.size(); ++i)
	{
		nSize += m_pDatabaseInfo->m_arrRotationTracks[i]->GetDataRawSize();
	}
	nSize += m_pDatabaseInfo->m_arrRotationTracks.size() * sizeof(DynArray<TrackRotationStoragePtr>);

	for (int32 i = 0; i < m_pDatabaseInfo->m_arrPositionTracks.size(); ++i)
	{
		nSize += m_pDatabaseInfo->m_arrPositionTracks[i]->GetDataRawSize();
	}

	nSize += m_pDatabaseInfo->m_arrPositionTracks.size() * sizeof(DynArray<TrackPositionStoragePtr>);

	for (int32 i = 0; i < (int32)m_pDatabaseInfo->m_Headers.size(); ++i)
	{
		if (m_pDatabaseInfo->m_Headers[i])
		{

			for (int32 j = 0; j < m_pDatabaseInfo->m_Headers[i]->m_pControllers.size(); ++j)
			{
				if (m_pDatabaseInfo->m_Headers[i]->m_pControllers[j])
					nSize += m_pDatabaseInfo->m_Headers[i]->m_pControllers[j]->SizeOfThis();

			}

			nSize += m_pDatabaseInfo->m_Headers[i]->m_arrControllerId.size() * sizeof(uint32);
			nSize += m_pDatabaseInfo->m_Headers[i]->m_FootPlantBits.size() * sizeof(uint8);
			nSize += m_pDatabaseInfo->m_Headers[i]->m_pControllers.size() * sizeof(DynArray</*IControllerAu *>* /	*/IController_AutoPtr>);
			nSize += sizeof(CCommonSkinningInfo);
		}
	}

	nSize += m_pDatabaseInfo->m_Storage.size();
	nSize += m_pDatabaseInfo->m_iTotalControllers * sizeof(IController_AutoPtr);
	//nSize += m_pDatabaseInfo->m_Headers[i]->m_pControllers.size() * sizeof(DynArray</*IControllerAu *>* /	*/IController_AutoPtr>);

	return nSize;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CLoaderDBA::ReadController903 (IChunkFile::ChunkDesc *pChunkDesc  )
{
	CONTROLLER_CHUNK_DESC_0903* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0903*)pChunkDesc->data;
	SwapEndian(*pCtrlChunk);
	m_pDatabaseInfo->m_iTotalControllers = 0;
	uint64 startedStatistics;
	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		startedStatistics = info.allocated - info.freed;
		g_pILog->UpdateLoadingScreen("ReadController903. Init. Memstat %li", (long int)(info.allocated - info.freed));
	}

	m_pDatabaseInfo->m_AnimationNames.resize(pCtrlChunk->numAnims);
	m_pDatabaseInfo->m_Headers.resize(pCtrlChunk->numAnims);

//	m_pDatabaseInfo->m_arrKeyTimes.resize(pCtrlChunk->numKeyTime);
//	m_pDatabaseInfo->m_arrPositionTracks.resize(pCtrlChunk->numKeyPos);
//	m_pDatabaseInfo->m_arrRotationTracks.resize(pCtrlChunk->numKeyRot);

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController903. TracksResize. Memstat %li", (long int)(info.allocated - info.freed));
	}


	char *pData = (char*)(pCtrlChunk+1);

	uint32 totalAnims = pCtrlChunk->numAnims;
	uint32 keyTime = pCtrlChunk->numKeyTime;
	uint32 keyPos = pCtrlChunk->numKeyPos;
	uint32 keyRot = pCtrlChunk->numKeyRot;


	// Keytimes pointers
	uint16 * pktSizes = (uint16 *)pData;
	pData += keyTime * sizeof(uint16);
	uint32 * pktFormats = (uint32 *)pData;
	pData += (eBitset + 1) * sizeof(uint32);


	// Positions pointers
	uint16 * ppSizes = (uint16 *)pData;
	pData += keyPos * sizeof(uint16);
	uint32 * ppFormats = (uint32 *)pData;
	pData += eAutomaticQuat * sizeof(uint32);

	//Rotations pointers
	uint16 *prSizes = (uint16 *)pData;
	pData += keyRot * sizeof(uint16);
	uint32 *prFormats = (uint32 *)pData;
	pData += eAutomaticQuat * sizeof(uint32);

	std::vector<uint32> rCount(eAutomaticQuat + 1);
	for (uint32 i = 1; i < eAutomaticQuat + 1; ++i)
	{
		rCount[i] += rCount[i-1] + prFormats[i-1];
	}

	std::vector<uint32> pCount(eAutomaticQuat + 1);
	for (uint32 i = 1; i < eAutomaticQuat + 1; ++i)
	{
		pCount[i] += pCount[i-1] + ppFormats[i-1];
	}

	std::vector<uint32> ktCount(eBitset + 2);
	for (uint32 i = 1; i < eBitset + 2; ++i)
	{
		ktCount[i] += ktCount[i-1] + pktFormats[i-1];
	}

/*
uint32 GetPositionsFormatSizeOf(uint32 format);
uint32 GetRotationFormatSizeOf(uint32 format);
uint32 GetKeyTimesFormatSizeOf(uint32 format);

*/
	uint32 totalAlloc= 0;
	uint32 curFormat = 0;

	while (ktCount[curFormat + 1] == 0)
		++curFormat;
	uint32 curSizeof = ControllerHelper::GetKeyTimesFormatSizeOf(curFormat);

	std::vector<uint32> ktOffsets(keyTime);

	for (uint32 i = 0; i < keyTime; ++i)
	{
		while( i >= ktCount[curFormat + 1])
		{
			++curFormat;
			curSizeof = ControllerHelper::GetKeyTimesFormatSizeOf(curFormat);
		}
		ktOffsets[i] = totalAlloc;
		totalAlloc += pktSizes[i] * curSizeof;
	}

	curFormat = 0;
	while (pCount[curFormat + 1] == 0)
		++curFormat;
	
	curSizeof = ControllerHelper::GetPositionsFormatSizeOf(curFormat);

	std::vector<uint32> pOffsets(keyPos);

	for (uint32 i = 0; i < keyPos; ++i)
	{
		while( i >= pCount[curFormat + 1])
		{
			++curFormat;
			curSizeof = ControllerHelper::GetPositionsFormatSizeOf(curFormat);
		}
		pOffsets[i] = totalAlloc;
		totalAlloc += ppSizes[i] * curSizeof;
	}

	curFormat = 0;
	while (rCount[curFormat + 1] == 0)
		++curFormat;

	curSizeof = ControllerHelper::GetRotationFormatSizeOf(curFormat);

	std::vector<uint32> rOffsets(keyRot);

	for (uint32 i = 0; i < keyRot; ++i)
	{
		while( i >= rCount[curFormat + 1])
		{
			++curFormat;
			curSizeof = ControllerHelper::GetRotationFormatSizeOf(curFormat);
		}
		rOffsets[i] = totalAlloc;
		totalAlloc += prSizes[i] * curSizeof;
	}

	m_pDatabaseInfo->m_Storage.resize(totalAlloc);

	memcpy(&(m_pDatabaseInfo->m_Storage[0]), pData, totalAlloc);
	pData += totalAlloc;

	// set pointers to keytimes, positions, rotations

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController903. All dynamic data resize. Memstat %li", (long int)(info.allocated - info.freed));
	}



	uint32 rc =0, pc = 0, cc = 0;

	for (uint32 i = 0; i < totalAnims; ++i)
	{

		uint16 strSize;// = pData;
		memcpy(&strSize, pData, sizeof(uint16));
		pData += sizeof(uint16);
		char tmp[1024];

		memset(tmp,0,1024);
		memcpy(tmp, pData, strSize);

		m_pDatabaseInfo->m_AnimationNames[i] = tmp;

		pData += strSize;


		// m_pDatabaseInfo->m_AnimationCRCMap.insert(std::make_pair<uint32, uint32>(g_pCrc32Gen->GetCRC32(tmp), i));  // as found
		m_pDatabaseInfo->m_AnimationCRCMap.insert(std::pair<uint32, uint32>(g_pCrc32Gen->GetCRC32(tmp), i));

		CStoredSkinningInfo info;

		memcpy(&info, pData, sizeof(info));
		pData += sizeof(info);


		// fill 
		m_pDatabaseInfo->m_Headers[i] = new CCommonSkinningInfo;
		m_pDatabaseInfo->m_Headers[i]->m_nTicksPerFrame = info.m_nTicksPerFrame;
		m_pDatabaseInfo->m_Headers[i]->m_secsPerTick = info.m_secsPerTick;
		m_pDatabaseInfo->m_Headers[i]->m_nStart = info.m_nStart;
		m_pDatabaseInfo->m_Headers[i]->m_nEnd = info.m_nEnd;
		m_pDatabaseInfo->m_Headers[i]->m_fSpeed = info.m_Speed;
		m_pDatabaseInfo->m_Headers[i]->m_fDistance = info.m_Distance;
		m_pDatabaseInfo->m_Headers[i]->m_fSlope = info.m_Slope;
		m_pDatabaseInfo->m_Headers[i]->m_Looped = info.m_Looped;
		m_pDatabaseInfo->m_Headers[i]->m_LHeelStart = info.m_LHeelStart,
			m_pDatabaseInfo->m_Headers[i]->m_LHeelEnd = info.m_LHeelEnd;
		m_pDatabaseInfo->m_Headers[i]->m_LToe0Start = info.m_LToe0Start;
		m_pDatabaseInfo->m_Headers[i]->m_LToe0End = info.m_LToe0End;
		m_pDatabaseInfo->m_Headers[i]->m_RHeelStart = info.m_RHeelStart;
		m_pDatabaseInfo->m_Headers[i]->m_RHeelEnd = info.m_RHeelEnd;
		m_pDatabaseInfo->m_Headers[i]->m_RToe0Start = info.m_RToe0Start;
		m_pDatabaseInfo->m_Headers[i]->m_RToe0End = info.m_RToe0End;
		m_pDatabaseInfo->m_Headers[i]->m_MoveDirectionInfo = info.m_MoveDirection; // raw storage

		// footplants
		uint16 footplans;// = m_arrAnimations[i].m_FootPlantBits.size();
		memcpy(&footplans, pData, sizeof(uint16));
		pData += sizeof(uint16);

		m_pDatabaseInfo->m_Headers[i]->m_FootPlantBits.resize(footplans);
		if (footplans)
		{
			memcpy(&(m_pDatabaseInfo->m_Headers[i]->m_FootPlantBits[0]), pData, footplans);
			pData += footplans;
		}

		// 
		uint16 controllerInfo;// = m_arrAnimations[i].m_arrControlerInfo.size();
		memcpy(&controllerInfo, pData, sizeof(uint16));
		pData += sizeof(uint16);

//		DynArray<CControllerInfo> controllers;
//		controllers.resize(controllerInfo);

		if (controllerInfo == 0)
			continue;

		//memcpy(&(controllers[0]), pData, controllerInfo * sizeof(CControllerInfo));

		CControllerInfo * pController = (CControllerInfo *)pData;
		pData += controllerInfo * sizeof(CControllerInfo);

		m_pDatabaseInfo->m_Headers[i]->m_pControllers.resize(controllerInfo);
		m_pDatabaseInfo->m_Headers[i]->m_arrControllerId.resize(controllerInfo);

		m_pDatabaseInfo->m_iTotalControllers += controllerInfo;

		bool b = true;
		for (uint32  t = 0 ; t < controllerInfo; ++t)
		{
			uint32 p = pController[t].m_nPosTrack;
			uint32 r = pController[t].m_nRotTrack;
			uint32 pk = pController[t].m_nPosKeyTimeTrack;
			uint32 rk = pController[t].m_nRotKeyTimeTrack;
			uint32 id = pController[t].m_nControllerID;

			uint32 rotkt = (rk == -1) ? eNoFormat : FindFormat(rk, ktCount);//formatKeyTimes[rk];
			uint32 rotk = (r == -1) ? eNoFormat : FindFormat(r, rCount);//pFormatRot[r];
			uint32 poskt = (pk == -1) ? eNoFormat : FindFormat(pk, ktCount);//formatKeyTimes[pk];
			uint32 posk = (p == -1) ? eNoFormat : FindFormat(p, pCount);// pFormatPos[p];

			IControllerOpt_AutoPtr pController( ControllerHelper::CreateController(rotk, rotkt, posk, poskt));

			if (!pController)
			{
				return false;
			}

			if (r != -1 && rk != -1)
			{
//				assert(sizesKeyTimes[rk] == pSizesRot[r]);
				pController->SetRotationKeyTimes(pktSizes[rk], &m_pDatabaseInfo->m_Storage[ktOffsets[rk]]);
				pController->SetRotationKeys(prSizes[r], &m_pDatabaseInfo->m_Storage[rOffsets[r]]);//m_pDatabaseInfo->m_arrRotationTracks[r]->GetData());
			}

			if (p != -1 && pk != -1)
			{
				pController->SetPositionKeyTimes(pktSizes[pk], &m_pDatabaseInfo->m_Storage[ktOffsets[pk]]);
				pController->SetPositionKeys(ppSizes[p], &m_pDatabaseInfo->m_Storage[pOffsets[p]]);   ;//m_pDatabaseInfo->m_arrPositionTracks[p]->GetData());

			}

			pController->m_nControllerId = id;
			m_pDatabaseInfo->m_Headers[i]->m_pControllers[t] = pController;
			m_pDatabaseInfo->m_Headers[i]->m_arrControllerId[t] = id;
		}
	}

	if (Console::GetInst().ca_MemoryUsageLog)
	{ 
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController903. Finished. Memstat %li", (long int)(info.allocated - info.freed));
		uint64 endStatistics = info.allocated - info.freed;
		g_AnimStatisticsInfo.m_iDBASizes += endStatistics - startedStatistics;
		g_pILog->UpdateLoadingScreen("Current DBA. Memstat %li", (long int)(endStatistics - startedStatistics)); 
		g_pILog->UpdateLoadingScreen("ALL DBAs %li", (long int)g_AnimStatisticsInfo.m_iDBASizes); 
	}


	m_pDatabaseInfo->Clear();


	return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


inline void SwapEndians_(void* pData, size_t nCount, size_t nSizeCheck)
{
	// Primitive type.
	switch (nSizeCheck)
	{
	case 1:
		break;
	case 2:
		{
			while (nCount--)
			{
				uint16& i = *((uint16*&)pData)++;
				i = (i>>8) + (i<<8);
			}
			break;
		}
	case 4:
		{
			while (nCount--)
			{
				uint32& i = *((uint32*&)pData)++;
				i = (i>>24) + ((i>>8)&0xFF00) + ((i&0xFF00)<<8) + (i<<24);
			}
			break;
		}
	case 8:
		{
			while (nCount--)
			{
				uint64& i = *((uint64*&)pData)++;
				i = (i>>56) + ((i>>40)&0xFF00) + ((i>>24)&0xFF0000) + ((i>>8)&0xFF000000)
					+ ((i&0xFF000000)<<8) + ((i&0xFF0000)<<24) + ((i&0xFF00)<<40) + (i<<56);
			}
			break;
		}
	default:
		assert(0);
	}
}


template<class T>
void SwapEndians(T* t, std::size_t count)
{
//	SwapEndians_(t, count, sizeof(T));
}


template<class T>
void SwapEndians(T& t)
{
//	SwapEndians(&t, 1);
}


bool CLoaderDBA::ReadController904 (IChunkFile::ChunkDesc *pChunkDesc  )
{
	CONTROLLER_CHUNK_DESC_0904* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0904*)pChunkDesc->data;
	SwapEndian(*pCtrlChunk);
	m_pDatabaseInfo->m_iTotalControllers = 0;
	uint64 startedStatistics;
	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		startedStatistics = info.allocated - info.freed;
		g_pILog->UpdateLoadingScreen("ReadController903. Init. Memstat %li", (long int)(info.allocated - info.freed));
	}

	m_pDatabaseInfo->m_AnimationNames.resize(pCtrlChunk->numAnims);
	m_pDatabaseInfo->m_Headers.resize(pCtrlChunk->numAnims);

	//	m_pDatabaseInfo->m_arrKeyTimes.resize(pCtrlChunk->numKeyTime);
	//	m_pDatabaseInfo->m_arrPositionTracks.resize(pCtrlChunk->numKeyPos);
	//	m_pDatabaseInfo->m_arrRotationTracks.resize(pCtrlChunk->numKeyRot);

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController903. TracksResize. Memstat %li", (long int)(info.allocated - info.freed));
	}


	char *pData = (char*)(pCtrlChunk+1);

	uint32 totalAnims = pCtrlChunk->numAnims;
	uint32 keyTime = pCtrlChunk->numKeyTime;
	uint32 keyPos = pCtrlChunk->numKeyPos;
	uint32 keyRot = pCtrlChunk->numKeyRot;


	// Keytimes pointers
	uint16 * pktSizes = (uint16 *)pData;
	pData += keyTime * sizeof(uint16);
	uint32 * pktFormats = (uint32 *)pData;
	pData += (eBitset + 1) * sizeof(uint32);


	// Positions pointers
	uint16 * ppSizes = (uint16 *)pData;
	pData += keyPos * sizeof(uint16);
	uint32 * ppFormats = (uint32 *)pData;
	pData += eAutomaticQuat * sizeof(uint32);

	//Rotations pointers
	uint16 *prSizes = (uint16 *)pData;
	pData += keyRot * sizeof(uint16);
	uint32 *prFormats = (uint32 *)pData;
	pData += eAutomaticQuat * sizeof(uint32);

	uint32 * ktOffsets = (uint32 *)pData;
	pData += keyTime * sizeof(uint32);

	uint32 * pOffsets = (uint32 *)pData;
	pData += keyPos * sizeof(uint32);

	uint32 * rOffsets = (uint32 *)pData;
	pData += (keyRot+ 1) * sizeof(uint32);

	uint32 offset = keyRot * sizeof(uint32) + keyRot * sizeof(uint16);
	offset += keyPos * sizeof(uint32) + keyPos * sizeof(uint16);
	offset += keyTime * sizeof(uint32) + keyTime * sizeof(uint16);

	offset += (keyRot + keyPos + keyTime + 1) * sizeof(uint32);

	offset = offset % 4;

	pData += offset;

	/*

	std::vector<uint32> ktOffsets(keyTime);
	std::vector<uint32> rOffsets(keyRot);
	std::vector<uint32> pOffsets(keyPos);
	*/



	std::vector<uint32> rCount(eAutomaticQuat + 1);
	for (uint32 i = 1; i < eAutomaticQuat + 1; ++i)
	{
		SwapEndians(rCount[i-1]);
		SwapEndians(prFormats[i-1]);
		rCount[i] += rCount[i-1] + prFormats[i-1];
	}

	std::vector<uint32> pCount(eAutomaticQuat + 1);
	for (uint32 i = 1; i < eAutomaticQuat + 1; ++i)
	{
		SwapEndians(pCount[i-1]);
		SwapEndians(ppFormats[i-1]);
		pCount[i] += pCount[i-1] + ppFormats[i-1];
	}

	std::vector<uint32> ktCount(eBitset + 2);
	for (uint32 i = 1; i < eBitset + 2; ++i)
	{
		SwapEndians(ktCount[i-1]);
		SwapEndians(pktFormats[i-1]);
		ktCount[i] += ktCount[i-1] + pktFormats[i-1];
	}

	/*
	uint32 GetPositionsFormatSizeOf(uint32 format);
	uint32 GetRotationFormatSizeOf(uint32 format);
	uint32 GetKeyTimesFormatSizeOf(uint32 format);

	*/
	uint32 totalAlloc= 0;
	uint32 curFormat = 0;

	while (ktCount[curFormat + 1] == 0)
		++curFormat;
	uint32 curSizeof = ControllerHelper::GetKeyTimesFormatSizeOf(curFormat);



	for (uint32 i = 0; i < keyTime; ++i)
	{
		while( i >= ktCount[curFormat + 1])
		{
			++curFormat;
			curSizeof = ControllerHelper::GetKeyTimesFormatSizeOf(curFormat);
		}
		SwapEndians(ktOffsets[i]);
//		ktOffsets[i] += pData;
		SwapEndians(pktSizes[i]);
		totalAlloc += pktSizes[i] * curSizeof;
		totalAlloc += totalAlloc % 4;
	}

	curFormat = 0;
	while (pCount[curFormat + 1] == 0)
		++curFormat;

	curSizeof = ControllerHelper::GetPositionsFormatSizeOf(curFormat);



	for (uint32 i = 0; i < keyPos; ++i)
	{
		while( i >= pCount[curFormat + 1])
		{
			++curFormat;
			curSizeof = ControllerHelper::GetPositionsFormatSizeOf(curFormat);
		}
		SwapEndians(pOffsets[i]);
		SwapEndians(ppSizes[i]);
		totalAlloc += ppSizes[i] * curSizeof;
		totalAlloc += totalAlloc % 4;
	}

	curFormat = 0;
	while (rCount[curFormat + 1] == 0)
		++curFormat;

	curSizeof = ControllerHelper::GetRotationFormatSizeOf(curFormat);



	for (uint32 i = 0; i < keyRot; ++i)
	{
		while( i >= rCount[curFormat + 1])
		{
			++curFormat;
			curSizeof = ControllerHelper::GetRotationFormatSizeOf(curFormat);
		}
		SwapEndians(rOffsets[i]);
		SwapEndians(prSizes[i]);
		totalAlloc += prSizes[i] * curSizeof;
		totalAlloc += totalAlloc % 4;
	}

	totalAlloc = rOffsets[keyRot];


	SwapEndians(totalAlloc);

	m_pDatabaseInfo->m_Storage.resize(totalAlloc);

	memcpy(&(m_pDatabaseInfo->m_Storage[0]), pData, totalAlloc);
	pData += totalAlloc;// - 2;// - 4;

	// set pointers to keytimes, positions, rotations

	if (Console::GetInst().ca_MemoryUsageLog)
	{
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController903. All dynamic data resize. Memstat %li", (long int)(info.allocated - info.freed));
	}



	uint32 rc =0, pc = 0, cc = 0;

	for (uint32 i = 0; i < totalAnims; ++i)
	{

		uint16 strSize;// = pData;
		memcpy(&strSize, pData, sizeof(uint16));
		pData += sizeof(uint16);
		SwapEndians(strSize);
		char tmp[1024];

		memset(tmp,0,1024);
		memcpy(tmp, pData, strSize);

		m_pDatabaseInfo->m_AnimationNames[i] = tmp;

		pData += strSize;


		// m_pDatabaseInfo->m_AnimationCRCMap.insert(std::make_pair<uint32, uint32>(g_pCrc32Gen->GetCRC32(tmp), i));  // as found
		m_pDatabaseInfo->m_AnimationCRCMap.insert(std::pair<uint32, uint32>(g_pCrc32Gen->GetCRC32(tmp), i));

		CStoredSkinningInfo info;

		memcpy(&info, pData, sizeof(info));
		pData += sizeof(info);


		// fill 
		m_pDatabaseInfo->m_Headers[i] = new CCommonSkinningInfo;
		m_pDatabaseInfo->m_Headers[i]->m_nTicksPerFrame = info.m_nTicksPerFrame;
		m_pDatabaseInfo->m_Headers[i]->m_secsPerTick = info.m_secsPerTick;
		m_pDatabaseInfo->m_Headers[i]->m_nStart = info.m_nStart;
		m_pDatabaseInfo->m_Headers[i]->m_nEnd = info.m_nEnd;
		m_pDatabaseInfo->m_Headers[i]->m_fSpeed = info.m_Speed;
		m_pDatabaseInfo->m_Headers[i]->m_fDistance = info.m_Distance;
		m_pDatabaseInfo->m_Headers[i]->m_fSlope = info.m_Slope;
		m_pDatabaseInfo->m_Headers[i]->m_Looped = info.m_Looped;
		m_pDatabaseInfo->m_Headers[i]->m_LHeelStart = info.m_LHeelStart,
			m_pDatabaseInfo->m_Headers[i]->m_LHeelEnd = info.m_LHeelEnd;
		m_pDatabaseInfo->m_Headers[i]->m_LToe0Start = info.m_LToe0Start;
		m_pDatabaseInfo->m_Headers[i]->m_LToe0End = info.m_LToe0End;
		m_pDatabaseInfo->m_Headers[i]->m_RHeelStart = info.m_RHeelStart;
		m_pDatabaseInfo->m_Headers[i]->m_RHeelEnd = info.m_RHeelEnd;
		m_pDatabaseInfo->m_Headers[i]->m_RToe0Start = info.m_RToe0Start;
		m_pDatabaseInfo->m_Headers[i]->m_RToe0End = info.m_RToe0End;
		m_pDatabaseInfo->m_Headers[i]->m_MoveDirectionInfo = info.m_MoveDirection; // raw storage

		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_nTicksPerFrame);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_secsPerTick);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_nStart);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_nEnd);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_fSpeed);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_fDistance);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_fSlope);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_Looped);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_LHeelStart);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_LHeelEnd);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_LToe0Start);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_LToe0End);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_RHeelStart);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_RHeelEnd);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_RToe0Start);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_RToe0End);
		SwapEndians(m_pDatabaseInfo->m_Headers[i]->m_MoveDirectionInfo);





		// footplants
		uint16 footplans;// = m_arrAnimations[i].m_FootPlantBits.size();
		memcpy(&footplans, pData, sizeof(uint16));
		pData += sizeof(uint16);

		SwapEndians(footplans);

		m_pDatabaseInfo->m_Headers[i]->m_FootPlantBits.resize(footplans);
		if (footplans)
		{
			memcpy(&(m_pDatabaseInfo->m_Headers[i]->m_FootPlantBits[0]), pData, footplans);
			pData += footplans;
		}

		// 
		uint16 controllerInfo;// = m_arrAnimations[i].m_arrControlerInfo.size();
		memcpy(&controllerInfo, pData, sizeof(uint16));
		pData += sizeof(uint16);

		//		DynArray<CControllerInfo> controllers;
		//		controllers.resize(controllerInfo);

		if (controllerInfo == 0)
			continue;

		//memcpy(&(controllers[0]), pData, controllerInfo * sizeof(CControllerInfo));

		CControllerInfo * pController = (CControllerInfo *)pData;
		pData += controllerInfo * sizeof(CControllerInfo);

		m_pDatabaseInfo->m_Headers[i]->m_pControllers.resize(controllerInfo);
		m_pDatabaseInfo->m_Headers[i]->m_arrControllerId.resize(controllerInfo);

		m_pDatabaseInfo->m_iTotalControllers += controllerInfo;

		bool b = true;
		for (uint32  t = 0 ; t < controllerInfo; ++t)
		{
			uint32 p = pController[t].m_nPosTrack;
			uint32 r = pController[t].m_nRotTrack;
			uint32 pk = pController[t].m_nPosKeyTimeTrack;
			uint32 rk = pController[t].m_nRotKeyTimeTrack;
			uint32 id = pController[t].m_nControllerID;

			SwapEndians(p);
			SwapEndians(r);
			SwapEndians(pk);
			SwapEndians(rk);
			SwapEndians(id);

			uint32 rotkt = (rk == -1) ? eNoFormat : FindFormat(rk, ktCount);//formatKeyTimes[rk];
			uint32 rotk = (r == -1) ? eNoFormat : FindFormat(r, rCount);//pFormatRot[r];
			uint32 poskt = (pk == -1) ? eNoFormat : FindFormat(pk, ktCount);//formatKeyTimes[pk];
			uint32 posk = (p == -1) ? eNoFormat : FindFormat(p, pCount);// pFormatPos[p];

			IControllerOpt_AutoPtr pNewController( ControllerHelper::CreateController(rotk, rotkt, posk, poskt));

			if (!pNewController)
			{
				return false;
			}

			if (r != -1 && rk != -1)
			{
				//				assert(sizesKeyTimes[rk] == pSizesRot[r]);
				pNewController->SetRotationKeyTimes(pktSizes[rk], &m_pDatabaseInfo->m_Storage[ktOffsets[rk]]);
				pNewController->SetRotationKeys(prSizes[r], &m_pDatabaseInfo->m_Storage[rOffsets[r]]);//m_pDatabaseInfo->m_arrRotationTracks[r]->GetData());
			}

			if (p != -1 && pk != -1)
			{
				pNewController->SetPositionKeyTimes(pktSizes[pk], &m_pDatabaseInfo->m_Storage[ktOffsets[pk]]);
				pNewController->SetPositionKeys(ppSizes[p], &m_pDatabaseInfo->m_Storage[pOffsets[p]]);   ;//m_pDatabaseInfo->m_arrPositionTracks[p]->GetData());

			}

			pNewController->m_nControllerId = id;
			m_pDatabaseInfo->m_Headers[i]->m_pControllers[t] = pNewController;
			m_pDatabaseInfo->m_Headers[i]->m_arrControllerId[t] = id;
		}
	}

	if (Console::GetInst().ca_MemoryUsageLog)
	{ 
		CryModuleMemoryInfo info;
		CryGetMemoryInfoForModule(&info);
		g_pILog->UpdateLoadingScreen("ReadController903. Finished. Memstat %li", (long int)(info.allocated - info.freed));
		uint64 endStatistics = info.allocated - info.freed;
		g_AnimStatisticsInfo.m_iDBASizes += endStatistics - startedStatistics;
		g_pILog->UpdateLoadingScreen("Current DBA. Memstat %li", (long int)(endStatistics - startedStatistics)); 
		g_pILog->UpdateLoadingScreen("ALL DBAs %li", (long int)g_AnimStatisticsInfo.m_iDBASizes); 
	}


	m_pDatabaseInfo->Clear();


	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool CLoaderDBA::ReadController902 (IChunkFile::ChunkDesc *pChunkDesc)
{

	CONTROLLER_CHUNK_DESC_0902* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0902*)pChunkDesc->data;
	SwapEndian(*pCtrlChunk);

	m_pDatabaseInfo->m_arrStartDirs.resize(pCtrlChunk->numStartDirs);
	char *pData = (char*)(pCtrlChunk+1);
	memcpy(&(m_pDatabaseInfo->m_arrStartDirs[0]), pData, pCtrlChunk->numStartDirs* sizeof(QuatT) );

	return true;

}

bool CLoaderDBA::ReadControllers(IChunkFile::ChunkDesc *pChunkDesc)
{

	if (pChunkDesc->hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0900::VERSION)
	{
		return ReadController900(pChunkDesc);
	}
	else
		if (pChunkDesc->hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0902::VERSION)
		{
			return ReadController902(pChunkDesc);
		}
		else
			if (pChunkDesc->hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0903::VERSION)
			{
				return ReadController903(pChunkDesc);
			}
			else
				if (pChunkDesc->hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0904::VERSION)
				{
					return ReadController904(pChunkDesc);
				}

			return true;
}
