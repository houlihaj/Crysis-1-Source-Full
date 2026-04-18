////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   DialogScriptRecord.h
//  Version:     v1.00
//  Created:     11-09-2006 by AlexL
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __DIALOGSCRIPTRECORD_H__
#define __DIALOGSCRIPTRECORD_H__

#pragma once

#include "DialogManager.h"

class CDialogScriptRecord : public CXTPReportRecord
{
public:
	CDialogScriptRecord();
	CDialogScriptRecord(CEditorDialogScript* pScript, const CEditorDialogScript::SScriptLine* pLine);
	const CEditorDialogScript::SScriptLine* GetLine() const { return &m_line; }
	void Swap(CDialogScriptRecord* pOther);

protected:
	void OnBrowseSound(string& value, CDialogScriptRecord* pRecord);
	void OnBrowseFacial(string& value, CDialogScriptRecord* pRecord);
	void OnBrowseAG(string& value, CDialogScriptRecord* pRecord);
	void GetItemMetrics(XTP_REPORTRECORDITEM_DRAWARGS* pDrawArgs, XTP_REPORTRECORDITEM_METRICS* pItemMetrics);
	void FillItems();

protected:
	CEditorDialogScript* m_pScript;
	CEditorDialogScript::SScriptLine m_line;
};

#endif // 
