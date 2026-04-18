#include "StdAfx.h"

#include "AnimationGraphStates.h"


#define SIMPLE_RECURSION(f)		{ for (int i = 0; i < m_layers.size(); ++i) { m_layers[i]->f; } }

#define INPUTID_RECURSION(inputID,f) \
{ \
	std::map<InputID, std::vector<InputID> >::iterator itLayerInputsIDs = m_wrapperToLayerInputIDs.find( inputID ); \
	if ( itLayerInputsIDs != m_wrapperToLayerInputIDs.end() ) \
	{ \
		std::vector< InputID >& layerInputIDs = itLayerInputsIDs->second; \
		for ( int layer = 0; layer < m_layers.size(); ++layer ) \
		{ \
			InputID layerInputID = layerInputIDs[layer]; \
			if ( layerInputID != InputID(-1) ) \
				m_layers[layer]->f; \
		} \
	} \
}

#define AG_SEND_EVENT( func ) \
{ \
	m_callingListeners = m_listeners; \
	for (std::vector<SListener>::reverse_iterator agSendEventIter = m_callingListeners.rbegin(); agSendEventIter != m_callingListeners.rend(); ++agSendEventIter) \
	agSendEventIter->pListener->func; \
}


CAnimationGraphStates::~CAnimationGraphStates()
{
	while ( !m_layers.empty() )
		DestroyedState( m_layers[0] );
	AG_SEND_EVENT( DestroyedState(this) );
}

void CAnimationGraphStates::DestroyedState( IAnimationGraphState* agState )
{
	for ( LayerIndex i = 0; i < m_layers.size(); ++i )
	{
		if ( m_layers[i] == agState )
		{
			std::list< LayerListener >::iterator itListeners = m_layerListeners.begin();
			while ( itListeners != m_layerListeners.end() )
			{
				if ( itListeners->layerIndex == i )
				{
					agState->RemoveListener( &*itListeners );
					m_layerListeners.erase( itListeners++ );
				}
				else if ( itListeners->layerIndex > i )
				{
					itListeners->layerIndex--;
					++itListeners;
				}
				else
					++itListeners;
			}

			QueryIDBindings::iterator itBindings = m_QueryIDBindings.begin();
			while ( itBindings != m_QueryIDBindings.end() )
			{
				if ( itBindings->layerIndex == i )
				{
					m_QueryIDBindings.erase( itBindings++ );
				}
				else if ( itBindings->layerIndex > i )
				{
					itBindings->layerIndex--;
					++itBindings;
				}
				else
					++itBindings;
			}

			std::map< InputID, std::vector<InputID> >::iterator it;
			for ( it = m_wrapperToLayerInputIDs.begin(); it != m_wrapperToLayerInputIDs.end(); ++it )
			{
				std::vector< InputID >& layerIDs = it->second;
				layerIDs.clear();
			}

			m_layers.erase( m_layers.begin()+i );
			break;
		}
	}
}

TAnimationGraphQueryID* CAnimationGraphStates::AddQueryIDPair( TAnimationGraphQueryID wrapperQueryID, LayerIndex layerIndex )
{
	TQueryIDBinding entry = { layerIndex, 0, wrapperQueryID };
	m_QueryIDBindings.push_back( entry );
	return & m_QueryIDBindings.back().layerQueryId;
}

void CAnimationGraphStates::RemoveLastQueryIDPair( TAnimationGraphQueryID wrapperQueryID, LayerIndex layerIndex )
{
	if ( m_QueryIDBindings.back().layerIndex == layerIndex && m_QueryIDBindings.back().wrapperQueryId == wrapperQueryID )
		m_QueryIDBindings.pop_back();
	else
	{
		QueryIDBindings::reverse_iterator it, itEnd = m_QueryIDBindings.rend();
		for ( it = m_QueryIDBindings.rbegin(); it != itEnd; ++it )
		{
			if ( it->layerIndex == layerIndex && it->wrapperQueryId == wrapperQueryID )
			{
				m_QueryIDBindings.erase(--(it.base()));
				break;
			}
		}
	}
}

