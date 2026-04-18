#include "StdAfx.h"
#include "StateIndex.h"
#include <iterator>
#include <IAnimationStateNode.h>

#define LOG_QUERY_PROGRESS 0


//helper for AG serialization
/*
struct FileSerializationHelper;
{
	bool reading;
	bool swappingEndian;
	FILE *file;
	ICryPak *pak;

	FileSerializationHelper() : file(NULL), pak(NULL)
	{}

	FileSerializationHelper(bool read, FILE* f, ICryPak* p)
	{
		Init(read, f, p);
	}

	void Init(bool read, FILE* f, ICryPak* p)
	{
		reading = read;
		file = f;
		pak = p;
		swappingEndian = false;
		#ifdef NEED_ENDIAN_SWAP
			swappingEndian = true;
		#endif
	}

	void StringValue(string *s)
	{
		if(!file)
			return;
		if(reading)
		{
			int size = 0;
			pak->FRead(&size, 1, file, swappingEndian);
			char *str = new char[size];
			pak->FRead(str, size, file, swappingEndian);
			s->clear();
			s->append(str);
			assert(s->length() == size-1);
			delete[] str;
		}
		else
		{
			int size = 1+ s->length();
			pak->FWrite(&size, 1, file);
			pak->FWrite(s->c_str(), size, file);
		}
	}

	template<class T>
	void Value(T* data, int elements = 1)
	{
		if(!file)
			return;
		if(reading)
			pak->FRead(data, elements, file, swappingEndian);
		else
			pak->FWrite(data, elements, file);
	}
};
*/

void CStateIndex::InputData::SerializeAsFile(bool reading, AG_FILE* pAGI)
{
	FileSerializationHelper h(reading, pAGI);

	h.Value(&id);
	h.Value(&value);
	h.Value(&numStates);
	h.Value(&numExplicitStates);
	h.Value(&priority);
	h.Value(&dbOffset);
	h.Value(&dbExplicitOffset);
}

CStateIndex::CStateIndex()
{
}

void CStateIndex::Init( std::auto_ptr<Builder> builder )
{
	m_database.resize(0);
	m_inputData.resize(0);
	m_numInputIDs = 0;
	m_numStates = 0;
  m_bKeepLowValueRankings = false;

	if (builder->m_declaredInputs.empty())
	{
		assert( builder->m_restrictions.empty() );
		return;
	}
	// keep a copy of all declared states around for when we're passed no query info
	std::copy( builder->m_declaredStates.begin(), builder->m_declaredStates.end(), back_inserter(m_database) );
	m_numStates = m_database.size();
	// number of input id's
	m_numInputIDs = builder->m_declaredInputs.rbegin()->first + 1;
	// copy the data we need
	InputData selector;
	for (Builder::MapRestrictions::iterator iR = builder->m_restrictions.begin(); iR != builder->m_restrictions.end(); ++iR)
	{
		selector.id = iR->first;
		for (Builder::MapValueStates::iterator iV = iR->second.begin(); iV != iR->second.end(); ++iV)
		{
			selector.value = iV->first;
			selector.numStates = iV->second.size();
			selector.dbOffset = m_database.size();
			std::copy( iV->second.begin(), iV->second.end(), back_inserter(m_database) );
			const Builder::SetStates& explicitStates = builder->m_explicit[selector.id][selector.value];
			selector.numExplicitStates = explicitStates.size();
			selector.dbExplicitOffset = m_database.size();
			std::copy( explicitStates.begin(), explicitStates.end(), back_inserter(m_database) );
			selector.priority = builder->m_priorities[selector.id];
			m_inputData.push_back(selector);

/* debug code:
			string output;
			for (Builder::SetStates::iterator iter = iV->second.begin(); iter != iV->second.end(); ++iter)
				output += string().Format("%.4x ", *iter);
			output += "| ";
			for (Builder::SetStates::const_iterator iter = explicitStates.begin(); iter != explicitStates.end(); ++iter)
				output += string().Format("%.4x ", *iter);
			CryLogAlways("%.2x@%.2x: %s", iR->first, iV->first, output.c_str());
//*/
		}
	}

	int size_before = m_database.size();

	for (size_t i=1; i<m_inputData.size(); i++)
	{
		InputData& curDat = m_inputData[i];

		if (curDat.numStates && curDat.numStates < curDat.dbOffset)
		{
			for (size_t ofs1=0; ofs1<curDat.dbOffset - curDat.numStates; ofs1++)
			{
				if (m_database[ofs1] == m_database[curDat.dbOffset])
				{
					bool same = true;
					for (size_t ofs2=1; same && ofs2<curDat.numStates; ofs2++)
						same &= m_database[ofs1 + ofs2] == m_database[curDat.dbOffset + ofs2];
					if (same)
					{
						for (size_t ofs3=curDat.dbOffset+curDat.numStates; ofs3 < m_database.size(); ofs3++)
							m_database[ofs3 - curDat.numStates] = m_database[ofs3];
						m_database.resize(m_database.size() - curDat.numStates);
						curDat.dbOffset = ofs1;
						curDat.dbExplicitOffset -= curDat.numStates;
						for (size_t j=i+1; j<m_inputData.size(); j++)
						{
							m_inputData[j].dbOffset -= curDat.numStates;
							m_inputData[j].dbExplicitOffset -= curDat.numStates;
						}
						break;
					}
				}
			}
		}
		if (curDat.numExplicitStates && curDat.numExplicitStates < curDat.dbOffset)
		{
			for (size_t ofs1=0; ofs1<curDat.dbExplicitOffset - curDat.numExplicitStates; ofs1++)
			{
				if (m_database[ofs1] == m_database[curDat.dbExplicitOffset])
				{
					bool same = true;
					for (size_t ofs2=1; same && ofs2<curDat.numExplicitStates; ofs2++)
						same &= m_database[ofs1 + ofs2] == m_database[curDat.dbExplicitOffset + ofs2];
					if (same)
					{
						for (size_t ofs3=curDat.dbExplicitOffset+curDat.numExplicitStates; ofs3 < m_database.size(); ofs3++)
							m_database[ofs3 - curDat.numExplicitStates] = m_database[ofs3];
						m_database.resize(m_database.size() - curDat.numExplicitStates);
						curDat.dbExplicitOffset = ofs1;
						for (size_t j=i+1; j<m_inputData.size(); j++)
						{
							m_inputData[j].dbOffset -= curDat.numExplicitStates;
							m_inputData[j].dbExplicitOffset -= curDat.numExplicitStates;
						}
						break;
					}
				}
			}
		}
	}

	int size_after = m_database.size();

	CryLog("Similar state compression saved %d bytes", (size_before - size_after) * sizeof(m_database[0]));
}

