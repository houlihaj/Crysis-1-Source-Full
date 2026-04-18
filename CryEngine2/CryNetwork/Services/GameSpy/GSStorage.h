#ifndef __GSSTORAGE_H__
#define __GSSTORAGE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "GameSpy/sake/sake.h"

struct IStatsReader;
struct IStatsWriter;
struct IStatsDeleter;

class CGSStorage : public CMultiThreadRefCount
{
private:
  struct SRequestBase;
  struct SCreateRecord;
  struct SUpdateRecord;
  struct SReadRecordsBase;
  struct SReadRecords;
  struct SDeleteRecord;
  struct SSearchForRecords;
	struct SPreorderFlag
	{
		int		id;
		bool	preorder;
	};
public:
  CGSStorage();
  ~CGSStorage();
  void  SetUser(int profile, const char* ticket);
  
  void  TestGetData();
  void  TestSetData();

  void  ReadRecords(IStatsReader* rdr);
  void  WriteRecords(IStatsWriter* wrtr);
	void  DeleteRecords(IStatsDeleter* dltr);
	void  SearchRecords(IStatsReader* rdr, int ownerid);
	bool  CheckPreorderCache(int id, bool& preorder);
	void  CachePreorderFlag(int id, bool preorder);

  void GetMemoryStatistics(ICrySizer * pSizer);

	bool IsDead()const{return false;}

private:
  void  Think();
  static void TimerCallback( NetTimerId id, void * pUser, CTimeValue tm );
	
	void	AddRequest(SRequestBase* request);
	void	GC_Report(SRequestBase* request, bool ok);
	
  SAKE        m_sake;
  CTimeValue  m_lastThinkTime;
  NetTimerId  m_updateTimer;
	std::vector<SRequestBase*> m_requests;
	std::vector<SRequestBase*> m_queue;

	std::vector<int>		m_preopdercache;
	std::vector<int>    m_notpreordercache;
	CTimeValue					m_lastCacheResetTime;
};

#endif //__GSSTORAGE_H__