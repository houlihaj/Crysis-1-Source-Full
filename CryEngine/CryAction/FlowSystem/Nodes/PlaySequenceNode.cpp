
#include "StdAfx.h"
#include <ISystem.h>
#include <ICryAnimation.h>
#include <IMovieSystem.h>
#include <IViewSystem.h>
#include "FlowBaseNode.h"

class CPlaySequence_Node : public CFlowBaseNode, public IMovieListener
{
	_smart_ptr<IAnimSequence> m_pSeq;
	SActivationInfo m_actInfo;
	bool m_bPlaying;

	enum INPUTS
	{
		EIP_Sequence = 0,
		EIP_Trigger,
		EIP_Stop,
		EIP_BreakOnStop,
		EIP_BlendPosSpeed,
		EIP_BlendRotSpeed,
		EIP_PerformBlendOut,
	};

	enum OUTPUTS
	{
		EOP_Started = 0,
		EOP_Done,
		EOP_Finished,
		EOP_Aborted,
	};

public:
	CPlaySequence_Node( SActivationInfo * pActInfo )
	{
		m_pSeq = 0;
		m_actInfo = *pActInfo;
		m_bPlaying = false;
	};

	virtual void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
	}

	~CPlaySequence_Node()
	{
		if (m_pSeq)
		{
			IMovieSystem *movieSys = gEnv->pMovieSystem;
			if (movieSys != 0)
				movieSys->RemoveMovieListener(m_pSeq, this);
		}
	};

	IFlowNodePtr Clone( SActivationInfo * pActInfo )
	{
		return new CPlaySequence_Node(pActInfo);
	}

	virtual void Serialize(SActivationInfo *pActInfo, TSerialize ser)
	{
		ser.BeginGroup("Local");
		if (ser.IsWriting()) 
		{
			ser.Value("m_bPlaying", m_bPlaying);
			float curTime = -1.0f;
			if (m_pSeq && gEnv->pMovieSystem)
			{
				curTime = gEnv->pMovieSystem->GetPlayingTime(m_pSeq);
			}
			ser.Value("curTime", curTime);
		}
		else
		{
			bool playing;
			float curTime = 0.0f;
			ser.Value("m_bPlaying", playing);
			ser.Value("curTime", curTime);
			if (playing)
			{
				// restart sequence
				StartSequence(pActInfo, /* GetPortBool(pActInfo, 3) ? 0.0f : */ curTime, false);
			} 
			else
			{
				// this unregisters only! because all sequences have been stopped already
				// by MovieSystem's Reset in GameSerialize.cpp
				StopSequence(pActInfo, true);
			}
		}
		ser.EndGroup();
	}

	virtual void GetConfiguration( SFlowNodeConfig &config )
	{
		static const SInputPortConfig in_config[] = {
			InputPortConfig<string>( "seq_Sequence_File",_HELP("Name of the Sequence"), _HELP("Sequence") ),
			InputPortConfig_Void( "Trigger",_HELP("Starts the sequence"), _HELP("StartTrigger") ),
			InputPortConfig_Void( "Stop", _HELP("Stops the sequence"), _HELP("StopTrigger") ),
			InputPortConfig<bool>( "BreakOnStop", false, _HELP("If set to 'true', stopping the sequence doesn't jump to end.") ),
			InputPortConfig<float>("BlendPosSpeed", 0.0f, _HELP("Speed at which position gets blended into animation.") ),
			InputPortConfig<float>("BlendRotSpeed", 0.0f, _HELP("Speed at which rotation gets blended into animation.") ),
			InputPortConfig<bool>( "PerformBlendOut", false, _HELP("If set to 'true' the cutscene will blend out after it has finished to the new view (please reposition player when 'Started' happens).") ),
			{0}
		};
		static const SOutputPortConfig out_config[] = {
			OutputPortConfig_Void("Started", _HELP("Triggered when sequence is started") ),
			OutputPortConfig_Void("Done", _HELP("Triggered when sequence is stopped [either via StopTrigger or aborted via Code]"), _HELP("Done") ),
			OutputPortConfig_Void("Finished", _HELP("Triggered when sequence finished normally") ),
			OutputPortConfig_Void("Aborted", _HELP("Triggered when sequence is aborted (Stopped and BreakOnStop true or via Code)") ),
			{0}
		};
		config.sDescription = _HELP( "Plays a Trackview Sequence" );
		config.pInputPorts = in_config;
		config.pOutputPorts = out_config;
		config.SetCategory(EFLN_APPROVED);
	}

	virtual void ProcessEvent( EFlowEvent event,SActivationInfo *pActInfo )
	{
		switch (event)
		{
			case eFE_Activate:
			{
				if (IsPortActive(pActInfo, EIP_Stop))
				{
					bool wasPlaying (m_bPlaying);
					const bool bLeaveTime = GetPortBool(pActInfo, EIP_BreakOnStop);
					StopSequence(pActInfo, false, true, bLeaveTime);
					// we trigger manually, as we unregister before the callback happens
					if (wasPlaying)
					{
						ActivateOutput(pActInfo, EOP_Done, true); // signal we're done
						ActivateOutput(pActInfo, EOP_Aborted, true); // signal it's been aborted
					}
				}
				if (IsPortActive(pActInfo, EIP_Trigger))
				{
					StartSequence(pActInfo);
				}
				break;
			}

			case eFE_Initialize:
			{
				StopSequence(pActInfo);
			}
			break;
		};
	};

	virtual void OnMovieEvent(IMovieListener::EMovieEvent event, IAnimSequence* pSequence) {
		// CryLogAlways("CPlaySequence_Node::OnMovieEvent event=%d  seq=%p", event, pSequence);
		if (event == IMovieListener::MOVIE_EVENT_STOP)
		{
			ActivateOutput(&m_actInfo, EOP_Done, true);
			ActivateOutput(&m_actInfo, EOP_Finished, true);
			m_bPlaying = false;
		}
		else if (event == IMovieListener::MOVIE_EVENT_ABORTED)
		{
			ActivateOutput(&m_actInfo, EOP_Done, true);
			ActivateOutput(&m_actInfo, EOP_Aborted, true);
			m_bPlaying = false;
		}
	}

