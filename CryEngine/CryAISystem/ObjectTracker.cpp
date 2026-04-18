#include "StdAfx.h"
#include "ObjectTracker.h"
#include "ISerialize.h"
#include "AILog.h"

const CObjectTracker::TID CObjectTracker::s_invalidID = 0;
const char* const CObjectTracker::s_unsetClassName = "UnsetClassName";
const char* const CObjectTracker::s_uncheckedClassName = "UncheckedClassName";

//====================================================================
// RetrieveObject
// could speed this up by just using ID as an index... but keep it
// general for now!
//====================================================================
void* CObjectTracker::RetrieveObjectFromID(TID ID, const char* objectType, bool takingOwnership, bool& found, bool touch)
{
	TRecords::iterator it = std::find(m_records.begin(), m_records.end(), ID);
	if (it == m_records.end())
	{
		AIWarning("CObjectTracker::RetrieveObjectFromID Trying to retrieve object ID %d of type %s from object tracker but it's not registered",
			ID, objectType);
		found = false;
		return 0;
	}
	else
	{
		AIAssert(ID == it->m_ID);
		if (takingOwnership)
		{
			if (!it->m_waitingToBeOwned)
				AIWarning("CObjectTracker::RetrieveObjectFromID object %p already owned", it->m_pObject);
			it->m_waitingToBeOwned = false;
		}
		if (touch && !it->m_touched)
			it->m_touched = true;
		found = true;
		return it->m_pObject;
	}
}

//====================================================================
// RetrieveIDFromObject
//====================================================================
CObjectTracker::TID CObjectTracker::RetrieveIDFromObject(void* pObject, const char* objectType)
{
	TRecords::iterator it = std::find(m_records.begin(), m_records.end(), pObject);
	if (it == m_records.end())
	{
		/* debug, slower and risky (pAIObject could be invalid)
		CAIObject* pAIObject = strcmp(objectType,"CAIObject") ? NULL :  (CAIObject*)pObject;
		AIWarning("CObjectTracker::RetrieveIDFromObject Trying to retrieve object %p (%s) of type %s but it's not there",
		pObject, (pAIObject ? pAIObject->GetName() : "<not AI object>"),objectType);
		*/
		AIWarning("CObjectTracker::RetrieveIDFromObject Trying to retrieve object %p of type %s but it's not there",
		pObject, objectType);
		return s_invalidID;
	}
	else
	{
		AIAssert(pObject == it->m_pObject);
		return it->m_ID;
	}
}

//====================================================================
// Serialize
//====================================================================
void CObjectTracker::Serialize(TSerialize ser)
{
	ser.BeginGroup("ObjectTracker");

	if (ser.IsReading())
	{
		Clear();
		unsigned numRecords;
		ser.Value("numRecords", numRecords);
		m_records.resize(numRecords);
		for (unsigned iRecord = 0 ; iRecord < numRecords ; ++iRecord)
		{
			ser.BeginGroup("obj");
			CRecord& record = m_records[iRecord];
			ser.Value("ID", record.m_ID);
			ser.Value("externalID", record.m_externalID);
			ser.Value("type", record.m_objectType);
			ser.Value("needToCreate", record.m_needToCreate);
			ser.EndGroup();
		}
	}
	else
	{
		unsigned numRecords = m_records.size();
		ser.Value("numRecords", numRecords);
		for (unsigned iRecord = 0 ; iRecord < numRecords ; ++iRecord)
		{
			ser.BeginGroup("obj");
			CRecord& record = m_records[iRecord];
			ser.Value("ID", record.m_ID);
			ser.Value("externalID", record.m_externalID);
			ser.Value("type", record.m_objectType);
			ser.Value("needToCreate", record.m_needToCreate);
			ser.EndGroup();
		}
	}
	ser.EndGroup();
}

//====================================================================
// CreateObjects
//====================================================================
void CObjectTracker::CreateObjects( const CObjectCreator & objectCreator )
{
	unsigned numRecords = m_records.size();
	for (unsigned iRecord = 0 ; iRecord < numRecords ; ++iRecord)
	{
		CRecord& record = m_records[iRecord];
		if (record.m_needToCreate)
		{
			if (record.m_pObject)
				AIWarning("object %p already created!", record.m_pObject);
			else
			{
				record.m_waitingToBeOwned = true;
				record.m_pObject = objectCreator.CreateObject(record.m_ID, record.m_externalID, record.m_objectType.c_str());
			}
			record.m_needToCreate = false;
			AIAssert(record.m_pObject);
		}
	}
}

//====================================================================
// ValidateOwnership
//====================================================================
bool CObjectTracker::ValidateOwnership() const
{
	unsigned numWaiting = 0;
	unsigned numRecords = m_records.size();
	for (unsigned iRecord = 0 ; iRecord < numRecords ; ++iRecord)
	{
		const CRecord& record = m_records[iRecord];
		if (record.m_waitingToBeOwned && record.m_pObject)
		{
			AIWarning("Object %p ID = %d externalID = %d type = %s still waiting to be owned", 
				record.m_pObject, record.m_ID, record.m_externalID, record.m_objectType.c_str());
			++numWaiting;
		}
	}
	if (numWaiting)
		AIWarning("%d objects waiting to be owned", numWaiting);
	return numWaiting == 0;
}

//====================================================================
// Clear
//====================================================================
void CObjectTracker::Clear()
{
	m_records.clear();
}