#include "StdAfx.h"
#include "AnimationGraph.h"
#include "AnimationGraphState.h"
#include "AnimationGraphManager.h"
#include "AnimationGraphTemplate.h"
#include "StateIndex.h"
#include "ITimer.h"
#include "CryAction.h"
#include "AGAnimation.h"
#include "AGParams.h"
#include "PreprocessLinksAndStates.h"
#include <queue>
#include <stack>
#include <deque>
#include <set>
#include "AnimatedCharacter.h"

char* g_szMCMString[eMCM_COUNT] = { "Undefined", "Entity", "Animation", "DecoupledCatchUp", "ClampedEntity", "SmoothedEntity" };
char* g_szColliderModeString[eColliderMode_COUNT] = { "Undefined", "Disabled", "GroundedOnly", "Pushable", "NonPushable", "PushesPlayersOnly", "Spectator" };
char* g_szColliderModeLayerString[eColliderModeLayer_COUNT] = { "AG", "Game", "Script", "FG", "Sleep", "Debug" };

// penalty for pathfinding if a state has force-follow links
static const uint32 FORCE_FOLLOW_COST = 0;

static const int GuardOpCode_LessThanEqual0 = 0;
static const int GuardOpCode_GreaterThanEqual0 = GuardOpCode_LessThanEqual0 + 256;
static const int GuardOpCode_NotEqualTo0 = GuardOpCode_GreaterThanEqual0 + 256;
static const int GuardOpCode_AlwaysTrue = GuardOpCode_NotEqualTo0 + 256;
static const int GuardOpCodeCount = GuardOpCode_AlwaysTrue + 1;
static bool s_guardSucceedTableInitialized = false;
static uint32 s_guardSucceedTable[GuardOpCodeCount][8];

// this is only for debugging, used to 'decode' StateID's to names.
// it's set to the state's graph at the beginning of CAnimationGraphState::Update and reset to NULL on exit
CAnimationGraph* CAnimationGraph::CURRENT_ANIMGRAPH_DEBUG = NULL;

CAnimationGraph::SGuard::SGuard() : input(0), opCode(GuardOpCode_AlwaysTrue)
{
}

bool CAnimationGraph::SStateInfo::EvaluateGuards( const CStateIndex::InputValue * pInputs ) const
{
	uint32 fails = 0;
	for (uint8 i=0; i < 2*MAX_GUARDS; i++)
	{
		const SGuard& g = guards[i];
		//ok &= (pInputs[g.input] >= g.minValue && pInputs[g.input] <= g.maxValue);
		uint32 idx = pInputs[g.input];
		uint32 mask = 1u << (idx & 31u);
		fails |= ~(s_guardSucceedTable[g.opCode][idx/32] & mask) & mask;
	}
	return (fails == 0);
}

void CAnimationGraph::SStateInfo::SerializeAsFile(bool reading, AG_FILE *pAGI, CAnimationGraphManager* pManager, const std::map<int, IAnimationStateNodeFactory*>& idToFactory, const std::map<IAnimationStateNodeFactory*, int>& factoryToId)
{
	if(!reading)
	{
		FileSerializationHelper h(reading, pAGI);

		h.Value(&id);
		h.Value(&parentState._value);
		h.Value(&factoryStart);
		h.Value(&factoryLength);
		bool temp = allowSelect;
		ICryPak *pPack = gEnv->pCryPak;
		pPack->FWrite(&temp, 1, pAGI->file);
		temp = canMix;
		pPack->FWrite(&temp, 1, pAGI->file);
		temp = hasForceFollows;
		pPack->FWrite(&temp, 1, pAGI->file);
		temp = skipFirstPerson;
		pPack->FWrite(&temp, 1, pAGI->file);
		temp = animationControlledView;
		pPack->FWrite(&temp, 1, pAGI->file);
		temp = noPhysicalCollider;
		pPack->FWrite(&temp, 1, pAGI->file);
		temp = hurryable;
		pPack->FWrite(&temp, 1, pAGI->file);
		temp = matchesSignalledInputs;
		pPack->FWrite(&temp, 1, pAGI->file);
		temp = evaluateGuardsOnExit;
		pPack->FWrite(&temp, 1, pAGI->file);
		temp = hasAnyLinksFrom;
		pPack->FWrite(&temp, 1, pAGI->file);
		temp = hasAnyLinksTo;
		pPack->FWrite(&temp, 1, pAGI->file);
		pPack->FWrite(&MovementControlMethodH, 1, pAGI->file);
		pPack->FWrite(&MovementControlMethodV, 1, pAGI->file);
		pPack->FWrite(&cost, 1, pAGI->file);
		pPack->FWrite(&linkOffset, 1, pAGI->file);
		pPack->FWrite(&additionalTurnMultiplier, 1, pAGI->file);
		pPack->FWrite(&prevState, 1, pAGI->file);
		for(int i = 0; i < 2*MAX_GUARDS; ++i)
		{
			uint8 input = guards[i].input;
			uint16 opCode = guards[i].opCode;
			pPack->FWrite(&input, 1, pAGI->file);
			pPack->FWrite(&opCode, 1, pAGI->file);
		}
		pPack->FWrite(&animDesc.movement.translation, 1, pAGI->file);
		pPack->FWrite(&animDesc.movement.rotation, 1, pAGI->file);
		pPack->FWrite(&animDesc.movement.duration, 1, pAGI->file);
		pPack->FWrite(&animDesc.startLocation.t, 1, pAGI->file);
		pPack->FWrite(&animDesc.startLocation.q, 1, pAGI->file);
	}
	else
	{
		FileSerializationHelper h(reading, pAGI);

		bool swap = false;
#ifdef NEED_ENDIAN_SWAP
		swap = true; 
#endif
		h.Value(&id);
		h.Value(&parentState._value);
		h.Value(&factoryStart);
		h.Value(&factoryLength);
		bool temp = false;
		h.Value(&temp);
		allowSelect = temp;
		h.Value(&temp);
		canMix = temp;
		h.Value(&temp);
		hasForceFollows = temp;
		h.Value(&temp);
		skipFirstPerson = temp;
		h.Value(&temp);
		animationControlledView = temp;
		h.Value(&temp);
		noPhysicalCollider = temp;
		h.Value(&temp);
		hurryable = temp;
		h.Value(&temp);
		matchesSignalledInputs = temp;
		h.Value(&temp);
		evaluateGuardsOnExit = temp;
		h.Value(&temp);
		hasAnyLinksFrom = temp;
		h.Value(&temp);
		hasAnyLinksTo = temp;
		h.Value(&MovementControlMethodH);
		h.Value(&MovementControlMethodV);
//		uint8 guardNum = 0;
//		h.Value(&guardNum);
//		numGuards = guardNum;
		h.Value(&cost);
		h.Value(&linkOffset);
		h.Value(&additionalTurnMultiplier);
		h.Value(&prevState);
		for(int i = 0; i < 2*MAX_GUARDS; ++i)
		{
			uint8 input;
			uint16 opCode;
			h.Value(&input);
			h.Value(&opCode);
			guards[i].input = input;
			guards[i].opCode = opCode;
		}
		h.Value(&animDesc.movement.translation);
		h.Value(&animDesc.movement.rotation);
		h.Value(&animDesc.movement.duration);
		h.Value(&animDesc.startLocation.t);
		h.Value(&animDesc.startLocation.q);
	}
}

char CAnimationGraph::RegisterVariationInput( const string& varInputName )
{
	return m_variationInputIDs.insert( MapVariationInputIDs::value_type(varInputName, m_variationInputIDs.size()) ).first->second;
}

const char* CAnimationGraph::RegisterVariationInputs( const char* animationName )
{
	static string temp;
	temp.clear();
	const char* beginning = animationName;
	const char* i = animationName;
	while ( i = strchr(i,'{') )
	{
		const char* j = strchr( i, '}' );
		if ( j == NULL )
			return animationName;

		temp.append( beginning, i++ - beginning );
		temp.append( "%" );

		char id = RegisterVariationInput( string(i, j-i) ) + 128;
		temp += id;

		i = beginning = j+1;
	}
	temp.append(beginning);
	return temp.empty() ? animationName : temp.c_str();
}

int CAnimationGraph::GetVariationInputID( const char* variationInputName ) const
{
	MapVariationInputIDs::const_iterator f = m_variationInputIDs.find( variationInputName );
	if ( f != m_variationInputIDs.end() )
		return f->second;
	return -1;
}


/*
 * CIntegerInputValue
 */

class CAnimationGraph::CIntegerInputValue : public IInputValue
{
public:
	CIntegerInputValue() //this is used to simplify file serialization (a lot)
	{}

	CIntegerInputValue( const string& name, const CRangeMerger& ranges, const CRangeMerger::Range& clampRange ) : IInputValue(eAGIT_Integer, name), m_clampRange(clampRange)
	{
		stl::push_back_range( m_ranges,ranges.begin(), ranges.end() );
	}

	void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->AddContainer(m_ranges);
		IInputValue::GetMemoryStatistics(s);
	}

	virtual void Release() { delete this; }
	virtual CStateIndex::InputValue EncodeInput( const char * str )
	{
		ANIM_PROFILE_FUNCTION;
		int value = 0;
		if (1 != sscanf(str, "%d", &value))
			return CStateIndex::INPUT_VALUE_DONT_CARE;
		else
			return EncodeInput(value);
	}
	virtual CStateIndex::InputValue EncodeInput( float value )
	{
		return EncodeInput(int(value));
	}
	virtual CStateIndex::InputValue EncodeInput( int value )
	{
		ANIM_PROFILE_FUNCTION;
		value = CLAMP(value, m_clampRange.minimum, m_clampRange.maximum);
		// TODO: binary search
		for (size_t i=0; i<m_ranges.size(); ++i)
			if (m_ranges[i].minimum <= value && m_ranges[i].maximum >= value)
				return i;
		return CStateIndex::INPUT_VALUE_DONT_CARE;
	}
	virtual void DecodeInput( char * buf, CStateIndex::InputValue input )
	{
		if (input < m_ranges.size())
			sprintf( buf, "[%d,%d]", m_ranges[input].minimum, m_ranges[input].maximum );
		else
			strcpy( buf, "<<not set>>" );
	}
	virtual void Serialize( TSerialize ser, CStateIndex::InputValue* pInputValue, float* pInputFloat )
	{
		int inputAsInt = int(*pInputFloat);
		ser.Value("value", inputAsInt);
		*pInputFloat = float(inputAsInt);
		*pInputValue = EncodeInput(inputAsInt);
	}
	virtual void DebugText( char * buf, CStateIndex::InputValue encoded, const float * floats )
	{
		if (encoded == CStateIndex::INPUT_VALUE_DONT_CARE)
			strcpy( buf, "<<not set>>" );
		else if (floats)
			sprintf( buf, "%d", (int)floats[m_inputID] );
		else
			buf[0] = 0;
	}
	virtual float AsFloat( const void * basePtr )
	{
		const int * pValue = GetValue<int>(basePtr);
		if (pValue)
			return *pValue;
		else
			return 0.0f;
	}
	virtual void DeclInputs( CStateIndex::Builder * pBuilder, InputID& curID )
	{
		m_inputID = curID ++;
		for (size_t i=0; i<m_ranges.size(); i++)
			pBuilder->DeclInputValue( m_inputID, CStateIndex::InputValue(i), priority );
	}
	virtual bool GetRestrictions( const XmlNodeRef& node, std::vector<CStateIndex::InputValue>& inputValues )
	{
		CRangeMerger::Range range;
		if (node->getAttr("value", range.minimum))
		{
			range.maximum = range.minimum;
		}
		else if (node->getAttr("min", range.minimum) && node->getAttr("max", range.maximum))
		{
			if (range.maximum < range.minimum)
			{
				GameWarning("Maximum must be bigger than minimum (max=%d, min=%d) on '%s'", range.maximum, range.minimum, node->getTag());
				return false;
			}
		}
		else
		{
			//GameWarning("No value or min/max attributes on integer key selection criteria '%s'", node->getTag());
			//return false;
			return true;
		}
		for (std::vector<CRangeMerger::Range>::const_iterator iter = m_ranges.begin(); iter != m_ranges.end(); ++iter)
		{
			if (range.maximum < iter->minimum || range.minimum > iter->maximum)
				continue;
			inputValues.push_back(iter - m_ranges.begin());
		}
		return true;
	}

	virtual void SerializeAsFile( bool reading, AG_FILE* pAGI )
	{
		IInputValue::SerializeAsFile(reading, pAGI);
		FileSerializationHelper h(reading, pAGI);
		int ranges = m_ranges.size();
		h.Value(&ranges);
		if(reading)
		{
			m_ranges.clear();
			m_ranges.resize(ranges);
		}
		for(int i = 0; i < ranges; ++i)
		{
			h.Value(&(m_ranges[i].minimum));
			h.Value(&(m_ranges[i].maximum));
		}
		h.Value(&(m_clampRange.minimum));
		h.Value(&(m_clampRange.maximum));
		h.Value(&(m_inputID));
	}

private:
	//Remember : if you add members, you have to update SerializeAsFile
	std::vector<CRangeMerger::Range> m_ranges;
	CRangeMerger::Range m_clampRange;
	InputID m_inputID;
};

/*
* CFloatInputValue
*/

class CAnimationGraph::CFloatInputValue : public IInputValue
{
public:
	CFloatInputValue() //this is used to simplify file serialization (a lot)
	{}

	CFloatInputValue( const string& name, const CRangeMerger& ranges, const CRangeMerger::Range& clampRange, float precision, float offset ) : IInputValue(eAGIT_Float, name), m_clampRange(clampRange), m_precision(precision), m_offset(offset)
	{
		stl::push_back_range( m_ranges,ranges.begin(), ranges.end() );
	}

