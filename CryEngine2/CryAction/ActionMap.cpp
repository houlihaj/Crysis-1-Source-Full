/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 7:9:2004   17:48 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "ActionMap.h"
#include "ActionMapManager.h"
#include "IConsole.h"
#include "GameObjects/GameObject.h"
#include <crc32.h>

struct CryNameSorter
{
	bool operator() (const CCryName& lhs, const CCryName& rhs) const
	{
		assert (lhs.c_str() != 0);
		assert (rhs.c_str() != 0);
		return strcmp(lhs.c_str(),rhs.c_str()) < 0;
	}
};

class CConsoleActionListener : public IActionListener
{
public:
	void OnAction( const ActionId& actionId, int activationMode, float value )
	{
		IConsole * pCons = gEnv->pConsole;
		pCons->ExecuteString( actionId.c_str() );
	}
};

//------------------------------------------------------------------------
CActionMap::CActionMap(CActionMapManager *pActionMapManager, const char* name)
: m_pActionMapManager(pActionMapManager),
	m_enabled(true),
	m_listenerId(0),
	m_name(name)
{
}

//------------------------------------------------------------------------
CActionMap::~CActionMap()
{
	m_pActionMapManager->RemoveActionMap(this);
}

//------------------------------------------------------------------------
void CActionMap::CreateAction(const char *name, int activationMode, const char* defaultKey0, const char* defaultKey1)
{
	// remove any previous binds of this action in actionmapmanager
	// the manager manages its own set of CRC32->BindData
	TActionMap::iterator iter = m_actions.find(name);
	if (iter != m_actions.end())
	{
		const SAction& act = iter->second;
		ActionId actId = iter->first;
		for (int i = 0; i < KEYS_PER_ACTION; i++)
		{
			RemoveBind(act.crcKey[i], actId, i);
		}
	}

	SAction action;

	action.activationMode = activationMode;
	action.defaultKey[0] = defaultKey0;
	action.defaultKey[1] = defaultKey1;
	
	ActionId actId = name;

	for (int i = 0; i < KEYS_PER_ACTION; i++)
	{
		action.key[i] = action.defaultKey[i];
		action.crcKey[i] = m_pActionMapManager->GetCrc32Gen().GetCRC32Lowercase(action.key[i].c_str());
		AddBind(action.crcKey[i], actId, i);
	}

	m_actions[actId] = action; // overrides if already present
}

//------------------------------------------------------------------------
void CActionMap::BindAction(const char *name, const char *keyName, int keyNumber)
{
	if (keyNumber < KEYS_PER_ACTION)
	{
		uint32 crc = m_pActionMapManager->GetCrc32Gen().GetCRC32Lowercase(keyName);
		ActionId actionId(name);

		TActionMap::iterator it = m_actions.find(actionId);
		if (it != m_actions.end())
		{
			SAction &action = it->second;

			// if a position was specified,
			// override the key at that position
			if (keyNumber >= 0)
			{
				RemoveBind(action.crcKey[keyNumber], actionId, keyNumber);
				action.key[keyNumber] = keyName;
				action.crcKey[keyNumber] = crc;
				AddBind(action.crcKey[keyNumber], actionId, keyNumber);
			}
			else
			{
				// try to find an empty spot
				for (int i = 0; i < KEYS_PER_ACTION; i++)
				{
					if (action.key[i] < 0)
					{
						action.key[i] = keyName;
						action.crcKey[i] = crc;
						AddBind(crc, actionId, i);
						return;
					}
				}

				// didn't find an empty spot
				// rotate and override the 1st
				for(int i = KEYS_PER_ACTION-1; i > 0; --i)
				{
					RemoveBind(action.crcKey[i], actionId, i);
					action.key[i] = action.key[i-1];
					action.crcKey[i] = action.crcKey[i-1];
					AddBind(action.crcKey[i], actionId, i);
				}

				action.key[0] = keyName;
				action.crcKey[0] = crc;
				AddBind(action.crcKey[0], actionId, 0);
			}
		}
	}
}

//------------------------------------------------------------------------
void CActionMap::AddBind(uint32 crc, const ActionId& actionId, uint32 keyNumber)
{
	m_pActionMapManager->AddBind(crc, actionId, keyNumber, this);
}

