////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002-2006.
// -------------------------------------------------------------------------
//  File name:   SelectSequenceDialog.cpp
//  Version:     v1.00
//  Created:     14/03/2006 by AlexL.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SelectSequenceDialog.h"
#include <IMovieSystem.h>

// CSelectSequence dialog

IMPLEMENT_DYNAMIC(CSelectSequenceDialog, CGenericSelectItemDialog)

//////////////////////////////////////////////////////////////////////////
CSelectSequenceDialog::CSelectSequenceDialog(CWnd* pParent) :	CGenericSelectItemDialog(pParent)
{
	m_dialogID = "Dialogs\\SelSequence";
}

//////////////////////////////////////////////////////////////////////////
/* virtual */ BOOL
CSelectSequenceDialog::OnInitDialog()
{
	SetTitle(_T("Select Sequence"));
	SetMode(eMODE_LIST);
	return __super::OnInitDialog();
}

//////////////////////////////////////////////////////////////////////////
/* virtual */ void
CSelectSequenceDialog::GetItems(std::vector<SItem>& outItems)
{
	IMovieSystem *pMovieSys = GetIEditor()->GetMovieSystem();
	ISequenceIt* pIter = pMovieSys->GetSequences();
	IAnimSequence* pSeq = pIter->first();
	while (pSeq != 0)
	{
		const char* seqName = pSeq->GetName();
		SItem item;
		item.name = seqName;
		outItems.push_back(item);
		pSeq = pIter->next();
	}
	pIter->Release();
}