	virtual void Release() { delete this; }
	virtual CStateIndex::InputValue EncodeInput( const char * str )
	{
		ANIM_PROFILE_FUNCTION;
		float value = 0;
		if (1 != sscanf(str, "%f", &value))
			return CStateIndex::INPUT_VALUE_DONT_CARE;
		else
			return EncodeInput(value);
	}
	virtual CStateIndex::InputValue EncodeInput( float fvalue )
	{
		ANIM_PROFILE_FUNCTION;
		int value = (int)CLAMP((fvalue + m_offset)/m_precision, m_clampRange.minimum, m_clampRange.maximum);
		// TODO: binary search
		for (size_t i=0; i<m_ranges.size(); ++i)
			if (m_ranges[i].minimum <= value && m_ranges[i].maximum >= value)
				return i;
		return CStateIndex::INPUT_VALUE_DONT_CARE;
	}
	virtual CStateIndex::InputValue EncodeInput( int value )
	{
		return EncodeInput(float(value));
	}
	virtual void DecodeInput( char * buf, CStateIndex::InputValue input )
	{
		if (input < m_ranges.size())
			sprintf( buf, "[%f,%f]", m_ranges[input].minimum*m_precision, m_ranges[input].maximum*m_precision );
		else
			strcpy( buf, "<<not set>>" );
	}
	virtual void Serialize( TSerialize ser, CStateIndex::InputValue* pInputValue, float* pInputFloat )
	{
		ser.Value("value", *pInputFloat);
		*pInputValue = EncodeInput(*pInputFloat);
	}
	virtual void DebugText( char * buf, CStateIndex::InputValue encoded, const float * floats )
	{
		if (floats)
			sprintf( buf, "%f", floats[m_inputID] );
		else
			buf[0] = 0;
	}
	virtual float AsFloat( const void * basePtr )
	{
		const float * pValue = GetValue<float>(basePtr);
		if (pValue)
			return *pValue;
		else
			return 0.0f;
	}
	virtual void DeclInputs( CStateIndex::Builder * pBuilder, InputID& curID )
	{
		m_inputID = curID ++;
		for (size_t i=0; i<m_ranges.size(); i++)
			pBuilder->DeclInputValue( m_inputID, CStateIndex::InputValue(i), priority );
	}
	virtual bool GetRestrictions( const XmlNodeRef& node, std::vector<CStateIndex::InputValue>& inputValues )
	{
		CRangeMerger::Range range;
		float minimum, maximum;
		if (node->getAttr("value", minimum))
		{
			minimum -= m_precision*0.5f;
			maximum = minimum + m_precision;
		}
		else if (node->getAttr("min", minimum) && node->getAttr("max", maximum))
		{
			if (maximum < minimum)
			{
				GameWarning("Maximum must be bigger than minimum (max=%d, min=%d) on '%s'", range.maximum, range.minimum, node->getTag());
				return false;
			}
		}
		else
		{
			//GameWarning("No value or min/max attributes on integer key selection criteria '%s'", node->getTag());
			//return false;
			return true;
		}
		range.minimum = (int)((minimum + m_offset) / m_precision);
		range.maximum = (int)((maximum + m_offset) / m_precision);
		for (std::vector<CRangeMerger::Range>::const_iterator iter = m_ranges.begin(); iter != m_ranges.end(); ++iter)
		{
			if (range.maximum < iter->minimum || range.minimum > iter->maximum)
				continue;
			inputValues.push_back(iter - m_ranges.begin());
		}
		return true;
	}

	virtual void SerializeAsFile( bool reading, AG_FILE* pAGI )
	{
		IInputValue::SerializeAsFile(reading, pAGI);

		FileSerializationHelper h(reading, pAGI);
		int ranges = m_ranges.size();
		h.Value(&ranges);
		if(reading)
		{
			m_ranges.clear();
			m_ranges.resize(ranges);
		}
		for(int i = 0; i < ranges; ++i)
		{
			h.Value(&(m_ranges[i].minimum));
			h.Value(&(m_ranges[i].maximum));
		}
		h.Value(&(m_clampRange.minimum));
		h.Value(&(m_clampRange.maximum));
		h.Value(&(m_precision));
		h.Value(&(m_offset));
		h.Value(&(m_inputID));

	}
	void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->AddContainer(m_ranges);
		IInputValue::GetMemoryStatistics(s);
	}

private:
	//Remember : if you add members, you have to update SerializeAsFile
	std::vector<CRangeMerger::Range> m_ranges;
	CRangeMerger::Range m_clampRange;
	float m_precision;
	float m_offset;
	InputID m_inputID;
};

/*
* CKeyedInputValue
*/
class CAnimationGraph::CKeyedInputValue : public IInputValue
{
public:
	CKeyedInputValue()  //this is used to simplify file serialization (a lot)
	{}

	CKeyedInputValue( const string& name, const std::set<string>& values ) : IInputValue(eAGIT_String, name)
	{
		InputID id = 0;
		for (std::set<string>::const_iterator iter = values.begin(); iter != values.end(); ++iter)
			m_values[*iter] = id++;
	}

	virtual void Release() { delete this; }
	virtual void EncodeInputs( CStateIndex::InputValue* input, const void * basePtr )
	{
		ANIM_PROFILE_FUNCTION;
		input[m_inputID] = CStateIndex::INPUT_VALUE_DONT_CARE;
		const char * const*pValue = GetValue<const char *>(basePtr);
		if (pValue)
		{
			const char * value = *pValue;
			assert(value);
			ValueMap::const_iterator iter = m_values.find(CONST_TEMP_STRING(value));
			if (iter != m_values.end())
				input[m_inputID] = iter->second;
		}
	}
	virtual CStateIndex::InputValue EncodeInput( const char * value )
	{
		assert(value);
		ValueMap::const_iterator iter = m_values.find(CONST_TEMP_STRING(value));
		if (iter != m_values.end())
			return iter->second;
		else
			return CStateIndex::INPUT_VALUE_DONT_CARE;
	}
	virtual CStateIndex::InputValue EncodeInput( float fvalue )
	{
		char buf[32];
		sprintf(buf, "%f", fvalue);
		return EncodeInput(buf);
	}
	virtual CStateIndex::InputValue EncodeInput( int value )
	{
		char buf[32];
		sprintf(buf, "%d", value);
		return EncodeInput(buf);
	}
	virtual void Serialize( TSerialize ser, CStateIndex::InputValue* pInputValue, float* pInputFloat )
	{
		if (ser.IsReading())
		{
			string value;
			ser.Value("value", value);

			*pInputFloat = 0.0f;
			*pInputValue = EncodeInput( value.c_str() );
		}
		else
		{
			char buf[256];
			DecodeInput( buf, *pInputValue );
			string value = buf;
			ser.Value("value", value);
		}
	}
	virtual void DebugText( char * buf, CStateIndex::InputValue encoded, const float * floats )
	{
		DecodeInput( buf, encoded );
	}
	virtual void DebugText( char * buf, const void * basePtr )
	{
		const char * const*pValue = GetValue<const char *>(basePtr);
		if (pValue)
			strcpy(buf, *pValue);
		else
			strcpy(buf, "<<not set>>");
	}
	virtual void DecodeInput( char * buf, CStateIndex::InputValue input )
	{
		for (ValueMap::const_iterator iter = m_values.begin(); iter != m_values.end(); ++iter)
		{
			if (iter->second == input)
			{
				strcpy( buf, iter->first.c_str() );
				return;
			}
		}
		strcpy( buf, "<<not set>>" );
	}
	virtual float AsFloat( const void * )
	{
		return 0.0f;
	}
	virtual void DeclInputs( CStateIndex::Builder* pBuilder, InputID& curID )
	{
		m_inputID = curID ++;
		for (ValueMap::const_iterator iter = m_values.begin(); iter != m_values.end(); ++iter)
			pBuilder->DeclInputValue( m_inputID, iter->second, priority );
	}
	virtual bool GetRestrictions( const XmlNodeRef& node, std::vector<CStateIndex::InputValue>& inputValues )
	{
		if (!node->haveAttr("value"))
			return true;
		const char * value = node->getAttr("value");
		if (!value[0])
			return true;
		ValueMap::const_iterator iter = m_values.find(CONST_TEMP_STRING(value));
		if (iter == m_values.end())
		{
			GameWarning("Invalid key value '%s' for '%s'", value, node->getTag());
			// this is non-fatal... ish
			return true;
		}
		inputValues.push_back(iter->second);
		return true;
	}

	virtual void SerializeAsFile( bool reading, AG_FILE* pAGI )
	{
		IInputValue::SerializeAsFile(reading, pAGI);
		if(!reading)
		{
			ICryPak *pPack = gEnv->pCryPak;
			int values = m_values.size();
			pPack->FWrite((void*)&values, 4, 1, pAGI->file);
			ValueMap::iterator it = m_values.begin();
			for(;it != m_values.end();++it)
			{
				int nameLen = 1 + it->first.length();
				pPack->FWrite((void*)&nameLen, 4, 1, pAGI->file);
				pPack->FWrite((void*)it->first.c_str(), 1, nameLen, pAGI->file);
				pPack->FWrite((void*)&(it->second), 1, 1, pAGI->file);
			}
			pPack->FWrite((void*)&m_inputID, 1, 1, pAGI->file);
		}
		else
		{
			FileSerializationHelper h(reading, pAGI);
			int values = 0;
			h.Value(&values);
			m_values.clear();
			m_values.reserve(values);
			for(int i = 0; i < values; ++i)
			{
				int nameLen = 0;
				h.Value(&nameLen);
				char* tempName = new char[nameLen];
				h.Value(tempName, nameLen);
				assert(string(tempName).length() == nameLen-1);
				InputID id = 0;
				h.Value(&id);
				m_values[tempName] = id;
				delete[] tempName;
			}
			h.Value(&m_inputID);
		}
	}
	void GetMemoryStatistics(ICrySizer * s)
	{
		s->Add(*this);
		s->AddContainer(m_values);
		for (ValueMap::iterator iter = m_values.begin(); iter != m_values.end(); ++iter)
			s->Add(iter->first);
		IInputValue::GetMemoryStatistics(s);
	}

private:
	//Remember : if you add members, you have to update SerializeAsFile
	typedef VectorMap<string, InputID> ValueMap;
	ValueMap m_values;
	InputID m_inputID;
};

/*
 * CStateLoader
 */

class CAnimationGraph::CStateLoader
{
public:
	CStateLoader();
	bool Init( const XmlNodeRef& root );

	size_t GetNumberOfStates() const { return m_loadOrder.size(); }
	const CCryName& GetLoadOrderState( size_t i ) const { return m_loadOrder[i]; }
	XmlNodeRef GetNodeRef( const CCryName& id ) const { return stl::find_in_map(m_stateMap, id, XmlNodeRef()); }
	XmlNodeRef GetNodeRef( size_t i ) const { return GetNodeRef( GetLoadOrderState(i) ); }

	bool AllowInGame( const CCryName& state ) const { return m_notInGame.find(state) == m_notInGame.end(); }

private:
	std::map<CCryName, XmlNodeRef> m_stateMap;
	std::vector<CCryName> m_loadOrder;
	std::set<CCryName> m_notInGame;
};

CAnimationGraph::CStateLoader::CStateLoader()
{
}

struct LoadOrderTuple
{
	LoadOrderTuple( CCryName s, CCryName e) : state(s), extend(e) {}
	CCryName state;
	CCryName extend;
};

bool CAnimationGraph::CStateLoader::Init( const XmlNodeRef& root )
{
	XmlNodeRef states = root->findChild("States");
	if (!states)
	{
		GameWarning("No states");
		return false;
	}

	bool succeeded = true;

	std::set<CCryName> alreadyInLoadOrder;
	std::queue< LoadOrderTuple > waitingForLoadOrder;

	int numStates = states->getChildCount();
	for (int i=0; i<numStates; i++)
	{
		XmlNodeRef state = states->getChild(i);

		CCryName id = state->getAttr("id");
		if (!id.c_str()[0])
		{
			GameWarning("State without an id");
			succeeded = false;
			continue;
		}

		bool include = true;
		if (state->getAttr("includeInGame", include) && !include)
		{
			m_notInGame.insert( id );
			continue;
		}

		m_stateMap[id] = state;

		if (state->haveAttr("extend"))
		{
			CCryName extend = state->getAttr("extend");
			waitingForLoadOrder.push( LoadOrderTuple(id, extend) );
		}
		else
		{
			alreadyInLoadOrder.insert( id );
			m_loadOrder.push_back( id );
		}
	}

	int nSkips = 0;
	while (!waitingForLoadOrder.empty() && nSkips < waitingForLoadOrder.size())
	{
		LoadOrderTuple current = waitingForLoadOrder.front();
		waitingForLoadOrder.pop();
		if (!AllowInGame(current.extend))
		{
			CryLogAlways( "Skipping state '%s' because parent state '%s' is not allowed in game", current.state.c_str(), current.extend.c_str() );
			m_notInGame.insert( current.state );
			nSkips = 0;
			continue;
		}
		bool okToAdd = true;
		if (!current.extend.empty())
			okToAdd &= alreadyInLoadOrder.find(current.extend) != alreadyInLoadOrder.end();

		if (okToAdd)
		{
			alreadyInLoadOrder.insert( current.state );
			m_loadOrder.push_back( current.state );
			nSkips = 0;
		}
		else
		{
			waitingForLoadOrder.push( current );
			nSkips ++;
		}
	}
	if (nSkips)
	{
		GameWarning("Couldn't resolve complete load order for graph");
		while (!waitingForLoadOrder.empty())
		{
			GameWarning("  %s failed to load due to state ordering (needed '%s')", waitingForLoadOrder.front().state.c_str(), waitingForLoadOrder.front().extend.c_str());
			waitingForLoadOrder.pop();
		}
		succeeded = false;
	}

	return succeeded;
}


namespace
{
	class CCompareStateNodeFactories
	{
	public:
		bool operator()( IAnimationStateNodeFactory * pA, IAnimationStateNodeFactory * pB ) const
		{
			int cCat = strcmp(pA->GetCategory(), pB->GetCategory());
			if (cCat < 0)
				return true;
			else if (cCat > 0)
				return false;
			int cName = strcmp(pA->GetName(), pB->GetName());
			if (cName < 0)
				return true;
			else if (cName > 0)
				return false;
			return pA->IsLessThan(pB);
		}
	};

	typedef std::set<IAnimationStateNodeFactory*, CCompareStateNodeFactories> TStateNodeFactorySet;

	IAnimationStateNodeFactory * Uniquify( IAnimationStateNodeFactory * pFactory, TStateNodeFactorySet& allFactories, int& numRemoved )
	{
		TStateNodeFactorySet::iterator iter = allFactories.find(pFactory);
		if (iter == allFactories.end())
		{
			allFactories.insert(pFactory);
			return pFactory;
		}
		else
		{
			numRemoved ++;
			pFactory->Release();
			return *iter;
		}
	}
}


/*
 * CAnimationGraph
 */

CAnimationGraph::CAnimationGraph( CAnimationGraphManager * pManager, string name ) 
: m_serial(100)
, m_pManager(pManager)
, m_numInputIDs(0)
, m_nextOutputHigh(200)
, m_nextOutputLow(300)
, m_name(name)
, m_replaceMeAnimation(INVALID_STATE)
, m_nextBlendValueID(eAGUD_NUM_BUILTINS)
, m_hasAnimDesc(false)
, m_pathFindInitialized(false)
, m_nRefs(0)
{
	m_lastTouched = StateID(-2);

	if (!s_guardSucceedTableInitialized)
	{
		for (int opCodeIndex=0; opCodeIndex<256; opCodeIndex++)
		{
			for (int idx=0; idx<256; idx++)
			{
				uint32 mask = 1u << (idx & 31u);
				if (idx <= opCodeIndex)
					s_guardSucceedTable[GuardOpCode_LessThanEqual0 + opCodeIndex][idx/32] |= mask;
				if (idx >= opCodeIndex)
					s_guardSucceedTable[GuardOpCode_GreaterThanEqual0 + opCodeIndex][idx/32] |= mask;
				if (idx != opCodeIndex)
					s_guardSucceedTable[GuardOpCode_NotEqualTo0 + opCodeIndex][idx/32] |= mask;
				s_guardSucceedTable[GuardOpCode_AlwaysTrue][idx/32] |= mask; // already set up - static var is zero by default (not anymoooore!)
			}
		}
		s_guardSucceedTableInitialized = true;
	}
}