void CActionMap::RemoveBind(uint32 crc, const ActionId& actionId, uint32 keyNumber)
{
	m_pActionMapManager->RemoveBind(crc, actionId, keyNumber, this);
}
//------------------------------------------------------------------------
void CActionMap::SetActionListener(EntityId id)
{
	m_listenerId = id;
}

//------------------------------------------------------------------------
void CActionMap::Reset()
{
  m_enabled = true;

	for (TActionMap::iterator it = m_actions.begin(); it != m_actions.end(); it++)
	{
		SAction &action = it->second;

		for (int i = 0; i < KEYS_PER_ACTION; i++)
		{
			action.key[i] = action.defaultKey[i];
			action.crcKey[i] = m_pActionMapManager->GetCrc32Gen().GetCRC32Lowercase(action.key[i].c_str());
		}
	}
}

//------------------------------------------------------------------------
void CActionMap::ProcessInput(const ActionId& actionId, const SInputEvent &inputEvent, uint32 keyNumber, uint32 inputCrc)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);
	if (!m_enabled)
		return;

	IActionListener * pEntityListener = NULL;

	if (IEntity * pEntity = gEnv->pEntitySystem->GetEntity(m_listenerId))
		if (CGameObject * pGameObject = (CGameObject*) pEntity->GetProxy(ENTITY_PROXY_USER))
			pEntityListener = pGameObject;
	
	CConsoleActionListener consoleActionListener;

	// look up the actions which are associated with this
	TActionMap::iterator actionIter = m_actions.find(actionId);
	if (actionIter == m_actions.end())
		return;

	// process the action
	SAction& action = actionIter->second;

	IActionListener* pListener = pEntityListener;
	if (action.activationMode & eAAM_ConsoleCmd)
		pListener = &consoleActionListener;

	if (!pListener)
		return;

	if (action.activationMode & eAAM_NoModifiers)
	{
		if (inputEvent.modifiers & eMM_Modifiers)
			return;
	}

	int currentMode = action.activationMode & inputEvent.state;
	if (inputEvent.state == eIS_Changed)
		currentMode = eAAM_Always;

	bool filteredOut = m_pActionMapManager->ActionFiltered(actionId);

	if (currentMode && !filteredOut)
	{
		if (action.activationMode & (eAAM_OnPress|eAAM_OnRelease))
		{
			if (inputEvent.state == eIS_Released || inputEvent.state == eIS_Pressed)
				action.currentState = inputEvent.state;
		}
		if (!gEnv->pInput->Retriggering() || (action.activationMode & eAAM_Retriggerable))
			pListener->OnAction(actionIter->first, currentMode, inputEvent.value);
	}

#ifdef _DEBUG
	if ((action.crcKey[keyNumber] == inputCrc) && (action.key[keyNumber] != inputEvent.keyName))
	{
		GameWarning("Hash values are identical, but values are different! %s - %s", action.key[keyNumber].c_str(), inputEvent.keyName.c_str());
	}

	if ((action.crcKey[keyNumber] != inputCrc) && (action.key[keyNumber] == inputEvent.keyName))
	{
		GameWarning("Hash values are different, but values are identical! %s - %s", action.key[keyNumber].c_str(), inputEvent.keyName.c_str());
	}
#endif
}

//------------------------------------------------------------------------
void CActionMap::InputProcessed()
{
	IActionListener * pEntityListener = NULL;

	if (IEntity * pEntity = gEnv->pEntitySystem->GetEntity(m_listenerId))
		if (CGameObject * pGameObject = (CGameObject*) pEntity->GetProxy(ENTITY_PROXY_USER))
			pEntityListener = pGameObject;

	if (pEntityListener)
		pEntityListener->AfterAction();
}