protected:
	
	void StopSequence(SActivationInfo* /*pActInfo*/, bool bUnRegisterOnly=false, bool bAbort=false, bool bLeaveTime=false)
	{
		IMovieSystem *movieSys = gEnv->pMovieSystem;
		if (!movieSys)
			return;

		if (m_pSeq)
		{
			// we remove first to NOT get notified!
			movieSys->RemoveMovieListener(m_pSeq, this);
			if (!bUnRegisterOnly && movieSys->IsPlaying(m_pSeq))
			{
				if (bAbort) // stops sequence and leaves it at current position
					movieSys->AbortSequence(m_pSeq, bLeaveTime);
				else
					movieSys->StopSequence(m_pSeq);
			}
			m_pSeq = 0;
		}
		m_bPlaying = false;
	}

	void StartSequence(SActivationInfo* pActInfo, float curTime = 0.0f, bool bNotifyStarted = true)
	{
		IMovieSystem *movieSys = gEnv->pMovieSystem;
		if (!movieSys)
			return;

		if (m_pSeq)
		{
			movieSys->RemoveMovieListener(m_pSeq, this);
			movieSys->StopSequence(m_pSeq);
			m_pSeq = 0;
			m_bPlaying = false;
		}
		m_pSeq = movieSys->FindSequence(GetPortString(pActInfo, EIP_Sequence));
		if (m_pSeq)
		{
			m_bPlaying = true;
			movieSys->AddMovieListener(m_pSeq, this);
			movieSys->PlaySequence(m_pSeq, NULL, true, false);
			movieSys->SetPlayingTime(m_pSeq, curTime);

			// set blend parameters
			IViewSystem* pViewSystem = CCryAction::GetCryAction()->GetIViewSystem();
			if (pViewSystem)
			{
				float blendPosSpeed = GetPortFloat(pActInfo, EIP_BlendPosSpeed);
				float blendRotSpeed = GetPortFloat(pActInfo, EIP_BlendRotSpeed);
				bool performBlendOut = GetPortBool(pActInfo, EIP_PerformBlendOut);
				pViewSystem->SetBlendParams(blendPosSpeed, blendRotSpeed, performBlendOut);
			}

			if (bNotifyStarted)
				ActivateOutput(pActInfo, EOP_Started, true);
			// CryLogAlways("[flow] Animations:PlaySequence: Sequence \"%s\" start", GetPortString(pActInfo, 0));
		} 
		else
		{
			// sequence was not found -> hint 
			GameWarning("[flow] Animations:PlaySequence: Sequence \"%s\" not found", GetPortString(pActInfo, 0).c_str());
			// maybe we should trigger the output, but if sequence is not found this should be an error
		}
	}
};

REGISTER_FLOW_NODE( "Animations:PlaySequence", CPlaySequence_Node );

