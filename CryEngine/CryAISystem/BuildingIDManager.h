#ifndef BUILDINGIDMANAGER_H
#define BUILDINGIDMANAGER_H

#if _MSC_VER > 1000
#pragma once
#endif

#include <set>
#include <vector>

class CBuildingIDManager
{
public:
  CBuildingIDManager();
  ~CBuildingIDManager();
  int GetId();
  void FreeId(int nID);
  void FreeAll();

private:
  void MakeSomeIDsIfNecessary();
  std::set<int> m_allocatedIDs;
  std::vector<int> m_availableIDs;
};

#endif