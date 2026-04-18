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
#include "serializebuffer.h"
#include "ObjMan.h"
/*
bool CStatObj::Serialize(int & nPos, uchar * pSerBuf, bool bSave, const char * szFolderName)
{
  char szSignature[16];
  if(!LoadBuffer(szSignature, 8, pSerBuf, nPos) || strcmp(szSignature,"StatObj"))
    return false;

	// @TODO:
	// Load CGF ChunkFile from the buffer.

	return true;
}
*/