CAnimationGraph::~CAnimationGraph()
{
	std::set<IAnimationStateNodeFactory*> released;

	for (std::vector<IAnimationStateNodeFactory*>::iterator iter = m_factories.begin(); iter != m_factories.end(); ++iter)
	{
		if (released.find(*iter) != released.end())
			continue;
		released.insert(*iter);
		delete *iter;
	}

	while (!m_inputValues.empty())
	{
		m_inputValues.back()->Release();
		m_inputValues.pop_back();
	}
}

bool CAnimationGraph::Init( const XmlNodeRef& root )
{
	std::auto_ptr<CStateIndex::Builder> pStateIndexBuilder(new CStateIndex::Builder());

	if (!PreprocessParameterization(root))
		GameWarning("Failed processing state parameterization");

	// run through any templates and merge them into a single animation graph
	if (!PreprocessTemplates(root))
		GameWarning("Failed processing templates");
	if (!PreprocessLinksAndStates(root))
		GameWarning("Failed processing links and states");

#if defined(USER_dejan) || defined(USER_david)
	// for debugging: write it out again
	char fnm[100];
	static int debugIndex = 0;
	sprintf(fnm, "ag_processed_%d.xml", debugIndex++);
	root->saveToFile(fnm);

	CryLog("Animation graph complete hash is '%s'", GetISystem()->GetXmlUtils()->HashXml(root));
#endif

	// get basic state information, load order
	CStateLoader stateLoader;
	if (!stateLoader.Init(root))
		GameWarning("Animation graph state loader failed");
	// load inputs
	if (!LoadInputs( root, pStateIndexBuilder.get() ))
		GameWarning("Animation graph failed loading inputs");
	// load states
	if (!LoadStates( root, pStateIndexBuilder.get(), stateLoader ))
		GameWarning("Animation graph failed loading states");
	// initialize state index - destroys pStateIndexBuilder
	m_stateIndex.Init( pStateIndexBuilder );
	// load transitions
	if (!LoadTransitions( root, stateLoader ))
		GameWarning("Animation graph failed loading transitions");
	// cache some useful stuff
	if (!CachePass())
		GameWarning("Animation graph failed caching state");
	CalculateStatePositions();

	m_statePathfindState.resize(m_states.size());
	m_pathFindInitialized = false;
	m_lastTouched = StateID(-2);

	return true;
}

void CAnimationGraph::AddRef()
{
	++m_nRefs;
}

void CAnimationGraph::Release()
{
	if (--m_nRefs <= 0)
		delete this;
}

bool CAnimationGraph::IsLastRef()
{
	return m_nRefs == 1;
}

IAnimationGraphState * CAnimationGraph::CreateState()
{
	return new CAnimationGraphState(this);
}

uint8 CAnimationGraph::GetBlendValueID( const char * name )
{
	string sName = name;
	VectorMap<string,uint8>::const_iterator iter = m_blendValueIDs.find(sName);
	if (iter == m_blendValueIDs.end())
	{
		uint8 id = m_nextBlendValueID++;
		if (id >= NUM_ANIMATION_USER_DATA_SLOTS)
		{
			GameWarning("Too many animation user data slots for %s: returning the maximum (things WILL look very wrong)", name);
			id = NUM_ANIMATION_USER_DATA_SLOTS-1;
		}
		iter = m_blendValueIDs.insert( std::make_pair(sName, id) ).first;
	}
	return iter->second;
}

bool CAnimationGraph::SerializeAsFile(const char* fileName, bool reading)
{
	//this function serializes all members of the AnimationGraph to/from a file
	ICryPak  *pPack = gEnv->pCryPak;
	FILE * pAGI = NULL;

	std::map<int, IAnimationStateNodeFactory*> idToFactory;
	std::map<IAnimationStateNodeFactory*, int> factoryToId;
	int nextFactoryId = 0;

	int temp = 0;

	AG_FILE ag_file;
	AG_FILE *pAGFile = &ag_file;

	if(!reading)
	{
		pAGI = pPack->FOpen(fileName, "wb", ICryArchive::FLAGS_CREATE_NEW);
		if(!pAGI)
			return false;
		pAGFile->SetFile( reading,pAGI );

		FileSerializationHelper h(reading, pAGFile);

		h.Value((int*)&AG_VERSION);		//write version number
		int checksum = sizeof(CAnimationGraph);  // sizeof() is not consistent across 32 and 64 bits!
		h.Value(&checksum);

		//serialize state node factories
		for (size_t i = 0; i<m_factories.size(); i++)
		{
			IAnimationStateNodeFactory * pFactory = m_factories[i];
			if (!pFactory)
				continue;
			if (factoryToId.find(pFactory) != factoryToId.end())
				continue;
			int id = nextFactoryId ++;
			idToFactory[id] = pFactory;
			factoryToId[pFactory] = id;
		}
		temp = idToFactory.size();
		h.Value(&temp);
		for (std::map<int, IAnimationStateNodeFactory*>::iterator iter = idToFactory.begin(); iter != idToFactory.end(); ++iter)
		{
			IAnimationStateNodeFactory * pFactory = iter->second;
			string name = pFactory->GetName();
			h.StringValue(&name);
			pFactory->SerializeAsFile( reading, pAGFile );
		}
		temp = m_factories.size();
		h.Value(&temp);
		h.Value( &m_factorySlotIndices[0], m_factorySlotIndices.size() );
		for (int i=0; i<temp; i++)
		{
			int id = factoryToId[m_factories[i]];
			h.Value(&id);
		}

		//serialize input values
		temp = m_inputValues.size();
		h.Value(&temp);
		for(int i = 0; i < temp; ++i)
		{
			h.Value(&(m_inputValues[i]->type));
			m_inputValues[i]->SerializeAsFile(reading, pAGFile);
		}
		temp = m_blendWeightInputValues.size();
		pPack->FWrite(&temp, 1, pAGI);
		for(int i = 0; i < temp; ++i)
		{
			bool set = m_blendWeightInputValues[i]? true:false;
			h.Value(&set);
			if(set)
			{
				h.Value(&(m_blendWeightInputValues[i]->type));
				m_blendWeightInputValues[i]->SerializeAsFile(reading, pAGFile);
			}
		}
		pPack->FWrite(m_blendWeightMoveSpeeds, 9, pAGI);
		pPack->FWrite(&m_numInputIDs, 1, pAGI);

		//serialize variation input IDs
		temp = m_variationInputIDs.size();
		h.Value(&temp);
		for( MapVariationInputIDs::iterator it = m_variationInputIDs.begin(), itEnd = m_variationInputIDs.end(); it != itEnd; ++it )
		{
			h.StringValue(&it->first);
			h.Value(&it->second);
		}

		//serialize states & links
		temp = m_states.size();
		pPack->FWrite(&temp, 1, pAGI);
		for(int i = 0; i < temp; ++i)
			m_states[i].SerializeAsFile(reading, pAGFile, m_pManager, idToFactory, factoryToId);
		temp = m_links.size();
		pPack->FWrite(&temp, 1, pAGI);
		for(int i = 0; i < temp; ++i)
		{
			pPack->FWrite(&(m_links[i].from), 1, pAGI);
			pPack->FWrite(&(m_links[i].to), 1, pAGI);
			pPack->FWrite(&(m_links[i].forceFollowChance), 1, pAGI);
			pPack->FWrite(&(m_links[i].cost), 1, pAGI);
			pPack->FWrite(&(m_links[i].overrideTransitionTime), 1, pAGI);
		}
		temp = m_stateNameToID.size();
		pPack->FWrite(&temp, 1, pAGI);
		if(temp)
		{
			VectorMap<CCryName, StateID>::iterator it = m_stateNameToID.begin();
			for(; it != m_stateNameToID.end(); ++it)
			{
				temp = 1 + it->first.length();
				pPack->FWrite(&temp, 1, pAGI);
				pPack->FWrite(it->first.c_str(), temp, pAGI);
				pPack->FWrite(&(it->second), 1, pAGI);
			}
		}

		//stateIndex class
		m_stateIndex.SerializeAsFile(reading, pAGFile);

		pPack->FWrite(&m_replaceMeAnimation, 1, pAGI);
		pPack->FWrite(&m_hasAnimDesc, 1, pAGI);

		//serializing outputs
		temp = m_stringToOutputHigh.size();
		h.Value(&temp);
		if(temp)
		{
			VectorMap<string, int>::iterator it = m_stringToOutputHigh.begin();
			for(; it != m_stringToOutputHigh.end(); ++it)
			{
				h.StringValue((string *)(&(it->first)));
				h.Value(&(it->second));
			}
		}

		temp = m_stringToOutputLow.size();
		h.Value(&temp);
		if(temp)
		{
			VectorMap<string, int>::iterator it = m_stringToOutputLow.begin();
			for(; it != m_stringToOutputLow.end(); ++it)
			{
				h.StringValue((string *)(&(it->first)));
				h.Value(&(it->second));
			}
		}

		h.Value(&m_nextOutputHigh);
		h.Value(&m_nextOutputLow);

		temp = m_outputs.size();
		h.Value(&temp);
		if(temp)
		{
			VectorMap< int, std::pair<string,string> >::iterator it = m_outputs.begin();
			for(; it != m_outputs.end(); ++it)
			{
				h.Value((int*)&(it->first));
				h.StringValue((string *)&(it->second.first));
				h.StringValue((string *)&(it->second.second));
			}
		}

		h.Value(&m_nextBlendValueID);

		temp = m_blendValueIDs.size();
		h.Value(&temp);
		if(temp)
		{
			VectorMap<string, uint8>::iterator it = m_blendValueIDs.begin();
			for(; it != m_blendValueIDs.end(); ++it)
			{
				h.StringValue((string*)&(it->first));
				h.Value((uint8*)&(it->second));
			}
		}
	}
	else
	{
		pAGI = pPack->FOpen(fileName, "rb", 0);
		if(!pAGI)
			return false;
		pAGFile->SetFile( reading,pAGI );

		FileSerializationHelper h(reading, pAGFile);

		int version = 0;
		h.Value(&version);
		int checksum = 0;
		h.Value(&checksum);
		if(version != AG_VERSION)
		{
			GameWarning("Tried to read animation graph with different version! Filename=%s Current=%d File=%d", fileName, AG_VERSION, version);
			pPack->FClose(pAGI);
			return false;
		}

		// load state node factories
		h.Value(&temp);
		for (int id=0; id<temp; id++)
		{
			string name;
			h.StringValue(&name);
			IAnimationStateNodeFactory *pFactory = m_pManager->CreateStateFactory(name);
			if (!pFactory)
			{
				GameWarning("Unable to create factory %s; load aborted", name.c_str());
				pPack->FClose(pAGI);
				return false;
			}
			pFactory->SerializeAsFile( reading, pAGFile );
			idToFactory[id] = pFactory;
			factoryToId[pFactory] = id;
		}
		h.Value(&temp);
		m_factorySlotIndices.resize(temp);
		m_factories.resize(0);
		m_factories.reserve(temp);
		h.Value(&m_factorySlotIndices[0], m_factorySlotIndices.size());
		for (int i=0; i<temp; i++)
		{
			int id;
			h.Value(&id);
			m_factories.push_back(idToFactory[id]);
		}

		//load input values
		h.Value(&temp);	//read inputValues vector length
		m_inputValues.clear();
		m_inputValues.resize(temp);
		for(int i = 0; i < temp; ++i)
		{
			int iType = 0;
			h.Value(&iType);
			IInputValue* tempVal;
			if(iType == eAGIT_Float)
				tempVal = (IInputValue*)(new CFloatInputValue);
			else if(iType == eAGIT_Integer)
				tempVal = (IInputValue*)(new CIntegerInputValue);
			else if(iType == eAGIT_String)
				tempVal = (IInputValue*)(new CKeyedInputValue);
			else {GameWarning("AG File Serialization Failed!"); return false;}
			m_inputValues[i] = tempVal;
			m_inputValues[i]->SerializeAsFile(reading, pAGFile);
		}
		h.Value(&temp);
		m_blendWeightInputValues.clear();
		m_blendWeightInputValues.resize(temp);
		for(int i = 0; i < temp; ++i)
		{
			bool set = false;
			h.Value(&set);
			if(set)
			{
				int iType = 0;
				h.Value(&iType);
				IInputValue* tempVal;
				if(iType == eAGIT_Float)
					tempVal = (IInputValue*)(new CFloatInputValue);
				else if(iType == eAGIT_Integer)
					tempVal = (IInputValue*)(new CIntegerInputValue);
				else if(iType == eAGIT_String)
					tempVal = (IInputValue*)(new CKeyedInputValue);
				else {GameWarning("AG File Serialization Failed!"); return false;}
				m_blendWeightInputValues[i] = tempVal;
				m_blendWeightInputValues[i]->SerializeAsFile(reading, pAGFile);
			}
			else
				m_blendWeightInputValues[i] = NULL;
		}
		h.Value(m_blendWeightMoveSpeeds, 9);
		h.Value(&m_numInputIDs);

		//serialize variation input IDs
		m_variationInputIDs.clear();
		h.Value(&temp);
		for( int i = 0; i < temp; ++i )
		{
			string name;
			h.StringValue(&name);
			char id;
			h.Value(&id);
			m_variationInputIDs[name] = id;
		}

		//load states & links
		h.Value(&temp); //number of states
		m_states.clear();
		m_states.resize(temp);
		for(int i = 0; i < temp; ++i)
			m_states[i].SerializeAsFile(reading, pAGFile, m_pManager, idToFactory, factoryToId);

		h.Value(&temp); //number of links
		m_links.clear();
		m_links.resize(temp);
		for(int i = 0; i < temp; ++i)
		{
			h.Value(&m_links[i].from);
			h.Value(&m_links[i].to);
			h.Value(&m_links[i].forceFollowChance);
			h.Value(&m_links[i].cost);
			h.Value(&m_links[i].overrideTransitionTime);
		}

		h.Value(&temp); //stateToNameID count
		m_stateNameToID.clear();
		for(int i = 0; i < temp; ++i)
		{
			int nameLen = 0;
			h.Value(&nameLen);
			char *tempName = new char[nameLen];
			h.Value(tempName, nameLen);
			StateID id = 0;
			h.Value(&id);
			m_stateNameToID[tempName] = id;
			delete[] tempName;
		}

		//stateIndex class
		m_stateIndex.SerializeAsFile(reading, pAGFile);

		h.Value(&m_replaceMeAnimation);
		h.Value(&m_hasAnimDesc);

		h.Value(&temp);	//stringToOutputHigh Num
		m_stringToOutputHigh.clear();
		for(int i = 0; i < temp; ++i)
		{
			int strLen = 0;
			int secondVal = 0;
			h.Value(&strLen);
			char* tempStr = new char[strLen];
			h.Value(tempStr, strLen);
			h.Value(&secondVal);
			m_stringToOutputHigh[tempStr] = secondVal;
			delete[] tempStr;
		}

		h.Value(&temp);	//stringToOutputLow Num
		m_stringToOutputLow.clear();
		for(int i = 0; i < temp; ++i)
		{
			int strLen = 0;
			int secondVal = 0;
			h.Value(&strLen);
			char* tempStr = new char[strLen];
			h.Value(tempStr, strLen);
			h.Value(&secondVal);
			m_stringToOutputLow[tempStr] = secondVal;
			delete[] tempStr;
		}

		h.Value(&m_nextOutputHigh);
		h.Value(&m_nextOutputLow);

		h.Value(&temp);	//num of outputs
		m_outputs.clear();
		for(int i = 0; i < temp; ++i)
		{
			int firstVal = 0;
			h.Value(&firstVal);
			std::pair<string, string> secondVal;
			h.StringValue((string *)(&secondVal.first));
			h.StringValue((string *)(&secondVal.second));
			m_outputs[firstVal] = secondVal;
		}

		h.Value(&m_nextBlendValueID);

		h.Value(&temp);
		m_blendValueIDs.clear();
		for(int i = 0; i < temp; ++i)
		{
			int strLen = 0;
			h.Value(&strLen);
			char* tempStr = new char[strLen];
			h.Value(tempStr, strLen);
			uint8 secondVal = 0;
			h.Value(&secondVal);
			m_blendValueIDs[tempStr] = secondVal;
			delete []tempStr;
		}

		m_statePathfindState.resize(m_states.size());
	}

	pPack->FClose(pAGI);
	return true;
}

