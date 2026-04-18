
#ifndef __GSCONTEXTVIEWEXT_H__
#define __GSCONTEXTVIEWEXT_H__

#include "Context/IContextViewExtension.h"
#include "NetHelpers.h"
#include "GSStatsTrack.h"

class CGameSpy;
class CGSServerReport;
class CContextView;

struct SHWStruct
{
  void SerializeWith(TSerialize ser)
  {

  }
};

enum ECDKeyMessageType
{
  eCMT_firstvalue = 0,
  eCMT_challenge = 0,
  eCMT_response,
  eCMT_rechallenge,
  eCMT_reresponse,
  eCMT_lastvalue // This should be last
};

struct SCDKeyAuthMessage
{
  ECDKeyMessageType   m_type;
  string              m_message;
  void SerializeWith( TSerialize ser )
  {
    ser.EnumValue("type",m_type,eCMT_firstvalue,eCMT_lastvalue);
    ser.Value("message",m_message);
  }
};

struct SSessionIdMessage
{
	void SerializeWith( TSerialize ser )
	{
		for(int i=0;i<SC_SESSION_GUID_SIZE;++i)
			ser.Value("v",id[i],'ui8');
	}
	uint8	id[SC_SESSION_GUID_SIZE];
};

struct SConnectionIdMessage
{
	void SerializeWith( TSerialize ser )
	{
		for(int i=0;i<SC_CONNECTION_GUID_SIZE;++i)
			ser.Value("v",id[i],'ui8');
	}
	uint8	id[SC_CONNECTION_GUID_SIZE];
};

struct SProfileInfoMessage
{
  int     m_profileId;
  string  m_nick;
  void SerializeWith( TSerialize ser )
  {
    ser.Value("profileid",m_profileId);
    ser.Value("nick",m_nick);
  }
};

struct SPreorderedStatusMessage
{
	void SerializeWith( TSerialize ser)
	{
	}
};

class CGSCVExtension : public CNetMessageSinkHelper<CGSCVExtension, IContextViewExtension>
{
private:
	struct SPreorderReader;
public:
  CGSCVExtension(bool server, CGameSpy*);
  ~CGSCVExtension();

  //IContextViewExtension
  virtual void SetParent(CContextView*);
  virtual void OnWitnessDeclared(EntityId);
  virtual void Release();
	virtual void Die();
  virtual bool EnterState(EContextViewState state );
  
  //INetMessageSink
  void DefineProtocol( IProtocolBuilder * pBuilder );

  //other stuff
  void  RequestCDKeyAuth(const char* challenge, bool reauth);
  void  CancelCDKeyCheck();
  void  SendProfileInfo(int profileid, const char* nick);
	void  SetPreordered(bool pr, bool fromcache);

	//Stats tracking
	void SendSessionId(const uint8 sessionId[SC_SESSION_GUID_SIZE]);
	void SendConnectionId(const uint8 connectionId[SC_CONNECTION_GUID_SIZE]);

  EntityId GetPlayerId()const;
	TNetChannelID GetLocalId()const;
  bool IsCDKeyCheckTimedOut()const;
  CContextView* GetParent()const
  {
    return m_parent;
  }

  bool IsServer()const;
  
protected:
  NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(CDKeyCheck, SCDKeyAuthMessage);
  NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(ProfileInfo, SProfileInfoMessage);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(SessionId, SSessionIdMessage);
	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(ConnectionId, SConnectionIdMessage);

	NET_DECLARE_SIMPLE_IMMEDIATE_MESSAGE(PreorderStatus, SPreorderedStatusMessage);

private:
  bool          m_server;
  CGameSpy*     m_service;
  CContextView* m_parent;
  EntityId      m_playerId;
	TNetChannelID m_localId;
  string        m_challenge;//generated challenge
  CGSServerReport* m_report;
  CTimeValue    m_cdkeyCheckSent;
  bool          m_cdkeyCheckWaiting;
  int           m_profileId;
	bool					m_dead;
	std::vector<SPreorderReader*>	m_requests;
};

#endif //__GSCONTEXTVIEWEXT_H__