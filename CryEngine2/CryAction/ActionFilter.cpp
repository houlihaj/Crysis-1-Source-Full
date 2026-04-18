/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 8:9:2004   10:33 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ActionFilter.h"
#include "ActionMapManager.h"


//------------------------------------------------------------------------
CActionFilter::CActionFilter(CActionMapManager *pActionMapManager, const char* name, EActionFilterType type)
: m_pActionMapManager(pActionMapManager),
	m_enabled(false),
	m_type(type),
	m_name(name)
{
}

//------------------------------------------------------------------------
CActionFilter::~CActionFilter()
{
	m_pActionMapManager->RemoveActionFilter(this);
}

//------------------------------------------------------------------------
void CActionFilter::Filter(const ActionId& action)
{
	m_filterActions.insert(action);
}

//------------------------------------------------------------------------
void CActionFilter::Enable(bool enable)
{ 
	m_enabled = enable;

	// auto-release all actions which 
	if (m_enabled && m_type == eAFT_ActionFail)
	{
		TFilterActions::const_iterator it = m_filterActions.begin();
		TFilterActions::const_iterator end = m_filterActions.end();
		for (; it!=end; ++it)
		{
			m_pActionMapManager->ReleaseActionIfActive(*it);
		}
	}
};
//------------------------------------------------------------------------
bool CActionFilter::ActionFiltered(const ActionId& action)
{
	// never filter anything out when disabled
	if (!m_enabled)
		return false;

	TFilterActions::const_iterator it = m_filterActions.find(action);

	if (m_type == eAFT_ActionPass)
	{
		return (it == m_filterActions.end());
	}
	else
	{
		return (it != m_filterActions.end());
	}
}

//------------------------------------------------------------------------
bool CActionFilter::SerializeXML(const XmlNodeRef& root, bool bLoading)
{
	if (bLoading)
	{
		// loading
		const XmlNodeRef& child = root;
		if (strcmp(child->getTag(), "actionfilter") != 0)
			return false;

		EActionFilterType actionFilterType = eAFT_ActionFail;
		if (!strcmp(child->getAttr("type"), "actionFail"))
			actionFilterType = eAFT_ActionFail;
		if (!strcmp(child->getAttr("type"), "actionPass"))
			actionFilterType = eAFT_ActionPass;

		m_type = actionFilterType;

		int nFilters = child->getChildCount();
		for (int f=0; f<nFilters; ++f)
		{
			XmlNodeRef filter = child->getChild(f);
			Filter(filter->getAttr("name"));
		}
	}
	else
	{
		// saving
		XmlNodeRef filterRoot = root->newChild("actionfilter");
		filterRoot->setAttr("name", m_name);
		filterRoot->setAttr("type", m_type == eAFT_ActionPass ? "actionPass" : "actionFail");
		filterRoot->setAttr("version", m_pActionMapManager->GetVersion());
		TFilterActions::const_iterator iter = m_filterActions.begin();
		while (iter != m_filterActions.end())
		{
			XmlNodeRef filterChild = filterRoot->newChild("filter");
			filterChild->setAttr("name", iter->c_str());
			++iter;
		}
	}
	return true;
}

void CActionFilter::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->AddContainer(m_filterActions);
//	for (TFilterActions::iterator iter = m_filterActions.begin(); iter != m_filterActions.end(); ++iter)
//		s->Add(*iter);
}
