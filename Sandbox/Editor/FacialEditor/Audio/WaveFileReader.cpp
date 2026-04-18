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

#include "StdAfx.h"
#include "WaveFileReader.h"
#include "WaveFile.h"
#include "IStreamEngine.h"

//////////////////////////////////////////////////////////////////////////
CWaveFileReader::CWaveFileReader()
{
}

//////////////////////////////////////////////////////////////////////////
CWaveFileReader::~CWaveFileReader()
{
	DestroyData();
}

bool CWaveFileReader::LoadFile(const char *sFileName)
{
	DestroyData();

	CCryFile file;
	if (!file.Open( sFileName,"rb "))
	{
		m_bLoadFailure = true;
		return false;
	}

	int filesize = file.GetLength();
	char *buf = new char[filesize+16];

	file.ReadRaw( buf,filesize );

	void* pSample = LoadAsSample( buf,filesize );
	if (!pSample)
	{
		delete []buf;
		m_bLoadFailure = true;
		return false;
	}		

	return true;
}

//////////////////////////////////////////////////////////////////////////
void* CWaveFileReader::LoadAsSample(const char *AssetDataPtr, int nLength)
{
	ATG::WaveFileMemory MemWaveFile;
	ATG::WAVEFORMATEXTENSIBLE wfx;
	DWORD dwNewSize;
	//void* pSoundData = NULL;

	// Initialize the WaveFile Class and read all the chunks
	MemWaveFile.Init(AssetDataPtr, nLength);
	MemWaveFile.GetFormat(&wfx);

	// Create new memory for the sound data
	MemWaveFile.GetDuration( &dwNewSize );

	if (!m_MemBlock.Allocate(nLength))
		return NULL;

	// read the sound data from the file
	MemWaveFile.ReadSample(0, m_MemBlock.GetBuffer(), dwNewSize, &dwNewSize);

	// Pass some information to the SoundBuffer
	m_nBaseFreq			= wfx.Format.nSamplesPerSec;
	m_nBitsPerSample = wfx.Format.wBitsPerSample;
	m_nChannels			= wfx.Format.nChannels;
	m_nLengthInBytes	= dwNewSize;

	// Check for embedded loop points
	DWORD dwLoopStart  = 0;
	DWORD dwLoopLength = dwNewSize;
	MemWaveFile.GetLoopRegionBytes( &dwLoopStart, &dwLoopLength );
	// TODO enable to read the loop points from the file and store is somewhere

	if (dwNewSize > 0)
	{
		float fBaseFreqMs = m_nBaseFreq/1000.0f;
		m_nLengthInMs			= (8*m_nLengthInBytes)/(fBaseFreqMs*m_nChannels*m_nBitsPerSample);
		m_nLengthInSamples = (8*m_nLengthInBytes)/(m_nChannels*m_nBitsPerSample);
	}

	return (m_MemBlock.GetBuffer());
}

//void SwapTyp( uint16 &x )
//{
//	x = (x<<8) | (x>>8);
//}

int32 CWaveFileReader::GetSample(uint32 nPos)
{
	int32 nData = 0;
	if (m_nLengthInSamples > nPos)
	{		
		
		uint32 nOffset = nPos*m_nChannels;
		
		switch(m_nBitsPerSample)
		{
			case 8: 
				{
					uint8 *pPointer = (uint8*)m_MemBlock.GetBuffer();
					uint8 nData8 = pPointer[nOffset];
					nData = (int8)nData8;
					break;
				}
			case 16: 
				{
					uint16 *pPointer = (uint16*)m_MemBlock.GetBuffer();
					uint16 nData16 = pPointer[nOffset];

					nData = (int16)nData16;
					break;
				}
			default:
				assert(0);
				break;
		}
			
	}
	return nData;
}

//////////////////////////////////////////////////////////////////////////
f32 CWaveFileReader::GetSampleNormalized(uint32 nPos)
{

	int32 nData = GetSample(nPos);
	f32 fData = 0;

	switch(m_nBitsPerSample)
	{
	case 8: 
		{
			fData = nData/256.0f;
			break;
		}
	case 16: 
		{
			fData = nData/(float)(1<<15);
			break;
		}
	default:
		assert(0);
		break;
	}

	return fData;
}

//////////////////////////////////////////////////////////////////////////
void  CWaveFileReader::GetSamplesMinMax( int nPos,int nSamplesCount,float &vmin,float &vmax )
{
	if (nPos < 0)
	{
		nSamplesCount -= nPos;
		nPos = 0;
	}
	if (nPos >= m_nLengthInSamples || nSamplesCount < 0)
	{
		vmin = 0;
		vmax = 0;
		return;
	}
	uint32 nEnd = nPos+nSamplesCount;
	if (nEnd > m_nLengthInSamples)
	{
		nEnd = m_nLengthInSamples;
	}
		
	float fMinValue = 0.0f;
	float fMaxValue = 0.0f;

	for (uint32 i = nPos; i < nEnd; ++i)
	{
		float fValue = GetSampleNormalized(i);
		fMinValue += min(fMinValue, fValue);
		fMaxValue += max(fMaxValue, fValue);
		fMinValue = fMinValue*0.5f;
		fMaxValue = fMaxValue*0.5f;
	}
	vmin = fMinValue;
	vmax = fMaxValue;
}
	
//////////////////////////////////////////////////////////////////////////
void CWaveFileReader::DestroyData()
{
	m_MemBlock.Free();

	m_nBaseFreq = 0;
	m_nBitsPerSample = 0;
	m_nChannels = 0;
	m_nVolume = 0;
	m_nTimesUsed = 0;
	m_nLengthInBytes = 0;
	m_nLengthInMs = 0;
	m_nLengthInSamples = 0;
	m_bLoadFailure = 0;
}
