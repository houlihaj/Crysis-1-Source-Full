/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Action manager and dispatcher.
  
 -------------------------------------------------------------------------
  History:
  - 7:9:2004   17:36 : Created by Márcio Martins

*************************************************************************/
#ifndef __ACTIONMAPMANAGER_H__
#define __ACTIONMAPMANAGER_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IActionMapManager.h"
#include "IInput.h"
class Crc32Gen;

typedef std::map<string, class CActionMap *>		TActionMapMap;
typedef std::map<string, class CActionFilter *>	TActionFilterMap;

class CActionMapManager :
	public IActionMapManager,
	public IInputEventListener
{
public:
	CActionMapManager(IInput *pInput);

	void Release() { delete this; };

	// IInputEventListener
	virtual bool OnInputEvent(const SInputEvent &event);
	// ~IInputEventListener

	// IActionMapManager
	virtual void Update();
	virtual void Reset();
	virtual void Clear();

	virtual void LoadFromXML(const XmlNodeRef& node, bool bCheckVersion=false);
	virtual void SaveToXML(const XmlNodeRef& node);

	virtual IActionMap *CreateActionMap(const char *name);
	virtual IActionMap *GetActionMap(const char *name);
	virtual IActionFilter *CreateActionFilter(const char *name, EActionFilterType type=eAFT_ActionFail);
	virtual IActionFilter *GetActionFilter(const char *name);
	virtual IActionMapIteratorPtr CreateActionMapIterator();
	virtual IActionFilterIteratorPtr CreateActionFilterIterator();

	virtual void Enable(bool enable);
	virtual void EnableActionMap(const char *name, bool enable);
	virtual void EnableFilter(const char *name, bool enable);
	virtual bool IsFilterEnabled(const char *name);
	// ~IActionMapManager

	int GetVersion() const { return  m_version; }
	void SetVersion(int version) { m_version = version; }

	bool ActionFiltered(const ActionId& action);

	void RemoveActionMap(CActionMap *pActionMap);
	void RemoveActionFilter(CActionFilter *pActionFilter);

	void ReleaseActionIfActive(const ActionId& actionId);

	const Crc32Gen&	GetCrc32Gen() const;

	void AddBind(uint32 crc, const ActionId& actionId, uint32 keyNumber, CActionMap* pActionMap);
	bool RemoveBind(uint32 crc, const ActionId& actionId, uint32 keyNumber, CActionMap* pActionMap);

	void GetMemoryStatistics(ICrySizer * s);

protected:
	virtual ~CActionMapManager();

private:
	struct SBindData
	{
		ActionId		actionId;
		uint32			keyNumber;
		CActionMap*	pActionMap;

		SBindData(ActionId id, uint32 k, CActionMap* pAM)
		{
			actionId = id;
			keyNumber = k;
			pActionMap = pAM;
		}
	};
	typedef std::multimap<uint32, SBindData> TInputCRCToBind;


	void DispatchEvent(const SInputEvent &inputEvent);

	IInput*						m_pInput;
	TActionMapMap			m_actionMaps;
	TActionFilterMap	m_actionFilters;
	TInputCRCToBind		m_inputCRCToBind;
	int               m_version;
	bool							m_enabled;
};


#endif //__ACTIONMAPMANAGER_H__