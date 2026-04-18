////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   PlaySoundNode.cpp
//  Version:     v1.00
//  Created:     Oct 24th 2005 by AlexL
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <IEntityProxy.h>
#include <ISound.h>
#include "FlowBaseNode.h"

#define SAFE_SOUND_UNREGISTER
// #undef  SAFE_SOUND_UNREGISTER

class CFlowNode_PlaySound : public CFlowBaseNode, public ISoundEventListener
{

public:
	CFlowNode_PlaySound( SActivationInfo * pActInfo ) 
		: m_curEntityId(0),
		m_pGraph(NULL),
		m_myID(InvalidFlowNodeId),
		m_SoundID(INVALID_SOUNDID),
		m_bHasPlayed(false),
		m_bPrecacheOnce(true),
		m_bPostSerializeStop(false)
	{
	}


	~CFlowNode_PlaySound()
	{
		ISound* pSound = GetSound();
		if (pSound) 
		{
			pSound->RemoveEventListener(this);
			pSound->Stop(); // use pSound directly as soundproxy might already be deleted
		}
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowNode_PlaySound(pActInfo);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		if (ser.IsWriting()) 
		{
			bool playing = m_SoundID != INVALID_SOUNDID;
			ser.Value("playing", playing);
			ser.Value("m_bHasPlayed", m_bHasPlayed);
		} 
		else 
		{
			// On Reading we stop any previously playing sounds... proper serialization should be done in via MusicSystem
			StopSound();

			if (pActInfo->pEntity != 0) 
				m_curEntityId = pActInfo->pEntity->GetId();
			else
				m_curEntityId = 0;

			ser.Value("m_bHasPlayed", m_bHasPlayed);

			m_bPostSerializeStop = false;
			ser.Value("playing", m_bPostSerializeStop);
			if (m_bPostSerializeStop)
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		}
		ser.EndGroup();
	}

	enum INPUTS 
	{
		eIN_ENABLE,
		eIN_NAME,
		eIN_PLAY,
		eIN_STOP,
		eIN_LOOP,
		eIN_ONCE,
		eIN_VOL,
		eIN_INNER_RAD,
		eIN_OUTER_RAD,
		eIN_VOICE,
		eIN_PLAY_DEP
	};

	enum OUTPUTS 
	{
		eOUT_PLAYING,
		eOUT_DONE
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = 
		{
			InputPortConfig<bool>   ("Enable", true, _HELP("Enable/Disable SoundNode")),
				InputPortConfig<string> ("sound_SoundName", _HELP("Name of Sound to play"), _HELP("SoundName")),
				InputPortConfig_AnyType ("PlayTrigger", _HELP("Play"), _HELP("Play")),
				InputPortConfig_AnyType ("StopTrigger", _HELP("Stop"), _HELP("Stop")),
				InputPortConfig<bool>   ("Loop", false, _HELP("Loop the sound")),
				InputPortConfig<bool>   ("Once", false, _HELP("Play only once")),
				InputPortConfig<float>  ("Volume", 1.0f, _HELP("Volume (dynamic)")),
				InputPortConfig<float>  ("InnerRadius", 2.0f, _HELP("Inner Radius")),
				InputPortConfig<float>  ("OuterRadius", 10.0f, _HELP("Outer Radius")),
				InputPortConfig<bool>   ("Voice", false,_HELP("If True this is a voice sound") ),
				InputPortConfig<bool>   ("Play", _HELP("Play/Stop playing"), _HELP("Play/Stop[deprecated]")),  // deprecated now
			{0}
		};
		static const SOutputPortConfig outputs[] = 
		{
			OutputPortConfig<bool>   ("IsPlaying", _HELP("True if Sound is playing (triggered on changed)")),
			OutputPortConfig_AnyType ("Done", _HELP("Triggered if Sound has stopped playing")),
			{0}
		};    
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Play a sound on an entity");
		config.SetCategory(EFLN_DEBUG);
	}

