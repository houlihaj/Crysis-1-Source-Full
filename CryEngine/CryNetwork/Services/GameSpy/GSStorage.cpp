#include "StdAfx.h"
#include "Network.h"
#include "GSStorage.h"
#include "GameSpy.h"
#include "INetworkService.h"

#include "GameSpy/common/gsCore.h"


const float UPDATE_INTERVAL = 0.05f;
const int		MAX_SIMULTANEOUS_REQUESTS = 10;

class StringCache
{
public:
  char* CacheString(const string& str)
  {
    std::set<string>::iterator it = m_cache.find(str);
    if(it == m_cache.end())
    {
      it = m_cache.insert(str).first;
    }
    return (char*)(*it).c_str();
  }
  void GetMemoryStatistics(ICrySizer * pSizer)
  {
    pSizer->AddContainer(m_cache);
  }
private:
  std::set<string> m_cache;
};

struct SRecord
{
  void AddField(char* name, char* val)
  {
    SAKEField f;
    f.mName = name;
    f.mType = SAKEFieldType_ASCII_STRING;
    f.mValue.mAsciiString = val;
    m_fields.push_back(f);
  }

  void AddField(char* name, int val)
  {
    SAKEField f;
    f.mName = name;
    f.mType = SAKEFieldType_INT;
    f.mValue.mInt = val;
    m_fields.push_back(f);    
  }
  void AddField(char* name, float val)
  {
    SAKEField f;
    f.mName = name;
    f.mType = SAKEFieldType_FLOAT;
    f.mValue.mFloat = val;
    m_fields.push_back(f);
  }
  std::vector<SAKEField>  m_fields;
};

enum ERequestType
{
  eRT_get,
  eRT_update,
  eRT_create,
  eRT_delete,
  eRT_search
};

struct CGSStorage::SRequestBase
{
  SRequestBase(CGSStorage* st, ERequestType t):
  m_storage(st),
  m_type(t)
  {}
  virtual ~SRequestBase()
	{}
  static void SAKERequestCallback(SAKE sake, SAKERequest request, SAKERequestResult result, void *inputData, void* outputData, void* userData)
  {
    SRequestBase* pObj = (SRequestBase*)userData;
    NET_ASSERT(pObj->m_storage->m_sake == sake);
    NET_ASSERT(pObj->m_request == request);

    pObj->ProcessResult(result,outputData);
  }
  virtual bool Execute()=0;
  virtual void ProcessResult(SAKERequestResult result, void* output)=0;
  virtual void GetMemoryStatistics(ICrySizer * pSizer) = 0;
	virtual void Report(bool ok) = 0;

	void DoReport(bool ok)
	{
		TO_GAME(&CGSStorage::GC_Report,m_storage,this,ok);
	}

  CGSStorage  *m_storage;
  ERequestType m_type;
  SAKERequest  m_request;
};

struct CGSStorage::SCreateRecord : public CGSStorage::SRequestBase
{
  SCreateRecord(CGSStorage* st):SRequestBase(st,eRT_create)
  {}

  bool Execute()
  {
    m_data.mFields = &m_record.m_fields[0];
    m_data.mNumFields = m_record.m_fields.size();
    m_data.mTableId = (char*)m_table.c_str();
    m_request = sakeCreateRecord(m_storage->m_sake, &m_data, &CGSStorage::SRequestBase::SAKERequestCallback, this);
		return m_request != NULL;
	}

  void ProcessResult(SAKERequestResult result, void *output)
  {
    SAKECreateRecordOutput *pOut = (SAKECreateRecordOutput*)output;
    if(result == SAKERequestResult_SUCCESS)
    {
      m_recordId = pOut->mRecordId;
      DoReport(true);
    }
    else
    {
      DoReport(false);
    }
  }

  void Report(bool ok)
  {
    if(m_callback)
      m_callback->OnResult(m_idx,m_recordId,ok);
  }

