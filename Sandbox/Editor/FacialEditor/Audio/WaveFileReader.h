////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   WaveFileReader.h
//  Version:     v1.00
//  Created:     31/10/2005 by Tomas.
//  Compilers:   Visual Studio.NET
//  Description: Implementation of simple raw wave file reader
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __WAVEFILEREADER_H__
#define __WAVEFILEREADER_H__

#pragma once

#include <IStreamEngine.h>

class CWaveFileReader
{
public:

	CWaveFileReader();
	~CWaveFileReader(void);

	bool LoadFile(const char *sFileName);
	int32	GetSample(uint32 nPos);
	float GetSampleNormalized(uint32 nPos);
	void  GetSamplesMinMax( int nPos,int nSamplesCount,float &vmin,float &vmax );
	int32	GetLengthMs() const { return m_nLengthInMs; };

	uint32	GetSampleCount() { return m_nLengthInSamples; }
	uint32	GetSamplesPerSec() { return m_nBaseFreq; }
	uint32	GetBitDepth() { return m_nBitsPerSample; }
	
protected:
	void* LoadAsSample(const char *AssetDataPtrOrName, int nLength);

	// Closes down Stream or frees memory of the Sample
	void	DestroyData();

	CMemoryBlock				m_MemBlock;

	int32						m_nBaseFreq;
	int32						m_nBitsPerSample;
	int32						m_nChannels;
	uint32					m_nVolume;
	uint32					m_nTimesUsed;
	uint32					m_nLengthInBytes;
	uint32					m_nLengthInMs;
	uint32					m_nLengthInSamples;
	bool						m_bLoadFailure;
};

#endif