// returns 0 is none, 1 if last one, 2 if two or more. (apart from the found wrapper id, of course =).
int CAnimationGraphStates::FindAndRemoveQueryIDPair( TAnimationGraphQueryID layerQueryID, LayerIndex layerIndex, TAnimationGraphQueryID & wrapperQueryID )
{
	QueryIDBindings::iterator it = m_QueryIDBindings.begin(), itEnd = m_QueryIDBindings.end();
	while ( it != itEnd && (it->layerQueryId != layerQueryID || it->layerIndex != layerIndex) )
		++it;

	if ( it == itEnd )
		return 0;

	wrapperQueryID = it->wrapperQueryId;
	m_QueryIDBindings.erase( it );
	itEnd = m_QueryIDBindings.end();

	// count remaining bindings to same wrapper id
	for ( it = m_QueryIDBindings.begin(); it != itEnd; ++it )
		if ( wrapperQueryID == it->wrapperQueryId )
			return 2;

	return 1;
}

// implementing Listener callback
void CAnimationGraphStates::SetOutput( const char * output, const char * value )
{
	AG_SEND_EVENT( SetOutput(output, value) );
}

void CAnimationGraphStates::LayerListener::QueryComplete( TAnimationGraphQueryID queryID, bool succeeded )
{
	owner->QueryComplete( queryID, succeeded, layerIndex );
}

void CAnimationGraphStates::QueryComplete( TAnimationGraphQueryID layerQueryID, bool succeeded, LayerIndex layerIndex )
{
	m_bQueryCompleteReceivedWhileSettingInput = true;

	while (1)
	{
		TAnimationGraphQueryID wrapperQueryID;
		int bindingCount = FindAndRemoveQueryIDPair( layerQueryID, layerIndex, wrapperQueryID );

		// we got a callback for a query that we don't care about.
		if ( bindingCount == 0 )
			return;

		// process only non-dummy pairs
		if ( layerIndex != 100 )
		{
			VectorQueryIDs::iterator itWait = std::find( m_waitForEnterStateWrapperIDs.begin(), m_waitForEnterStateWrapperIDs.end(), wrapperQueryID );
			if ( itWait != m_waitForEnterStateWrapperIDs.end() )
			{
				m_waitForEnterStateWrapperIDs.erase( itWait );

				if ( succeeded )
				{
					// no need to generate it here, we know that the next id was reserved already for this
					m_layers[layerIndex]->QueryLeaveState( AddQueryIDPair(wrapperQueryID + 1, layerIndex) );
					m_nextLeaveStateQueryID = wrapperQueryID+1;
				}
			}
		}

		if ( bindingCount == 1 )
		{
			if ( !succeeded && HasAlreadySucceeded(wrapperQueryID) )
				succeeded = true;

			// succeded may be false here, if this is the last one, and it failed, and no other has already succeeded.
			SendQueryComplete( wrapperQueryID, succeeded );
			m_nextLeaveStateQueryID = 0;

			ClearSucceeded( wrapperQueryID );
		}
		else
		{
			assert( bindingCount == 2 );

			if ( succeeded )
				RememberSucceeded( wrapperQueryID );
		}
	}
}

void CAnimationGraphStates::RebindInputs()
{
	// Remove mappings from wrapper input IDs to layer input IDs.
	m_wrapperToLayerInputIDs.clear();

	// Create new mappings from wrapper input IDs to layer input IDs (given what ever layers we have now).
	for ( LayerIndex layerIndex = 0; layerIndex < m_layers.size(); ++layerIndex )
	{
		IAnimationGraphState* pLayer = m_layers[layerIndex];
		int layerInputID = 0;
		while ( const char* layerInputName = pLayer->GetInputName(layerInputID) )
		{
			std::map<string, InputID>::iterator it = m_wrapperInputIDs.find(CONST_TEMP_STRING(layerInputName));
			InputID wrapperInputID = -1;
			if ( it == m_wrapperInputIDs.end() )
			{
				wrapperInputID = GenerateWrapperInputID();
				m_wrapperInputIDs[ layerInputName ] = wrapperInputID;   //wrapperinput = wrapperIDs.add(name, new unique wrapper id)
			}
			else
			{
				wrapperInputID = it->second;
			}
			m_wrapperToLayerInputIDs[wrapperInputID].resize( m_layers.size(), InputID(-1) );
			m_wrapperToLayerInputIDs[wrapperInputID][layerIndex] = layerInputID;

			++layerInputID;
		}
	}

	// Iterator through all names and remove those that are not represented in any layer (those were represented before, but not any more).
	std::map<string, InputID>::iterator it = m_wrapperInputIDs.begin();
	while ( it != m_wrapperInputIDs.end() )
	{
		InputID wrapperInputID = it->second;
		std::map<InputID, std::vector<InputID> >::iterator mapping = m_wrapperToLayerInputIDs.find( wrapperInputID );
		if ( mapping == m_wrapperToLayerInputIDs.end() )
			m_wrapperInputIDs.erase( it++ );
		else
			++it;
	}
}