  virtual void GetMemoryStatistics(ICrySizer * pSizer)
  {
    pSizer->Add(*this);
    pSizer->Add(m_table);
    pSizer->AddContainer(m_record.m_fields);
    m_cache.GetMemoryStatistics(pSizer);
  }
  SAKECreateRecordInput m_data;

  //inputs
  SRecord               m_record;
  string                m_table;
  StringCache           m_cache;
  //outputs
  int                   m_recordId;
  IStatsWriter*         m_callback;
  int                   m_idx;
};

struct CGSStorage::SUpdateRecord : public CGSStorage::SRequestBase
{
  SUpdateRecord(CGSStorage* st):SRequestBase(st,eRT_update)
  {}

  bool Execute()
  {
    m_data.mFields = &m_record.m_fields[0];
    m_data.mNumFields = m_record.m_fields.size();
    m_data.mTableId = (char*)m_table.c_str();
    m_data.mRecordId = m_recordId;
    m_request = sakeUpdateRecord(m_storage->m_sake, &m_data, &CGSStorage::SRequestBase::SAKERequestCallback, this);
		return m_request != NULL;
	}

  void ProcessResult(SAKERequestResult result, void *output)
  {
      Report(result == SAKERequestResult_SUCCESS);
  }

  void Report(bool ok)
  {
    if(m_callback)
      m_callback->OnResult(m_idx,-1,ok);
  }

  virtual void GetMemoryStatistics(ICrySizer * pSizer)
  {
    pSizer->Add(*this);
    pSizer->Add(m_table);
    pSizer->AddContainer(m_record.m_fields);
    m_cache.GetMemoryStatistics(pSizer);
  }

  SAKEUpdateRecordInput m_data;

  //inputs
  SRecord               m_record;
  string                m_table;
  int                   m_recordId;
  StringCache           m_cache;
  //outputs
  IStatsWriter*         m_callback;
  int                   m_idx;
};

struct CGSStorage::SReadRecords : public CGSStorage::SRequestBase
{
  SReadRecords(CGSStorage* st):SRequestBase(st,eRT_get)
  {}

  void AddField(const char* field)
  {
    m_fieldnames.push_back(m_cache.CacheString(field));
  }

  bool Execute()
  {
    m_data.mNumFields = m_fieldnames.size();
    m_data.mFieldNames = &m_fieldnames[0];
    m_data.mTableId = (char*)m_table.c_str();
    m_request = sakeGetMyRecords(m_storage->m_sake, &m_data, &CGSStorage::SRequestBase::SAKERequestCallback, this);
		return m_request != NULL;
  }

  void ProcessResult(SAKERequestResult result, void *output)
  {
    SAKEGetMyRecordsOutput *pOut = (SAKEGetMyRecordsOutput*)output;
    if(result != SAKERequestResult_SUCCESS)
    {
      DoReport(false);
      return;
    }
    m_records.resize(pOut->mNumRecords);
    for(int i=0;i<pOut->mNumRecords;++i)
    {
      SAKEField* pFields = pOut->mRecords[i];
      m_records[i].m_fields.resize(m_fieldnames.size());
      for(int j=0;j<m_fieldnames.size();++j)
      {
        SAKEField f;
        f.mName = m_cache.CacheString(pFields[j].mName);
        f.mType = pFields[j].mType;
        NET_ASSERT(f.mType != SAKEFieldType_UNICODE_STRING && f.mType != SAKEFieldType_BINARY_DATA);
        if(f.mType == SAKEFieldType_ASCII_STRING)
          f.mValue.mAsciiString = m_cache.CacheString(pFields[j].mValue.mAsciiString);
        else
          f.mValue = pFields[j].mValue;
        m_records[i].m_fields[j] = f;
      }
    }
//    TO_GAME(SReadRecords::Report,this);
    DoReport(true);
  }