//------------------------------------------------------------------------
bool CActionMap::SerializeXML(const XmlNodeRef& root, bool bLoading)
{
	if (bLoading)
	{
		// loading
		const XmlNodeRef& child = root;
		if (strcmp(child->getTag(), "actionmap") != 0)
			return false;

		int nActions = child->getChildCount();
		for (int a=0; a<nActions; ++a)
		{
			XmlNodeRef action = child->getChild(a);
			if (!strcmp(action->getTag(), "action"))
			{
				const char* actionName = 0;
				int	actionFlags = 0;
				actionName = action->getAttr("name");
				if (!strcmp(action->getAttr("onPress"), "1"))
					actionFlags |= eAAM_OnPress;
				if (!strcmp(action->getAttr("onRelease"), "1"))
					actionFlags |= eAAM_OnRelease;
				if (!strcmp(action->getAttr("onHold"), "1"))
					actionFlags |= eAAM_OnHold;
				if (!strcmp(action->getAttr("always"), "1"))
					actionFlags |= eAAM_Always;
				if (!strcmp(action->getAttr("consoleCmd"), "1"))
					actionFlags |= eAAM_ConsoleCmd;
				if (!strcmp(action->getAttr("noModifiers"), "1"))
					actionFlags |= eAAM_NoModifiers;
				if (!strcmp(action->getAttr("retriggerable"), "1"))
					actionFlags |= eAAM_Retriggerable;

				// get binds
				int nBinds = action->getChildCount();
				std::vector<const char*> keys;
				for (int b=0; b<nBinds; ++b)
				{
					XmlNodeRef bind = action->getChild(b);
		
					keys.push_back(bind->getAttr("name"));
				}
		
				if (keys.size()==1)
					((IActionMap*)this)->CreateAction(actionName, actionFlags, keys[0]);
				else if(keys.size()==2)
					this->CreateAction(actionName, actionFlags, keys[0], keys[1]);
			}
		}
	}
	else
	{
		// saving
		CCryName TheUnknownKeyName ("<unknown>");

#if 0
		// for debug reasons, we sort the ActionMap alphabetically 
		// CryName normally sorts by pointer address
		std::map<ActionId, SAction, CryNameSorter> sortedActions (m_actions.begin(), m_actions.end()); 
		std::map<ActionId, SAction, CryNameSorter>::const_iterator iter = sortedActions.begin();
		std::map<ActionId, SAction, CryNameSorter>::const_iterator iterEnd = sortedActions.end();
#else
		TActionMap::const_iterator iter = m_actions.begin();
		TActionMap::const_iterator iterEnd = m_actions.end();
#endif

		XmlNodeRef actionRoot = root->newChild("actionmap");
		actionRoot->setAttr("name", m_name);
		actionRoot->setAttr("version", m_pActionMapManager->GetVersion());
		while (iter != iterEnd)
		{
			ActionId actionId = iter->first;
			const SAction& action = iter->second;
			XmlNodeRef actionNode = actionRoot->newChild("action");
			actionNode->setAttr("name", actionId.c_str());

			if (action.activationMode & eAAM_OnPress)
				actionNode->setAttr("onPress", "1");
			if (action.activationMode & eAAM_OnRelease)
				actionNode->setAttr("onRelease", "1");
			if (action.activationMode & eAAM_OnHold)
				actionNode->setAttr("onHold", "1");
			if (action.activationMode & eAAM_Always)
				actionNode->setAttr("always", "1");
			if (action.activationMode & eAAM_ConsoleCmd)
				actionNode->setAttr("consoleCmd", "1");
			if (action.activationMode & eAAM_NoModifiers)
				actionNode->setAttr("noModifiers", "1");
			if (action.activationMode & eAAM_Retriggerable)
				actionNode->setAttr("retriggerable", "1");

			for (int i=0; i < KEYS_PER_ACTION; ++i)
			{
				const CCryName& keyName = action.key[i];
				if (keyName.empty() == false && keyName != TheUnknownKeyName)
				{
					XmlNodeRef keyChild = actionNode->newChild("key");
					keyChild->setAttr("name", keyName.c_str());
				}
			}
			++iter;
		}
	}
	return true;

}

#define USE_SORTED_ACTIONS_ITERATOR
#undef  USE_SORTED_ACTIONS_ITERATOR