void CAnimationGraph::SwapAndIncrementVersion( _smart_ptr<CAnimationGraph> pGraph )
{
	std::swap( m_pManager, pGraph->m_pManager );
	m_inputValues.swap( pGraph->m_inputValues );
	m_blendWeightInputValues.swap( pGraph->m_blendWeightInputValues );
	//std::swap(m_blendWeightMoveSpeeds, pGraph->m_blendWeightMoveSpeeds);
	for (int i=0; i<sizeof(m_blendWeightMoveSpeeds)/sizeof(float); i++)
		std::swap(m_blendWeightMoveSpeeds[i], pGraph->m_blendWeightMoveSpeeds[i]);
	std::swap( m_numInputIDs, pGraph->m_numInputIDs );
	m_states.swap( pGraph->m_states );
	m_links.swap( pGraph->m_links );
	m_stateNameToID.swap( pGraph->m_stateNameToID );
	m_stateIndex.Swap( pGraph->m_stateIndex );
	m_statePathfindState.swap( pGraph->m_statePathfindState );
	std::swap( m_nextOutputHigh, pGraph->m_nextOutputHigh );
	std::swap( m_nextOutputLow, pGraph->m_nextOutputLow );
	m_stringToOutputHigh.swap(pGraph->m_stringToOutputHigh);
	m_stringToOutputLow.swap(pGraph->m_stringToOutputLow);
	m_outputs.swap(pGraph->m_outputs);
	m_name.swap( pGraph->m_name );
	std::swap( m_replaceMeAnimation, pGraph->m_replaceMeAnimation );
	std::swap( m_hasAnimDesc, pGraph->m_hasAnimDesc );
	m_factories.swap(pGraph->m_factories);
	m_factorySlotIndices.swap(pGraph->m_factorySlotIndices);
	m_variationInputIDs.swap(pGraph->m_variationInputIDs);

	m_serial ++;
}

bool CAnimationGraph::LoadInputs( const XmlNodeRef& root, CStateIndex::Builder * pStateIndexBuilder )
{
	PROFILE_LOADING_FUNCTION;

	XmlNodeRef inputs = root->findChild( "Inputs" );
	XmlNodeRef states = root->findChild( "States" );
	if (!inputs || !states)
		return false;

	bool succeeded = true;
	for (int i=0; i<sizeof(m_blendWeightMoveSpeeds)/sizeof(float); i++)
		m_blendWeightMoveSpeeds[i] = 0;

	int numChildren = inputs->getChildCount();
	for (int i=0; i<numChildren; i++)
	{
		// get node
		XmlNodeRef inputNode = inputs->getChild(i);
		// get the name of the node
		const char * name = inputNode->getAttr("name");
		if (!name[0])
		{
			GameWarning("No name for input %d", i);
			succeeded = false;
			continue;
		}
//		CryLog("Processing input: %s", name);
		if (FindInputValue(name))
		{
			GameWarning("Duplicate input value %s", name);
			succeeded = false;
			continue;
		}
		// figure out the type and build the support logic
		const char * nodeType = inputNode->getTag();
		IInputValue * pInputValue = NULL;
		if (0 == strcmp("KeyState", nodeType))
		{
			std::set<string> values;
			int numValues = inputNode->getChildCount();
			for (int v=0; v<numValues; v++)
			{
				XmlNodeRef valueNode = inputNode->getChild(v);
				if (0 != strcmp(valueNode->getTag(), "Key"))
				{
					GameWarning("Non-key value found on Key node (tag was %s, parsing animation data)", valueNode->getTag());
					succeeded = false;
					continue;
				}
				if (valueNode->getNumAttributes() == 0)
					continue;
				if (!valueNode->haveAttr("value"))
				{
					GameWarning("No value attribute on Key node %d", v);
					succeeded = false;
					continue;
				}
				if (!values.insert(valueNode->getAttr("value")).second)
				{
					GameWarning("Duplicate key %s", valueNode->getAttr("value"));
					succeeded = false;
					continue;
				}
			}
			pInputValue = new CKeyedInputValue( name, values );
		}
		else if (0 == strcmp("IntegerState", nodeType))
		{
			CRangeMerger rangeMerger;
			CRangeMerger::Range range;

			// scan all states for selection criteria
			int numStates = states->getChildCount();
			for (int state = 0; state < numStates; state++)
			{
				XmlNodeRef stateNode = states->getChild(state);
				if (!stateNode)
					continue;
				XmlNodeRef selectWhenNode = stateNode->findChild("SelectWhen");
				if (!selectWhenNode)
					continue;
				XmlNodeRef myValueNode = selectWhenNode->findChild(name);
				if (!myValueNode)
					continue;
				if (myValueNode->getNumAttributes() == 0)
					continue;
				else if (myValueNode->getAttr("value", range.minimum))
					range.maximum = range.minimum;
				else if (myValueNode->getAttr("min", range.minimum) && myValueNode->getAttr("max", range.maximum))
					;
				else
				{
					GameWarning("Unable to get criteria for integer state %s in state %s", name, stateNode->getAttr("id"));
					succeeded = false;
					continue;
				}
				rangeMerger.AddRange( range );
			}
			for (int state = 0; state < numStates; state++)
			{
				XmlNodeRef stateNode = states->getChild(state);
				if (!stateNode)
					continue;
				XmlNodeRef guardNode = stateNode->findChild("Guard");
				if (!guardNode)
					continue;
				XmlNodeRef myValueNode = guardNode->findChild(name);
				if (!myValueNode)
					continue;
				if (myValueNode->getAttr("min", range.minimum) && myValueNode->getAttr("max", range.maximum))
					;
				else
				{
					GameWarning("Unable to get guard for integer state %s in state %s", name, stateNode->getAttr("id"));
					succeeded = false;
					continue;
				}
				rangeMerger.AddRange( range );
			}

			// tidy up our input ranges so they fit the system correctly
			if (!inputNode->getAttr("min", range.minimum) || !inputNode->getAttr("max", range.maximum))
			{
				GameWarning("No min/max on IntegerState %s", name);
				succeeded = false;
				continue;
			}

			if ( !rangeMerger.empty() )
			{
				rangeMerger.ClipToRange( range );
				if ( rangeMerger.empty() )
					GameWarning( "All values out of range for input %s. Input discarded!", name );
				rangeMerger.FillGaps( range );
			}

			pInputValue = new CIntegerInputValue(name, rangeMerger, range);
		}
		else if (0 == strcmp("FloatState", nodeType))
		{
			CRangeMerger rangeMerger;
			CRangeMerger::Range range;
			float minimum, maximum;

			float precision = 1.0f / 1024.0f;
			inputNode->getAttr("precision", precision);

			if (!inputNode->getAttr("min", minimum) || !inputNode->getAttr("max", maximum))
			{
				GameWarning("No min/max on FloatState %s", name);
				succeeded = false;
				continue;
			}
			float offset = -minimum;

			int numStates = states->getChildCount();
			for (int state = 0; state < numStates; state++)
			{
				XmlNodeRef stateNode = states->getChild(state);
				if (!stateNode)
					continue;
				XmlNodeRef selectWhenNode = stateNode->findChild("SelectWhen");
				if (!selectWhenNode)
					continue;
				XmlNodeRef myValueNode = selectWhenNode->findChild(name);
				if (!myValueNode)
					continue;
				if (myValueNode->getNumAttributes() == 0)
					continue;
				else if (myValueNode->getAttr("value", minimum))
				{
					minimum -= precision * 0.5f;
					maximum = minimum + precision;
				}
				else if (myValueNode->getAttr("min", minimum) && myValueNode->getAttr("max", maximum))
					;
				else
				{
					GameWarning("Unable to get criteria for float state %s in state %s", name, stateNode->getAttr("id"));
					succeeded = false;
					continue;
				}
				if (minimum > maximum)
					std::swap(minimum, maximum);
				range.minimum = int((minimum+offset)/precision);
				range.maximum = int((maximum+offset)/precision);
				rangeMerger.AddRange( range );
			}
			for (int state = 0; state < numStates; state++)
			{
				XmlNodeRef stateNode = states->getChild(state);
				if (!stateNode)
					continue;
				XmlNodeRef guardNode = stateNode->findChild("Guard");
				if (!guardNode)
					continue;
				XmlNodeRef myValueNode = guardNode->findChild(name);
				if (!myValueNode)
					continue;
				if (myValueNode->getAttr("min", minimum) && myValueNode->getAttr("max", maximum))
					;
				else
				{
					GameWarning("Unable to get guard for float state %s in state %s", name, stateNode->getAttr("id"));
					succeeded = false;
					continue;
				}
				if (minimum > maximum)
					std::swap(minimum, maximum);
				range.minimum = int((minimum+offset)/precision);
				range.maximum = int((maximum+offset)/precision);
				rangeMerger.AddRange( range );
			}

			// tidy up our input ranges so they fit the system correctly
			if (!inputNode->getAttr("min", minimum) || !inputNode->getAttr("max", maximum))
			{
				GameWarning("No min/max on FloatState %s", name);
				succeeded = false;
				continue;
			}

			range.minimum = int((minimum+offset)/precision);
			range.maximum = int((maximum+offset)/precision);
			if ( !rangeMerger.empty() )
			{
				rangeMerger.ClipToRange( range );
				if ( rangeMerger.empty() )
					GameWarning( "All values out of range for input %s. Input discarded!", name );
				rangeMerger.FillGaps( range );
			}

			pInputValue = new CFloatInputValue(name, rangeMerger, range, precision, offset);
		}
		else
		{
			// should we give a warning here? - we give one momentarily anyway
			GameWarning("Unknown input type");
			succeeded = false;
		}

		if (!pInputValue)
		{
			GameWarning("Input value %s failed to create a mapping", name);
			succeeded = false;
			continue;
		}
		else
		{
			inputNode->getAttr( "signalled", pInputValue->signalled );
			if (inputNode->haveAttr("defaultValue"))
				pInputValue->defaultValue = pInputValue->EncodeInput( inputNode->getAttr("defaultValue") );
			int priority;
			if (inputNode->getAttr("priority", priority))
				priority = CLAMP(priority, 0, 255);
			else
				priority = 1;
			pInputValue->priority = (uint8) priority;
			pInputValue->id = m_inputValues.size();
			pInputValue->DeclInputs( pStateIndexBuilder, m_numInputIDs );
			m_inputValues.push_back(pInputValue);

			// map blend weights
			string attachToBlendWeight = inputNode->getAttr("attachToBlendWeight");
			int curPos = 0;
			string weightVal = attachToBlendWeight.Tokenize(",",curPos);
			while (!weightVal.empty())
			{
				int weight = -1;
				if (sscanf(weightVal.c_str(), "%d", &weight))
				{
					if (weight >= 0)
					{
						if (weight >= m_blendWeightInputValues.size())
							m_blendWeightInputValues.resize(weight+1, NULL);
						m_blendWeightInputValues[weight] = pInputValue;

						if (weight < 4)
							inputNode->getAttr("blendWeightMoveSpeed", m_blendWeightMoveSpeeds[weight]);
					}
				}
				weightVal = attachToBlendWeight.Tokenize(",",curPos);
			}
		}
	}

	return succeeded;
}