  void Report(bool ok)
  {
		if(ok)
		{
			for(int i=0;i<m_records.size();++i)
			{
				m_callback->NextRecord(m_records[i].m_fields[0].mValue.mInt);
				for(int j=1;j<m_fieldnames.size();++j)
				{
					SAKEField &f = m_records[i].m_fields[j];
					NET_ASSERT(!strcmp(f.mName,m_fieldnames[j]));
					switch(f.mType)
					{
					case SAKEFieldType_INT:
						m_callback->OnValue(j-1,f.mValue.mInt);
						break;
					case SAKEFieldType_BOOLEAN:
						m_callback->OnValue(j-1,f.mValue.mBoolean?1:0);
						break;
					case SAKEFieldType_FLOAT:
						m_callback->OnValue(j-1,f.mValue.mFloat);
						break;
					case SAKEFieldType_ASCII_STRING:
						m_callback->OnValue(j-1,f.mValue.mAsciiString);
						break;
					}
				}
			}
			m_callback->End(true);
		}
		else
		{
			m_callback->End(false);
		}
  }

  virtual void GetMemoryStatistics(ICrySizer * pSizer)
  {
    pSizer->Add(*this);
    pSizer->Add(m_table);
    pSizer->AddContainer(m_records);
    pSizer->AddContainer(m_fieldnames);
    m_cache.GetMemoryStatistics(pSizer);
    for(int i=0;i<m_records.size();++i)
    {
      pSizer->AddContainer(m_records[i].m_fields);
    }
  }

  SAKEGetMyRecordsInput     m_data;
  //inputs
  std::vector<char*>        m_fieldnames;
  string                    m_table;
  //outputs
  std::vector<SRecord>      m_records;
  StringCache               m_cache;
  //callback
  IStatsReader*             m_callback;
};

struct CGSStorage::SDeleteRecord : public CGSStorage::SRequestBase
{
  SDeleteRecord(CGSStorage* st):SRequestBase(st,eRT_delete)
  {}

  bool Execute()
  {
    m_data.mRecordId = m_record;
    m_data.mTableId = (char*)m_table.c_str();
    m_request = sakeDeleteRecord(m_storage->m_sake, &m_data, &CGSStorage::SRequestBase::SAKERequestCallback, this);
		return m_request != NULL;
  }

  void ProcessResult(SAKERequestResult result, void *output)
  {
    DoReport(result == SAKERequestResult_SUCCESS);
  }

	void Report(bool ok)
	{
		m_callback->OnResult(m_idx, m_record, true);
	}

  virtual void GetMemoryStatistics(ICrySizer * pSizer)
  {
    pSizer->Add(*this);
    pSizer->Add(m_table);
  }

  SAKEDeleteRecordInput     m_data;
  string                    m_table;
  int                       m_record;
	int												m_idx;

	IStatsDeleter*            m_callback;
};

struct CGSStorage::SSearchForRecords : public CGSStorage::SRequestBase
{
  SSearchForRecords(CGSStorage* st):SRequestBase(st,eRT_search)
  {}

  void AddField(const char* field)
  {
    m_fieldnames.push_back(m_cache.CacheString(field));
  }

  bool Execute()
  {
    m_data.mNumFields = m_fieldnames.size();
    m_data.mFieldNames = &m_fieldnames[0];
    m_data.mTableId = (char*)m_table.c_str();
    m_data.mFilter = (char*)m_filter.c_str();
    m_data.mSort = (char*)m_sort.c_str();
		m_data.mMaxRecords = 50;
		m_data.mOffset = 0;
		m_data.mTargetRecordFilter="";
		m_data.mSurroundingRecordsCount = 0;
		m_data.mOwnerIds = 0;
		m_data.mNumOwnerIds = 0;
		m_data.mCacheFlag = gsi_true;

    m_request = sakeSearchForRecords(m_storage->m_sake, &m_data, &CGSStorage::SRequestBase::SAKERequestCallback, this);
		return m_request != NULL;
  }

