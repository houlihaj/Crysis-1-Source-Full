/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Action Map implementation. Maps Actions to Keys.
  
 -------------------------------------------------------------------------
  History:
  - 7:9:2004   17:47 : Created by Márcio Martins

*************************************************************************/
#ifndef __ACTIONMAP_H__
#define __ACTIONMAP_H__

#if _MSC_VER > 1000
# pragma once
#endif


#include "IActionMapManager.h"


#define KEYS_PER_ACTION (3)


struct SAction
{
	SAction()
	{
		activationMode=eAAM_OnPress;
		currentState=eIS_Unknown;
	}

	CCryName		key[KEYS_PER_ACTION];
	CCryName		defaultKey[KEYS_PER_ACTION];
	uint32			crcKey[KEYS_PER_ACTION];
	int					activationMode;
	EInputState	currentState;
};


class CActionMapManager;

class CActionMap :
	public IActionMap
{
public:
	CActionMap(CActionMapManager *pActionMapManager, const char* name);
	virtual ~CActionMap();

	// IActionMap
	virtual void GetMemoryStatistics(ICrySizer * s);
	virtual void Release() { delete this; };
	virtual void CreateAction(const char *name, int activationMode, const char*defaultKey0, const char*defaultKey1);
	virtual void BindAction(const char *name, const char *keyName, int keyNumber);
	virtual void SetActionListener(EntityId id);
	virtual void Reset();
	virtual bool SerializeXML(const XmlNodeRef& root, bool bLoading);
	virtual IActionMapBindInfoIteratorPtr CreateBindInfoIterator();
	virtual const char* GetName() { return m_name.c_str(); }
	virtual bool GetBindInfo(const ActionId& actionName, SActionMapBindInfo& outInfo, int maxKeys);
	virtual void Enable(bool enable);
	virtual bool Enabled() { return m_enabled; };
	// ~IActionMap

	void ProcessInput(const ActionId& actionId, const SInputEvent &inputEvent, uint32 keyNumber, uint32 inputCrc);
	void InputProcessed();
	void ReleaseActionIfActive(const ActionId& actionId);

private:
	void AddBind(uint32 crc, const ActionId& actionId, uint32 keyNumber);
	void RemoveBind(uint32 crc, const ActionId& actionId, uint32 keyNumber);
	void ReleaseActionIfActiveInternal(const ActionId& actionId, SAction &action);

	typedef std::map<ActionId, SAction> TActionMap;

	bool								m_enabled;
	CActionMapManager*	m_pActionMapManager;
	TActionMap					m_actions;
	EntityId						m_listenerId;
	string							m_name;
};


#endif //__ACTIONMAP_H__