bool CAnimationGraph::LoadStates( const XmlNodeRef& root, CStateIndex::Builder * pStateIndexBuilder, CStateLoader& stateLoader )
{
	PROFILE_LOADING_FUNCTION;

	std::vector<CStateIndex::InputValue> inputValueRestrictions;
	std::vector<StateID> selectableStates;

	TStateNodeFactorySet allFactories;
	int numRemoved = 0;

	bool succeeded = true;

	std::map<int, IAnimationStateNodeFactory*> stateFactories;

	for (size_t i=0; i<stateLoader.GetNumberOfStates(); i++)
	{
		stateFactories.clear();

		// grab the node
		XmlNodeRef node = stateLoader.GetNodeRef(i);
		// add to the collection
		CCryName id = node->getAttr("id");
		StateID myID = m_stateNameToID[id] = m_states.size();
		m_states.push_back(SStateInfo());
		// and just use a reference for load performance
		SStateInfo& info = m_states.back();
		info.id = id;
		info.factoryLength = 0;
		info.factoryStart = m_factories.size();
		info.linkOffset = 0;
		info.hasForceFollows = false;
		int costFromXml = 100;
		node->getAttr("cost", costFromXml);
		info.cost = CLAMP(costFromXml, 1, 65536) - 1;
		//info.numGuards = 0;
		info.matchesSignalledInputs = false;
		info.evaluateGuardsOnExit = true;
		info.hasAnyLinksFrom = false;
		info.hasAnyLinksTo = false;

		bool flag;
		info.hurryable = flag = false;
		if (node->getAttr("hurryable", flag))
			info.hurryable = flag;

		info.noPhysicalCollider = flag = false;
		if (node->getAttr("noCollider", flag))
			info.noPhysicalCollider = flag;
		else if (node->getAttr("noCylinder", flag))
			info.noPhysicalCollider = flag;

		info.skipFirstPerson = flag = false;
		if (node->getAttr("skipFirstPerson", flag))
			info.skipFirstPerson = flag;

		info.MovementControlMethodH = eMCM_Entity;
		if (node->haveAttr("MovementControlMethodH"))
		{
			int temp = eMCM_Entity;
			node->getAttr("MovementControlMethodH", temp);
			info.MovementControlMethodH = temp;
		}

		info.MovementControlMethodV = eMCM_Entity;
		if (node->haveAttr("MovementControlMethodV"))
		{
			int temp = eMCM_Entity;
			node->getAttr("MovementControlMethodV", temp);
			info.MovementControlMethodV = temp;
		}

		info.animationControlledView = false;
		if (node->haveAttr("animationControlledView"))
		{
			bool temp = false;
			node->getAttr("animationControlledView", temp);
			info.animationControlledView = temp;
		}

		info.additionalTurnMultiplier = 3.0f;
		if (node->haveAttr("additionalTurnMultiplier"))
			node->getAttr("additionalTurnMultiplier", info.additionalTurnMultiplier);

		if (id == "_replaceme")
			m_replaceMeAnimation = myID;

		// setup the hierarchy
		if (node->haveAttr("extend"))
		{
			// CStateLoader guarantees we can do this without having to check if find goes off the end
			info.parentState = m_stateNameToID.find(node->getAttr("extend"))->second;
		}
		else
		{
			info.parentState = INVALID_STATE;
		}

		bool allowSelect = true;
		node->getAttr("allowSelect", allowSelect);
		info.allowSelect = allowSelect;

		bool canMix = true;
		node->getAttr("canMix", canMix);
		info.canMix = canMix;

		// now fill in the child details
		int numChildren = node->getChildCount();
		for (int c=0; c<numChildren; c++)
		{
			XmlNodeRef childNode = node->getChild(c);
			const char * childNodeTag = childNode->getTag();
			if (0 == strcmp(childNodeTag, "SelectWhen"))
				; // SelectWhen has special handling below
			else if (0 == strcmp(childNodeTag, "Param"))
				; // template parameters are handled specially elsewhere
			else if (0 == strcmp(childNodeTag, "MixIn"))
				; // mixins get merged during preprocessing
			else if (0 == strcmp(childNodeTag, "Guard"))
			{
				int numGuards = childNode->getChildCount();
				if (numGuards > MAX_GUARDS)
				{
					GameWarning("Too many guard nodes -- none loaded");
					succeeded = false;
				}
				else
				{
					for (int g=0; g<numGuards; g++)
					{
						XmlNodeRef guard = childNode->getChild(g);
						IInputValue * pInputValue = FindInputValue(guard->getTag());
						if (!pInputValue)
						{
							GameWarning("Invalid guard '%s'", guard->getTag());
							GameWarning("Error loading state '%s'", info.id.c_str());
							succeeded = false;
							break;
						}
						const char * minValue = guard->getAttr("min");
						const char * maxValue = guard->getAttr("max");
						SGuard* ginfo = &info.guards[2*g];
						int minValueEnc = pInputValue->EncodeInput( minValue );
						int maxValueEnc = pInputValue->EncodeInput( maxValue );
						if (minValueEnc == CStateIndex::INPUT_VALUE_DONT_CARE || maxValueEnc == CStateIndex::INPUT_VALUE_DONT_CARE || minValueEnc > maxValueEnc)
						{
							GameWarning("Invalid min/max for guard '%s' ('%s', '%s')", guard->getTag(), minValue, maxValue);
							GameWarning("Error loading state '%s'", info.id.c_str());
							succeeded = false;
							break;
						}
						ginfo[0].input = pInputValue->id;
						ginfo[0].opCode = GuardOpCode_GreaterThanEqual0 + minValueEnc;
						ginfo[1].input = pInputValue->id;
						ginfo[1].opCode = GuardOpCode_LessThanEqual0 + maxValueEnc;
					}
				}
			}
			else
			{
				IAnimationStateNodeFactory * pStateNodeFactory = 0;
				const CAnimationGraphManager::SCategoryInfo * catInfo;
				int insertSlot = -1;
				if (!(pStateNodeFactory = m_pManager->CreateStateFactory( childNodeTag )))
					GameWarning("No such animation state configuration %s (in state %s)", childNodeTag, id.c_str());
				else if (!(catInfo = m_pManager->GetCategory( pStateNodeFactory->GetCategory() )))
				{
					GameWarning("No such category %s", pStateNodeFactory->GetCategory());
				}
				else if (!pStateNodeFactory->Init(childNode, this))
				{
					GameWarning("Animation state configuration %s failed to initialize (in state %s)", childNodeTag, id.c_str());
				}
				else if (catInfo->overrideSlot >= 0)
				{
					insertSlot = catInfo->overrideSlot;
//					info.stateNodeFactories[catInfo->overrideSlot] = Uniquify( pStateNodeFactory, allFactories, numRemoved );
				}
				else
				{
					insertSlot = m_pManager->GetOverrideSlotCount();

					//marcok: check back with Craig if this is correct once he's back
					for (std::map<int, IAnimationStateNodeFactory*>::iterator i=stateFactories.begin(); i!=stateFactories.end(); ++i)
					{
						insertSlot = std::max( insertSlot, i->first );
					}
					if (!stateFactories.empty())
						insertSlot++;
				}
				if (insertSlot >= 0)
				{
					assert(pStateNodeFactory);
					stateFactories[insertSlot] = Uniquify(pStateNodeFactory, allFactories, numRemoved);
				}
				else
				{
					if (pStateNodeFactory != 0)
						pStateNodeFactory->Release();
					pStateNodeFactory = 0;
					succeeded = false;
				}
			}
		}

		// populate the factory database
		for ( std::map<int, IAnimationStateNodeFactory*>::iterator iter = stateFactories.begin(); iter != stateFactories.end(); ++iter )
		{
			m_factorySlotIndices.push_back(iter->first);
			m_factories.push_back(iter->second);
			info.factoryLength ++;
			assert( info.factoryLength + info.factoryStart == m_factories.size() );
			assert( m_factories.size() == m_factorySlotIndices.size() );
		}

		// and the selection criteria
		if (info.allowSelect)
		{
			pStateIndexBuilder->DeclState( myID );
			selectableStates.push_back(myID);
		}
	}

	CryLog("AnimGraph: %d duplicate state nodes removed", numRemoved);

	// cull duplicate entries from the factory database
	/*
	for (size_t i=1; i<m_states.size(); i++)
	{
	SStateInfo& state = m_states[i];
	if (!state.factoryLength)
	{
	state.factoryStart = 0;
	continue;
	}
	if (state.factoryLength > state.factoryStart)
	continue;
	for (size_t j=0; j<=state.factoryStart-state.factoryLength; j++)
	{
	if (m_factorySlotIndices[j] == m_factorySlotIndices[state.factoryStart] && m_factories[j] == m_factories[state.factoryStart])
	{
	bool match = true;
	for (int k=1; match && k<state.factoryLength; k++)
	if (m_factorySlotIndices[j+k] != m_factorySlotIndices[state.factoryStart+k] || m_factories[j+k] != m_factories[state.factoryStart+k])
	match = false;
	if (match)
	{
	state.factoryStart = j;
	for (int k=state.factoryStart; k<m_factories.size()-state.factoryLength; k++)
	{
	m_factories[k] = m_factories[k+state.factoryLength];
	m_factorySlotIndices[k] = m_factorySlotIndices[k+state.factoryLength];
	}
	for (int k=i+1; k<m_states.size(); k++)
	{
	m_states[k].factoryStart -= state.factoryLength;
	}
	for (int k=0; k<state.factoryLength; k++)
	{
	m_factorySlotIndices.pop_back();
	m_factories.pop_back();
	}
	break;
	}
	}
	}
	}
	*/

	for (std::vector<StateID>::iterator iter = selectableStates.begin(); iter != selectableStates.end(); ++iter)
	{
		StateID myID = *iter;
		SStateInfo& info = m_states[myID];
		StateID stateID = myID;
		std::stack<XmlNodeRef> selectWhen;
		while (stateID != INVALID_STATE /* && m_states[stateID].allowSelect*/)
		{
			if (XmlNodeRef selectWhenNode = stateLoader.GetNodeRef(stateID)->findChild("SelectWhen"))
				selectWhen.push(selectWhenNode);
			stateID = m_states[stateID].parentState;
		}
		std::map<string, XmlNodeRef> criteriaMap;
		while (!selectWhen.empty())
		{
			int numRestrictions = selectWhen.top()->getChildCount();
			for (int i=0; i<numRestrictions; i++)
			{
				XmlNodeRef restrictionNode = selectWhen.top()->getChild(i);
				if (restrictionNode->getNumAttributes() != 0)
					criteriaMap[restrictionNode->getTag()] = restrictionNode;
				else
					criteriaMap.erase(restrictionNode->getTag()); // handles the case of an empty criteria node being used to cancel parent effects
			}
			selectWhen.pop();
		}
		for (std::map<string, XmlNodeRef>::const_iterator iterCrit = criteriaMap.begin(); iterCrit != criteriaMap.end(); ++iterCrit)
		{
			XmlNodeRef restrictionNode = iterCrit->second;
			InputID inputID;
//			if (0 == strcmp(restrictionNode->getTag(), "DesiredSpeed") && m_states[myID].id == "CombatWalkRifleForward")
//				DEBUG_BREAK;
			IInputValue * pInputValue = FindInputValue(restrictionNode->getTag(), &inputID);
			if (!pInputValue)
			{
				GameWarning("Invalid selection criteria '%s'", restrictionNode->getTag());
				GameWarning("Error loading state '%s'", info.id.c_str());
				succeeded = false;
				continue;
			}
			inputValueRestrictions.resize(0);
			if (!pInputValue->GetRestrictions( restrictionNode, inputValueRestrictions ))
			{
				GameWarning("Error loading state '%s'", info.id.c_str());
				succeeded = false;
				continue;
			}
			if (!inputValueRestrictions.empty())
				pStateIndexBuilder->RestrictStateToInputValues( myID, inputID, &inputValueRestrictions[0], inputValueRestrictions.size() );
			else
				pStateIndexBuilder->RestrictStateToInputValues( myID, inputID, NULL, 0 );
		}
	}

	return succeeded;
}

bool CAnimationGraph::AddLinkToMap( LinkMap& m, const CCryName& from, const CCryName& to, const XmlNodeRef& node )
{
	if (!HaveState(from))
	{
		GameWarning("No node %s for transition %s->%s", from.c_str(), from.c_str(), to.c_str());
		return false;
	}
	if (!HaveState(to))
	{
		GameWarning("No node %s for transition %s->%s", to.c_str(), from.c_str(), to.c_str());
		return false;
	}
	std::map<CCryName, XmlNodeRef>& childMap = m[from];
	if (childMap.find(to) != childMap.end())
	{
		GameWarning("Duplicate link %s->%s", from.c_str(), to.c_str());
		return false;
	}
	childMap.insert( std::make_pair(to, node) );
	return true;
}

bool CAnimationGraph::LoadTransitions( const XmlNodeRef& root, const CStateLoader& stateLoader )
{
	PROFILE_LOADING_FUNCTION;

	XmlNodeRef transitionsNode = root->findChild("Transitions");
	if (!transitionsNode)
	{
		GameWarning("No transitions node");
		return false;
	}

	bool succeeded = true;

	// first, build up a structure (links) which contains only uni-directional links
	int numChildren = transitionsNode->getChildCount();
	LinkMap links;
	for (int i=0; i<numChildren; i++)
	{
		XmlNodeRef childNode = transitionsNode->getChild(i);
		const char * childTag = childNode->getTag();
		if (0 == strcmp(childTag, "Link"))
		{
			CCryName from = childNode->getAttr("from");
			CCryName to = childNode->getAttr("to");
			if (!stateLoader.AllowInGame(from) || !stateLoader.AllowInGame(to))
				continue;
			if (!AddLinkToMap(links, from, to, childNode))
			{
				succeeded = false;
				continue;
			}
		}
		else if (0 == strcmp(childTag, "BiLink"))
		{
			CCryName from = childNode->getAttr("from");
			CCryName to = childNode->getAttr("to");
			if (!stateLoader.AllowInGame(from) || !stateLoader.AllowInGame(to))
				continue;
			if (!AddLinkToMap(links, from, to, childNode))
			{
				succeeded = false;
				continue;
			}
			if (!AddLinkToMap(links, to, from, childNode))
			{
				succeeded = false;
				continue;
			}
		}
		else
		{
			GameWarning("No such linking mechanism %s", childTag);
			succeeded = false;
			continue;
		}
	}

	// next, go through and build up the transitions
	for (LinkMap::const_iterator iterStart = links.begin(); iterStart != links.end(); ++iterStart)
	{
		for (LinkTermAndXmlMap::const_iterator iterEnd = iterStart->second.begin(); iterEnd != iterStart->second.end(); ++iterEnd)
		{
			SLinkInfo linkInfo;
			linkInfo.from = m_stateNameToID[iterStart->first];
			linkInfo.to = m_stateNameToID[iterEnd->first];

			XmlNodeRef linkNode = iterEnd->second;

			int forceFollowChance = 0;
			linkNode->getAttr("forceFollowChance", forceFollowChance);
			linkInfo.forceFollowChance = CLAMP(forceFollowChance, 0, 65535);

			int costFromXml = 100;
			linkNode->getAttr("cost", costFromXml);
			linkInfo.cost = CLAMP(costFromXml, 1, 65536) - 1;

			float ttFromXml = 0.0f;
			linkNode->getAttr("transitionTime", ttFromXml);
			linkInfo.overrideTransitionTime = ttFromXml;

			m_links.push_back(linkInfo);

			m_states[linkInfo.from].hasAnyLinksFrom = true;
			m_states[linkInfo.to].hasAnyLinksTo = true;
		}
	}
	std::sort( m_links.begin(), m_links.end() );

	for (StateInfoVec::iterator iter = m_states.begin(); iter != m_states.end(); ++iter)
	{
		SLinkInfo dummyInfo = {iter - m_states.begin(),0};
		iter->linkOffset = std::lower_bound( m_links.begin(), m_links.end(), dummyInfo ) - m_links.begin();
	}

	return succeeded;
}

bool CAnimationGraph::CachePass()
{
	PROFILE_LOADING_FUNCTION;

	// figure out states with forced transitions
	for (StateInfoVec::iterator iterState = m_states.begin(); iterState != m_states.end(); ++iterState)
	{
		iterState->hasForceFollows = false;
		for (LinkInfoVec::const_iterator iterLink = m_links.begin() + iterState->linkOffset; iterLink != m_links.end() && iterLink->from == (iterState - m_states.begin()); ++iterLink)
		{
			if (iterLink->forceFollowChance > 0)
			{
				iterState->hasForceFollows = true;
			
				// check the destination state if the only way out of it goes back through this state then disable the guards
				if (iterState->allowSelect)
				{
					SStateInfo& toState = m_states[ iterLink->to ];
					toState.evaluateGuardsOnExit = false;
					for (LinkInfoVec::const_iterator iterLink2 = m_links.begin() + toState.linkOffset; iterLink2 != m_links.end() && iterLink2->from == iterLink->to; ++iterLink2)
					{
						if (iterLink2->to != iterLink->from)
						{
							toState.evaluateGuardsOnExit = true;
							break;
						}
					}
				}
			}
		}
		if (iterState->allowSelect)
		{
			for (InputValues::const_iterator iterInput = m_inputValues.begin(); !iterState->matchesSignalledInputs && iterInput != m_inputValues.end(); ++iterInput)
			{
				if ((*iterInput)->signalled)
				{
					if (m_stateIndex.StateMatchesInput(iterState-m_states.begin(), (*iterInput)->id, (*iterInput)->defaultValue, eSMIF_ConsiderMatchesAny))
						continue;
					if (m_stateIndex.StateMatchesExplicitly( iterState-m_states.begin(), (*iterInput)->id ))
						iterState->matchesSignalledInputs = true;
				}
			}
		}
	}

	// cache the cost of the destination state into the cost of the link so calling CalculateCost wouldn't be needed
	for (LinkInfoVec::iterator iterLink = m_links.begin(); iterLink != m_links.end(); ++iterLink)
	{
		SLinkInfo& link = *iterLink;
		SStateInfo& state = m_states[link.to];
		link.cost += state.cost + 1 + state.hasForceFollows * FORCE_FOLLOW_COST + state.matchesSignalledInputs;
	}

	return true;
}

