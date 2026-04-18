/*************************************************************************
  Crytek Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Implements a client-side IGameChannel interface.
  
 -------------------------------------------------------------------------
  History:
  - 11:8:2004   11:38 : Created by Márcio Martins

*************************************************************************/
#ifndef __GAMECLIENTCHANNEL_H__
#define __GAMECLIENTCHANNEL_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "GameChannel.h"

struct SBasicSpawnParams;
class CGameClientNub;

struct SGameTypeParams : public ISerializable
{
	SGameTypeParams() {}
	SGameTypeParams( uint16 rules, const string& level, bool imm, uint64 cs, const string& url )
		: rulesClass(rules), levelName(level), immersive(imm), levelChecksum(cs), levelDownloadURL(url) {}

	uint16 rulesClass;
	string levelName;
	uint64 levelChecksum;
	string levelDownloadURL;
	bool immersive;

	virtual void SerializeWith( TSerialize ser )
	{
		ser.Value("rules", rulesClass);
		ser.Value("level", levelName);
		ser.Value("levelChecksum", levelChecksum);
		ser.Value("levelDownloadURL", levelDownloadURL);
		ser.Value("immersive", immersive);
	}
};

struct SEntityClassRegistration : public ISerializable
{
	SEntityClassRegistration() {}
	SEntityClassRegistration( uint16 i, const string& n ) : id(i), name(n) 
	{
	}

	string name;
	uint16 id;

	virtual void SerializeWith( TSerialize ser )
	{
		ser.Value( "id", id );
		ser.Value( "name", name );
	}
};

struct SEntityIdParams : public ISerializable
{
	SEntityIdParams() : id(0) {}
	SEntityIdParams( EntityId e ) : id(e) {}

	EntityId id;

	virtual void SerializeWith( TSerialize ser )
	{
		ser.Value( "id", id, 'eid' );
	}
};

struct SClientConsoleVariableParams : public ISerializable
{
	SClientConsoleVariableParams() {}
	SClientConsoleVariableParams( const string& key_, const string& value_ ): key(key_), value(value_) {}

	string key, value;

	virtual void SerializeWith( TSerialize ser )
	{
		ser.Value( "key", key );
		ser.Value( "value", value );
	}
};

struct SBreakEvent;

struct STimeOfDayInitParams
{
	float tod;

	void SerializeWith( TSerialize ser )
	{
		ser.Value("TimeOfDay", tod);
	}
};

class CGameClientChannel :
	public CNetMessageSinkHelper<CGameClientChannel, CGameChannel>
{
public:
	CGameClientChannel(INetChannel *pNetChannel, CGameContext * pContext, CGameClientNub * pNub);
	virtual ~CGameClientChannel();

	void ClearPlayer() { SetPlayerId(0); }

	// IGameChannel
	virtual void Release();
	virtual void OnDisconnect(EDisconnectionCause cause, const char *description);
	// ~IGameChannel

	// INetMessageSink
	virtual void DefineProtocol(IProtocolBuilder * pBuilder);
	// ~INetMessageSink

	void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	void AddUpdateLevelLoaded( IContextEstablisher * pEst );
	bool CheckLevelLoaded() const;
	bool IsServer() { return false; }

	// simple messages
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE( RegisterEntityClass, SEntityClassRegistration );
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE( SetGameType, SGameTypeParams );
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE( ResetMap, SNoParams );
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE( SetConsoleVariable, SClientConsoleVariableParams );
	// TODO: remove this and replace with something better (probably based around network knowing more..)
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE( SetPlayerId_LocalOnly, SEntityIdParams );

	NET_DECLARE_ATSYNC_MESSAGE( ConfigureContextMessage );

	// spawners - must be unreliable ordered 
	// (the net framework takes care of reliability in a special way for 
	//  these - as there is a group of messages that must be reliably sent)
	NET_DECLARE_IMMEDIATE_MESSAGE( DefaultSpawn );

private:
	CGameClientNub *m_pNub;

	void SetPlayerId( EntityId id );
	void CallOnSetPlayerId();

	bool m_hasLoadedLevel;
	std::map<string, string> m_originalCVarValues;
};

#endif //__GAMECLIENTCHANNEL_H__