void CStateIndex::Swap( CStateIndex& rhs )
{
	m_inputData.swap( rhs.m_inputData );
	m_rankings.swap( rhs.m_rankings );
	m_database.swap( rhs.m_database );
	std::swap( m_numInputIDs, rhs.m_numInputIDs );
	std::swap( m_numStates, rhs.m_numStates );
	m_queryInputs.swap( rhs.m_queryInputs );
	m_intermediateQuery[0].swap( rhs.m_intermediateQuery[0] );
	m_intermediateQuery[1].swap( rhs.m_intermediateQuery[1] );
	std::swap( m_bKeepLowValueRankings, rhs.m_bKeepLowValueRankings );
}

void CStateIndex::Query( const InputValue * query, StateIDVec& result, QueryStats& stats )
{
//	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	// cleanup result
	result.resize(0);

	// cleanup cache-data structures
	m_queryInputs.resize(0);

	// determine the set of input values that need to be queried
	InputDataVec::const_iterator iterInputData = m_inputData.begin();
	for (InputID id = 0; id < m_numInputIDs; id++)
	{
		if (query[id] != INPUT_VALUE_DONT_CARE)
		{
			InputData tempInputData;
			tempInputData.id = id;
			tempInputData.value = query[id];
			iterInputData = std::lower_bound( iterInputData, (InputDataVec::const_iterator) m_inputData.end(), tempInputData, InputData::CompareIDValue() );
			if (iterInputData != m_inputData.end() && iterInputData->id == id && iterInputData->value == query[id])
				m_queryInputs.push_back(*iterInputData);
		}
	}

	stats.databaseSize = m_database.size();
	stats.inputDataSize = m_inputData.size();
	stats.nInputsConsidered = m_queryInputs.size();
	stats.nQueryableStates = m_numStates;
	stats.nIntersectionValuesTouched = 0;
	stats.nRankingValuesTouched = 0;
	stats.nInputsUsedForRanking = 0;

	// perform the actual queries
	switch (m_queryInputs.size())
	{
	case 0:
		// no queries need performing: this is bad - all states are a result
		GameWarning( "Performance warning: input to CStateIndex::Query yielded no search results... returning all possible states (%d of them)!", m_numStates._value );
		std::copy( &m_database[0], &m_database[m_numStates], back_inserter(result) );
		break;
	case 1:
		// only one query - we can just copy directly into result (easy!)
		result.reserve( m_queryInputs[0].numStates );
		stats.nInputsUsedForRanking = 1;
		std::copy( m_queryInputs[0].Begin(m_database), m_queryInputs[0].End(m_database), back_inserter(result) );
		RankResults( result, stats );
		break;
	default:
		// multiple queries... separate function!
		MultipleQuery( result, stats );
		RankResults( result, stats );
		break;
	}
}