	// always uses the m_curEntityId
	// side effects: m_SoundID will be INVALID_SOUNDID
	void StopSound()
	{
		if (m_SoundID != INVALID_SOUNDID)
		{
			IEntitySoundProxy *pSoundProxy = GetSoundProxy(m_curEntityId);
			if (pSoundProxy == NULL) 
			{
				if (m_curEntityId != 0)
					CryLogAlways("[flow] Sound:StopSound - Can't get sound proxy of entity!");
			} 
			else
			{
				pSoundProxy->StopSound(m_SoundID);
			}
			ISound* pSound = GetSound();
			if (pSound) 
			{
				pSound->RemoveEventListener(this);
			}
			m_SoundID = INVALID_SOUNDID;
		}
	}

	// side effect: m_SoundID will be valid (on success)
	// returns true if sound started, false otherwise
	bool StartSound(SActivationInfo *pActInfo)
	{
#ifdef SAFE_SOUND_UNREGISTER
		// we have to do this here
		// because otherwise we could end up in a situation that two sounds have been triggered
		// and we registered at both of them. but none of them has called as back
		// but we can only unregister from one in our d'tor. so we have to unregister here
		if (m_SoundID != INVALID_SOUNDID)
		{
			ISound* pSound = GetSound();
			if (pSound)
			{
				pSound->RemoveEventListener(this);
			}
			m_SoundID = INVALID_SOUNDID;
		}
#endif

		IEntity *pEnt = pActInfo->pEntity;
		if (!pEnt)
			return false;

		IEntitySoundProxy *pSoundProxy = static_cast<IEntitySoundProxy*> (GetOrMakeProxy(pEnt, ENTITY_PROXY_SOUND));
		m_curEntityId = pSoundProxy ? pEnt->GetId() : 0;

		if (!pSoundProxy)
		{
			CryLogAlways("[flow] Sound:StartSound - Can't get sound proxy of entity!");
			return false;
		}

		// don't start disabled sounds
		if (GetPortBool(pActInfo, eIN_ENABLE) == false)
			return false;

		if ( GetPortBool(pActInfo, eIN_ONCE) == true && m_bHasPlayed)
		{
			// CryLogAlways("<flow> PlaySoundNode: sound %s already played", GetPortString(pActInfo, eIN_NAME));
			return false;
		}
		m_bHasPlayed = true;

		// start a new sound
		int sFlags = FLAG_SOUND_DEFAULT_3D | FLAG_SOUND_START_PAUSED;
		sFlags |= GetPortBool(pActInfo, eIN_LOOP) ? FLAG_SOUND_LOOP : 0;
		sFlags |= GetPortBool(pActInfo, eIN_VOICE) ? FLAG_SOUND_VOICE : 0;
		sFlags |= IsClientActor(m_curEntityId) ? FLAG_SOUND_RELATIVE : 0;

		m_SoundID = pSoundProxy->PlaySoundEx(GetPortString(pActInfo, eIN_NAME), Vec3(ZERO), FORWARD_DIRECTION, sFlags, 
			GetPortFloat(pActInfo, eIN_VOL), GetPortFloat(pActInfo, eIN_INNER_RAD), GetPortFloat(pActInfo, eIN_OUTER_RAD), eSoundSemantic_FlowGraph);
		if (m_SoundID == INVALID_SOUNDID) 
		{
			CryLogAlways("[flow] Sound:PlaySound - Can't play sound '%s'!", GetPortString(pActInfo, eIN_NAME).c_str());
			return false;
		}

		// set properties and register as listener to get notified on stop
		ISound* pSound = GetSound();
		if (pSound != NULL) 
		{
			// register as listener
			pSound->AddEventListener(this, "PlaySoundNode");

			// set some properties ...

			pSound->SetPaused(false);

			// signal we're playing because the START_PAUSED does not work currently
			ActivateOutput(pActInfo,eOUT_PLAYING, true);
			return true;
		}

		return false; // we should never come here...
	}