  void ProcessResult(SAKERequestResult result, void *output)
  {
    SAKESearchForRecordsOutput *pOut = (SAKESearchForRecordsOutput*)output;
    if(result != SAKERequestResult_SUCCESS)
    {
			DoReport(false);
      return;
    }
    m_records.resize(pOut->mNumRecords);
    for(int i=0;i<pOut->mNumRecords;++i)
    {
      SAKEField* pFields = pOut->mRecords[i];
      m_records[i].m_fields.resize(m_fieldnames.size());
      for(int j=0;j<m_fieldnames.size();++j)
      {
        SAKEField f;
        f.mName = m_cache.CacheString(pFields[j].mName);
        f.mType = pFields[j].mType;
        NET_ASSERT(f.mType != SAKEFieldType_UNICODE_STRING && f.mType != SAKEFieldType_BINARY_DATA);
        if(f.mType == SAKEFieldType_ASCII_STRING)
          f.mValue.mAsciiString = m_cache.CacheString(pFields[j].mValue.mAsciiString);
        else
          f.mValue = pFields[j].mValue;
        m_records[i].m_fields[j] = f;
      }
    }
		DoReport(true);
  }

	void Report(bool ok)
	{
		if(ok)
		{
			for(int i=0;i<m_records.size();++i)
			{
				m_callback->NextRecord(m_records[i].m_fields[0].mValue.mInt);
				for(int j=1;j<m_fieldnames.size();++j)
				{
					SAKEField &f = m_records[i].m_fields[j];
					NET_ASSERT(!strcmp(f.mName,m_fieldnames[j]));
					switch(f.mType)
					{
					case SAKEFieldType_INT:
						m_callback->OnValue(j-1,f.mValue.mInt);
						break;
					case SAKEFieldType_BOOLEAN:
						m_callback->OnValue(j-1,f.mValue.mBoolean?1:0);
						break;
					case SAKEFieldType_FLOAT:
						m_callback->OnValue(j-1,f.mValue.mFloat);
						break;
					case SAKEFieldType_ASCII_STRING:
						m_callback->OnValue(j-1,f.mValue.mAsciiString);
						break;
					}
				}
			}
			m_callback->End(true);
		}
		else
		{
			m_callback->End(false);
		}
	}


  virtual void GetMemoryStatistics(ICrySizer * pSizer)
  {
    pSizer->Add(*this);
    pSizer->AddContainer(m_records);
    pSizer->AddContainer(m_fieldnames);
    pSizer->Add(m_table);
    pSizer->Add(m_filter);
    pSizer->Add(m_sort);
    m_cache.GetMemoryStatistics(pSizer);
    for(int i=0;i<m_records.size();++i)
    {
      pSizer->AddContainer(m_records[i].m_fields);
    }
  }

  SAKESearchForRecordsInput m_data;
  string                    m_table;
  string                    m_filter;
  string                    m_sort;
  std::vector<char*>        m_fieldnames;
  int                       m_maxrecords;

  std::vector<SRecord>      m_records;
  StringCache               m_cache;

	IStatsReader*							m_callback;
};

 
CGSStorage::CGSStorage()
{
  gsCoreInitialize();
  {
    SCOPED_GLOBAL_LOCK;
    m_updateTimer = TIMER.AddTimer( g_time + UPDATE_INTERVAL, TimerCallback, this);
  }
  sakeStartup(&m_sake);
  gsi_char  secret_key[9];
  secret_key[0] = 'z';
  secret_key[1] = 'K';
  secret_key[2] = 'b';
  secret_key[3] = 'Z';
  secret_key[4] = 'i';
  secret_key[5] = 'M';
  secret_key[6] = '\0';
  sakeSetGame(m_sake,GAMESPY_GAME_NAME,CDKEY_PRODUCT_ID,secret_key);
	m_lastCacheResetTime = g_time;
}

