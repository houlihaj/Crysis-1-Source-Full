////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002-2006.
// -------------------------------------------------------------------------
//  File name:   SelectMissionObjectiveDialog.cpp
//  Version:     v1.00
//  Created:     14/03/2006 by AlexL.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SelectMissionObjectiveDialog.h"
#include <IMovieSystem.h>

// CSelectMissionObjectiveDialog

IMPLEMENT_DYNAMIC(CSelectMissionObjectiveDialog, CGenericSelectItemDialog)

//////////////////////////////////////////////////////////////////////////
CSelectMissionObjectiveDialog::CSelectMissionObjectiveDialog(CWnd* pParent) :	CGenericSelectItemDialog(pParent)
{
	m_dialogID = "Dialogs\\SelMissionObj";
}

//////////////////////////////////////////////////////////////////////////
/* virtual */ BOOL
CSelectMissionObjectiveDialog::OnInitDialog()
{
	SetTitle(_T("Select MissionObjective"));
	SetMode(eMODE_TREE);
	ShowDescription(true);
	return __super::OnInitDialog();
}

//////////////////////////////////////////////////////////////////////////
/* virtual */ void
CSelectMissionObjectiveDialog::GetItems(std::vector<SItem>& outItems)
{
	// load MOs
	CString path = Path::GetGameFolder()+ "/Libs/UI/Objectives_new.xml";
	XmlNodeRef missionObjectives = GetISystem()->LoadXmlFile(path.GetString());
	if (missionObjectives == 0)
	{
		Error( _T("Error while loading MissionObjective file '%s'"), path.GetString() );
		return;
	}

	for(int tag = 0; tag < missionObjectives->getChildCount(); ++tag)
	{
		XmlNodeRef mission = missionObjectives->getChild(tag);
		const char* attrib;
		const char* objective;
		const char* text;
		for(int obj = 0; obj < mission->getChildCount(); ++obj)
		{
			XmlNodeRef objectiveNode = mission->getChild(obj);
			CString id(mission->getTag());
			id += ".";
			id += objectiveNode->getTag();
			if(objectiveNode->getAttributeByIndex(0, &attrib, &objective) && objectiveNode->getAttributeByIndex(1, &attrib, &text))
			{
				SObjective obj;
				obj.shortText = objective;
				obj.longText = text;
				m_objMap [id] = obj;
				SItem item;
				item.name = id;
				outItems.push_back(item);
			}
			else
			{
				Error( _T("Error while loading MissionObjective file '%s'"), path.GetString() );
				return;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
/* virtual */ void CSelectMissionObjectiveDialog::ItemSelected()
{
	TObjMap::const_iterator iter = m_objMap.find(m_selectedItem);
	if (iter != m_objMap.end())
	{
		const SObjective& obj = (*iter).second;
		CString objText = obj.shortText;
		if (objText.IsEmpty() == false && objText.GetAt(0) == '@')
		{
			ILocalizationManager::SLocalizedInfo locInfo;
			const char* key = objText.GetString() + 1;
			bool bFound = gEnv->pSystem->GetLocalizationManager()->GetLocalizedInfo(key, locInfo);
			objText = "Label: ";
			objText+=obj.shortText;
			if (bFound)
			{
				objText += "\r\nPlain Text: ";
				objText += locInfo.sEnglish;
			}
		}
		m_desc.SetWindowText(objText);
#if 0
		CryLogAlways("Objective: ID='%s' Text='%s' LongText='%s'", 
			m_selectedItem.GetString(), obj.shortText.GetString(), obj.longText.GetString());
#endif
	}
	else
	{
		m_desc.SetWindowText(_T("<No Objective Selected>"));
	}
}