bool CAnimationGraphStates::SetInput( InputID inputID, float inputValue, TAnimationGraphQueryID * pQueryID /* = 0 */ )
{
	FRAME_PROFILER( "CAnimationGraphStates::SetInput[float]", GetISystem(), PROFILE_GAME );
	return SetInputT( inputID, inputValue, pQueryID );
}

bool CAnimationGraphStates::SetInput( InputID inputID, int inputValue, TAnimationGraphQueryID * pQueryID /* = 0 */ )
{
	FRAME_PROFILER( "CAnimationGraphStates::SetInput[int]", GetISystem(), PROFILE_GAME );
	return SetInputT( inputID, inputValue, pQueryID );
}

bool CAnimationGraphStates::SetInput( InputID inputID, const char * inputValue, TAnimationGraphQueryID * pQueryID /* = 0 */ )
{
	FRAME_PROFILER( "CAnimationGraphStates::SetInput[key]", GetISystem(), PROFILE_GAME );
	return SetInputT( inputID, inputValue, pQueryID );
}

bool CAnimationGraphStates::SetInputOptional( InputID inputID, const char * inputValue, TAnimationGraphQueryID * pQueryID /* = 0 */ )
{
	FRAME_PROFILER( "CAnimationGraphStates::SetInputOptional[key]", GetISystem(), PROFILE_GAME );

	std::map<InputID, std::vector<InputID> >::const_iterator itLayerInputIDs = m_wrapperToLayerInputIDs.find( inputID );
	if ( itLayerInputIDs == m_wrapperToLayerInputIDs.end() )
		return false;

	TAnimationGraphQueryID wrapperQueryID = 0;
	if ( pQueryID )
	{
		*pQueryID = wrapperQueryID = GenerateWrapperQueryID();

		// reserve the next query id for QueryLeaveState
		// (we don't have to remember it since it's wrapperQueryID+1)
		GenerateWrapperQueryID();

		// this will put a dummy QueryIDPair to postpone sending QueryComplete
		// until we set the inputs for all layers (sometimes SetInput can call QueryComplete)
		*AddQueryIDPair(wrapperQueryID, 100) = 0;
	}

	bool success = false;

	// figure out which layers to forward request to
	for ( LayerIndex layerIndex = 0; layerIndex < m_layers.size(); ++layerIndex )
	{
		InputID layerInputID = itLayerInputIDs->second[layerIndex];
		if ( layerInputID == InputID(-1) )
			continue;

		m_bQueryCompleteReceivedWhileSettingInput = false;
		IAnimationGraphState* pLayer = m_layers[ layerIndex ];
		if ( pQueryID == NULL )
		{
			success |= pLayer->SetInputOptional( layerInputID, inputValue );
		}
		else if ( pLayer->SetInputOptional(layerInputID, inputValue, AddQueryIDPair(wrapperQueryID, layerIndex)) )
		{
			if ( pLayer->IsSignalledInput(layerInputID) && !m_bQueryCompleteReceivedWhileSettingInput )
				m_waitForEnterStateWrapperIDs.push_back( wrapperQueryID );
			success = true;
		}
		else
			RemoveLastQueryIDPair( wrapperQueryID, layerIndex );
	}

	// remove the dummy QueryIDPair and send QueryComplete(false) if needed
	if ( pQueryID )
		QueryComplete( 0, false, 100 );

	return success;
}

