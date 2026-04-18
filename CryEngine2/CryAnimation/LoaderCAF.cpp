////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   CGFLoader.cpp
//  Version:     v1.00
//  Created:     6/11/2004 by Timur.
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
#include <CGFContent.h>
#include "LoaderCAF.h"


//////////////////////////////////////////////////////////////////////////
CLoaderCAF::CLoaderCAF()
{
	m_pChunkFile = NULL;
	m_pSkinningInfo = 0;
	m_bHasNewControllers = false;
	m_bLoadOldChunks = false;
	m_bHasOldControllers = false;
	m_bLoadOnlyCommon = false;
}

//////////////////////////////////////////////////////////////////////////
CLoaderCAF::~CLoaderCAF()
{
	if (m_pSkinningInfo)
	{
		delete m_pSkinningInfo;
		m_pSkinningInfo = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
CInternalSkinningInfo * CLoaderCAF::LoadCAF( const char *filename,ILoaderCAFListener* pListener )
{
	m_pListener = pListener;

	IChunkFile  * chunkFile = g_pI3DEngine->CreateChunkFile(true);
	//FIXME:
	CInternalSkinningInfo* pInfo = LoadCAF( filename,chunkFile,pListener );
	chunkFile->Release();
	m_pListener = 0;

	return pInfo;
}

//////////////////////////////////////////////////////////////////////////
CInternalSkinningInfo * CLoaderCAF::LoadCAF( const char *filename,IChunkFile * pChunkFile,ILoaderCAFListener* pListener )
{
	m_pListener = pListener;

	string strPath = filename;
	CryStringUtils::UnifyFilePath(strPath);
	string fileExt = PathUtil::GetExt(strPath);

	m_filename = filename;
	m_pChunkFile = pChunkFile;

	if (!pChunkFile->Read( filename ))
	{
		m_LastError = pChunkFile->GetLastError();
		return 0;
	}

	// Load mesh from chunk file.
	if (pChunkFile->GetFileHeader().Version != GeomFileVersion) 
	{
		m_LastError.Format("Bad CAF file version: %s", m_filename.c_str() );
		return 0;
	}

	if (pChunkFile->GetFileHeader().FileType != FileType_Geom) 
	{
		if (pChunkFile->GetFileHeader().FileType != FileType_Anim) 
		{
			m_LastError.Format("Illegal File Type for .caf file: %s", m_filename.c_str() );
			return 0;
		}
	}

	m_pSkinningInfo = new CInternalSkinningInfo;

	if (!LoadChunks())
	{
		delete m_pSkinningInfo;
		m_pSkinningInfo = 0;

	}

	m_pListener = 0;

	return m_pSkinningInfo;
}


bool CLoaderCAF::LoadChunks()
{


	// Load Nodes.	
	uint32 numChunck = m_pChunkFile->NumChunks();

	uint32 statTCB(0),statNewChunck(0), statOldChunck(0);

	for (uint32 i=0; i<numChunck; i++)
	{

		const CHUNK_HEADER &hdr = m_pChunkFile->GetChunkHeader(i);

		if (hdr.ChunkType == ChunkType_Controller && hdr.ChunkVersion > CONTROLLER_CHUNK_DESC_0828::VERSION)
		{
			m_bHasNewControllers = true;
			++statNewChunck;
		}
		else
		{
			if (hdr.ChunkType == ChunkType_Controller && (hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0828::VERSION || hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0827::VERSION))
			{
				m_bHasOldControllers = true;
				++statOldChunck;
			}
			else
			{
				if (hdr.ChunkType == ChunkType_Controller && hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0826::VERSION)
					++statTCB;
			}
		}
	}

	if (!m_bHasNewControllers)
		m_bLoadOldChunks = true;

	if (!m_bLoadOnlyCommon) 
	{
		// Only old data will be loaded. 
		if (m_bLoadOldChunks)
		{
			m_pSkinningInfo->m_arrControllerId.reserve(statOldChunck);
			m_pSkinningInfo->m_pControllers.reserve(statOldChunck);
		}
		else
		{
			m_pSkinningInfo->m_arrControllerId.reserve(statNewChunck);
			m_pSkinningInfo->m_pControllers.reserve(statNewChunck);

		}

		// We have a TCB data. Reserve memory 
		if (statTCB > 0)
		{
			m_pSkinningInfo->m_arrControllers.resize(numChunck);
			m_pSkinningInfo->m_TrackVec3.reserve(statTCB);
			m_pSkinningInfo->m_TrackQuat.reserve(statTCB);
		}
		//m_pSkinningInfo->m_arrControllers.resize(numChunck);	
		//m_pSkinningInfo->m_numChunks = numChunck;	
	}

	for (uint32 i=0; i<numChunck; i++)
	{

		const CHUNK_HEADER &hdr = m_pChunkFile->GetChunkHeader(i);

		switch (hdr.ChunkType)
		{
			//---------------------------------------------------------
			//---       chunks for CGA-objects  -----------------------
			//---------------------------------------------------------

		case ChunkType_Controller:
			if (!ReadController( m_pChunkFile->GetChunk(i)) )
				return false;
			break;

		case ChunkType_Timing:
			if (!ReadTiming( m_pChunkFile->GetChunk(i)) )
				return false;
			break;


		case ChunkType_SpeedInfo:
			if (!ReadSpeedInfo( m_pChunkFile->GetChunk(i)) )
				return false;
			break;

		case ChunkType_FootPlantInfo:
			if (!ReadFootPlantInfo(m_pChunkFile->GetChunk(i)))
				return false;
			break;

		}
	}

	return true;
}




//////////////////////////////////////////////////////////////////////////
bool CLoaderCAF::ReadSpeedInfo (  IChunkFile::ChunkDesc *pChunkDesc  )
{



	if (pChunkDesc->hdr.ChunkVersion == SPEED_CHUNK_DESC::VERSION)
	{
		SPEED_CHUNK_DESC* pChunk = (SPEED_CHUNK_DESC*)pChunkDesc->data;
		SwapEndian(*pChunk);
		m_pSkinningInfo->m_fSpeed			= pChunk->Speed;
		m_pSkinningInfo->m_fDistance	= pChunk->Distance;
		m_pSkinningInfo->m_fSlope			= pChunk->Slope;
		m_pSkinningInfo->m_Looped			= pChunk->Looped;
		return 1;
	}

	if (pChunkDesc->hdr.ChunkVersion == SPEED_CHUNK_DESC_1::VERSION)
	{
		SPEED_CHUNK_DESC_1* pChunk = (SPEED_CHUNK_DESC_1*)pChunkDesc->data;
		SwapEndian(*pChunk);
		m_pSkinningInfo->m_fSpeed	= pChunk->Speed;
		m_pSkinningInfo->m_fDistance = pChunk->Distance;
		m_pSkinningInfo->m_fSlope = pChunk->Slope;
		m_pSkinningInfo->m_Looped = pChunk->Looped;
		m_pSkinningInfo->m_MoveDirectionInfo.x = 0; //pChunk->MoveDir[0];
		m_pSkinningInfo->m_MoveDirectionInfo.y = 1; //pChunk->MoveDir[1];
		m_pSkinningInfo->m_MoveDirectionInfo.z = 0; //pChunk->MoveDir[2];

		return 1;
	}

	if (pChunkDesc->hdr.ChunkVersion == SPEED_CHUNK_DESC_2::VERSION)
	{
		SPEED_CHUNK_DESC_2* pChunk = (SPEED_CHUNK_DESC_2*)pChunkDesc->data;
		SwapEndian(*pChunk);
		m_pSkinningInfo->m_fSpeed	= pChunk->Speed;
		m_pSkinningInfo->m_fDistance = pChunk->Distance;
		m_pSkinningInfo->m_fSlope = pChunk->Slope;
		m_pSkinningInfo->m_Looped = pChunk->Looped;

		m_pSkinningInfo->m_MoveDirectionInfo[0] = pChunk->MoveDir[0];
		m_pSkinningInfo->m_MoveDirectionInfo[1] = pChunk->MoveDir[1];
		m_pSkinningInfo->m_MoveDirectionInfo[2] = pChunk->MoveDir[2];

		m_pSkinningInfo->m_StartPosition = pChunk->StartPosition;

		return 1;
	}



	return 1;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCAF::ReadFootPlantInfo(IChunkFile::ChunkDesc *pChunkDesc)
{

	uint32 version = pChunkDesc->hdr.ChunkVersion;
	uint32 id = pChunkDesc->hdr.ChunkID;
	uint32 nChunkSize = pChunkDesc->size;

	{
		// the old chunk version - fixed-size name list
		FOOTPLANT_INFO* pChunk = (FOOTPLANT_INFO*)(pChunkDesc->data);
		SwapEndian(*pChunk);

		int nPoses = pChunk->nPoses;


		m_pSkinningInfo->m_LHeelStart = pChunk->m_LHeelStart;
		m_pSkinningInfo->m_LHeelEnd = pChunk->m_LHeelEnd;
		m_pSkinningInfo->m_LToe0Start = pChunk->m_LToe0Start;
		m_pSkinningInfo->m_LToe0End = pChunk->m_LToe0End;

		m_pSkinningInfo->m_RHeelStart = pChunk->m_RHeelStart;
		m_pSkinningInfo->m_RHeelEnd = pChunk->m_RHeelEnd;
		m_pSkinningInfo->m_RToe0Start = pChunk->m_RToe0Start;
		m_pSkinningInfo->m_RToe0End = pChunk->m_RToe0End;

		m_pSkinningInfo->m_FootPlantBits.resize(nPoses);
		// just fill in the pointers to the fixed-size string buffers
		const uint8* pFootPlants = (const uint8*)(pChunk+1);

		for (int i = 0; i < nPoses; ++i)
			m_pSkinningInfo->m_FootPlantBits[i] = *(pFootPlants++);
	}

	return 1;
}


//////////////////////////////////////////////////////////////////////////
bool CLoaderCAF::ReadTiming (  IChunkFile::ChunkDesc *pChunkDesc  )
{

	TIMING_CHUNK_DESC* pTimingChunk = (TIMING_CHUNK_DESC*)pChunkDesc->data;
	SwapEndian(*pTimingChunk);

	m_pSkinningInfo->m_nTicksPerFrame	= pTimingChunk->TicksPerFrame;
	m_pSkinningInfo->m_secsPerTick		= pTimingChunk->SecsPerTick;
	m_pSkinningInfo->m_nStart					= pTimingChunk->global_range.start;
	m_pSkinningInfo->m_nEnd						= pTimingChunk->global_range.end;

	return 1;
}

//////////////////////////////////////////////////////////////////////////
bool CLoaderCAF::ReadController (  IChunkFile::ChunkDesc *pChunkDesc  )
{

	if (m_bLoadOnlyCommon)
		return true;
	//m_pSkinningInfo->m_arrTracksPQLog.reserve( 200 );
	//m_pSkinningInfo->m_arrTracksTimes.reserve( 200 );
	//m_pSkinningInfo->m_TrackVec3.reserve(200);
	//m_pSkinningInfo->m_TrackQuat.reserve(200);
	//m_pSkinningInfo->m_pControllers.reserve(200);


	if (pChunkDesc->hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0826::VERSION)
	{
		CONTROLLER_CHUNK_DESC_0826* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0826*)pChunkDesc->data;
		SwapEndian(*pCtrlChunk);


		if (pCtrlChunk->type==CTRL_TCB3)
		{
			DynArray<CryTCB3Key> * pTrack = new DynArray<CryTCB3Key>;
			TCBFlags trackflags;

			CryTCB3Key* pKeys = (CryTCB3Key*)(pCtrlChunk+1);
			uint32 nkeys = pCtrlChunk->nKeys;
			pTrack->resize(nkeys);

			CControllerTCB * pTCB = new CControllerTCB;

			for (uint32 i=0; i<nkeys; i++)
			{
				//track[i] = pKeys[i];//
				pTrack->operator [](i) = (pKeys[i]);
#if defined(XENON) || defined(PS3)
        SwapEndian(pTrack->operator [](i));
#endif
			}

			if (pCtrlChunk->nFlags & CTRL_ORT_CYCLE)
				trackflags.f0=1;
			else if (pCtrlChunk->nFlags & CTRL_ORT_LOOP)
				trackflags.f1=1;

			uint32 numControllers = m_pSkinningInfo->m_TrackVec3.size();
			m_pSkinningInfo->m_TrackVec3Flags.push_back(trackflags);
			m_pSkinningInfo->m_TrackVec3.push_back(pTrack);
			m_pSkinningInfo->m_arrControllers[pCtrlChunk->chdr.ChunkID].type=0x55;
			m_pSkinningInfo->m_arrControllers[pCtrlChunk->chdr.ChunkID].index=numControllers;
			return 1;
		}

		if (pCtrlChunk->type==CTRL_TCBQ)
		{
			DynArray<CryTCBQKey> * pTrack = new DynArray<CryTCBQKey>;
			TCBFlags trackflags;

			CryTCBQKey *pKeys = (CryTCBQKey*)(pCtrlChunk+1);
			uint32 nkeys = pCtrlChunk->nKeys;
			pTrack->resize(nkeys);
			for (uint32 i=0; i<nkeys; i++)
			{
				//track[i] = pKeys[i];// * secsPerTick;
				pTrack->operator [](i) = pKeys[i];
#if defined(XENON) || defined(PS3)
        SwapEndian(pTrack->operator [](i));
#endif
			}

			if (pCtrlChunk->nFlags & CTRL_ORT_CYCLE)
				trackflags.f0=1;
			else if (pCtrlChunk->nFlags & CTRL_ORT_LOOP)
				trackflags.f1=1;;

			uint32 numControllers = m_pSkinningInfo->m_TrackQuat.size();
			m_pSkinningInfo->m_TrackQuatFlags.push_back(trackflags);
			m_pSkinningInfo->m_TrackQuat.push_back(pTrack);
			m_pSkinningInfo->m_arrControllers[pCtrlChunk->chdr.ChunkID].type=0xaa;
			m_pSkinningInfo->m_arrControllers[pCtrlChunk->chdr.ChunkID].index=numControllers;
			return 1;
		}

		if (pCtrlChunk->type==CTRL_CRYBONE)
		{
			m_LastError.Format("animation not loaded: controller-type CTRL_CRYBONE not supported");
			return 1;
		}		

		if (pCtrlChunk->type==CTRL_BEZIER3)
		{
			m_LastError.Format("animation not loaded: controller-type CTRL_BEZIER3 not supported");
			return 1;
		}		

		m_LastError.Format("animation not loaded: found unsupported controller type in chunk");
		return 0;
	}


	if (pChunkDesc->hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0827::VERSION)
	{

		if (m_bLoadOldChunks)
		{
			CONTROLLER_CHUNK_DESC_0827* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0827*)pChunkDesc->data;
			SwapEndian(*pCtrlChunk);

			uint32 numKeys = pCtrlChunk->numKeys;

			CryKeyPQLog* pCryKey = (CryKeyPQLog*)(pCtrlChunk+1);
			SwapEndian( pCryKey,numKeys );



			CControllerPQLog * pController = new CControllerPQLog;

			pController->m_nControllerId	= pCtrlChunk->nControllerId;


			pController->m_arrKeys.resize(numKeys);
			pController->m_arrTimes.resize(numKeys);

			int32 told=-10000;
			int32 tnew=0;
			for (uint32 i=0; i<numKeys; ++i)
			{
				tnew=pCryKey[i].nTime;
				int32 dif=tnew-told;
			//	assert(dif>=160);
				told=pCryKey[i].nTime;

				pController->m_arrTimes[i]	= pCryKey[i].nTime/TICKS_CONVERT; 
				pController->m_arrKeys[i].vPos		= pCryKey[i].vPos/100.0f;
				pController->m_arrKeys[i].vRotLog	= pCryKey[i].vRotLog;
			}
			m_pSkinningInfo->m_arrControllerId.push_back( pCtrlChunk->nControllerId );
			m_pSkinningInfo->m_pControllers.push_back(pController);
		}

		return 1;
	}

	if (pChunkDesc->hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0828::VERSION)
	{
		if (m_bLoadOldChunks)
		{

			CONTROLLER_CHUNK_DESC_0828* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0828*)pChunkDesc->data;
			SwapEndian(*pCtrlChunk);

			//pSkinningInfo->m_nControllerId = pCtrlChunk->nControllerId;

			uint32 numKeys = pCtrlChunk->numKeys;

			CryKeyPQLog* pCryKey = (CryKeyPQLog*)(pCtrlChunk+1);
			SwapEndian( pCryKey,numKeys );


			CControllerPQLog * pController = new CControllerPQLog;

			pController->m_nControllerId	= pCtrlChunk->nControllerId;


			uint32 told=-1;
			uint32 tnew=0;
			for (uint32 i=0; i<numKeys; ++i)
			{
				tnew=pCryKey[i].nTime;
				uint32 dif=tnew-told;
				assert(dif<160);
				told=pCryKey[i].nTime;

				pController->m_arrTimes[i]	= pCryKey[i].nTime/TICKS_CONVERT;
				pController->m_arrKeys[i].vPos		= pCryKey[i].vPos/100.0f;
				pController->m_arrKeys[i].vRotLog	= pCryKey[i].vRotLog;
			}

			m_pSkinningInfo->m_arrControllerId.push_back( pCtrlChunk->nControllerId );
			m_pSkinningInfo->m_pControllers.push_back(pController);
		}
		return 1;
	}

	/*
	enum {VERSION = 0x0829};

	enum { eKeyTimeRotation = 0, eKeyTimePosition = 1, eKeyTimeScale = 2 };
	CHUNK_HEADER	chdr;

	unsigned nControllerId;

	// Bitset with information of rotation info
	uint32 RotationFormat;
	uint32 numRotationKeys;
	// if we have rotation information we ALWAYS have a keytimes info
	uint32 PositionFormat;
	uint32 numPositionKeys;
	uint32 PositionKeysInfo;

	uint32 ScaleFormat;
	uint32 numScaleKeys;
	uint32 ScaleKeysInfo; 

	*/


	if (pChunkDesc->hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0829::VERSION)
	{

		if (m_bLoadOldChunks)
			return 1;

		CONTROLLER_CHUNK_DESC_0829* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0829*)pChunkDesc->data;
//		SwapEndian(*pCtrlChunk);


		CController * pController = new CController;

		pController->m_nControllerId = pCtrlChunk->nControllerId;

		//#ifdef XENON
		//					SwapEndian(pController->m_nControllerId);
		//#endif

		m_pSkinningInfo->m_arrControllerId.push_back( pController->m_nControllerId );
		m_pSkinningInfo->m_pControllers.push_back(pController);


		char *pData = (char*)(pCtrlChunk+1);


		KeyTimesInformationPtr pRotTimeKeys;
		KeyTimesInformationPtr pPosTimeKeys;
//		KeyTimesInformationPtr pSclTimeKeys;

		TrackInformationPtr pRotation;
		PositionInformationPtr pPosition;
//		PositionInformationPtr pScale;

		if (pCtrlChunk->numRotationKeys)
		{
			// we have a rotation info
			pRotation =  TrackInformationPtr(new RotationTrackInformation);

			TrackRotationStoragePtr pStorage = ControllerHelper::GetRotationControllerPtr(pCtrlChunk->RotationFormat);

			if (!pStorage)
				return false;

			pRotation->SetRotationStorage(pStorage);  


			pStorage->Resize(pCtrlChunk->numRotationKeys);

			// its not very safe but its extremely fast!
			cryMemcpy(pStorage->GetData(), pData, pStorage->GetDataRawSize());
			pData+=pStorage->GetDataRawSize();

			pRotTimeKeys = ControllerHelper::GetKeyTimesControllerPtr(pCtrlChunk->RotationTimeFormat);//new F32KeyTimesInformation;//CKeyTimesInformation;
			if (!pRotTimeKeys)
				return false;

			pRotTimeKeys->ResizeKeyTime(pCtrlChunk->numRotationKeys);

			cryMemcpy(pRotTimeKeys->GetData(), pData, pRotTimeKeys->GetDataRawSize());
			pData += pRotTimeKeys->GetDataRawSize();

			pRotation->SetKeyTimesInformation(pRotTimeKeys);

			pController->SetRotationController(pRotation);
		}

		if (pCtrlChunk->numPositionKeys)
		{
			pPosition = PositionInformationPtr(new PositionTrackInformation);

			TrackPositionStoragePtr pStorage = ControllerHelper::GetPositionControllerPtr(pCtrlChunk->PositionFormat);
			if (!pStorage)
				return false;

			pPosition->SetPositionStorage(pStorage);

			pStorage->Resize(pCtrlChunk->numPositionKeys);

			cryMemcpy(pStorage->GetData(), pData, pStorage->GetDataRawSize());
			pData += pStorage->GetDataRawSize();

			if (pCtrlChunk->PositionKeysInfo == CONTROLLER_CHUNK_DESC_0829::eKeyTimeRotation)
			{
				pPosition->SetKeyTimesInformation(pRotTimeKeys);
			}
			else
			{
				// load from chunk
				pPosTimeKeys = ControllerHelper::GetKeyTimesControllerPtr(pCtrlChunk->PositionTimeFormat);;//PositionTimeFormat;new F32KeyTimesInformation;//CKeyTimesInformation;
				pPosTimeKeys->ResizeKeyTime(pCtrlChunk->numPositionKeys);

				cryMemcpy(pPosTimeKeys->GetData(), pData, pPosTimeKeys->GetDataRawSize());
				pData += pPosTimeKeys->GetDataRawSize();

				pPosition->SetKeyTimesInformation(pPosTimeKeys);
			}

			pController->SetPositionController(pPosition);
		}


		/*
		if (pCtrlChunk->numScaleKeys)
		{
		pScale = ControllerHelper::GetPositionControllerPtr(pCtrlChunk->ScaleFormat);

		pScale->Resize(pCtrlChunk->numScaleKeys);

		cryMemcpy(pScale->GetData(), pData, pScale->GetDataRawSize());
		pData += pScale->GetDataRawSize();

		if (pCtrlChunk->ScaleKeysInfo == CONTROLLER_CHUNK_DESC_0829::eKeyTimeRotation)
		{
		pScale->SetKeyTimesInformation(pRotTimeKeys);
		}
		else
		if(pCtrlChunk->ScaleKeysInfo == CONTROLLER_CHUNK_DESC_0829::eKeyTimePosition)
		{
		pScale->SetKeyTimesInformation(pPosTimeKeys);
		}
		else
		{
		pSclTimeKeys = new F32KeyTimesInformation;//CKeyTimesInformation;
		pSclTimeKeys->ResizeKeyTime(pCtrlChunk->numScaleKeys);

		cryMemcpy(pSclTimeKeys->GetData(), pData, pSclTimeKeys->GetDataRawSize());
		pData += pSclTimeKeys->GetDataRawSize();

		pScale->SetKeyTimesInformation(pSclTimeKeys);


		}

		pController->SetScaleController(pScale);

		}
		*/

		return 1;
	}



