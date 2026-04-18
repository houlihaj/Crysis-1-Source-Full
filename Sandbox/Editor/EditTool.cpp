////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001.
// -------------------------------------------------------------------------
//  File name:   EditTool.cpp
//  Version:     v1.00
//  Created:     18/12/2001 by Timur.
//  Compilers:   Visual C++ 6.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "EditTool.h"
#include "Plugin.h"

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNAMIC(CEditTool,CObject);

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CEditTool_ClassDesc : public CRefCountClassDesc
{
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }
	virtual REFGUID ClassID() {
		// {0A43AB8E-B1AE-44aa-93B1-229F73D58CA4}
		static const GUID guid = { 0xa43ab8e, 0xb1ae, 0x44aa, { 0x93, 0xb1, 0x22, 0x9f, 0x73, 0xd5, 0x8c, 0xa4 } };
		return guid;
	}
	virtual const char* ClassName() { return "EditTool.Default"; };
	virtual const char* Category() { return "EditTool"; };
	virtual CRuntimeClass* GetRuntimeClass() { return 0; }
};
CEditTool_ClassDesc g_stdClassDesc;

//////////////////////////////////////////////////////////////////////////
CEditTool::CEditTool()
{
	m_pClassDesc = &g_stdClassDesc;
	m_nRefCount = 0;
};

//////////////////////////////////////////////////////////////////////////
void CEditTool::SetParentTool( CEditTool *pTool )
{
	m_pParentTool = pTool;
}

//////////////////////////////////////////////////////////////////////////
CEditTool* CEditTool::GetParentTool()
{
	return m_pParentTool;
}

//////////////////////////////////////////////////////////////////////////
void CEditTool::Abort()
{
	if (m_pParentTool)
		GetIEditor()->SetEditTool(m_pParentTool);
	else
		GetIEditor()->SetEditTool(0);
}