template <class InputValueType>
bool CAnimationGraphStates::SetInputT( InputID inputID, InputValueType inputValue, TAnimationGraphQueryID * pQueryID /* = 0 */ )
{
	std::map<InputID, std::vector<InputID> >::const_iterator itLayerInputIDs = m_wrapperToLayerInputIDs.find( inputID );
	if ( itLayerInputIDs == m_wrapperToLayerInputIDs.end() )
		return false;

	TAnimationGraphQueryID wrapperQueryID = 0;
	if ( pQueryID )
	{
		*pQueryID = wrapperQueryID = GenerateWrapperQueryID();

		// reserve the next query id for QueryLeaveState
		// (we don't have to remember it since it's wrapperQueryID+1)
		GenerateWrapperQueryID();

		// this will put a dummy QueryIDPair to postpone sending QueryComplete
		// until we set the inputs for all layers (sometimes SetInput can call QueryComplete)
		*AddQueryIDPair(wrapperQueryID, 100) = 0;
	}

	bool success = false;

	// figure out which layers to forward request to
	for ( LayerIndex layerIndex = 0; layerIndex < m_layers.size(); ++layerIndex )
	{
		InputID layerInputID = itLayerInputIDs->second[layerIndex];
		if ( layerInputID == InputID(-1) )
			continue;

		m_bQueryCompleteReceivedWhileSettingInput = false;
		IAnimationGraphState* pLayer = m_layers[ layerIndex ];
		if ( pQueryID == NULL )
		{
			success |= pLayer->SetInput( layerInputID, inputValue );
		}
		else if ( pLayer->SetInput(layerInputID, inputValue, AddQueryIDPair(wrapperQueryID, layerIndex)) )
		{
			if ( pLayer->IsSignalledInput(layerInputID) && !m_bQueryCompleteReceivedWhileSettingInput )
				m_waitForEnterStateWrapperIDs.push_back( wrapperQueryID );
			success = true;
		}
		else
			RemoveLastQueryIDPair( wrapperQueryID, layerIndex );
	}

	// remove the dummy QueryIDPair and send QueryComplete(false) if needed
	if ( pQueryID )
		QueryComplete( 0, false, 100 );

	return success;
}

void CAnimationGraphStates::ClearInput( InputID inputID )
{
	INPUTID_RECURSION( inputID, ClearInput(layerInputID) );
}

void CAnimationGraphStates::LockInput( InputID inputID, bool locked )
{
	INPUTID_RECURSION( inputID, LockInput(layerInputID, locked) )
}

bool CAnimationGraphStates::SetVariationInput( const char* name, const char* value )
{
	bool result = false;
	for ( int i = 0; i < m_layers.size(); ++i )
		if ( m_layers[i]->SetVariationInput(name, value) )
			result = true;
	return result;
}

// assert all equal, use any (except if signalled, then return the one not equal to default, or return default of all default)
void CAnimationGraphStates::GetInput( InputID inputID, char * inputValue ) const
{
	inputValue[0] = 0;
	std::map<InputID, std::vector<InputID> >::const_iterator it = m_wrapperToLayerInputIDs.find( inputID );
	if ( it == m_wrapperToLayerInputIDs.end() )
		return;

	const std::vector< InputID >& layerInputIDs = it->second;
	for ( int i = 0; i < m_layers.size(); ++i )
	{
		InputID layerInputID = layerInputIDs[i];
		if ( layerInputID != InputID(-1) )
		{
			m_layers[i]->GetInput( layerInputID, inputValue );
			if ( !m_layers[i]->IsSignalledInput(layerInputID) )
				return;
			if ( !m_layers[i]->IsDefaultInputValue(layerInputID) )
				return;
		}
	}
}

// AND all layers
bool CAnimationGraphStates::IsDefaultInputValue( InputID inputID ) const
{
	std::map<InputID, std::vector<InputID> >::const_iterator it = m_wrapperToLayerInputIDs.find( inputID );
	if ( it == m_wrapperToLayerInputIDs.end() )
		return true;

	const std::vector< InputID >& layerInputIDs = it->second;
	for ( int i = 0; i < m_layers.size(); ++i )
	{
		InputID layerInputID = layerInputIDs[i];
		if ( layerInputID != InputID(-1) )
		{
			if ( !m_layers[i]->IsDefaultInputValue(layerInputID) )
				return false;
		}
	}
	return true;
}


// When QueryID of SetInput (reached queried state) is emitted this function is called by the outside, by CAnimationGraphState convention(verify!).
// Remember which layers supported the SetInput query and emit QueryLeaveState QueryComplete when all those layers have left those states.
void CAnimationGraphStates::QueryLeaveState( TAnimationGraphQueryID * pQueryID )
{
	// calls to CAnimationGraphStates::QueryLeaveState are only valid if they come from QueryComplete handler ;)
	if( m_nextLeaveStateQueryID )
	{
		*pQueryID = m_nextLeaveStateQueryID;
	}
	else
	{
		*pQueryID = GenerateWrapperQueryID();
		m_layers[0]->QueryLeaveState( AddQueryIDPair(*pQueryID, 0) );
	}
}