CAnimationGraph::IInputValue * CAnimationGraph::FindInputValue(const string& name, InputID * pID)
{
	for (size_t i=0; i<m_inputValues.size(); i++)
	{
		IInputValue * p = m_inputValues[i];
		if (p->name == name)
		{
			if (pID)
				*pID = i;
			return p;
		}
	}
	return 0;
}

ILINE uint32 CAnimationGraph::CalculateCost( const SLinkInfo& link, const SStateInfo& state )
{
	return link.cost;
	//return link.cost + state.cost + 1 + state.hasForceFollows * FORCE_FOLLOW_COST + state.matchesSignalledInputs;
}

bool CAnimationGraph::IsDirectlyLinked(CStateIndex::StateID fromID, CStateIndex::StateID toID) const
{
	SLinkInfo query;
	query.from = fromID;
	query.to = toID;

	int first = 0;
	int last = m_links.size();
	while (first <= last)
	{
		int mid = (first + last) / 2;
		if (query < m_links[mid])
		{
			last = mid-1;
		}
		else if (m_links[mid] < query)
		{
			first = mid+1;
		}
		else
		{
			return true;
		}
	}

	return false;
}

const CAnimationGraph::SLinkInfo* CAnimationGraph::FindLink(CStateIndex::StateID fromID, CStateIndex::StateID toID) const
{
	SLinkInfo query;
	query.from = fromID;
	query.to = toID;

	int first = 0;
	int last = m_links.size() - 1;
	while (first <= last)
	{
		int mid = (first + last) / 2;
		if (query < m_links[mid])
		{
			last = mid-1;
		}
		else if (m_links[mid] < query)
		{
			first = mid+1;
		}
		else
		{
			return &( m_links[mid] );
		}
	}

	return NULL;
}

const SAnimationDesc* CAnimationGraph::GetAnimationDesc(IEntity* pEntity, CStateIndex::StateID stateID, const CAnimationGraphState* pState)
{
	if (stateID == INVALID_STATE)
		return NULL;

	if (!ConstructAnimationDescForAllStates(pEntity, pState))
		return NULL;

	assert(stateID < m_states.size());
	if (stateID >= m_states.size())
		return NULL;

	const SAnimationDesc* desc = &(m_states[stateID].animDesc);
	if (!desc->initialized)
	{
		m_states[stateID].animDesc = ConstructAnimationDesc(pEntity, stateID, pState);
		if (m_states[stateID].animDesc.properties != NULL)
		{
			m_states[stateID].hasExpensiveGuards = m_states[stateID].animDesc.properties->m_bGuarded;
		}
		else
		{
			m_states[stateID].hasExpensiveGuards = false;
		}
	}

	return desc;
}

bool CAnimationGraph::ConstructAnimationDescForAllStates(IEntity* pEntity, const CAnimationGraphState* pState)
{
	if (!m_hasAnimDesc && pEntity && pEntity->GetCharacter(0))
	{
		CryLog("Getting animation movement data for animation graph %s from entity %s", m_name.c_str(), pEntity->GetName());

		if (!m_states.empty())
		{
			for (StateInfoVec::iterator i = m_states.begin(); i != m_states.end(); ++i)
			{
				i->animDesc = ConstructAnimationDesc(pEntity, i - m_states.begin(), pState);
				if (i->animDesc.properties != NULL)
				{
					i->hasExpensiveGuards = i->animDesc.properties->m_bGuarded;
				}
				else
				{
					i->hasExpensiveGuards = false;
				}
			}

			m_hasAnimDesc = true;
		}
	}

	return m_hasAnimDesc;
}

SAnimationDesc CAnimationGraph::ConstructAnimationDesc(IEntity * pEntity, CStateIndex::StateID stateID, const CAnimationGraphState* pState)
{
	ANIM_PROFILE_FUNCTION;

	// TODO: merge with GetAnimationMovement value
	static int animLayer0Slot = CCryAction::GetCryAction()->GetAnimationGraphManager()->GetCategory("AnimationLayer1")->overrideSlot;

	SAnimationDesc desc;

	while (stateID != INVALID_STATE)
	{
		IAnimationStateNodeFactory* pFactory = NULL;
		for (int i=0; !pFactory && i<m_states[stateID].factoryLength; i++)
		{
			uint32 factoryIndex = m_states[stateID].factoryStart + i;
			uint8 factorySlotIndex = m_factorySlotIndices[factoryIndex];
			if (factorySlotIndex == animLayer0Slot)
			{
				pFactory = m_factories[factoryIndex]; 
			}
		}
		if (pFactory && pEntity)
		{
			desc = static_cast<CAGAnimationLayer<0>*>(pFactory)->GetDesc(pEntity, pState);
			return desc;
		}
		stateID = m_states[stateID].parentState;
	}

	desc.initialized = true; // This state will never have a usefull desc.
	return desc;
}

Vec2 CAnimationGraph::GetStateMinMaxSpeed( IEntity * pEntity, StateID state )
{
	// TODO: merge with GetAnimationMovement value
	static int animLayer0Slot = CCryAction::GetCryAction()->GetAnimationGraphManager()->GetCategory("AnimationLayer1")->overrideSlot;

	Vec2 speed(0,0);
	while (state != INVALID_STATE)
	{
		IAnimationStateNodeFactory * pFactory = 0;
		for (int i=0; !pFactory && i<m_states[state].factoryLength; i++)
			if (m_factorySlotIndices[m_states[state].factoryStart+i] == animLayer0Slot)
				pFactory = m_factories[m_states[state].factoryStart+i];
		if (pFactory && pEntity)
		{
			speed = static_cast<CAGAnimationLayer<0>*>(pFactory)->GetMinMaxSpeed( pEntity );
			break;
		}
		state = m_states[state].parentState;
	}

	return speed;
}

bool CAnimationGraph::PushLinksFromState( StateID fromStateID, SPathFindParams& params )
{
	assert(fromStateID != INVALID_STATE);
	assert(params.destinationStateID != INVALID_STATE);

	bool toDest = false;

	CTargetPointRequest targetPointRequest;

	params.pStats->linksFollowed ++;

	const SStatePathfindState& state_from = m_statePathfindState[fromStateID];
	const SStatePathfindState& state_dest = m_statePathfindState[params.destinationStateID];

	LinkInfoVec::const_iterator iter = m_links.begin() + m_states[fromStateID].linkOffset;

	uint32 baseCost = state_from.cost;

	while (iter != m_links.end() && iter->from == fromStateID)
	{
		const SLinkInfo& linkInfo = *iter;
		SStatePathfindState& state_to = m_statePathfindState[linkInfo.to];

		assert(linkInfo.to != INVALID_STATE);
		params.pStats->nodesTouched ++;
		SAnimationMovement movement = state_from.movement;
		uint32 cost = baseCost + CalculateCost( *iter, m_states[linkInfo.to] );
		if (linkInfo.to == params.destinationStateID)
		{
			if (params.radius > 0.0f) // possibly redundant - may need some flag though
			{
				params.pStats->aiWalkabilityQueriesPerformed++;

				// CheckTargetMovement will return true, false, maybe
				ETriState success = params.pGraphState->CheckTargetMovement(movement, params.radius, targetPointRequest);
				
				if (success == eTS_maybe)
					params.pStats->maybeTargets++;

				// skip if it's false or maybe
				if (success != eTS_true)
				{
					++iter;
					continue;
				}
			}

			if (params.radius > 0.0f && movement.translation.GetLengthSquared() > params.radius*params.radius)
				cost += 10000000u;
			if (params.time > 0.0f && movement.duration > params.time)
				cost += 10000000u;
			toDest = true;
		}
		params.pStats->maxCost = max(cost, params.pStats->maxCost);
		int randomNumber = (params.randomNumber ^ (unsigned int) &linkInfo) & 0x7fff;
		bool isCheaper = false;
		isCheaper |= (cost < state_dest.cost) && (cost < state_to.cost);
		isCheaper |= (cost <= state_dest.cost) && (cost <= state_to.cost) && (randomNumber < (RAND_MAX/2));
		if (isCheaper)
		{
			if (linkInfo.to != params.destinationStateID)
			{
				if (!state_to.guardsEvaluated)
				{
					bool succ = true;
					if (m_states[fromStateID].evaluateGuardsOnExit)
					{
						params.pStats->guardEvaluationsPerformed ++;
						succ = m_states[linkInfo.to].EvaluateGuards(params.pCurInputValues);
						if (succ && m_states[linkInfo.to].hasExpensiveGuards)
						{
							if (params.radius > 0.0f)
							{
								succ = false;
							}
							else
							{
								CAnimatedCharacter* pAnimaterCharacter = params.pGraphState->GetAnimatedCharacter();
								if (pAnimaterCharacter != NULL)
								{
									params.pStats->expensiveGuardEvaluationsPerformed ++;
									succ = pAnimaterCharacter->ValidateAnimGraphPathNode(this, linkInfo.to);
								}
							}
						}
					}
					state_to.guardsSucceed = succ;
					// mark the fromStateID as not available anymore to avoid future guards evaluations
					state_to.guardsEvaluated = true;
					AddTouched(linkInfo.to);
					if (!succ)
					{
						params.pStats->guardEvaluationsFailed ++;
						state_to.cost = 0;
						++iter;
						continue;
					}
				}
				assert(state_to.guardsSucceed);

				movement += m_states[linkInfo.to].animDesc.movement;
				params.pStats->animationMovementQueriesPerformed ++;
			}
			uint32 oldCost = state_to.cost;
			params.pStats->replacementsMade += (oldCost != unsigned(-1));
			AddTouched(linkInfo.to);
			state_to.cost = cost;
			state_to.movement = movement;
			m_states[linkInfo.to].prevState = fromStateID;
			if (linkInfo.to != params.destinationStateID)
			{
				params.pStats->cumLinksInQueue ++;
				if (state_to.openMapIter != m_openMap.end())
					m_openMap.erase(state_to.openMapIter);
				state_to.openMapIter = m_openMap.insert( std::make_pair(cost, linkInfo.to) );
			}
			else
			{
				if (params.pTargetPointRequest)
					*(params.pTargetPointRequest) = targetPointRequest;
			}
		}
		++iter;
	}
	return toDest;
}

ETriState CAnimationGraph::PathFindBetweenStates( StateID fromStateID, SPathFindParams& params )
{
	ANIM_PROFILE_FUNCTION;

	if ( fromStateID == params.destinationStateID )
		return eTS_true;

	ConstructAnimationDescForAllStates(params.pEntity, params.pGraphState);
	
	m_openMap.clear();

	/* Luc - temp code to dump all AG states with id
	static bool dump = false;
	if(!dump)
	{
	CryLogAlways("----AG STATES-------------");
	for(int i=0;i<m_states.size();i++)
	{
	CryLogAlways("%d = %s",i,m_states[i].id);
	}
	CryLogAlways("----END AG STATES-------------");
	dump = true;
	}
	*/
	assert(fromStateID != INVALID_STATE);
	assert(params.destinationStateID != INVALID_STATE);

	params.pStats->maxCost = 0;
	params.pStats->largestOpenQueue = 0;
	params.pStats->nodesTouched = 0;
	params.pStats->linksFollowed = 0;
	params.pStats->cumLinksInQueue = 0;
	params.pStats->guardEvaluationsPerformed = 0;
	params.pStats->guardEvaluationsFailed = 0;
	params.pStats->aiWalkabilityQueriesPerformed = 0;
	params.pStats->animationMovementQueriesPerformed = 0;
	params.pStats->maybeTargets = 0;
	params.pStats->replacementsMade = 0;
	params.pStats->expensiveGuardEvaluationsPerformed = 0;
	SStatePathfindState defState(m_openMap.end());
	if (!m_pathFindInitialized)
	{
		std::fill( m_statePathfindState.begin(), m_statePathfindState.end(), defState );
		m_pathFindInitialized = true;
	}
	else
	{
		// cleanup after last round
		while (m_lastTouched != StateID(-2))
		{
			// DEJAN: temp. fix for crashes on ag_reload. We need Craig to find a better one.
			if (m_lastTouched >= StateID(m_statePathfindState.size()) || m_lastTouched == StateID(-1))
			{
				std::fill( m_statePathfindState.begin(), m_statePathfindState.end(), defState );
				m_pathFindInitialized = true;
				break;
			}
			StateID state = m_lastTouched;
			m_lastTouched = m_statePathfindState[state].nextTouched;
			m_statePathfindState[state] = defState;
		}
	}

	m_lastTouched = fromStateID;
	m_statePathfindState[fromStateID].nextTouched = StateID(-2);
	AddTouched(params.destinationStateID);
	m_statePathfindState[fromStateID].cost = 0;
	m_states[params.destinationStateID].prevState = fromStateID;
	m_states[fromStateID].prevState = INVALID_STATE;

	bool toDest = false;
	
	// no links from current node... return a bad result (there is an implicit link between any two nodes whenever we cannot find a path!)
	{
		toDest |= PushLinksFromState(fromStateID, params);
		while (!m_openMap.empty())
		{
			params.pStats->largestOpenQueue = max(m_openMap.size(), params.pStats->largestOpenQueue);
			std::pair<uint32, StateID> link = *m_openMap.begin();
			m_statePathfindState[link.second].openMapIter = m_openMap.end();
			m_openMap.erase(m_openMap.begin());
			if (link.first < m_statePathfindState[params.destinationStateID].cost)
				toDest |= PushLinksFromState(link.second, params);
			else
				break;
		}
	}

	StateID curState = params.destinationStateID;
	do
	{
		assert(curState != INVALID_STATE);
		params.pTransitions->PushFront(curState);
		curState = m_states[curState].prevState;
	}
	while ( (curState != INVALID_STATE) && (!params.pTransitions->Full()) &&
		((params.radius <= 0) || (m_states[curState].animDesc.properties == NULL) || (m_states[curState].animDesc.properties->m_bLocomotion == false)) );

	if (!toDest)
	{
		// we don't want to report if pathfinding was to or from an isolated state
		if ( m_states[fromStateID].hasAnyLinksFrom && m_states[params.destinationStateID].hasAnyLinksTo )
			if (!gEnv->pSystem->IsDedicated())
				GameWarning("Pathfinding in animation graph failed (%s) - no path from '%s' to '%s'",
				params.pEntity != NULL ? params.pEntity->GetName() : "MusicGraph", 
				m_states[fromStateID].id.c_str(), m_states[params.destinationStateID].id.c_str());
	}

	if (!toDest && params.radius > 0)
	{
		if (params.pStats->maybeTargets > 0)
			return eTS_maybe;
		else
			return eTS_false;
	}
	params.pStats->finalCost = m_statePathfindState[params.destinationStateID].cost;
	*(params.pMovement) = m_statePathfindState[params.destinationStateID].movement;

	assert(params.pTransitions->Front() == fromStateID || params.radius > 0);
	if (params.pTransitions->Front() == fromStateID)
		params.pTransitions->Pop();

	return eTS_true;
}

