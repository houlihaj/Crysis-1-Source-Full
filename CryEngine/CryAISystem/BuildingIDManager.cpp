#include "StdAfx.h"
#include "BuildingIDManager.h"
#include "AILog.h"

//===================================================================
// CBuildingIDManager
//===================================================================
CBuildingIDManager::CBuildingIDManager()
{
}

//===================================================================
// ~CBuildingIDManager
//===================================================================
CBuildingIDManager::~CBuildingIDManager()
{
}

//===================================================================
// MakeSomeIDsIfNecessary
//===================================================================
void CBuildingIDManager::MakeSomeIDsIfNecessary()
{
  const int numToMake = 64;
  if (m_availableIDs.empty())
  {
    int minFreeID = m_allocatedIDs.size();
    for (int i = numToMake ; i-- != 0 ; )
      m_availableIDs.push_back(minFreeID + i);
  }
}

//===================================================================
// GetId
//===================================================================
int CBuildingIDManager::GetId()
{
  MakeSomeIDsIfNecessary();

  int ID = m_availableIDs.back();
  m_availableIDs.pop_back();

  AIAssert(m_allocatedIDs.find(ID) == m_allocatedIDs.end());

  m_allocatedIDs.insert(ID);

  return ID;
}



//===================================================================
// FreeId
//===================================================================
void CBuildingIDManager::FreeId(int nID)
{
  if (nID < 0)
    return;
  std::set<int>::iterator it = m_allocatedIDs.find(nID);
  if (it == m_allocatedIDs.end())
  {
    // not an assert because we're not in control of the input
    AIWarning("CBuildingIDManager::FreeId ID %d not found", nID);
  }
  else
  {
    m_allocatedIDs.erase(it);
    m_availableIDs.push_back(nID);
  }
}

//===================================================================
// FreeAll
//===================================================================
void CBuildingIDManager::FreeAll(void)
{
  m_allocatedIDs.clear();
  m_availableIDs.clear();
}