void CStateIndex::MultipleQuery( StateIDVec& result, QueryStats& stats )
{
	// sort the queries in ascending order of allowed states
	std::sort( m_queryInputs.begin(), m_queryInputs.end(), InputData::ComparePriorityThenNumStates() );

	result.resize(0);

	const StateID * begins[32];
	const StateID * it[32];
	const StateID * ends[32];
	stats.nInputsUsedForRanking = 0;
	for ( int i = 0; i < m_queryInputs.size(); ++i )
	{
		begins[i] = m_queryInputs[i].Begin(m_database);
		ends[i] = m_queryInputs[i].End(m_database);
		if ( begins[i] != ends[i] )
			stats.nInputsUsedForRanking++;
		else
			break;
	}

	int numInputsThisRound = 0;
	while ( result.empty() && stats.nInputsUsedForRanking > 0 )
	{
		if ( stats.nInputsUsedForRanking == 1 )
		{
			std::copy( begins[0], ends[0], back_inserter(result) );
			break;
		}
		for ( int i = 0; i < stats.nInputsUsedForRanking; ++i )
			it[i] = begins[i];
		numInputsThisRound = 0;
		while ( it[0] != ends[0] )
		{
			if ( numInputsThisRound == 0 )
				numInputsThisRound = 1;

			StateID stateID = *(it[0]);
			for ( int i = 1; i < stats.nInputsUsedForRanking; ++i )
			{
				if ( numInputsThisRound <= i )
					numInputsThisRound = i+1;

				it[i] = std::lower_bound( it[i], ends[i], stateID );
				if ( it[i] == ends[i] )
				{
					stateID = ~0;
					it[0] = ends[0];
					break;
				}
				if ( stateID != *(it[i]) )
				{
					stateID = *(it[i]);
					break;
				}
			}
			if ( stateID == *(it[0]) )
			{
				result.push_back( stateID );
				++(it[0]);
			}
			else
			{
				it[0] = std::lower_bound( it[0], ends[0], stateID );
			}
		}
		if ( result.empty() && numInputsThisRound >= 2 )
			stats.nInputsUsedForRanking = numInputsThisRound - 1;
	}
	stats.nInputsUsedForRanking = numInputsThisRound;

// this is how was this function before optimization:
/*
	// sort the queries in ascending order of allowed states
	std::sort( m_queryInputs.begin(), m_queryInputs.end(), InputData::ComparePriorityThenNumStates() );

	// first query is pretty much a no-op, just store the pointer to the results
	const StateID * curBegin = m_queryInputs[0].Begin(m_database);
	const StateID * curEnd = m_queryInputs[0].End(m_database);
	int intermediateIdx = 0;
	stats.nInputsUsedForRanking ++;

	if ( curBegin == curEnd )
	{
		result.resize(0);
		return; // early out!
	}

	DumpQueryInfo( 0, curBegin, curEnd );

	for (size_t queryIdx = 1; queryIdx < m_queryInputs.size(); ++queryIdx)
	{
		StateIDVec * pOutputVec;
		// if we're on the last query, we want to store in result, otherwise
		// we'll use our intermediate vectors
		if (queryIdx != m_queryInputs.size()-1)
      pOutputVec = m_intermediateQuery + intermediateIdx;
		else
			pOutputVec = &result;
		pOutputVec->resize(0);

		// perform the intersection operation
		// TODO: it ought to be possible to provide an optimized (sub-linear) version of this step, 
		//   because we have random-iterators and know that the first set is always smaller than the second
		//   OR it may be worthwhile compressing the state sets, in order to improve memory bandwidth usage
		const StateID * pBegin =  m_queryInputs[queryIdx].Begin(m_database);
		const StateID * pEnd = m_queryInputs[queryIdx].End(m_database);
		stats.nIntersectionValuesTouched += max((pEnd - pBegin), (curEnd - curBegin));
		std::set_intersection( curBegin, curEnd, pBegin, pEnd, back_inserter(*pOutputVec) );
		// update the current result pointers
		if (!pOutputVec->empty())
		{
			curBegin = &*pOutputVec->begin();
			curEnd = (const StateID*)(&pOutputVec->back()) + 1;
			stats.nInputsUsedForRanking ++;
		}
		else
		{
			// we exhausted the input states; since we sorted in prioritized order, just use the previous
			// set of states as our output (since it's probably the best we'll get - and better to play
			// a somewhat matching animation than none)
			result.resize(0);
			result.insert(result.end(), curBegin, curEnd);
			assert(!result.empty());
			return; // early out!
		}
		DumpQueryInfo( queryIdx, curBegin, curEnd );
		// and toggle the intermediate buffer
		intermediateIdx ^= 1;
	}
*/
}