CAnimationGraph::InputID CAnimationGraph::LookupInputId( const char * name )
{
	for (int i=0; i<(int)m_inputValues.size(); ++i)
		if (m_inputValues[i]->name == name)
			return i;
	return InputID(-1);
}

int CAnimationGraph::RegisterOutput( string& value, VectorMap<string, int>& sto, int& next )
{
	VectorMap<string, int>::const_iterator it = sto.find(value);
	if (it != sto.end())
	{
		value = it->first;
		return it->second;
	}
	else
	{
		int out = next++;
		sto[value] = out;
		return out;
	}
}

int CAnimationGraph::DeclareOutput( const char * name, const char * value )
{
	string sName = name;
	string sValue = value;
	int iName = RegisterOutput( sName, m_stringToOutputHigh, m_nextOutputHigh );
	int iValue = RegisterOutput( sValue, m_stringToOutputLow, m_nextOutputLow );
	int id = 1000*iName + iValue;
	m_outputs[id] = std::make_pair(sName, sValue);
	return id;
}


typedef std::map< string, string > TMapParamValues;
typedef std::vector< TMapParamValues > TVectorParamValues;
typedef std::map< string, XmlNodeRef > TMapExpandedNodes;
typedef std::set< string > TSetStrings;
typedef std::map< string, TSetStrings > TMapParameters;


struct SExpandInfo
{
	XmlNodeRef parameterizationNode;
	TMapParameters mapParameters;
	TMapParamValues mapParamValues;
	TMapExpandedNodes mapExpandedNodes;
	TSetStrings setExcludedNodeNames;
};


class CartesianProductHelper
{
public:
	CartesianProductHelper( TMapParameters& definition, TVectorParamValues& result )
		: m_params( definition ), m_result( result )
	{
		Multiply( m_params.begin() );
	}

private:
	TMapParameters& m_params;
	TMapParamValues m_current;
	TVectorParamValues& m_result;

	void Multiply( TMapParameters::const_iterator itParams )
	{
		if ( itParams == m_params.end() )
			m_result.push_back( m_current );
		else
		{
			string& current = m_current[ itParams->first ];
			const TSetStrings& values = itParams->second;
			++itParams;
			for ( TSetStrings::const_iterator it = values.begin(); it != values.end(); ++it )
			{
				current = *it;
				Multiply( itParams );
			}
		}
	}
};

// a functor class to be used to replace all params in a string with real values
class ReplaceParamsFunc
{
	bool m_bResult;
	string& m_value;
public:
	ReplaceParamsFunc( string& value ) : m_value( value ), m_bResult( false ) {}
	void operator () ( const std::pair< const string, string >& param )
	{
		string::size_type len = param.first.length() + 3;
		char* temp = new char[len];
		temp[0] = '[';
		memcpy( temp+1, param.first.c_str(), len-3 );
		temp[len-2] = ']';
		temp[len-1] = 0;
		string::size_type pos = 0;
		while ( (pos = m_value.find( temp, pos )) != string::npos )
		{
			string lstr( param.second );
			lstr.MakeLower();
			m_value.replace( pos, len-1, lstr.c_str() );
			m_bResult = true;
		}
		temp[1] = ::toupper( temp[1] );
		pos = 0;
		while ( (pos = m_value.find( temp, pos )) != string::npos )
		{
			m_value.replace( pos, len-1, param.second.c_str() );
			m_bResult = true;
		}
		for ( int i = 2; i < len-2; ++i )
			temp[i] = ::toupper( temp[i] );
		pos = 0;
		while ( (pos = m_value.find( temp, pos )) != string::npos )
		{
			string ustr( param.second );
			ustr.MakeUpper();
			m_value.replace( pos, len-1, ustr.c_str() );
			m_bResult = true;
		}
		delete [] temp;
	}
	operator bool () const { return m_bResult; }
};

void ReplaceParamsInTree( XmlNodeRef node, TMapParamValues& paramValues )
{
	for ( int i = 0; i < node->getNumAttributes(); ++i )
	{
		const char* key;
		const char* value;
		node->getAttributeByIndex( i, &key, &value );
		string temp( value );
		if (std::for_each( paramValues.begin(), paramValues.end(), ReplaceParamsFunc(temp) ))
			node->setAttr( key, temp );
		//assert( temp.find_first_of("[]") == string::npos );
	}
	for ( int i = 0; i < node->getChildCount(); ++i )
	{
		XmlNodeRef child = node->getChild(i);
		ReplaceParamsInTree( child, paramValues );
	}
}

bool CAnimationGraph::PreprocessParameterization( XmlNodeRef rootGraph )
{
	PROFILE_LOADING_FUNCTION;

	bool ok = true;

	// this map helps in expanding the links
	typedef std::map< string, SExpandInfo > TMapParameterization;
	TMapParameterization mapParameterization;


	XmlNodeRef allStatesNode = rootGraph->findChild( "States" );
	if ( !allStatesNode )
		return false;

	// process all states
	for ( int i = 0; i < allStatesNode->getChildCount(); ++i )
	{
		XmlNodeRef stateNode = allStatesNode->getChild(i);

		// except those not included in game
		bool include = true;
		stateNode->getAttr( "includeInGame", include );
		if ( !include )
			continue;

		// ignore non-parameterized nodes
		XmlNodeRef parameterizationNode = stateNode->findChild( "Parameterization" );
		if ( !parameterizationNode )
			continue;

		string stateId = stateNode->getAttr("id");

		// only parameters used in the name are valid
		std::set< string > nameParameters;
		int pos = 0;
		string token;
		while ( (pos = stateId.find( "[", pos )) != string::npos )
		{
			token = stateId.Tokenize( "]", ++pos );
			if ( pos == string::npos )
				break;
			token.MakeLower();
			nameParameters.insert( token );
		}

		// no valid params => ignore this state
		if ( nameParameters.empty() )
			continue;

		// this map will contain all possible values for each valid parameter
		TMapParameters mapParameters;

		// the order of parameters as they appear in the XML is important for creating override-id strings
		typedef std::vector< string > TVectorStrings;
		TVectorStrings paramsOrder;

		for ( int j = 0; j < parameterizationNode->getChildCount(); ++j )
		{
			XmlNodeRef parameterNode = parameterizationNode->getChild(j);
			if ( !strcmp(parameterNode->getTag(),"Parameter") )
			{
				string parameterId = parameterNode->getAttr("id");
				parameterId.MakeLower();
				paramsOrder.push_back( parameterId );
				if ( nameParameters.find(parameterId) == nameParameters.end() )
					continue;

				TSetStrings& values = mapParameters.insert(std::make_pair( parameterId, TSetStrings() )).first->second;
				for ( int j = 0; j < parameterNode->getChildCount(); ++j )
				{
					string value = parameterNode->getChild(j)->getAttr("id");
					values.insert( value );
				}
			}
		}

		// remove parameters with no values
		TMapParameters::iterator it = mapParameters.begin();
		while ( it != mapParameters.end() )
			if ( it->second.empty() )
				mapParameters.erase( it++ );
			else
				++it;
		if ( mapParameters.empty() )
			continue;

		// parameterized states need to be removed and replaced with expanded non-parameterized states
		allStatesNode->deleteChildAt(i--);

		// stateNode will be used for cloning
		stateNode->removeChild( parameterizationNode );

		// parameterizationNode will be kept for expanding the links
		SExpandInfo& currentExpandInfo = mapParameterization[ stateId ];
		currentExpandInfo.parameterizationNode = parameterizationNode;

		// also mapParameters will be needed later
		currentExpandInfo.mapParameters = mapParameters;

		TVectorParamValues vProduct;
		CartesianProductHelper( mapParameters, vProduct );

		TVectorParamValues::iterator itProduct, itProductEnd = vProduct.end();
		for ( itProduct = vProduct.begin(); itProduct != itProductEnd; ++itProduct )
		{
			TMapParamValues& paramValues = *itProduct;

			// store it for later reference
			currentExpandInfo.mapParamValues = paramValues;

			XmlNodeRef clone = stateNode->clone();
			for ( int j = 0; j < clone->getNumAttributes(); ++j )
			{
				const char* key;
				const char* value;
				clone->getAttributeByIndex( j, &key, &value );
				string clonedValue( value );
				if (std::for_each( paramValues.begin(), paramValues.end(), ReplaceParamsFunc(clonedValue) ))
					clone->setAttr( key, clonedValue );
			//	assert( clonedValue.find_first_of("[]") == string::npos );
			}

			string paramId;
			TVectorStrings::iterator itOrder = paramsOrder.begin(), itOrderEnd = paramsOrder.end();
			while ( itOrder != itOrderEnd )
			{
				paramId += paramValues[ *itOrder ];
				++itOrder;
				if ( itOrder != itOrderEnd )
					paramId += ',';
			}

			for ( int j = 0; j < parameterizationNode->getChildCount(); ++j )
			{
				XmlNodeRef overrideNode = parameterizationNode->getChild(j);
				if ( !strcmp(overrideNode->getTag(),"Override") && overrideNode->getAttr("id") == paramId )
				{
					for ( int k = 0; k < overrideNode->getChildCount(); ++k )
					{
						XmlNodeRef overrideChild = overrideNode->getChild(k);
						if ( !strcmp(overrideChild->getTag(),"ExcludeFromGraph") )
						{
							string name = clone->getAttr("id");
							currentExpandInfo.setExcludedNodeNames.insert(name);
							clone = NULL;
							break;
						}
						else if ( !strcmp(overrideChild->getTag(),"SelectWhen") )
						{
							XmlNodeRef selectWhenNode = clone->findChild( "SelectWhen" );
							if ( !selectWhenNode )
								clone->addChild( overrideChild->clone() );
							else
							{
								for ( int l = 0; l < overrideChild->getChildCount(); ++l )
								{
									XmlNodeRef overrideSelectWhenChild = overrideChild->getChild(l);
									XmlNodeRef selectWhenChild = selectWhenNode->findChild( overrideSelectWhenChild->getTag() );
									if ( selectWhenChild )
										selectWhenNode->removeChild( selectWhenChild );
									selectWhenNode->addChild( overrideSelectWhenChild->clone() );
								}
							}
						}
						else if ( !strcmp(overrideChild->getTag(),"Template") )
						{
							XmlNodeRef templateNode = clone->findChild( "Template" );
							if ( templateNode )
							{
								for ( int l = 0; l < overrideChild->getNumAttributes(); ++l )
								{
									const char* key;
									const char* value;
									overrideChild->getAttributeByIndex( l, &key, &value );
									templateNode->setAttr( key, value );
								}
							}
						}
					}
					break;
				}
			}

			// only if not excluded from graph
			if ( clone )
			{
				ReplaceParamsInTree( clone, paramValues );

				// done - add it as a regular state node
				allStatesNode->addChild( clone );

				// store the cloned state node for later reference
				currentExpandInfo.mapExpandedNodes[ paramId ] = clone;
			}
		}
	}

	XmlNodeRef allLinksNode = rootGraph->findChild( "Transitions" );
	if ( !allLinksNode )
		return false;

	// process all links
	for ( int i = 0; i < allLinksNode->getChildCount(); ++i )
	{
		XmlNodeRef linkNode = allLinksNode->getChild(i);
		string fromNode = linkNode->getAttr("from");
		string toNode = linkNode->getAttr("to");
		TMapParameterization::iterator itFrom = mapParameterization.find( fromNode );
		SExpandInfo* pExpandInfoFrom = itFrom == mapParameterization.end() ? NULL : &itFrom->second;
		TMapParameterization::iterator itTo = mapParameterization.find( toNode );
		SExpandInfo* pExpandInfoTo = itTo == mapParameterization.end() ? NULL : &itTo->second;

		// ignore links connecting two non-parameterized nodes
		if ( !pExpandInfoFrom && !pExpandInfoTo )
			continue;

		// remove the node, will be replaced with nodes containing only expanded state names
		allLinksNode->deleteChildAt(i--);

		char id = 'A';
		TMapParamValues from_id2paramName, to_id2paramName;
		TMapParameters combinedMapParams;
		TMapParameters fromMapParams, toMapParams;
		if ( pExpandInfoFrom )
			fromMapParams = pExpandInfoFrom->mapParameters;
		if ( pExpandInfoTo )
			toMapParams = pExpandInfoTo->mapParameters;

		for ( int j = 0; j < linkNode->getChildCount(); ++j )
		{
			XmlNodeRef child = linkNode->getChild(j);
			if ( !strcmp("Mapping",child->getTag()) )
			{
				bool bFromIsParam = false;
				bool bToIsParam = false;
				string fromMapping = child->getAttr("from");
				string toMapping = child->getAttr("to");

				TMapParameters::iterator itFrom = fromMapParams.end();
				TMapParameters::iterator itTo = toMapParams.end();

				if ( fromMapping[0] == '[' )
				{
					bFromIsParam = true;
					fromMapping = fromMapping.substr( 1, fromMapping.length()-2 );
					itFrom = fromMapParams.find(fromMapping);
					if ( itFrom == fromMapParams.end() )
						continue;
				}
				if ( toMapping[0] == '[' )
				{
					bToIsParam = true;
					toMapping = toMapping.substr( 1, toMapping.length()-2 );
					itTo = toMapParams.find(toMapping);
					if ( itTo == toMapParams.end() )
						continue;
				}

				string sID(id);
				TSetStrings& values = combinedMapParams[sID];
				if ( bFromIsParam && bToIsParam )
				{
					// find set intersection
					TSetStrings& fromValues = itFrom->second;
					TSetStrings& toValues = itTo->second;
					TSetStrings::iterator itFromValues = fromValues.begin(), itFromValuesEnd = fromValues.end();
					TSetStrings::iterator itToValues = toValues.begin(), itToValuesEnd = toValues.end();
					while ( itFromValues != itFromValuesEnd && itToValues != itToValuesEnd )
					{
						if ( *itFromValues < *itToValues )
							++itFromValues;
						else if ( *itFromValues > *itToValues )
							++itToValues;
						else
						{
							values.insert( *itFromValues );
							++itFromValues;
							++itToValues;
						}
					}
					if ( values.empty() )
					{
						// there are no values shared in these two parameters!
						// ignore this relation since it doesn't make any sense
						combinedMapParams.erase(sID);
						continue;
					}
				}
				else if ( bFromIsParam )
				{
					values.insert( toMapping );
				}
				else if ( bToIsParam )
				{
					values.insert( fromMapping );
				}
				else
				{
					assert( !"Error in mapping of parameters!" );
					continue;
				}

				// remove mapped parameters out of the list of non-mapped
				if ( bFromIsParam )
				{
					from_id2paramName[sID] = fromMapping;
					fromMapParams.erase(fromMapping);
				}
				if ( bToIsParam )
				{
					to_id2paramName[sID] = toMapping;
					toMapParams.erase(toMapping);
				}

				++id;
			}
		}

		// add non-mapped parameters and all of their values
		TMapParameters::iterator it, itEnd = fromMapParams.end();
		for ( it = fromMapParams.begin(); it != itEnd; ++it )
		{
			string sID(id++);
			from_id2paramName[sID] = it->first;
			combinedMapParams[sID] = it->second;
		}
		itEnd = toMapParams.end();
		for ( it = toMapParams.begin(); it != itEnd; ++it )
		{
			string sID(id++);
			to_id2paramName[sID] = it->first;
			combinedMapParams[sID] = it->second;
		}

		// child nodes are processed so we don't need them anymore. clean it up and make it ready for cloning
		linkNode->removeAllChilds();

		// once we have the final list of parameters and lists of values for each of them we can multiply them
		TVectorParamValues vProduct;
		CartesianProductHelper( combinedMapParams, vProduct );

		// now links can be expanded and added back to the XML
		TVectorParamValues::iterator itProduct, itProductEnd = vProduct.end();
		for ( itProduct = vProduct.begin(); itProduct != itProductEnd; ++itProduct )
		{
			TMapParamValues& paramValues = *itProduct;
			TMapParamValues fromParamValues, toParamValues;
			TMapParamValues::iterator itValues, itValuesEnd = paramValues.end();
			for ( itValues = paramValues.begin(); itValues != itValuesEnd; ++itValues )
			{
				// map back to the 'from' state
				TMapParamValues::iterator found;
				found = from_id2paramName.find( itValues->first );
				if ( found != from_id2paramName.end() )
					fromParamValues[found->second] = itValues->second;

				// map back to the 'to' state
				found = to_id2paramName.find( itValues->first );
				if ( found != to_id2paramName.end() )
					toParamValues[found->second] = itValues->second;
			}

			XmlNodeRef clone = linkNode->clone(); clone->removeAllChilds();
			if ( pExpandInfoFrom )
			{
				string value = clone->getAttr( "from" );
				if (std::for_each( fromParamValues.begin(), fromParamValues.end(), ReplaceParamsFunc(value) ))
					clone->setAttr( "from", value );

				// don't create links for "excluded from graph" states
				if ( pExpandInfoFrom->setExcludedNodeNames.find(value) != pExpandInfoFrom->setExcludedNodeNames.end() )
					continue;
			}
			if ( pExpandInfoTo )
			{
				string value = clone->getAttr( "to" );
				if (std::for_each( toParamValues.begin(), toParamValues.end(), ReplaceParamsFunc(value) ))
					clone->setAttr( "to", value );

				// don't create links for "excluded from graph" states
				if ( pExpandInfoTo->setExcludedNodeNames.find(value) != pExpandInfoTo->setExcludedNodeNames.end() )
					continue;
			}
			allLinksNode->addChild( clone );
		}
	}

	XmlNodeRef allViewsNode = rootGraph->findChild( "Views" );
	if ( !allViewsNode )
		return false;

	// process all views
	for ( int i = 0; i < allViewsNode->getChildCount(); ++i )
	{
		XmlNodeRef viewNode = allViewsNode->getChild(i);
		if ( strcmp(viewNode->getTag(),"View") )
			continue;

		// process all states in the view
		for ( int j = 0; j < viewNode->getChildCount(); ++j )
		{
			XmlNodeRef stateNode = viewNode->getChild(j);
			if ( strcmp(stateNode->getTag(),"State") )
				continue;

			// ignore non-parameterized nodes
			string nodeId = stateNode->getAttr("id");
			TMapParameterization::iterator itNode = mapParameterization.find( nodeId );
			if ( itNode == mapParameterization.end() )
				continue;

			SExpandInfo& expandInfo = itNode->second;
			int x = 0, y = 0;
			stateNode->getAttr( "x", x );
			stateNode->getAttr( "y", y );

			// remove the state node, will be replaced with nodes containing only expanded state names
			viewNode->deleteChildAt(j--);

			// insert expanded states
			TMapExpandedNodes::const_iterator it, itEnd = expandInfo.mapExpandedNodes.end();
			for ( it = expandInfo.mapExpandedNodes.begin(); it != itEnd; ++it )
			{
				XmlNodeRef clone = stateNode->clone(); clone->removeAllChilds();
				clone->setAttr( "id", it->second->getAttr("id") );
				clone->setAttr( "x", x );
				x += 4;
				clone->setAttr( "y", y );
				y += 4;
				viewNode->addChild( clone );
			}
		}
	}

#ifdef USER_dejan
	rootGraph->saveToFile("PreprocessedAG.xml");
#endif

	return ok;
}

