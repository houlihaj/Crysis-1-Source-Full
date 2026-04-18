////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   RasterMPMan.cpp
//  Version:     v1.00
//  Created:     20/08/2006 by MichaelG
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "RasterMPMan.h"
#include <CryThread.h>

CRasterMPMan::SMPData CRasterMPMan::scMPData[scMaxMatChunks][2];
HANDLE CRasterMPMan::scEventID;			
HANDLE CRasterMPMan::scWorkerEventID;
uint32 CRasterMPMan::scMPDataCount[2];
uint32 CRasterMPMan::scActiveSet;

void CRasterMPMan::StartProjectTrianglesOS()
{
	if(m_CoreCount <= 1)
	{
		for(int i=0; i<scMPDataCount[scActiveSet]; ++i)
		{
			SMPData& rMPData = scMPData[i][scActiveSet];
			rMPData.pBBoxRaster->ProjectTrianglesOS
			(
				rMPData.cWorldMin,
				rMPData.cWorldMax, 
				*rMPData.cpOSToWorldRotMatrix,
				*rMPData.cpChunks,
				rMPData.useConservativeFill, 
				rMPData.cpTriangleValidator
			);
		}
		scActiveSet = (scActiveSet + 1) & 0x1;//switch set
		scMPDataCount[scActiveSet] = 0;//reset it
	}
	else
	{
		if(m_ThreadActive)
			WaitForSingleObject(scEventID,INFINITE);//wait til last call was finished
		scActiveSet = (scActiveSet + 1) & 0x1;//switch set
		m_ThreadActive = true;
		scMPDataCount[scActiveSet] = 0;//reset it
		SetEvent(scWorkerEventID);//signal next processing 
	}
}

#if defined(DO_RASTER_MP)
DWORD WINAPI CRasterMPMan::ProjectTrianglesOSMPCore(LPVOID)
{
	CryThreadSetName( -1,"SHLighting2" );
	while(true)
	{
		WaitForSingleObject(scWorkerEventID,INFINITE);
		const bool cSetToProcess = (scActiveSet + 1) & 0x1;
		if(scMPDataCount[cSetToProcess] == 0xFFFFFFFF)//signaled thread exiting
		{
			SetEvent(scEventID);
			return -1;//finish thread routine
		}
		for(int i=0; i<scMPDataCount[cSetToProcess]; ++i)
		{
			SMPData& rMPData = scMPData[i][cSetToProcess];
			rMPData.pBBoxRaster->ProjectTrianglesOS
			(
				rMPData.cWorldMin,
				rMPData.cWorldMax, 
				*rMPData.cpOSToWorldRotMatrix,
				*rMPData.cpChunks,
				rMPData.useConservativeFill, 
				rMPData.cpTriangleValidator
			);
		}
		SetEvent(scEventID);
	}
	return 0;
}
#endif