ILINE void CStateIndex::DumpQueryInfo( size_t i, const StateID * curBegin, const StateID * curEnd )
{
#if LOG_QUERY_PROGRESS
	string output;
	output += string().Format("QUERY(%d;in=%d,val=%d)", i, m_queryInputs[i].id, m_queryInputs[i].value);
	while (curBegin != curEnd)
	{
		output += string().Format(" %d", *curBegin);
		++curBegin;
	}
	CryLogAlways("%s", output.c_str());
#endif
}

int CStateIndex::GetRankings( const InputValue * query, std::vector<uint16>& rankings )
{
	StateIDVec temp;
	QueryStats stats;
	Query( query, temp, stats );

	rankings.resize(0);
	rankings.reserve(m_rankings.size());
	for (size_t i=0; i<m_rankings.size(); ++i)
		rankings.push_back( m_rankings[i].ranking );

	return stats.nInputsUsedForRanking;
}

void CStateIndex::RankResults( StateIDVec& result, QueryStats& stats )
{
//	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	if (result.size() <= 1)
		return;

	m_rankings.resize(0);
	m_rankings.reserve(result.size());
	for (StateIDVec::const_iterator iterState = result.begin(); iterState != result.end(); ++iterState)
	{
		SRankingNode n;
		n.state = *iterState;
		n.ranking = 0;
		m_rankings.push_back( n );
	}
	for (InputDataVec::const_iterator iterInput = m_queryInputs.begin(); m_rankings.size() > 1 && iterInput != m_queryInputs.end(); ++iterInput)
	{
		const StateID * pBegin = iterInput->BeginExplicit(m_database);
		const StateID * pEnd = iterInput->EndExplicit(m_database);
		for (std::vector<SRankingNode>::iterator iterRanking = m_rankings.begin(); iterRanking != m_rankings.end(); ++iterRanking)
		{
			stats.nRankingValuesTouched += (pEnd - pBegin);
			iterRanking->ranking += iterInput->priority * std::binary_search( pBegin, pEnd, iterRanking->state );
		}
		std::sort(m_rankings.begin(), m_rankings.end());
		if ( !m_bKeepLowValueRankings )
		{
			while (m_rankings.back().ranking != m_rankings.front().ranking)
				m_rankings.pop_back();
		}
	}
	for (size_t i=0; i<m_rankings.size(); i++)
		result[i] = m_rankings[i].state;
	result.resize(m_rankings.size());

#if 0
	m_rankings.resize(0);
	m_rankings.reserve(result.size());
	for (StateIDVec::const_iterator iterState = result.begin(); iterState != result.end(); ++iterState)
	{
		SRankingNode n;
		n.state = *iterState;
		n.ranking = 0;
		for (InputDataVec::const_iterator iterInput = m_queryInputs.begin(); iterInput != m_queryInputs.end(); ++iterInput)
		{
			const StateID * pBegin = iterInput->BeginExplicit(m_database);
			const StateID * pEnd = iterInput->EndExplicit(m_database);
			stats.nRankingValuesTouched += (pEnd - pBegin);
			n.ranking += iterInput->priority * std::binary_search( pBegin, pEnd, n.state );
		}
		m_rankings.push_back(n);
	}
	std::sort(m_rankings.begin(), m_rankings.end());
	uint16 bestRanking = m_rankings[0].ranking;
	size_t i;
	for (i=0; i<m_rankings.size() && m_rankings[i].ranking == bestRanking; i++)
		result[i] = m_rankings[i].state;
	result.resize(i);
#endif
}