	ISound* GetSound()
	{
		if (m_SoundID != INVALID_SOUNDID && gEnv->pSoundSystem)
			return gEnv->pSoundSystem->GetSound( m_SoundID );
		return 0;
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				IEntity *pEnt = pActInfo->pEntity; // might be 0 when dynamically set / ActionGraphs

				StopSound();
				m_pGraph = pActInfo->pGraph;
				m_myID = pActInfo->myID;
				m_bHasPlayed = false;
				m_curEntityId = pEnt ? pEnt->GetId() : 0;

				if (gEnv->pSoundSystem && m_bPrecacheOnce)
				{
					const string& soundName = GetPortString(pActInfo, eIN_NAME);
					gEnv->pSoundSystem->Precache(soundName.c_str(), 0, 0);
					m_bPrecacheOnce = false;
				}
			}
			break;

		case eFE_SetEntityId:
			{
				// stop any playing sound on OLD entity
				StopSound(); 
				IEntity *pEnt = pActInfo->pEntity;
				m_curEntityId = pEnt ? pEnt->GetId() : 0;
			}
			break;

		case eFE_Activate:
			{
				bool bStarted = false;
				if ( IsPortActive(pActInfo, eIN_ONCE))
				{
					m_bHasPlayed = false; // tbd. when the Once flag is touched we reset the hasPlayed flag
				}
				if ( (IsPortActive(pActInfo, eIN_PLAY_DEP) && GetPortBool(pActInfo, eIN_PLAY_DEP) == false) || IsPortActive(pActInfo, eIN_STOP)) 
				{
					StopSound();
					// TODO: only activate outputs if it was actually playing before stopping
					ActivateOutput(pActInfo, eOUT_PLAYING, false);
					ActivateOutput(pActInfo, eOUT_DONE, true);
				}
				if ( (IsPortActive(pActInfo, eIN_PLAY_DEP) && GetPortBool(pActInfo, eIN_PLAY_DEP) == true) || IsPortActive(pActInfo, eIN_PLAY)) 
				{
					StartSound(pActInfo);
					bStarted = true;
				}
				// only set params if we're not started this frame (because otherwise they're already set in StartSound)
				if (!bStarted) {
					if ( IsPortActive(pActInfo, eIN_VOL) ) 
					{
						ISound* pSound = GetSound();
						if (pSound != 0)
							pSound->SetVolume(GetPortFloat(pActInfo, eIN_VOL));
					}
					if ( IsPortActive(pActInfo, eIN_INNER_RAD) || IsPortActive(pActInfo, eIN_OUTER_RAD) )
					{
						ISound* pSound = GetSound();
						if (pSound != 0)
							pSound->SetMinMaxDistance(GetPortFloat(pActInfo, eIN_INNER_RAD), GetPortFloat(pActInfo, eIN_OUTER_RAD));
					}
					if (IsPortActive(pActInfo, eIN_LOOP))
					{
						ISound* pSound = GetSound();
						if (pSound != 0)
							pSound->SetLoopMode(GetPortBool(pActInfo, eIN_LOOP));
					}
				}
			}
			break;
		case eFE_Update:
			if (m_bPostSerializeStop)
			{
				m_bPostSerializeStop = false;
				ActivateOutput(pActInfo, eOUT_PLAYING, false);
				ActivateOutput(pActInfo, eOUT_DONE, true);
			}
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// ISoundEventListener implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void OnSoundEvent( ESoundCallbackEvent event,ISound *pSound ) 
	{
		switch (event) 
		{
		case SOUND_EVENT_ON_STOP:
			{
				// CryLogAlways("[flow] Sound:PlaySound - event stop");
				if (m_pGraph)
				{
					SFlowAddress addr (m_myID, eOUT_PLAYING, true);
					m_pGraph->ActivatePort(addr, false);
					addr.port = eOUT_DONE;
					m_pGraph->ActivatePort(addr, true);		
					if (pSound) // we remove from the sound which called us back
					{
						pSound->RemoveEventListener(this);
						if (pSound->GetId() == m_SoundID) 
						{
							// only set the m_SoundID to invalid if this was the sound which we're currently playing
							// it could be the case that a new sound with a new ID has been triggered but the OLD sound calls us back
							m_SoundID = INVALID_SOUNDID;
						}
					}
				}
			}
			break;
		case SOUND_EVENT_ON_START:
			{
				SFlowAddress addr (m_myID, eOUT_PLAYING, true);
				if (m_pGraph) m_pGraph->ActivatePort(addr, true);
			}
			break;
		}
	}

protected:
	EntityId    m_curEntityId;
	IFlowGraph *m_pGraph;
	TFlowNodeId m_myID;
	tSoundID    m_SoundID; 
	bool        m_bHasPlayed; // for 'Once'
	bool				m_bPrecacheOnce;
	bool				m_bPostSerializeStop;
};