namespace
{
	void MergeTemplateXmlNodes( XmlNodeRef node, XmlNodeRef parent )
	{
		for ( int i = 0; i < parent->getChildCount(); ++i )
		{
			bool bMerge = true;
			XmlNodeRef childNode = parent->getChild(i);
			if ( childNode->isTag("Param") )
			{
				// params are merged by name
				string paramName = childNode->getAttr("name");
				for ( int j = 0; j < node->getChildCount(); ++j )
				{
					XmlNodeRef paramNode = node->getChild(j);
					if ( paramNode->isTag("Param") && paramName == paramNode->getAttr("name") )
					{
						bMerge = false;
						break;
					}
				}
			}
			else if ( childNode->isTag("SelectWhen") )
			{
				// SelectWhen tag has sub-nodes
				if ( XmlNodeRef selectWhen = node->findChild("SelectWhen") )
				{
					MergeTemplateXmlNodes( selectWhen, childNode );
					bMerge = false;
				}
			}
			else if ( childNode->isTag("Guard") )
			{
				// Guard tag has sub-nodes
				if ( XmlNodeRef guard = node->findChild("Guard") )
				{
					MergeTemplateXmlNodes( guard, childNode );
					bMerge = false;
				}
			}
			else if ( node->findChild(childNode->getTag()) )
				bMerge = false;

			if ( bMerge )
			{
				node->addChild( childNode->clone() );
			}
		}
	}
}

bool CAnimationGraph::PreprocessTemplates( XmlNodeRef rootGraph )
{
	PROFILE_LOADING_FUNCTION;

	bool ok = true;

	// phase 1: load the XML files for templates
	std::map<string, XmlNodeRef> templateXmls;

	ICryPak * pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	char filename[_MAX_PATH];

	string path = "Libs/AnimationGraphTemplates";
	string search = path + "/*.xml";
	intptr_t handle = pCryPak->FindFirst( search.c_str(), &fd );
	if (handle != -1)
	{
		int res = 0;

		do 
		{
			strcpy( filename, path.c_str() );
			strcat( filename, "/" );
			strcat( filename, fd.name );

			if (filename[strlen(filename)-1] == 'l')
			{
				XmlNodeRef rootFile = GetISystem()->LoadXmlFile( filename );
				if (rootFile)
				{
					string name = rootFile->getAttr( "name" );
					if ( templateXmls.find(name) != templateXmls.end() )
						CryError( "Duplicately named animation graph template %s", name.c_str());
					templateXmls[ name ] = rootFile;
				}
				else
				{
					GameWarning("Failed parsing animation graph template %s", filename);
					ok = false;
				}
			}

			res = pCryPak->FindNext( handle, &fd );
		}
		while (res >= 0);

		pCryPak->FindClose( handle );
	}

	// phase 2: process the template inheritance
	std::map<string, XmlNodeRef>::iterator itXmls, itXmlsEnd = templateXmls.end();
	for ( itXmls = templateXmls.begin(); itXmls != itXmlsEnd; ++itXmls )
	{
		XmlNodeRef current = itXmls->second;
		string extend = current->getAttr("extend");

		std::set<string> done;

		int curPos = 0;
		string singleExtend = extend.Tokenize(",",curPos);
		while ( !singleExtend.empty() )
		{
			if ( done.find(singleExtend) == done.end() )
			{
				done.insert( singleExtend );
				std::map<string, XmlNodeRef>::iterator itParentXml = templateXmls.find( singleExtend );
				if ( itParentXml == itXmlsEnd )
				{
					GameWarning( "Undefined template '%s' used as parent", singleExtend.c_str());
					ok = false;
				}
				else
				{
					XmlNodeRef parent = itParentXml->second;
					singleExtend = parent->getAttr("extend");
					if ( !singleExtend.empty() )
					{
						if ( curPos == -1 )
						{
							curPos = 0;
							extend = singleExtend;
						}
						else
						{
							singleExtend.insert( 0, "," );
							extend.insert( curPos, singleExtend );
						}
					}
					MergeTemplateXmlNodes( current, parent );
				}
			}
			singleExtend = extend.Tokenize(",",curPos);
		}

		if ( !extend.empty() )
		{
			assert( "!Potential circular inheritance of animation graph templates!" );
		}
	}

	// phase 3: create the templates from merged XML nodes
	std::map<string, CAnimationGraphTemplatePtr> templates;
	for ( itXmls = templateXmls.begin(); itXmls != itXmlsEnd; ++itXmls )
	{
		XmlNodeRef rootFile = itXmls->second;
		CAnimationGraphTemplatePtr pTemplate = new CAnimationGraphTemplate();
		if (!pTemplate->Init( rootFile ))
		{
			GameWarning("Failed loading animation graph template %s", itXmls->first.c_str());
			ok = false;
		}
		templates[ rootFile->getAttr("name") ] = pTemplate;
	}

	// phase 4: run through all states and see if they use templates, and if so, replace them
	XmlNodeRef nodeStates = rootGraph->findChild("States");
	if (!nodeStates)
		ok = false;
	else
	{
		int nStates = nodeStates->getChildCount();
		for (int i=0; i<nStates; i++)
		{
//			if (0 == stricmp("asianTruck_passenger01IdleBreak_01", nodeStates->getChild(i)->getAttr("id")))
//				DEBUG_BREAK;
			while (XmlNodeRef templateChild = nodeStates->getChild(i)->findChild("Template"))
			{
				CAnimationGraphTemplatePtr pTemplate = templates[templateChild->getAttr("name")];
				if (pTemplate)
					ok &= pTemplate->ProcessTemplate(templateChild);
				else if (0 == strcmp("Default", templateChild->getAttr("name")))
				{
				}
				else
				{
					CryLogAlways("No such template %s", templateChild->getAttr("name"));
					ok = false;
				}
				nodeStates->getChild(i)->removeChild(templateChild);
			}
		}
	}

	return ok;
}

const char * CAnimationGraph::GetName()
{
	return m_name.c_str();
}

void CAnimationGraph::FindDeadInputValues( IAnimationGraphDeadInputReportCallback * pCallback )
{
	struct IntermediateCallback : public CStateIndex::IInputSetCallback
	{
		_smart_ptr<CAnimationGraph> pGraph;
		IAnimationGraphDeadInputReportCallback * pCallback;

		IntermediateCallback( _smart_ptr<CAnimationGraph> pG, IAnimationGraphDeadInputReportCallback * pC ) : pGraph(pG), pCallback(pC) {}

		virtual void FoundInputSet( CStateIndex::InputValue * pInputs )
		{
			typedef std::pair<string,string> SP;
			typedef std::pair<const char*, const char*> CPP;

			std::vector<SP> values;
			for (int i=0; i<pGraph->m_numInputIDs; i++)
			{
				string name = pGraph->m_inputValues[i]->name;
				char value[256];
				pGraph->m_inputValues[i]->DecodeInput(value, pInputs[i]);
				values.push_back( SP(name, value) );
			}
			std::vector<CPP> valuesCP;
			for (std::vector<SP>::const_iterator iter = values.begin(); iter != values.end(); ++iter)
				valuesCP.push_back( CPP(iter->first.c_str(), iter->second.c_str()) );
			valuesCP.push_back( CPP(0,0) );
			pCallback->OnDeadInputValues( &valuesCP[0] );
		}
	};
	IntermediateCallback callback(this,pCallback);
	m_stateIndex.FindNoAnimationInputSets( &callback );
}

void CAnimationGraph::CalculateStatePositions()
{

}

bool CAnimationGraph::IsStateLooped( StateID state )
{
	static int paramsSlot = CCryAction::GetCryAction()->GetAnimationGraphManager()->GetCategory( CAGParamsLayer<0>().GetCategory() )->overrideSlot;

  bool looped = false;
  while (state != INVALID_STATE)
  {
    IAnimationStateNodeFactory * pFactory = 0;
    for (int i=0; !pFactory && i<m_states[state].factoryLength; i++)
      if (m_factorySlotIndices[m_states[state].factoryStart+i] == paramsSlot)
        pFactory = m_factories[m_states[state].factoryStart+i];    
    if (pFactory)
    {
      looped = static_cast<CAGParams*>(pFactory)->IsLooped();
      break;
    }    
    state = m_states[state].parentState;
  }

  return looped;
}

void CAnimationGraph::GetMemoryStatistics(ICrySizer * s)
{
	s->Add(*this);
	s->Add(m_name);
	{
		SIZER_SUBCOMPONENT_NAME(s, "Inputs");
		s->AddContainer(m_inputValues);
		s->AddContainer(m_blendWeightInputValues);
		for (size_t i=0; i<m_inputValues.size(); i++)
			m_inputValues[i]->GetMemoryStatistics(s);
		s->AddContainer(m_blendValueIDs);
	}
	{
		SIZER_SUBCOMPONENT_NAME(s, "States");
		s->AddContainer(m_states);
		s->AddContainer(m_stateNameToID);
	}
	{
		SIZER_SUBCOMPONENT_NAME(s,"Links");
		s->AddContainer(m_links);
	}
	m_stateIndex.GetMemoryStatistics(s);
	{
		SIZER_SUBCOMPONENT_NAME(s,"Factories");
		s->AddContainer(m_factorySlotIndices);
		s->AddContainer(m_factories);
		for (size_t i=0; i<m_factories.size(); i++)
			m_factories[i]->GetFactoryMemoryStatistics(s);
	}
	{
		SIZER_SUBCOMPONENT_NAME(s,"Outputs");
		s->AddContainer(m_stringToOutputHigh);
		s->AddContainer(m_stringToOutputLow);
		s->AddContainer(m_outputs);
	}
	{
		SIZER_SUBCOMPONENT_NAME(s,"PathFind");
		s->AddContainer(m_statePathfindState);
		s->AddContainer(m_openMap);
	}
}