bool CStateIndex::StateMatchesInput( StateID state, CStateIndex::InputID input, CStateIndex::InputValue value, unsigned flags )
{
	InputData tempInputData;
	tempInputData.id = input;
	tempInputData.value = value;
	InputDataVec::const_iterator iterInputData = std::lower_bound( m_inputData.begin(), m_inputData.end(), tempInputData, InputData::CompareIDValue() );

	// see if we get an exact match
	if (iterInputData != m_inputData.end() && iterInputData->id == input && iterInputData->value == value)
	{
		const StateID * begin, * end;
		if (flags & eSMIF_ConsiderMatchesAny)
		{
			begin = iterInputData->Begin(m_database);
			end = iterInputData->End(m_database);
		}
		else
		{
			begin = iterInputData->BeginExplicit(m_database);
			end = iterInputData->EndExplicit(m_database);
		}
		return std::binary_search( begin, end, state );
	}

	// fallback 1: do we *ever* do matching on this input (if not, assume its a match, regardless of considerMatchesAny)
	// this is a degenerate case!
	if (!(flags & eSMIF_ConsiderMatchesAny) && !(flags & eSMIF_EnforceMatchesInput))
	{
		tempInputData.value = 0;
		iterInputData = std::lower_bound( m_inputData.begin(), m_inputData.end(), tempInputData, InputData::CompareIDValue() );
		if (iterInputData == m_inputData.end() || iterInputData->id != input)
			return true;
	}

	// should we match anything
	if (flags & eSMIF_ConsiderMatchesAny)
    return true;
  else
    return false;
}

bool CStateIndex::StateMatchesExplicitly( StateID state, CStateIndex::InputID input )
{
	InputData tempInputData;
	tempInputData.id = input;
	tempInputData.value = 0;
	InputDataVec::const_iterator iterInputData = std::lower_bound( m_inputData.begin(), m_inputData.end(), tempInputData, InputData::CompareIDValue() );

	while (iterInputData != m_inputData.end() && iterInputData->id == input)
	{
		if (std::binary_search( iterInputData->Begin(m_database), iterInputData->End(m_database), state ))
			return true;
		++iterInputData;
	}

	return false;
}

void CStateIndex::SetKeepLowValueRankings( bool bKeep )
{
	m_bKeepLowValueRankings = bKeep;
}

bool CStateIndex::GetKeepLowValueRankings()
{
	return m_bKeepLowValueRankings;
}

void CStateIndex::FindNoAnimationInputSets( CStateIndex::IInputSetCallback* pCallback )
{
	typedef std::vector<InputValue> VInputValue;

	VInputValue topValues;
	topValues.resize(m_numInputIDs, 0);
	uint64 nQueries = 1;
	for (size_t input=0; input<m_numInputIDs; input++)
	{
		InputData tempInputData;
		tempInputData.id = input;
		tempInputData.value = 0;
		InputDataVec::const_iterator iterInputData = std::lower_bound( m_inputData.begin(), m_inputData.end(), tempInputData, InputData::CompareIDValue() );
		while (iterInputData != m_inputData.end() && iterInputData->id == input)
		{
			topValues[input] = std::max(topValues[input], InputValue(iterInputData->value+1));
			++iterInputData;
		}
		nQueries *= (topValues[input]+1);
	}

#if defined(__GNUC__)
	CryLogAlways("Performing %lld queries", (long long)nQueries);
#else
	CryLogAlways("Performing %I64d queries", nQueries);
#endif

	VInputValue inputValues;
	inputValues.resize(m_numInputIDs, 0);
	for (size_t input=0; input<m_numInputIDs; input++)
		if (!topValues[input])
			inputValues[input] = INPUT_VALUE_DONT_CARE;
	StateIDVec queryResult;
	QueryStats stats;

	uint64 queryNum = 0;
	while (true)
	{
		queryNum++;
		if ((queryNum & 0xfffff) == 0)
		{
#if defined(__GNUC__)
			CryLogAlways("Performing query %lld/%lld", (long long)queryNum, (long long)nQueries);
#else
			CryLogAlways("Performing query %I64d/%I64d", queryNum, nQueries);
#endif
		}

		queryResult.resize(0);
		Query( &inputValues[0], queryResult, stats );
		if (pCallback && queryResult.empty())
			pCallback->FoundInputSet( &inputValues[0] );
		
		size_t incInput = 0;
		while (true)
		{
			if (topValues[incInput] == 0)
			{
				++incInput;
				if (incInput >= m_numInputIDs)
					return;
				else
					continue;
			}
			if (inputValues[incInput] == INPUT_VALUE_DONT_CARE)
			{
				inputValues[incInput] = 0;
				++incInput;
				if (incInput >= m_numInputIDs)
					return;
				else
					continue;
			}
			inputValues[incInput] ++;
			if (inputValues[incInput] == topValues[incInput])
				inputValues[incInput] = INPUT_VALUE_DONT_CARE;
			break;
		}
	}
}