CGSStorage::~CGSStorage()
{
  if(m_updateTimer)
  {
    SCOPED_GLOBAL_LOCK;
    TIMER.CancelTimer(m_updateTimer);
  }

  sakeShutdown(m_sake);
  gsCoreShutdown();

  for(int i=0;i<m_requests.size();++i)
	{
		m_requests[i]->Report(false);
    delete m_requests[i];
	}

	for(int i=0;i<m_queue.size();++i)
	{
		m_queue[i]->Report(false);
		delete m_queue[i];
	}
}

void  CGSStorage::SetUser(int profile, const char* ticket)
{
  NET_ASSERT(strlen(ticket) == 24);

  sakeSetProfile(m_sake, profile, ticket);
}

void  CGSStorage::TestSetData()
{
  SCreateRecord *mr = new SCreateRecord(this);
  mr->m_table = "info";
	AddRequest(mr);
}

void  CGSStorage::TestGetData()
{
  SReadRecords *mr = new SReadRecords(this);
  mr->AddField("name");
  mr->AddField("age");
  mr->AddField("recordid");
  mr->AddField("ownerid");
  mr->m_table = "info";
  AddRequest(mr);
}

void  CGSStorage::Think()
{
  int ms = (g_time - m_lastThinkTime).GetMilliSecondsAsInt64();
  gsCoreThink(ms);

	if(m_requests.size() < MAX_SIMULTANEOUS_REQUESTS)
	{
		int to_add = min(MAX_SIMULTANEOUS_REQUESTS-m_requests.size(),m_queue.size());
		for(int i=0;i<to_add;++i)
		{
			m_requests.push_back(m_queue[i]);
			NetLog("Executed request 0x%x", m_queue[i]);
			if(!m_queue[i]->Execute())
			{
				TO_GAME(&CGSStorage::GC_Report,this,m_queue[i],false);
			}
		}
		if(to_add)
		{
			m_queue.erase(m_queue.begin(),m_queue.begin()+to_add);
		}
	}

  m_lastThinkTime = g_time;
  m_updateTimer = TIMER.AddTimer( g_time + UPDATE_INTERVAL, TimerCallback, this );
}

void CGSStorage::TimerCallback( NetTimerId id, void * pUser, CTimeValue tm )
{
	CGSStorage *pStorage = (CGSStorage*)pUser;
	if(id != pStorage->m_updateTimer)
		return;
	SCOPED_GLOBAL_LOCK;
  pStorage->Think();
}

void  CGSStorage::ReadRecords(IStatsReader* rdr)
{
  SReadRecords *mr = new SReadRecords(this);
  mr->m_table = rdr->GetTableName();
  mr->AddField("recordid");
  for(int i=0;i<rdr->GetFieldsNum();++i)
  {
    mr->AddField(rdr->GetFieldName(i));
  }
  mr->m_callback =  rdr;
  AddRequest(mr);
}

void  CGSStorage::WriteRecords(IStatsWriter* wrtr)
{
  for(int i=0;i<wrtr->GetRecordsNum();++i)
  {
    int id = wrtr->NextRecord();
    if(id == -1)
    {
      SCreateRecord *cr = new SCreateRecord(this);
      cr->m_table = wrtr->GetTableName();
      for(int j=0;j<wrtr->GetFieldsNum();++j)
      {
        float fv;
        int   iv;
        const char* sv;
        if(wrtr->GetValue(j,fv))
        {
          cr->m_record.AddField(cr->m_cache.CacheString(wrtr->GetFieldName(j)),fv);
        }
        else if(wrtr->GetValue(j, iv))
        {
          cr->m_record.AddField(cr->m_cache.CacheString(wrtr->GetFieldName(j)),iv);
        }
        else if(wrtr->GetValue(j, sv))
        {
          cr->m_record.AddField(cr->m_cache.CacheString(wrtr->GetFieldName(j)),cr->m_cache.CacheString(sv));
        }
        else
        {
          NET_ASSERT(false);
        }
      }
      cr->m_idx = i;
      cr->m_callback = wrtr;
      AddRequest(cr);
    }
    else
    {
      SUpdateRecord *ur = new SUpdateRecord(this);
      ur->m_table = wrtr->GetTableName();
      for(int j=0;j<wrtr->GetFieldsNum();++j)
      {
        float fv;
        int   iv;
        const char* sv;
        if(wrtr->GetValue(j,fv))
        {
          ur->m_record.AddField(ur->m_cache.CacheString(wrtr->GetFieldName(j)),fv);
        }
        else if(wrtr->GetValue(j, iv))
        {
          ur->m_record.AddField(ur->m_cache.CacheString(wrtr->GetFieldName(j)),iv);
        }
        else if(wrtr->GetValue(j, sv))
        {
          ur->m_record.AddField(ur->m_cache.CacheString(wrtr->GetFieldName(j)),ur->m_cache.CacheString(sv));
        }
        else
        {
          NET_ASSERT(false);
        }
      }
      ur->m_recordId = id;
      ur->m_idx = i;
      ur->m_callback = wrtr;
      AddRequest(ur);
    }
  }
}

