////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   LoaderDBA.h
//  Version:     v1.00
//  Created:     31/08/2006 by Alexey Medvedev.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __DBALoader_h__
#define __DBALoader_h__
#pragma once

#include "LoaderCAF.h"
#include "ControllerPQ.h"

typedef std::vector<CCommonSkinningInfo * > THeaderVector;
typedef std::vector<string> TNamesVector;


struct CInternalDatabaseInfo
{
	TNamesVector m_AnimationNames;
	std::map<uint32, uint32> m_AnimationCRCMap;
	THeaderVector m_Headers;

	DynArray<QuatT> m_arrStartDirs;

	DynArray<char> m_Storage;

	DynArray<IKeyTimesInformation*> m_arrKeyTimesArray;
	DynArray<ITrackRotationStorage*> m_arrRotationTracksArray;
	DynArray<ITrackPositionStorage*> m_arrPositionTracksArray;

	DynArray<KeyTimesInformationPtr> m_arrKeyTimes;
	DynArray<TrackRotationStoragePtr> m_arrRotationTracks;
	DynArray<TrackPositionStoragePtr> m_arrPositionTracks;

	int m_iTotalControllers;

//	CControllerInplace * m_pControllers;
//	RotationTrackInformation * m_pRots;
//	PositionTrackInformation * m_pPoss;

	CInternalDatabaseInfo()/* : m_pControllers(0), m_pPoss(0), m_pRots(0)*/ 
	{
//		m_arrKeyTimesArray = 0;
//		m_arrRotationTracksArray = 0;
	};

	~CInternalDatabaseInfo()
	{
		Clear();
	}

	void Clear()
	{
		for (std::size_t i =0, end =m_arrRotationTracksArray.size(); i < end; ++i )
		{
			if (m_arrRotationTracksArray[i])
				ControllerHelper::DeleteRotationControllerPtrArray(m_arrRotationTracksArray[i]);
		}

		for (std::size_t i =0, end =m_arrKeyTimesArray.size(); i < end; ++i )
		{
			if (m_arrKeyTimesArray[i])
				ControllerHelper::DeleteTimesControllerPtrArray(m_arrKeyTimesArray[i]);
		}
		for (std::size_t i =0, end =m_arrPositionTracksArray.size(); i < end; ++i )
		{

			if (m_arrPositionTracksArray[i])
				ControllerHelper::DeletePositionControllerPtrArray(m_arrPositionTracksArray[i]);
		}

		//SAFE_DELETE_ARRAY(m_pControllers);
		//SAFE_DELETE_ARRAY(m_pPoss);
		//SAFE_DELETE_ARRAY(m_pRots);

	}

};

class CLoaderDBA
{

public:
	friend class CAnimationManager;
	CLoaderDBA();
	~CLoaderDBA();

	void AddRef()
	{
		++m_iRefCounter;
	}
	void Release()
	{
		--m_iRefCounter;
	}

	int GetRefcounter()
	{
		return m_iRefCounter;
	}

private:
	CInternalDatabaseInfo* LoadDBA(const char *filename);
	CInternalDatabaseInfo* LoadDBA( const char *filename,IChunkFile * pChunkFile);
	const char *GetLastError() const { return m_LastError; }

	const CCommonSkinningInfo * GetSkinningInfo(const char * filename) const;
	bool RemoveSkinningInfo(const char * filename);

	const size_t SizeOfThis() const;


	bool LoadChunks();

	bool ReadControllers(IChunkFile::ChunkDesc *pChunkDesc);
	bool ReadController900(IChunkFile::ChunkDesc *pChunkDesc);
	bool ReadController902 (IChunkFile::ChunkDesc *pChunkDesc);
	bool ReadController903 (IChunkFile::ChunkDesc *pChunkDesc);
	bool ReadController904 (IChunkFile::ChunkDesc *pChunkDesc);
	int32 FindFormat(uint32 num, std::vector<uint32>& vec)
	{
		for (size_t i =0; i < vec.size(); ++i)
		{
			if ( num < vec[i+1])
				return (int)i;
		}

		return -1;
	}


public:
	CInternalDatabaseInfo * m_pDatabaseInfo;
	string m_Filename;
private:
	int m_iRefCounter;
	string m_LastError;
	IChunkFile * m_pChunkFile;
};


#endif