//FIXME:!
//	if (pChunkDesc->hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0830::VERSION)
//	{
//
//		if (m_bLoadOldChunks)
//			return 1;
//
//		CONTROLLER_CHUNK_DESC_0830* pCtrlChunk = (CONTROLLER_CHUNK_DESC_0830*)pChunkDesc->data;
//		SwapEndian(*pCtrlChunk);
//
//
//		CCompressedController * pController = new CCompressedController;
//
//		//pController->m_nControllerId = pCtrlChunk->nControllerId;
//		//m_pSkinningInfo->m_arrControllerId.push_back( pCtrlChunk->nControllerId );
//		//m_pSkinningInfo->m_pControllers.push_back(pController);
//
//
//		char *pData = (char*)(pCtrlChunk+1);
//
//		int * head = (int *)(pCtrlChunk+1);
//
//		KeyTimesInformationPtr pRotTimeKeys;
//		KeyTimesInformationPtr pPosTimeKeys;
//		KeyTimesInformationPtr pSclTimeKeys;
//
//		TrackInformationPtr pRotation;
//		PositionInformationPtr pPosition;
//		PositionInformationPtr pScale;
//
//
//		if (pCtrlChunk->ChunkType & 1)
//		{
//			// we have an wavelet compressed rotation format data
//
//			int size = pCtrlChunk->numRotationKeys;
//
//			int rot0 = head[0];
//			int rot1 = head[rot0];//(int)((int*)pData[rot0 * sizeof(int)]);
//			int rot2 = head[rot0+rot1];//(int)((int*)pData[rot0* sizeof(int)+rot1* sizeof(int)]);
//
//			int totalKeys = size;
//
//			pController->m_Rotations0.resize(rot0);
//			pController->m_Rotations1.resize(rot1);
//			pController->m_Rotations2.resize(rot2);
//
//			cryMemcpy(&(pController->m_Rotations0[0]), pData, rot0* sizeof(int));
//			pData += rot0* sizeof(int);
//
////			totalKeys = pController->m_Rotations0[1];
//			cryMemcpy(&(pController->m_Rotations1[0]), pData, rot1* sizeof(int));
//			pData += rot1* sizeof(int);
//			cryMemcpy(&(pController->m_Rotations2[0]), pData, rot2* sizeof(int));
//			pData += rot2* sizeof(int);
//
//
//			pRotation =  ControllerHelper::GetRotationControllerPtr(pCtrlChunk->RotationFormat);
//			pRotTimeKeys = ControllerHelper::GetKeyTimesControllerPtr(pCtrlChunk->RotationTimeFormat);//new F32KeyTimesInformation;//CKeyTimesInformation;
//			pRotTimeKeys->ResizeKeyTime(totalKeys);
//
//			cryMemcpy(pRotTimeKeys->GetData(), pData, pRotTimeKeys->GetDataRawSize());
//			pData += pRotTimeKeys->GetDataRawSize();
//
//			pRotation->SetKeyTimesInformation(pRotTimeKeys);
//			pController->SetRotationController(pRotation);
//
//			if (pCtrlChunk->ChunkType < 2)
//				pController->UnCompress();
//
//		}
//		else
//		if (pCtrlChunk->numRotationKeys)
//		{
//			// we have a rotation info
//			pRotation =  ControllerHelper::GetRotationControllerPtr(pCtrlChunk->RotationFormat);
//
//			pRotation->Resize(pCtrlChunk->numRotationKeys);
//
//			// its not very safe but its extremely fast!
//			cryMemcpy(pRotation->GetData(), pData, pRotation->GetDataRawSize());
//			pData+=pRotation->GetDataRawSize();
//
//			pRotTimeKeys = ControllerHelper::GetKeyTimesControllerPtr(pCtrlChunk->RotationTimeFormat);//new F32KeyTimesInformation;//CKeyTimesInformation;
//			pRotTimeKeys->ResizeKeyTime(pCtrlChunk->numRotationKeys);
//
//			cryMemcpy(pRotTimeKeys->GetData(), pData, pRotTimeKeys->GetDataRawSize());
//			pData += pRotTimeKeys->GetDataRawSize();
//
//			pRotation->SetKeyTimesInformation(pRotTimeKeys);
//
//			pController->SetRotationController(pRotation);
//
//		}
//
//		if (pCtrlChunk->ChunkType & 2)
//		{
//			// we have an wavelet compressed position format data
//
//			int size = pCtrlChunk->numPositionKeys;
//
//			int pos0 = (int)((int*)pData[0]);
//			int pos1 = (int)((int*)pData[pos0 * sizeof(int)]);
//			int pos2 = (int)((int*)pData[pos0* sizeof(int)+pos1* sizeof(int)]);
//
//			int totalKeys = size;
//
//			pController->m_Positions0.resize(pos0);
//			pController->m_Positions1.resize(pos1);
//			pController->m_Positions2.resize(pos2);
//
//			cryMemcpy(&(pController->m_Positions0[0]), pData, pos0* sizeof(int));
//			pData += pos0* sizeof(int);
//
////			totalKeys = pController->m_Positions0[1];
//			cryMemcpy(&(pController->m_Positions1[0]), pData, pos1* sizeof(int));
//			pData += pos1* sizeof(int);
//			cryMemcpy(&(pController->m_Positions2[0]), pData, pos2* sizeof(int));
//			pData += pos2* sizeof(int);
//
//
//			pPosition = ControllerHelper::GetPositionControllerPtr(pCtrlChunk->PositionFormat);
//
//			if (pCtrlChunk->PositionKeysInfo == CONTROLLER_CHUNK_DESC_0829::eKeyTimeRotation)
//			{
//				pPosition->SetKeyTimesInformation(pRotTimeKeys);
//			}
//			else
//			{
//				// load from chunk
//				pPosTimeKeys = ControllerHelper::GetKeyTimesControllerPtr(pCtrlChunk->PositionTimeFormat);;//PositionTimeFormat;new F32KeyTimesInformation;//CKeyTimesInformation;
//				pPosTimeKeys->ResizeKeyTime(totalKeys);
//
//				cryMemcpy(pPosTimeKeys->GetData(), pData, pPosTimeKeys->GetDataRawSize());
//				pData += pPosTimeKeys->GetDataRawSize();
//
//				pPosition->SetKeyTimesInformation(pPosTimeKeys);
//			}
//
//			pController->SetPositionController(pPosition);
//
//			pController->UnCompress();
//
//		}
//		else
//		if (pCtrlChunk->numPositionKeys)
//		{
//			pPosition = ControllerHelper::GetPositionControllerPtr(pCtrlChunk->PositionFormat);
//
//			pPosition->Resize(pCtrlChunk->numPositionKeys);
//
//			cryMemcpy(pPosition->GetData(), pData, pPosition->GetDataRawSize());
//			pData += pPosition->GetDataRawSize();
//
//			if (pCtrlChunk->PositionKeysInfo == CONTROLLER_CHUNK_DESC_0829::eKeyTimeRotation)
//			{
//				pPosition->SetKeyTimesInformation(pRotTimeKeys);
//			}
//			else
//			{
//				// load from chunk
//				pPosTimeKeys = ControllerHelper::GetKeyTimesControllerPtr(pCtrlChunk->PositionTimeFormat);;//PositionTimeFormat;new F32KeyTimesInformation;//CKeyTimesInformation;
//				pPosTimeKeys->ResizeKeyTime(pCtrlChunk->numPositionKeys);
//
//				cryMemcpy(pPosTimeKeys->GetData(), pData, pPosTimeKeys->GetDataRawSize());
//				pData += pPosTimeKeys->GetDataRawSize();
//
//				pPosition->SetKeyTimesInformation(pPosTimeKeys);
//			}
//
//			pController->SetPositionController(pPosition);
//		}
//
//
//		CController * pCController = new CController;
//		pCController->SetPositionController(pController->GetPositionController());
//		pCController->SetRotationController(pController->GetRotationController());
//
//		pCController->m_nControllerId = pCtrlChunk->nControllerId;
//		m_pSkinningInfo->m_arrControllerId.push_back( pCtrlChunk->nControllerId );
//		m_pSkinningInfo->m_pControllers.push_back(pCController);
//
//		//pCController->SetPositionController(pController->GetPositionController());
//		/*
//		if (pCtrlChunk->numScaleKeys)
//		{
//		pScale = ControllerHelper::GetPositionControllerPtr(pCtrlChunk->ScaleFormat);
//
//		pScale->Resize(pCtrlChunk->numScaleKeys);
//
//		cryMemcpy(pScale->GetData(), pData, pScale->GetDataRawSize());
//		pData += pScale->GetDataRawSize();
//
//		if (pCtrlChunk->ScaleKeysInfo == CONTROLLER_CHUNK_DESC_0829::eKeyTimeRotation)
//		{
//		pScale->SetKeyTimesInformation(pRotTimeKeys);
//		}
//		else
//		if(pCtrlChunk->ScaleKeysInfo == CONTROLLER_CHUNK_DESC_0829::eKeyTimePosition)
//		{
//		pScale->SetKeyTimesInformation(pPosTimeKeys);
//		}
//		else
//		{
//		pSclTimeKeys = new F32KeyTimesInformation;//CKeyTimesInformation;
//		pSclTimeKeys->ResizeKeyTime(pCtrlChunk->numScaleKeys);
//
//		cryMemcpy(pSclTimeKeys->GetData(), pData, pSclTimeKeys->GetDataRawSize());
//		pData += pSclTimeKeys->GetDataRawSize();
//
//		pScale->SetKeyTimesInformation(pSclTimeKeys);
//
//
//		}
//
//		pController->SetScaleController(pScale);
//
//		}
//		*/
//
//		return 1;
//
//
//	}
//
//

	if (pChunkDesc->hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0831::VERSION)
	{
		return 1;
	}

	//if (pChunkDesc->hdr.ChunkVersion == CONTROLLER_CHUNK_DESC_0900::VERSION)
	//{
	//	// load tracks database
	//}
	return 0;
}


void CLoaderCAF::Warning(const char* szFormat, ...)
{
	if (m_pListener)
	{
		char szBuffer[1024];
		va_list args;
		va_start(args, szFormat);
		vsprintf_s(szBuffer, szFormat, args);
		m_pListener->Warning(szBuffer);
		va_end(args);
	}
}