void CGSStorage::DeleteRecords(IStatsDeleter* dltr)
{
	for(int i=0;i<dltr->GetRecordsNum();++i)
	{
		int id = dltr->NextRecord();
		SDeleteRecord *dr = new SDeleteRecord(this);
		dr->m_record = id;
		dr->m_idx = i;
		dr->m_table = dltr->GetTableName();
		dr->m_callback = dltr;			
		AddRequest(dr);
	}
}

void  CGSStorage::SearchRecords(IStatsReader* rdr, int ownerid)
{
	SSearchForRecords *sr = new SSearchForRecords(this);
	sr->m_filter.Format("ownerid = %d",ownerid);
	sr->m_sort = "ownerid";
	sr->m_table = rdr->GetTableName();
	sr->AddField("recordid");
	for(int i=0;i<rdr->GetFieldsNum();++i)
	{
		sr->AddField(rdr->GetFieldName(i));
	}
	sr->m_callback =  rdr;
	AddRequest(sr);
}

void CGSStorage::AddRequest(SRequestBase* request)
{
	if(m_requests.size()<MAX_SIMULTANEOUS_REQUESTS)
	{
		m_requests.push_back(request);
		CryLog("Executed request 0x%x",request);
		if(!request->Execute())
		{
			TO_GAME(&CGSStorage::GC_Report,this,request,false);
		}

	}
	else
		m_queue.push_back(request);
}

bool  CGSStorage::CheckPreorderCache(int id, bool& preorder)
{
	if( (g_time-m_lastCacheResetTime).GetSeconds() > 3600*6 )//every 6 hours
	{
		m_lastCacheResetTime = g_time;
		m_preopdercache.resize(0);
		m_notpreordercache.resize(0);
		return false;
	}

	if(std::binary_search(m_preopdercache.begin(),m_preopdercache.end(),id))
	{
		preorder = true;
		return true;
	}

	if(std::binary_search(m_notpreordercache.begin(),m_notpreordercache.end(),id))
	{
		preorder = false;
		return true;
	}
	return false;
}

void  CGSStorage::CachePreorderFlag(int id, bool preorder)
{
	if(preorder)
	{
		stl::binary_insert_unique(m_preopdercache,id);
	}
	else
	{
		stl::binary_insert_unique(m_notpreordercache,id);
	}
}

void CGSStorage::GC_Report(SRequestBase* request, bool ok)
{
	request->Report(ok);
	CryLog("Deleted request 0x%x",request);
	stl::find_and_erase(m_requests,request);
	delete request;
}

void CGSStorage::GetMemoryStatistics(ICrySizer * pSizer)
{
  SIZER_COMPONENT_NAME(pSizer, "GSStorage");
  pSizer->Add(*this);
  pSizer->AddContainer(m_requests);

  for(int i=0;i<m_requests.size();++i)
  {
    m_requests[i]->GetMemoryStatistics(pSizer);
  }
	for(int i=0;i<m_requests.size();++i)
	{
		m_queue[i]->GetMemoryStatistics(pSizer);
	}
}