void CStateIndex::SerializeAsFile(bool reading, AG_FILE* pAGI)
{
	assert(pAGI);
	if(!pAGI)		//this is part of the animation graph serialization
		return;

	FileSerializationHelper h(reading, pAGI);
	int inputs = m_inputData.size();  //number of input data
	h.Value(&inputs);
	if(reading)
	{
		m_inputData.clear();
		m_inputData.resize(inputs);
	}
	for(int i = 0; i < inputs; ++i)
		m_inputData[i].SerializeAsFile(reading, pAGI);
	int stateIds = m_database.size();
	h.Value(&stateIds);
	if(reading)
	{
		m_database.clear();
		m_database.resize(stateIds);
	}
	for(int i = 0; i < stateIds; ++i)
		h.Value(&m_database[i]._value);
	h.Value(&m_numInputIDs);
	h.Value(&m_numStates._value);
	int queryInputs = m_queryInputs.size();
	h.Value(&queryInputs);
	if(reading)
	{
		m_queryInputs.clear();
		m_queryInputs.resize(queryInputs);
	}
	for(int i = 0; i < queryInputs; ++i)
		m_queryInputs[i].SerializeAsFile(reading, pAGI);
	if (reading)
	{
		m_bKeepLowValueRankings = false;
	}
}

CStateIndex::Builder::Builder()
{
}

CStateIndex::Builder::~Builder()
{
}

void CStateIndex::Builder::DeclInputValue( InputID input, InputValue value, uint8 priority )
{
	m_declaredInputs[input].insert(value);
	m_priorities[input] = priority;
}

void CStateIndex::Builder::DeclState( StateID state )
{
	m_declaredStates.insert(state);
	for (MapRestrictions::iterator iR = m_restrictions.begin(); iR != m_restrictions.end(); ++iR)
		for (MapValueStates::iterator iV = iR->second.begin(); iV != iR->second.end(); ++iV)
			iV->second.insert(state);
}

void CStateIndex::Builder::RestrictStateToInputValues( StateID state, InputID input, InputValue * pValues, size_t numValues )
{
	std::sort( pValues, pValues + numValues );

	assert( m_declaredStates.find(state) != m_declaredStates.end() );
	MapInputValues::iterator iI = m_declaredInputs.find(input);
	assert( iI != m_declaredInputs.end() );
	if ( iI == m_declaredInputs.end() )
		return;

#ifndef NDEBUG
	for (size_t i=0; i<numValues; i++)
		assert( iI->second.find(pValues[i]) != iI->second.end() );
#endif

	MapRestrictions::iterator iR = m_restrictions.find(input);
	if (iR == m_restrictions.end())
		iR = m_restrictions.insert( std::make_pair(input, MapValueStates()) ).first;
	// strike out this state from being available on all other tables for this input, except the one
	// that we're restricted to
	for (SetInputValues::const_iterator iX = iI->second.begin(); iX != iI->second.end(); ++iX)
	{
		if (std::binary_search(pValues, pValues + numValues, *iX))
			continue;
		MapValueStates::iterator iV = iR->second.find(*iX);
		if (iV == iR->second.end())
		{
			// we haven't initialized this table yet, suppose we'd better!
			iV = iR->second.insert( std::make_pair(*iX, SetStates()) ).first;
			iV->second = m_declaredStates;
		}
		iV->second.erase( state );
	}

	iR = m_explicit.find(input);
	if (iR == m_explicit.end())
		iR = m_explicit.insert( std::make_pair(input, MapValueStates()) ).first;
	for (SetInputValues::const_iterator iX = iI->second.begin(); iX != iI->second.end(); ++iX)
	{
		if (std::binary_search(pValues, pValues + numValues, *iX))
		{
			MapValueStates::iterator iV = iR->second.find(*iX);
			if (iV == iR->second.end())
				iV = iR->second.insert( std::make_pair(*iX, SetStates()) ).first;
			iV->second.insert( state );
		}
	}
}

void CStateIndex::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "StateIndex");
	s->AddContainer(m_inputData);
	s->AddContainer(m_rankings);
	s->AddContainer(m_database);
	s->AddContainer(m_queryInputs);
	s->AddContainer(m_intermediateQuery[0]);
	s->AddContainer(m_intermediateQuery[1]);
}
