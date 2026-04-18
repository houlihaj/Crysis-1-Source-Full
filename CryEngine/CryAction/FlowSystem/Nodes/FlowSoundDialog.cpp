////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   FlowSoundDialog.cpp
//  Version:     v1.00
//  Created:     Jun 22th 2006 by TomasN
//  Compilers:   Visual Studio.NET 2005
//  Description: 
// -------------------------------------------------------------------------
//  History: copied from PlaySoundNode.cpp
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <IEntityProxy.h>
#include <ISound.h>
#include "FlowBaseNode.h"

#define SAFE_SOUND_UNREGISTER
// #undef  SAFE_SOUND_UNREGISTER

class CFlowNode_SoundDialog : public CFlowBaseNode, public ISoundEventListener
{

public:
	CFlowNode_SoundDialog( SActivationInfo * pActInfo )
		:	m_curEntityId(0),
		m_pGraph(NULL),
		m_myID(InvalidFlowNodeId),
		m_SoundID(INVALID_SOUNDID),
		m_bPrecacheOnce(true)
	{
	}

	~CFlowNode_SoundDialog()
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
		return new CFlowNode_SoundDialog(pActInfo);
	}

	virtual void Serialize(SActivationInfo* pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		if (ser.IsWriting()) 
		{
			bool playing = m_SoundID != INVALID_SOUNDID;
			ser.Value("playing", playing);
		} 
		else 
		{
			// On Reading we stop any previously playing sounds... proper serialization should be done in via MusicSystem
			StopSound();

			if (pActInfo->pEntity != 0) 
				m_curEntityId = pActInfo->pEntity->GetId();
			else
				m_curEntityId = 0;

		}
		ser.EndGroup();
	}

	enum INPUTS 
	{
		eIN_NAME = 0,
		eIN_PLAY,
		eIN_STOP,
	};

	enum OUTPUTS 
	{
		eOUT_DONE = 0,
		eOUT_PLAYING
	};

	virtual void GetConfiguration(SFlowNodeConfig& config)
	{
		static const SInputPortConfig inputs[] = 
		{
			InputPortConfig<string> ("sound_SoundName", _HELP("Name of Dialog Sound to play"), _HELP("SoundName")),
			InputPortConfig_Void ("PlayTrigger", _HELP("Play"), _HELP("Play")),
			InputPortConfig_Void ("StopTrigger", _HELP("Stop"), _HELP("Stop")),
			{0}
		};
		static const SOutputPortConfig outputs[] = 
		{
			OutputPortConfig_Void    ("Done", _HELP("Triggered if SoundEvent has stopped playing")),
			OutputPortConfig<bool>   ("IsPlaying", _HELP("True if SoundEvent is playing (triggered on changed)")),
			{0}
		};    
		config.nFlags |= EFLN_TARGET_ENTITY;
		config.pInputPorts = inputs;
		config.pOutputPorts = outputs;
		config.sDescription = _HELP("Play a DialogSound on an entity");
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
					CryLogAlways("[flow] Sound:StopDialog - Can't get sound proxy of entity!");
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
		static const bool bDontPlaySameSound = true;

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
				if (bDontPlaySameSound)
				{
					const string& soundName = GetPortString(pActInfo, eIN_NAME);
					if (soundName.compare(pSound->GetName()) == 0)
					{
						return false;
					}
				}
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
			CryLogAlways("[flow] Sound:Dialog - Can't get sound proxy of entity!");
			return false;
		}

		// start a new sound
		int sFlags = FLAG_SOUND_DEFAULT_3D | FLAG_SOUND_EVENT | FLAG_SOUND_VOICE;
		sFlags |= IsClientActor(m_curEntityId) ? FLAG_SOUND_RELATIVE : 0;
		
		ISound* pSound = NULL;
		m_SoundID = INVALID_SOUNDID;

		if (gEnv->pSoundSystem)
			pSound = gEnv->pSoundSystem->CreateSound(GetPortString(pActInfo, eIN_NAME), sFlags);

		if (!pSound && gEnv->pSoundSystem) 
		{
			CryLogAlways("[flow] Sound:Dialog - Can't play sound '%s'!", GetPortString(pActInfo, eIN_NAME).c_str());
			return false;
		}

		// set properties and register as listener to get notified on stop
		if (pSound != NULL) 
		{
			// register as listener
			pSound->AddEventListener(this, "FlowSoundDialog");
			m_SoundID = pSound->GetId();

			// set some properties ...
			// sound proxy uses head pos on dialog sounds
			pSoundProxy->PlaySound(pSound, Vec3(ZERO), FORWARD_DIRECTION, 1.0f);

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
				if ( IsPortActive(pActInfo, eIN_STOP)) 
				{
					StopSound();
					// TODO: only activate outputs if it was actually playing before stopping
					ActivateOutput(pActInfo, eOUT_DONE, true);
					ActivateOutput(pActInfo, eOUT_PLAYING, false);
				}
				if ( IsPortActive(pActInfo, eIN_PLAY)) 
				{
					StartSound(pActInfo);
					bStarted = true;
				}
			}
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

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

protected:
	EntityId    m_curEntityId;
	IFlowGraph *m_pGraph;
	TFlowNodeId m_myID;
	tSoundID    m_SoundID; 
	bool				m_bPrecacheOnce;
};


REGISTER_FLOW_NODE("Sound:Dialog", CFlowNode_SoundDialog);
