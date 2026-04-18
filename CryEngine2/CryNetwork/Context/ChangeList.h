#ifndef __CHANGELIST_H__
#define __CHANGELIST_H__

#pragma once

#include "ISerialize.h"

template <class T>
class CChangeList
{
public:
	typedef std::vector< std::pair<SNetObjectID,T> > Objects;

	bool AddChange( SNetObjectID id, const T& what )
	{
		bool firstChangeInThisChangeList = false;
		size_t sizeStart = m_changeListLoc.size();
		uint16 slot = id.id;
		if (sizeStart <= slot)
			m_changeListLoc.resize(slot+1, SNetObjectID::InvalidId);
		if (m_changeListLoc[slot] == SNetObjectID::InvalidId)
		{
			m_changeListLoc[slot] = m_objects.size();
			m_objects.push_back( std::make_pair(id,what) );
			firstChangeInThisChangeList = true;
		}
		else
		{
			NET_ASSERT(m_objects.size() > m_changeListLoc[slot]);
			NET_ASSERT(m_objects[m_changeListLoc[slot]].first == id);
			m_objects[m_changeListLoc[slot]].second |= what;
		}
		return firstChangeInThisChangeList;
	}

	Objects& GetObjects() { return m_objects; }

	void Flush()
	{
		for (typename Objects::const_iterator iter = m_objects.begin(); iter != m_objects.end(); ++iter)
			if (iter->first)
				m_changeListLoc[iter->first.id] = SNetObjectID::InvalidId;
		m_objects.resize(0);
	}

	void Remove(SNetObjectID id)
	{
		if (m_changeListLoc.size() <= id.id)
			return;
		if (m_changeListLoc[id.id] == SNetObjectID::InvalidId)
			return;
		if (m_objects[m_changeListLoc[id.id]].first.salt != id.salt)
		{
			NET_ASSERT(false);
			return;
		}
		for (size_t i=m_changeListLoc[id.id]+1; i!=m_objects.size(); ++i)
			m_changeListLoc[m_objects[i].first.id]--;
		m_objects.erase( m_objects.begin() + m_changeListLoc[id.id] );
		m_changeListLoc[id.id] = SNetObjectID::InvalidId;
	}

	bool IsChanged( SNetObjectID id, const T& what )
	{
		if (m_changeListLoc.size() <= id.id)
			return false;
		if (m_changeListLoc[id.id] == SNetObjectID::InvalidId)
			return false;
		if (m_objects[m_changeListLoc[id.id]].first.salt != id.salt)
		{
			NET_ASSERT(false);
			return false;
		}
		return m_objects[m_changeListLoc[id.id]].second & what;
	}

	void GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false)
	{
		SIZER_COMPONENT_NAME(pSizer, "CChangeList");

		if (countingThis)
			pSizer->Add(*this);
		pSizer->AddContainer(m_changeListLoc);
		pSizer->AddContainer(m_objects);
	}

private:
	std::vector<uint16> m_changeListLoc;
	Objects m_objects;
};

#endif