// assert all equal, forward to all layers, complete when all have changed once (trivial, since all change at once via SetInput).
// (except for signalled, forward only to layers which currently are not default, complete when all those have changed).
void CAnimationGraphStates::QueryChangeInput( InputID inputID, TAnimationGraphQueryID * query )
{
	*query = GenerateWrapperQueryID();
	std::map<InputID, std::vector<InputID> >::const_iterator it = m_wrapperToLayerInputIDs.find( inputID );
	if ( it == m_wrapperToLayerInputIDs.end() )
	{
		SendQueryComplete( *query, false );
		return;
	}

	bool failed = true;
	const std::vector< InputID >& layerInputIDs = it->second;
	for ( int i = 0; i < m_layers.size(); ++i )
	{
		InputID layerInputID = layerInputIDs[i];
		if ( layerInputID != InputID(-1) )
		{
			if ( m_layers[i]->IsSignalledInput(layerInputID) )
			{
				//char inputValue[256];
				//m_layers[i]->GetInput( layerInputID, inputValue );
				if ( m_layers[i]->IsDefaultInputValue(layerInputID) )
					continue;
			}
			
			m_layers[i]->QueryChangeInput( layerInputID, AddQueryIDPair(*query, i) );
			failed = false;
		}
	}

	//if ( failed )
	//	SendQueryComplete( *query, false );
}


// simply recurse (will be ignored by each layer individually if state not found)
void CAnimationGraphStates::PushForcedState( const char * state, TAnimationGraphQueryID * pQueryID /* = 0 */ )
{
	// use recursive query mechanism
	if ( pQueryID != NULL )
		assert( !"Not implemented yet!" );
	SIMPLE_RECURSION(PushForcedState( state, NULL ));
}

// simply recurse
void CAnimationGraphStates::ClearForcedStates()
{
	SIMPLE_RECURSION(ClearForcedStates());
}


void CAnimationGraphStates::ForceTeleportToQueriedState()
{
	SIMPLE_RECURSION(ForceTeleportToQueriedState());
}


// simply recurse
void CAnimationGraphStates::SetBasicStateData( const SAnimationStateData& data)
{
	SIMPLE_RECURSION(SetBasicStateData(data));
}

// same as GetInput above
float CAnimationGraphStates::GetInputAsFloat( InputID inputId )
{
	float result = 0.0f;
	std::map<InputID, std::vector<InputID> >::const_iterator it = m_wrapperToLayerInputIDs.find( inputId );
	if ( it == m_wrapperToLayerInputIDs.end() )
		return result;

	const std::vector< InputID >& layerInputIDs = it->second;
	for ( int i = 0; i < m_layers.size(); ++i )
	{
		InputID layerInputID = layerInputIDs[i];
		if ( layerInputID != InputID(-1) )
		{
			result = m_layers[i]->GetInputAsFloat( layerInputID );
			if ( !m_layers[i]->IsSignalledInput(layerInputID) )
				return result;
			if ( !m_layers[i]->IsDefaultInputValue(layerInputID) )
				return result;
		}
	}
	return result;
}

// wrapper generates it's own input IDs for the union of all inputs in all layers, and for each input it maps to the layer specific IDs.
CAnimationGraphStates::InputID CAnimationGraphStates::GetInputId( const char * input )
{
	std::map<string, InputID>::const_iterator it = m_wrapperInputIDs.find( CONST_TEMP_STRING(input) );
	if ( it == m_wrapperInputIDs.end() )
		return -1;
	return it->second;
}

// Just register and non-selectivly call QueryComplete on all listeners (regardless of what ID's they are actually interested in).
void CAnimationGraphStates::AddListener( const char * name, IAnimationGraphStateListener * pListener )
{
	// copy paste original implementation
	SListener listener;
	strncpy(listener.name, name, sizeof(listener.name));
	listener.name[sizeof(listener.name)-1] = 0;
	listener.pListener = pListener;
	stl::push_back_unique( m_listeners, listener );
}

