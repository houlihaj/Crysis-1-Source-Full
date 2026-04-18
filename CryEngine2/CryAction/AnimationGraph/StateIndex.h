#ifndef __STATEINDEX_H__
#define __STATEINDEX_H__

#pragma once

enum EStateMatchesInputFlags
{
  eSMIF_ConsiderMatchesAny = 0x0001,
  eSMIF_EnforceMatchesInput = 0x0002,
};

struct AG_FILE;

// a state index is supposed to allow efficient queries on a set of input data
// as a support structure for animation selection
class CStateIndex
{
public:
	typedef uint8 InputID;
	typedef uint8 InputValue;
	static const InputValue INPUT_VALUE_DONT_CARE = ~InputValue(0);
	struct StateID
	{
		StateID() {}
		StateID( uint16 id ) : _value(id) {}
		operator uint16 () const { return _value; }
		uint16 _value;

    AUTO_STRUCT_INFO
	};
	typedef std::vector<StateID> StateIDVec;
	class Builder;

	CStateIndex();
	void Init( std::auto_ptr<Builder> );
	void Swap( CStateIndex& rhs );

	struct IInputSetCallback
	{
		virtual void FoundInputSet( InputValue * pValues ) = 0;
	};

	struct QueryStats
	{
		int databaseSize;
		int inputDataSize;
		int nInputsConsidered;
		int nQueryableStates;
		int nIntersectionValuesTouched;
		int nRankingValuesTouched;
		int nInputsUsedForRanking;
	};

	void Query( const InputValue * query, StateIDVec& result, QueryStats& stats );
	// must be called after one query and before the next if rankings are needed
	int GetRankings( const InputValue * query, std::vector<uint16>& rankings );

	bool StateMatchesInput( StateID state, InputID input, InputValue value, unsigned flags /*eSMIF_**/ );
	bool StateMatchesExplicitly( StateID state, InputID input );

	void SetKeepLowValueRankings( bool bKeep );
	bool GetKeepLowValueRankings();

	void FindNoAnimationInputSets( IInputSetCallback * pCallback );
	void SerializeAsFile(bool reading, AG_FILE* pAGI);

	void GetMemoryStatistics(ICrySizer * s);

private:
	void MultipleQuery( StateIDVec& result, QueryStats& stats );
	void RankResults( StateIDVec& result, QueryStats& stats );
	void DumpQueryInfo( size_t i, const StateID * curBegin, const StateID * curEnd );

	struct InputData
	{
		InputID id;
		InputValue value;
		uint16 numStates;
		uint16 numExplicitStates;
		uint8 priority;
		// uint8 spare
		uint32 dbOffset;
		uint32 dbExplicitOffset;

		struct CompareIDValue
		{
			ILINE bool operator()( const InputData& lhs, const InputData& rhs ) const
			{
				return lhs.id < rhs.id || (lhs.id == rhs.id && lhs.value < rhs.value);
			}
		};
		struct ComparePriorityThenNumStates
		{
			ILINE bool operator()( const InputData& lhs, const InputData& rhs ) const
			{
				return (lhs.priority > rhs.priority) || (lhs.priority == rhs.priority && lhs.numStates < rhs.numStates);
			}
		};

		void SerializeAsFile(bool reading, AG_FILE* pAGI);

		const StateID* Begin( const StateIDVec& db ) const
		{
			return &db[0] + dbOffset;
		}
		const StateID* End( const StateIDVec& db ) const
		{
			return &db[0] + dbOffset + numStates;
		}
		const StateID* BeginExplicit( const StateIDVec& db ) const
		{
			return &db[0] + dbExplicitOffset;
		}
		const StateID* EndExplicit( const StateIDVec& db ) const
		{
			return &db[0] + dbExplicitOffset + numExplicitStates;
		}
	};

	struct SRankingNode
	{
		StateID state;
		uint16 ranking;
		bool operator<( const SRankingNode& rhs ) const
		{
			return ranking > rhs.ranking || (ranking == rhs.ranking && state > rhs.state);
		}
	};

	// REMEMBER: - update Swap()
	//					 - if you add members, you have to update SerializeAsFile
	typedef std::vector<InputData> InputDataVec;
	InputDataVec m_inputData;
	std::vector<SRankingNode> m_rankings;
	StateIDVec m_database;
	InputID m_numInputIDs;
	StateID m_numStates;

	// This member is not serialized
	bool m_bKeepLowValueRankings;

	// cache data - avoid unnecessary memory allocation
	InputDataVec m_queryInputs;
	StateIDVec m_intermediateQuery[2];

	class CCompareRankings;
};

// this class categorizes data so that it can be turned into a fast-to-query static dataset
// for the CStateIndex
class CStateIndex::Builder
{
	friend class CStateIndex;

public:
	Builder();
	~Builder();
	void DeclInputValue( InputID input, InputValue value, uint8 priority );
	void DeclState( StateID state );
  void RestrictStateToInputValue( StateID state, InputID input, InputValue value )
	{
		RestrictStateToInputValues( state, input, &value, 1 );
	}
	void RestrictStateToInputValues( StateID state, InputID input, InputValue * values, size_t numValues );

private:
	typedef std::set<StateID> SetStates;
	typedef std::set<InputValue> SetInputValues;
	typedef std::map<InputID, SetInputValues> MapInputValues;
	typedef std::map<InputValue, SetStates> MapValueStates;
	typedef std::map<InputID, MapValueStates> MapRestrictions;
	typedef std::map<InputID, uint8> MapPriorities;
	MapRestrictions m_restrictions;
	MapRestrictions m_explicit;
	SetStates m_declaredStates;
	MapInputValues m_declaredInputs;
	MapPriorities m_priorities;
};

#endif
