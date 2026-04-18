#include "StdAfx.h"
#include "AGParams.h"
#include "AnimationGraphState.h"

#define LAYER_NAMES LAYER_NAMES_AGPARAMS

static const char * LAYER_NAMES[] = {
	"ParamsLayer1",
	"ParamsLayer2",
	"ParamsLayer3",
	"ParamsLayer4",
	"ParamsLayer5",
	"ParamsLayer6",
	"ParamsLayer7",
	"ParamsLayer8",
	"ParamsLayer9",
};

void CAGParams::EnterState( SAnimationStateData& data, bool dueToRollback )
{
	data.params[m_layer] = m_preParams;

	if (m_structure == data.pState->GetCurrentStructure())
	{
		if (m_structure.length() > 0)
			data.params[m_layer].m_nFlags |= CA_TRANSITION_TIMEWARPING;
	}
	else
	{
		data.pState->SetCurrentStructure(m_structure);
	}
}

bool CAGParams::CanLeaveState( SAnimationStateData& data )
{
	return true;
}

void CAGParams::LeaveState( SAnimationStateData& data )
{
	// data.overrides[m_layer].ClearOverrides();
}

void CAGParams::GetCompletionTimes( SAnimationStateData& data, CTimeValue start, CTimeValue& hard, CTimeValue& sticky )
{
	hard = sticky = 0.0f;
}

const IAnimationStateNodeFactory::Params * CAGParams::GetParameters()
{
	static std::vector<Params> * pVec = 0;

	if (!pVec)
	{
		pVec = new std::vector<Params>();
		AddParams( "", *pVec );
		EndParams( *pVec );
	}
	return &((*pVec)[0]);
}

bool CAGParams::Init( const XmlNodeRef& node, IAnimationGraphPtr )
{
	return SerializeParams( "", m_preParams, node );
}

void CAGParams::Release()
{
	delete this;
}

IAnimationStateNode * CAGParams::Create()
{
	return this;
}

const char * CAGParams::GetCategory()
{
	return LAYER_NAMES[m_layer];
}

const char * CAGParams::GetName()
{
	return LAYER_NAMES[m_layer];
}

template <class F>
void CAGParams::ProcessParams( F& f )
{
	f.SimpleParam( "TransitionTime", &CryCharAnimationParams::m_fTransTime );
	f.SimpleParam( "KeyTime", &CryCharAnimationParams::m_fKeyTime );
	f.SimpleParam( "Structure", &CAGParams::m_structure );

	f.FlagParam( "ManualUpdate", CA_MANUAL_UPDATE );
	f.FlagParam( "LoopAnimation", CA_LOOP_ANIMATION );
	f.FlagParam( "RepeatLastKey", CA_REPEAT_LAST_KEY );

	f.FlagParam( "AllowAnimRestart", CA_ALLOW_ANIM_RESTART );
	f.FlagParam( "VTimeWarping", CA_TRANSITION_TIMEWARPING );
	f.FlagParam( "Idle2Move", CA_IDLE2MOVE );
	f.FlagParam( "Move2Idle", CA_MOVE2IDLE );

	f.FlagParam( "PartialBodyUpdate", CA_PARTIAL_SKELETON_UPDATE );
}

class CAGParams::Serializer
{
public:
	Serializer( const char * prefix, CryCharAnimationParams& params, CAGParams& agparams, const XmlNodeRef& node ) : m_prefix(prefix), m_params(params), m_agparams(agparams), m_node(node), m_ok(true)
	{
		params.m_nFlags = 0;
	}

	template <class T>
	void SimpleParam( const char * name, T (CryCharAnimationParams::* value) )
	{
		if (!m_node->getAttr( m_prefix+name, m_params.*value ))
		{
			CryLogAlways("Couldn't load parameter %s", (m_prefix+name).c_str());
			m_ok = false;
		}
	}

	void SimpleParam( const char * name, CCryName (CAGParams::* value) )
	{
		m_agparams.*value = m_node->getAttr( m_prefix+name );
	}

	void FlagParam( const char * name, uint32 value )
	{
		bool hasFlag = false;
		if (!m_node->getAttr( m_prefix+name, hasFlag ))
		{
// this is actually just fine...
//			CryLogAlways("Couldn't load parameter %s", (m_prefix+name).c_str());
//			m_ok = false;
		}
		else if (hasFlag)
			m_params.m_nFlags |= value;
	}

	bool Ok()
	{
		return m_ok;
	}

private:
	string m_prefix;
	CryCharAnimationParams& m_params;
	CAGParams& m_agparams;
	XmlNodeRef m_node;
	bool m_ok;
};

class CAGParams::Adder
{
public:
	Adder( const char * prefix, std::vector<Params>& params ) : m_prefix(prefix), m_params(params)
	{
	}

	void SimpleParam( const char * name, float (CryCharAnimationParams::*) )
	{
		AddParam("float", name, "0");
	}
	void SimpleParam( const char * name, uint32 (CryCharAnimationParams::*) )
	{
		AddParam("int", name, "0");
	}
	void SimpleParam( const char * name, bool (CryCharAnimationParams::*) )
	{
		AddParam("bool", name, "0");
	}
	void SimpleParam( const char * name, CCryName (CAGParams::*) )
	{
		AddParam("string", name, "");
	}
	void FlagParam( const char * name, uint32 )
	{
		AddParam("bool", name, "0");
	}

private:
	void AddParam( const char * type, const char * name, const char * def )
	{
		Params p = {true, type, GetString(m_prefix + name), GetString(m_prefix + name), def};
		m_params.push_back(p);
	}

	string m_prefix;
	std::vector<Params>& m_params;
};

bool CAGParams::SerializeParams( const char * prefix, CryCharAnimationParams& params, const XmlNodeRef& node )
{
	Serializer s(prefix, params, *this, node);
	ProcessParams(s);
	return s.Ok();
}

void CAGParams::AddParams( const char * prefix, std::vector<Params>& params )
{
	Adder a(prefix, params);
	ProcessParams(a);
}

void CAGParams::EndParams( std::vector<Params>& params )
{
	static Params end = {0};
	params.push_back(end);
}

const char * CAGParams::GetString( string s )
{
	static std::set<string> * pSet = 0;
	if (!pSet)
		pSet = new std::set<string>();
	if (pSet->find(s) == pSet->end())
		pSet->insert(s);
	return pSet->find(s)->c_str();
}

void CAGParams::SerializeAsFile(bool reading, AG_FILE *file)
{
	SerializeAsFile_NodeBase(reading, file);

	FileSerializationHelper h(reading, file);

	h.Value(&m_structure);
	h.Value(&m_preParams.m_fInterpolation);
	h.Value(&m_preParams.m_fKeyTime);
	h.Value(&m_preParams.m_fPlaybackSpeed);
	h.Value(&m_preParams.m_fTransTime);
	h.Value(m_preParams.m_fUserData, NUM_ANIMATION_USER_DATA_SLOTS);
//	h.Value(&m_preParams.m_nCopyTimeFromLayer);
	h.Value(&m_preParams.m_nFlags);
	h.Value(&m_preParams.m_nLayerID);
	h.Value(&m_preParams.m_nUserToken);
}

#undef LAYER_NAMES