void CAnimationGraphStates::RemoveListener( IAnimationGraphStateListener * pListener )
{
	// copy paste original implementation
	SListener listener;
	listener.name[0] = 0;
	listener.pListener = pListener;
	stl::find_and_erase( m_listeners, listener );
}

void CAnimationGraphStates::SendQueryComplete(TAnimationGraphQueryID wrapperQueryID, bool succeeded)
{
	// copy paste original implementation
	AG_SEND_EVENT( QueryComplete(wrapperQueryID, succeeded) );
}

// simply recurse (preserve order), and don't forget to serialize the wrapper stuff, ID's or whatever.
void CAnimationGraphStates::Serialize( TSerialize ser )
{
	SIMPLE_RECURSION( Serialize(ser) );

	if (ser.GetSerializationTarget() == eST_SaveGame)
	{
		if (ser.IsReading())
		{
			m_QueryIDBindings.clear();
			m_NextQueryID = 0;
			m_waitForEnterStateWrapperIDs.clear();
			m_nextLeaveStateQueryID = 0;
		}
	}
}

// simply recurse
void CAnimationGraphStates::SetAnimationActivation( bool activated )
{
	SIMPLE_RECURSION( SetAnimationActivation(activated) );
}

// is the same for all layers (equal assertion should not even be needed)
bool CAnimationGraphStates::GetAnimationActivation()
{
	if ( m_layers.size() == 0 )
		return false;
	return m_layers[0]->GetAnimationActivation();
}

// Concatenate all layers state names with '+'. Use only fullbody layer state name if upperbody layer is not allowed/mixed.
const char * CAnimationGraphStates::GetCurrentStateName()
{
	static string result;
	result.clear();
	for ( int layer = 0; layer < m_layers.size(); ++layer )
	{
		if ( layer )
			result += '+';
		result += m_layers[layer]->GetCurrentStateName();
		if ( !m_layers[layer]->IsMixingAllowedForCurrentState() )
			break;
	}
	return result.c_str();
}


// simply recurse
void CAnimationGraphStates::Pause( bool pause, EAnimationGraphPauser pauser )
{
	SIMPLE_RECURSION( Pause(pause, pauser) );
}

// is the same for all layers (equal assertion should not even be needed)
bool CAnimationGraphStates::IsInDebugBreak()
{
	if ( m_layers.size() == 0 )
		return false;
	return m_layers[0]->IsInDebugBreak();
}

// find highest layer that has output id, or null (this allows upperbody to override fullbody).
// Use this logic when calling SetOutput on listeners.
const char * CAnimationGraphStates::QueryOutput( const char * name )
{
	int layer = m_layers.size();
	while ( layer-- )
	{
		const char* result = m_layers[layer]->QueryOutput( name );
		if ( result )
			return result;
	}
	return NULL;
}

// Exact positioning: Forward to fullbody layer only (hardcoded)
IAnimationSpacialTrigger * CAnimationGraphStates::SetTrigger( const SAnimationTargetRequest& req, EAnimationGraphTriggerUser user, TAnimationGraphQueryID * pQueryStart, TAnimationGraphQueryID * pQueryEnd )
{
	if ( m_layers.size() == 0 )
		return NULL;

	*pQueryStart = GenerateWrapperQueryID();
	*pQueryEnd = GenerateWrapperQueryID();

	return m_layers[0]->SetTrigger( req, user, AddQueryIDPair(*pQueryStart, 0), AddQueryIDPair(*pQueryEnd, 0) );
}

void CAnimationGraphStates::ClearTrigger( EAnimationGraphTriggerUser user )
{
	if ( m_layers.empty() )
		return;
	m_layers[0]->ClearTrigger( user );
}

const SAnimationTarget* CAnimationGraphStates::GetAnimationTarget()
{
	if ( m_layers.empty() )
		return NULL;
	return m_layers[0]->GetAnimationTarget();
}

bool CAnimationGraphStates::HasAnimationTarget() const
{
	if ( m_layers.empty() )
		return NULL;
	return m_layers[0]->HasAnimationTarget();
}

void CAnimationGraphStates::SetTargetPointVerifier( IAnimationGraphTargetPointVerifier* verifier )
{
	if ( m_layers.empty() )
		return;
	m_layers[0]->SetTargetPointVerifier( verifier );
}

bool CAnimationGraphStates::IsUpdateReallyNecessary()
{
	if ( m_layers.empty() )
		return false;
	return m_layers[0]->IsUpdateReallyNecessary();
}