//------------------------------------------------------------------------
IActionMapBindInfoIteratorPtr CActionMap::CreateBindInfoIterator()
{
	class CBindInfo : public IActionMapBindInfoIterator
	{
		typedef std::map<ActionId, SAction, CryNameSorter> TSortedActionMap;
#ifndef USE_SORTED_ACTIONS_ITERATOR
		typedef TActionMap::iterator TIterator;
#else
		typedef TSortedActionMap::iterator TIterator;
#endif

	public:
		CBindInfo(TActionMap::iterator& itBegin, TActionMap::iterator& itEnd)
		{
#ifdef USE_SORTED_ACTIONS_ITERATOR
			m_sortedActions.insert(itBegin, itEnd);
			m_cur = m_sortedActions.begin();
			m_end = m_sortedActions.end();
#else
			m_cur = itBegin;
			m_end = itEnd;
#endif

			m_nInfo.keys = new const char*[KEYS_PER_ACTION];
			m_nInfo.nKeys = KEYS_PER_ACTION;
		}

		virtual ~CBindInfo()
		{
			delete[] m_nInfo.keys;
		}

		const SActionMapBindInfo* Next()
		{
			if (m_cur == m_end)
				return 0;
			const ActionId& actionId = m_cur->first;
			SAction& action = m_cur->second;
			m_nInfo.action = actionId.c_str();
			for (int i=0; i< KEYS_PER_ACTION; ++i)
			{
				m_nInfo.keys[i] = action.key[i].c_str();
			}
			++m_cur;
			return &m_nInfo;
		}

		void AddRef() {
			++m_nRefs;
		}

		void Release() {
			if (--m_nRefs <= 0)
				delete this;
		}
		int m_nRefs;
		struct SActionMapBindInfo m_nInfo;
		TIterator m_cur;
		TIterator m_end;
#ifdef USE_SORTED_ACTIONS_ITERATOR
		 TSortedActionMap m_sortedActions;
#endif
	};
	TActionMap::iterator actionsBegin(m_actions.begin());
	TActionMap::iterator actionsEnd(m_actions.end());
	return new CBindInfo(actionsBegin, actionsEnd);
}

bool CActionMap::GetBindInfo(const ActionId& actionId, SActionMapBindInfo& outInfo, int maxKeys)
{
	// lookup action
	TActionMap::const_iterator iter = m_actions.find(actionId);
	if (iter == m_actions.end())
		return false;

	const SAction& action = iter->second;
	outInfo.action = actionId.c_str();
	outInfo.nKeys = 0;
	
	if (outInfo.keys != 0)
	{
		// max out 
		maxKeys = std::min(maxKeys, KEYS_PER_ACTION);
		for (int i = 0; i < maxKeys; ++i)
		{
			// store bind info if key not emtpy
			if (action.key[i].empty() == false)
			{
				outInfo.keys[outInfo.nKeys++] = action.key[i].c_str();	
			}
		}
	}
	return true;
}

void CActionMap::Enable(bool enable)
{
	// detect nop
	if (enable == m_enabled)
		return;

	// now things get a bit interesting, when we get disabled we "release" all our
	// active actions
	if (!enable)
	{
		TActionMap::iterator it = m_actions.begin();
		TActionMap::iterator end = m_actions.end();
		for (; it!=end; ++it)
		{
			ReleaseActionIfActiveInternal(it->first, it->second);
		}
	}

	// finally set the flag
	m_enabled = enable;
}

void CActionMap::ReleaseActionIfActive(const ActionId& actionId)
{
	TActionMap::iterator it = m_actions.find(actionId);
	if (it != m_actions.end())
	{
		ReleaseActionIfActiveInternal(actionId, it->second);
	}
}

void CActionMap::ReleaseActionIfActiveInternal(const ActionId& actionId, SAction &action)
{
	if (action.activationMode & eAAM_OnRelease && action.currentState == eIS_Pressed)
	{
		IActionListener * pEntityListener = NULL;

		if (IEntity * pEntity = gEnv->pEntitySystem->GetEntity(m_listenerId))
			if (CGameObject * pGameObject = (CGameObject*) pEntity->GetProxy(ENTITY_PROXY_USER))
				pEntityListener = pGameObject;
		IActionListener* pListener = pEntityListener;
		if (pListener)
		{
			pListener->OnAction(actionId, eAAM_OnRelease, 0.0f);
		}
	}
}

void CActionMap::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->AddContainer(m_actions);
	s->Add(m_name);
}

