#include "StdAfx.h"
#include "SequenceTrack.h"

//////////////////////////////////////////////////////////////////////////
void CSequenceTrack::SerializeKey(ISequenceKey &key,XmlNodeRef &keyNode,bool bLoading)
{
	if (bLoading)
	{
		const char *szSelection;
		szSelection = keyNode->getAttr("node");
		strncpy(key.szSelection,szSelection,sizeof(key.szSelection));
		key.szSelection[sizeof(key.szSelection)-1] = 0;

		if (!keyNode->getAttr("overridetimes",key.bOverrideTimes))
			key.bOverrideTimes = false;

		if (key.bOverrideTimes == true)
		{
			if (!keyNode->getAttr("starttime",key.fStartTime))
				key.fStartTime = 0.0f;
			if (!keyNode->getAttr("endtime",key.fEndTime))
				key.fEndTime  = 0.0f;
		}
		else
		{
			key.fStartTime = 0.0f;
			key.fEndTime = 0.0f;
		}
	}
	else
	{
		keyNode->setAttr("node",key.szSelection);

		if (key.bOverrideTimes == true)
		{
			keyNode->setAttr("overridetimes",key.bOverrideTimes);
			keyNode->setAttr("starttime",key.fStartTime);
			keyNode->setAttr("endtime",key.fEndTime);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSequenceTrack::GetKeyInfo(int key,const char* &description,float &duration)
{
	assert(key >= 0 && key < (int)m_keys.size());
	CheckValid();
	description = 0;
	duration = m_keys[key].fDuration;
	if (strlen(m_keys[key].szSelection) > 0)
		description = m_keys[key].szSelection;
}