// (only used by vehicle code) (to support simultaneous layer query, IAnimationGraphExistanceQuery must implement it).
// Forward to fullbody layer only (hardcoded)
IAnimationGraphExistanceQuery * CAnimationGraphStates::CreateExistanceQuery()
{
	if ( m_layers.empty() )
		return NULL;
	return m_layers[0]->CreateExistanceQuery();
}

// simply recurse
void CAnimationGraphStates::Reset()
{
	SIMPLE_RECURSION( Reset() );
}

// we've been idle for a while, try to catch up and disrespect blending laws
// simply recurse
void CAnimationGraphStates::SetCatchupFlag()
{
	SIMPLE_RECURSION( SetCatchupFlag() );
}

// specify layer (or hardcoded forward to fullbody layer only) (used for exact positioning trigger and PMC::CAnimationGraphStates::UpdateMovementState()).
Vec2 CAnimationGraphStates::GetQueriedStateMinMaxSpeed()
{
	if ( m_layers.size() == 0 )
		return Vec2(0,0);
	return m_layers[0]->GetQueriedStateMinMaxSpeed();
}

// simply recurse (hurry all layers, let them hurry independently where they can)
void CAnimationGraphStates::Hurry()
{
	SIMPLE_RECURSION( Hurry() );
}

// simply recurse (first person skippable states are skipped independently by each layer)
void CAnimationGraphStates::SetFirstPersonMode( bool on )
{
	SIMPLE_RECURSION( SetFirstPersonMode(on) );
}

// simply recurse (will add all layer's containers to the sizer)
void CAnimationGraphStates::GetMemoryStatistics(ICrySizer * s)
{
	SIMPLE_RECURSION( GetMemoryStatistics(s) );
}

void CAnimationGraphStates::AddLayerReference( IAnimationGraphState* pAnimationGraphState )
{
	m_layerListeners.push_back( LayerListener(this, m_layers.size()) );
	pAnimationGraphState->AddListener( "wrapper", &m_layerListeners.back() );
	m_layers.push_back( pAnimationGraphState );
}

bool CAnimationGraphStates::DoesInputMatchState( InputID)
{
	// not implemented/needed. don't call it!
	assert( false );
	return false;
}

// TODO: This should be turned into registered callbacks or something instead (look at AnimationGraphStateListener).
// Use to access the CAnimationGraphStates::SelectLocomotionState() callback in CAnimatedCharacter.
// Only set for fullbody, null for upperbody.
void CAnimationGraphStates::SetAnimatedCharacter( class CAnimatedCharacter* animatedCharacter, int layerIndex, IAnimationGraphState* parentLayerState )
{
	// there's no parent layer for the wrapper
	assert( parentLayerState == NULL );

	if ( m_layers.size() == 0 )
		return;

	for ( LayerIndex i = 0; i < m_layers.size(); ++i )
	{
		IAnimationGraphState* pAGState = (i > 0) ? m_layers[i-1] : NULL;
		m_layers[i]->SetAnimatedCharacter( animatedCharacter, i, pAGState);
	}
}

// simply recurse (find out what to return if not all succeed, but not all fail)
bool CAnimationGraphStates::Update()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_GAME);

	bool allowMixing = false;
	for ( LayerIndex layerIndex = 0; layerIndex < m_layers.size(); ++layerIndex )
	{
		IAnimationGraphState* pLayer = m_layers[layerIndex];
		if ( layerIndex )
			pLayer->SetInput( "AllowMixing", (int) allowMixing );
		if ( !pLayer->Update() )
			return false;
		else
			allowMixing = pLayer->IsMixingAllowedForCurrentState();
	}
	return true;
}

void CAnimationGraphStates::Release()
{
	SIMPLE_RECURSION(Release());
}

bool CAnimationGraphStates::IsSignalledInput( InputID intputId ) const
{
	// not implemented/needed. don't call it!
	assert(0);
	return false;
}

const char* CAnimationGraphStates::GetInputName( InputID inputId ) const
{
	std::map<string, InputID>::const_iterator it, itEnd = m_wrapperInputIDs.end();
	for ( it = m_wrapperInputIDs.begin(); it != itEnd; ++it )
		if ( it->second == inputId )
			return it->first.c_str();
	return NULL;
}
