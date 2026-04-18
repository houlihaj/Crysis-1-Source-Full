#ifndef __BREAKREPLICATOR_H__
#define __BREAKREPLICATOR_H__

#pragma once

#include "ActionGame.h"
#include "NetHelpers.h"
#include "ObjectSelector.h"

#include "ProceduralBreak.h"
#include "DeformingBreak.h"
#include "PlaneBreak.h"
#include "IBreakReplicatorListener.h"
#include "IBreakPlaybackStream.h"
#include "ExplosiveObjectState.h"

class CGameContext;

struct SProceduralBreak;
struct SDeformingBreak;
struct SExplosiveBreak;
struct SSimulateCreateEntityPartMessage;
struct SSimulateRemoveEntityPartsMessage;
struct SExplosiveObjectState;
class CGenericRecordingListener;
class CGenericPlaybackListener;
class CNullListener;
class CProceduralBreakingBaseListener;
class CProceduralBreakingRecordingListener;
class CProceduralBreakingPlaybackListener;
class CDeformingBreak;
struct SJointBreakParams;

struct SSetMagicId
{
	SSetMagicId() : breakId(-1), magicId(-1) {}
	SSetMagicId(int bid, int mid) : breakId(bid), magicId(mid) {}

	int breakId;
	int magicId;

	void SerializeWith( TSerialize ser )
	{
		ser.Value("breakId", breakId, 'brId');
		ser.Value("magicId", magicId);
	}
};

class CBreakReplicator : public CNetMessageSinkHelper<CBreakReplicator, INetMessageSink>
{
public:
	CBreakReplicator( CGameContext * pCtx );
	~CBreakReplicator();

	// hack for DefineProtocol
	bool hack_defineProtocolMode_server;
	void DefineProtocol( IProtocolBuilder * pBuilder );

	void PlaybackBreakage( int breakId, INetBreakagePlaybackPtr pBreakage );

	void OnSpawn( IEntity *pEntity, SEntitySpawnParams &params );
	void OnRemove( IEntity *pEntity );
	void OnStartFrame();
	void OnEndFrame();
	void OnBrokeSomething( const SBreakEvent& be, bool isPlane );
	void GetMemoryStatistics( ICrySizer * s );

	void BeginEvent( IBreakReplicatorListenerPtr pListener );
	void EndEvent();

	void AddListener( IBreakReplicatorListenerPtr pListener, int nFrames );

	int PullOrderId();
	void PushBreak( int orderId, const SNetBreakDescription& desc );
	void PushAbandonment( int orderId );

	static CBreakReplicator * Get() { return m_pThis; }

	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(DeformingBreak, SDeformingBreakParams);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(PlaneBreak, SPlaneBreakParams);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(JointBreak, SJointBreakParams);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(SimulateCreateEntityPart, SSimulateCreateEntityPartMessage);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(SimulateRemoveEntityParts, SSimulateRemoveEntityPartsMessage);

	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(DeclareProceduralSpawnRec, SDeclareProceduralSpawnRec);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(DeclareJointBreakRec, SDeclareJointBreakRec);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(DeclareExplosiveObjectState, SDeclareExplosiveObjectState);
	NET_DECLARE_SIMPLE_ATSYNC_MESSAGE(SetMagicId, SSetMagicId);

private:
	// state debug
	int m_nEventSpawns;

	struct SListenerInfo
	{
		SListenerInfo() : timeout(-10) {}
		SListenerInfo(IBreakReplicatorListenerPtr p, int t) : timeout(t), pListener(p) {}
		int timeout;
		IBreakReplicatorListenerPtr pListener;
	};

	void EnterEvent();

	void OnUpdateMesh( const EventPhysUpdateMesh * pEvent );
	void OnCreatePhysEntityPart( const EventPhysCreateEntityPart * pEvent );
	void OnRemovePhysEntityParts( const EventPhysRemoveEntityParts * pEvent );
	void OnJointBroken( const EventPhysJointBroken * pEvent );

	static int OnUpdateMesh_Begin( const EventPhys *pEvent );
	static int OnCreatePhysEntityPart_Begin( const EventPhys *pEvent );
	static int OnRemovePhysEntityParts_Begin( const EventPhys *pEvent );
	static int OnJointBroken_Begin( const EventPhys * pEvent );

	static int OnUpdateMesh_End( const EventPhys *pEvent );
	static int OnCreatePhysEntityPart_End( const EventPhys *pEvent );
	static int OnRemovePhysEntityParts_End( const EventPhys *pEvent );
	static int OnJointBroken_End( const EventPhys * pEvent );

#if BREAK_HIERARCHICAL_TRACKING
	static int OnPostStepEvent( const EventPhys * pEvent );
	void OnPostStep( const EventPhysPostStep * pEvent );
#endif

	IBreakReplicatorListenerPtr AddProceduralBreakTypeListener( const IProceduralBreakTypePtr& pBT );

	void SpinPendingStreams();

	static CBreakReplicator * m_pThis;
	typedef std::vector<SListenerInfo> ListenerInfos;
	ListenerInfos m_listenerInfos;
	ListenerInfos m_listenerInfos_temp;
	IBreakReplicatorListenerPtr m_pActiveListener;
	IBreakReplicatorListenerPtr m_pGenericUpdateMesh;
	IBreakReplicatorListenerPtr m_pGenericJointBroken;
	IBreakReplicatorListenerPtr m_pGenericCreateEntityPart;
	IBreakReplicatorListenerPtr m_pGenericRemoveEntityParts;
	IBreakReplicatorListenerPtr m_pNullListener;
	_smart_ptr<CGenericPlaybackListener> m_pGenericPlaybackListener;

	std::vector<IBreakPlaybackStreamPtr> m_playbackMessageHandlers;

	bool BeginStream( int idx, const IProceduralBreakTypePtr& p );
	IBreakPlaybackStreamPtr GetStream(int idx);
	IBreakPlaybackStreamPtr PullStream(int idx);
	IBreakPlaybackStreamPtr m_pNullPlayback;

	struct SPendingPlayback
	{
		IBreakPlaybackStreamPtr pStream;
		INetBreakagePlaybackPtr pNetBreakage;
	};
	std::vector<SPendingPlayback> m_pendingPlayback;
	IBreakReplicatorListenerPtr m_pPlaybackListener;
	CGameContext * m_pContext;

	int m_nextOrderId;
	int m_loggedOrderId;
	std::map<int, SNetBreakDescription*> m_pendingLogs;
	void FlushPendingLogs();
};

#endif