class CFlowNode_PlaySoundEvent : public CFlowBaseNode, public ISoundEventListener
{

public:
	CFlowNode_PlaySoundEvent( SActivationInfo * pActInfo )
		:	m_curEntityId(0),
		m_pGraph(NULL),
		m_myID(InvalidFlowNodeId),
		m_SoundID(INVALID_SOUNDID),
		m_bHasPlayed(false),
		m_bPrecacheOnce(true),
		m_bPostSerializeStop(false)
	{
	}

	~CFlowNode_PlaySoundEvent()
	{
		ISound* pSound = GetSound();
		if (pSound) 
		{
			pSound->RemoveEventListener(this);
			pSound->Stop(); // use pSound directly as soundproxy might already be deleted
		}
	}

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CFlowNode_PlaySoundEvent(pActInfo);
	}

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		if (ser.IsWriting()) 
		{
			bool playing = m_SoundID != INVALID_SOUNDID;
			ser.Value("playing", playing);
			ser.Value("m_bHasPlayed", m_bHasPlayed);
		} 
		else 
		{
			// On Reading we stop any previously playing sounds... proper serialization should be done in via MusicSystem
			StopSound();

			if (pActInfo->pEntity != 0) 
				m_curEntityId = pActInfo->pEntity->GetId();
			else
				m_curEntityId = 0;

			ser.Value("m_bHasPlayed", m_bHasPlayed);

			m_bPostSerializeStop = false;
			ser.Value("playing", m_bPostSerializeStop);
			if (m_bPostSerializeStop)
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		}
		ser.EndGroup();
	}

	enum INPUTS 
	{
		eIN_ENABLE,
		eIN_NAME,
		eIN_PLAY,
		eIN_STOP,
		eIN_VOICE,
		eIN_ONCE,
	};

	enum OUTPUTS 
	{
		eOUT_MAX_RADIUS,
		eOUT_PLAYING,
		eOUT_DONE
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = 
		{
			InputPortConfig<bool>   ("Enable", true, _HELP("Enable/Disable SoundEventNode")),
			InputPortConfig<string> ("sound_SoundEvent", _HELP("Name of SoundEvent to play"), _HELP("SoundEvent")),
			InputPortConfig_AnyType ("PlayTrigger", _HELP("Play"), _HELP("Play")),
			InputPortConfig_AnyType ("StopTrigger", _HELP("Stop"), _HELP("Stop")),
			InputPortConfig<bool>   ("Voice", false,_HELP("If True this is a voice sound") ),
			InputPortConfig<bool>   ("Once", false, _HELP("Play only once")),
			{0}
		};
		static const SOutputPortConfig outputs[] = 
		{
			OutputPortConfig<float>  ("MaxRadius", _HELP("Debug information about the effect radius")),
			OutputPortConfig<bool>   ("IsPlaying", _HELP("True if SoundEvent is playing (triggered on changed)")),
			OutputPortConfig_AnyType ("Done", _HELP("Triggered if SoundEvent has stopped playing")),
			{0}
		};    
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Play a sound event on an entity");
		config.SetCategory(EFLN_APPROVED);
	}

	// side effects: m_SoundID will be INVALID_SOUNDID
	void StopSound()
	{
		if (m_SoundID != INVALID_SOUNDID)
		{
			IEntitySoundProxy *pSoundProxy = GetSoundProxy(m_curEntityId);
			if (pSoundProxy == NULL) 
			{
				if (m_curEntityId != 0)
					CryLogAlways("[flow] Sound:StopSound - Can't get sound proxy of entity!");
			} 
			else
			{
				pSoundProxy->StopSound(m_SoundID);
			}
			ISound* pSound = GetSound();
			if (pSound) 
			{
				pSound->RemoveEventListener(this);
			}
			m_SoundID = INVALID_SOUNDID;
		}
	}

	// side effect: m_SoundID will be valid (on success)
	// returns true if sound started, false otherwise
	bool StartSound(SActivationInfo *pActInfo)
	{
#ifdef SAFE_SOUND_UNREGISTER
		// we have to do this here
		// because otherwise we could end up in a situation that two sounds have been triggered
		// and we registered at both of them. but none of them has called as back
		// but we can only unregister from one in our d'tor. so we have to unregister here
		if (m_SoundID != INVALID_SOUNDID)
		{
			ISound* pSound = GetSound();
			if (pSound)
			{
				pSound->RemoveEventListener(this);
			}
			m_SoundID = INVALID_SOUNDID;
		}
#endif

		IEntity *pEnt = pActInfo->pEntity;
		if (!pEnt)
			return false;

		IEntitySoundProxy *pSoundProxy = static_cast<IEntitySoundProxy*> (GetOrMakeProxy(pEnt, ENTITY_PROXY_SOUND));
		m_curEntityId = pSoundProxy ? pEnt->GetId() : 0;

		if (!pSoundProxy)
		{
			CryLogAlways("[flow] Sound:StartSound - Can't get sound proxy of entity!");
			return false;
		}

		// don't start disabled sounds
		if (GetPortBool(pActInfo, eIN_ENABLE) == false)
			return false;

		if ( GetPortBool(pActInfo, eIN_ONCE) == true && m_bHasPlayed)
		{
			// CryLogAlways("<flow> SoundEventNode %s already played", GetPortString(pActInfo, eIN_NAME));
			return false;
		}
		m_bHasPlayed = true;

		// start a new sound
		int nFlags = FLAG_SOUND_DEFAULT_3D;
		if ( GetPortBool(pActInfo, eIN_VOICE))
			nFlags |= FLAG_SOUND_VOICE;
		nFlags |= IsClientActor(m_curEntityId) ? FLAG_SOUND_RELATIVE : 0;

		ISound* pSound = NULL;

		if (gEnv->pSoundSystem)
			pSound = gEnv->pSoundSystem->CreateSound(GetPortString(pActInfo, eIN_NAME), nFlags);
		
		if (!pSound) 
		{
			CryLogAlways("[flow] Sound:PlaySoundEvent - Can't play sound '%s'!", GetPortString(pActInfo, eIN_NAME).c_str());
			return false;
		}

		// set properties and register as listener to get notified on stop
		if (pSound != NULL) 
		{
			m_SoundID = pSound->GetId();

			// register as listener
			pSound->AddEventListener(this, "PlaySoundNode");

			pSoundProxy->PlaySound(pSound, Vec3(ZERO), FORWARD_DIRECTION, 1.0f);

			// signal we're playing because the START_PAUSED does not work currently
			ActivateOutput(pActInfo,eOUT_PLAYING, true);
			ActivateOutput(pActInfo,eOUT_MAX_RADIUS, pSound->GetMaxDistance());
			return true;
		}

		return false; // we should never come here...
	}

	ISound* GetSound()
	{
		if (m_SoundID != INVALID_SOUNDID && gEnv->pSoundSystem)
			return gEnv->pSoundSystem->GetSound( m_SoundID );
		return 0;
	}

	virtual void ProcessEvent( EFlowEvent event, SActivationInfo *pActInfo )
	{
		switch (event)
		{
		case eFE_Initialize:
			{
				IEntity *pEnt = pActInfo->pEntity; // might be 0 when dynamically set / ActionGraphs

				StopSound();
				m_pGraph = pActInfo->pGraph;
				m_myID = pActInfo->myID;
				m_bHasPlayed = false;
				m_curEntityId = pEnt ? pEnt->GetId() : 0;

				if (gEnv->pSoundSystem && m_bPrecacheOnce)
				{
					const string& soundName = GetPortString(pActInfo, eIN_NAME);
					gEnv->pSoundSystem->Precache(soundName.c_str(), 0, 0);
					m_bPrecacheOnce = false;
				}
			}
			break;

		case eFE_SetEntityId:
			{
				StopSound(); 
				IEntity *pEnt = pActInfo->pEntity;
				m_curEntityId = pEnt ? pEnt->GetId() : 0;
			}
			break;

		case eFE_Activate:
			{
				bool bStarted = false;
				if ( IsPortActive(pActInfo, eIN_ONCE))
				{
					m_bHasPlayed = false; // tbd. when the Once flag is touched we reset the hasPlayed flag
				}
				if ( IsPortActive(pActInfo, eIN_STOP)) 
				{
					StopSound();
					// TODO: only activate outputs if it was actually playing before stopping
					ActivateOutput(pActInfo, eOUT_PLAYING, false);
					ActivateOutput(pActInfo, eOUT_DONE, true);
				}
				if ( IsPortActive(pActInfo, eIN_PLAY)) 
				{
					StartSound(pActInfo);
					bStarted = true;
				}
			}
			break;
		case eFE_Update:
			if (m_bPostSerializeStop)
			{
				m_bPostSerializeStop = false;
				ActivateOutput(pActInfo, eOUT_PLAYING, false);
				ActivateOutput(pActInfo, eOUT_DONE, true);
			}
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// ISoundEventListener implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void OnSoundEvent( ESoundCallbackEvent event,ISound *pSound ) 
	{
		switch (event) 
		{
		case SOUND_EVENT_ON_STOP:
			{
				// CryLogAlways("[flow] Sound:PlaySound - event stop");
				if (m_pGraph)
				{
					SFlowAddress addr (m_myID, eOUT_PLAYING, true);
					m_pGraph->ActivatePort(addr, false);
					addr.port = eOUT_DONE;
					m_pGraph->ActivatePort(addr, true);				
					if (pSound) // we remove from the sound which called us back
					{
						pSound->RemoveEventListener(this);
						if (pSound->GetId() == m_SoundID) 
						{
							// only set the m_SoundID to invalid if this was the sound which we're currently playing
							// it could be the case that a new sound with a new ID has been triggered but the OLD sound calls us back
							m_SoundID = INVALID_SOUNDID;
						}
					}
				}
			}
			break;
		case SOUND_EVENT_ON_START:
			{
				SFlowAddress addr (m_myID, eOUT_PLAYING, true);
				if (m_pGraph) m_pGraph->ActivatePort(addr, true);
			}
			break;
		}
	}

protected:
	EntityId    m_curEntityId;
	IFlowGraph *m_pGraph;
	TFlowNodeId m_myID;
	tSoundID    m_SoundID; 
	bool        m_bHasPlayed; // for 'Once'
	bool				m_bPrecacheOnce;
	bool        m_bPostSerializeStop;
};


REGISTER_FLOW_NODE("Sound:PlaySound", CFlowNode_PlaySound);
REGISTER_FLOW_NODE("Sound:PlaySoundEvent", CFlowNode_PlaySoundEvent);
