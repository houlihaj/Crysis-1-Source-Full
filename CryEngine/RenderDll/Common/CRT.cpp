////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   CRT.cpp
//  Version:     v1.00
//  Created:     25/9/2004.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

void CRTFreeData(void *pData)
{
  free(pData);
}

void CRTDeleteArray(void *pData)
{
  delete [] (char*)pData;
}