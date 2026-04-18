#include "StdAfx.h"
#include "AnimationGraphManager.h"
#include "AnimationGraph.h"
#include "AnimationGraphState.h"
#include "AnimationGraphCVars.h"
#include "CryAction.h"

// hacky includes just for SetAnimationGraphActivation...
#include "IAnimatedCharacter.h"
#include "GameObjects/GameObject.h"

#define AG_MEM_DEBUG   // to track down usage of AnimationGraph
#undef  AG_MEM_DEBUG 

class CAnimationGraphManager::CategoryIt : public IAnimationGraphCategoryIterator
{
public:
	CategoryIt( CAnimationGraphManager * pMgr ) : m_nRefs(0), m_cur(pMgr->m_categoryInfo.begin()), m_end(pMgr->m_categoryInfo.end())
	{
	}
	void AddRef() 
	{ 
		++m_nRefs; 
	}
	void Release() 
	{ 
		if (0 == --m_nRefs) 
			delete this; 
	}
	bool Next(SCategory& cat)
	{
		if (m_cur != m_end)
		{
			cat.name = m_cur->first.c_str();
			cat.overridable = m_cur->second.overrideSlot != -1;
      cat.usableWithTemplate = m_cur->second.usableWithTemplate;
			++m_cur;
			return true;
		}
		else
		{
			cat.name = 0;
			cat.overridable = false;
      cat.usableWithTemplate = false;
			return false;
		}
	}

private:
	int m_nRefs;
	typedef std::map<string, SCategoryInfo>::const_iterator It;
	It m_cur;
	It m_end;
};

class CAnimationGraphManager::StateFactoryIt : public IAnimationGraphStateFactoryIterator
{
public:
	StateFactoryIt( CAnimationGraphManager * pMgr ) : m_nRefs(0), m_cur(pMgr->m_stateFactoryFactories.begin()), m_end(pMgr->m_stateFactoryFactories.end()), m_pFactory(0)
	{
	}
	void AddRef() 
	{ 
		++m_nRefs; 
	}
	void Release() 
	{ 
		SAFE_RELEASE(m_pFactory);
		if (0 == --m_nRefs) 
			delete this; 
	}
	bool Next(SStateFactory& fac)
	{
		SAFE_RELEASE(m_pFactory);
		if (m_cur != m_end)
		{
			m_pFactory = m_cur->second();
			fac.category = m_pFactory->GetCategory();
			fac.name = m_cur->first.c_str();
			fac.humanName = m_pFactory->GetName();
			fac.pParams = m_pFactory->GetParameters();
			++m_cur;
			return true;
		}
		else
		{
			fac.name = fac.humanName = fac.category = 0;
			fac.pParams = 0;
			return false;
		}
	}

private:
	int m_nRefs;
	typedef std::map<string, IAnimationStateNodeFactory*(*)()>::const_iterator It;
	It m_cur;
	It m_end;
	IAnimationStateNodeFactory * m_pFactory;
};

class CAnimationGraphManager::StateIt : public IAnimationGraphStateIterator
{
public:
	StateIt( CAnimationGraph* pAG , CAnimationGraphManager * pMgr ) : m_nRefs(0)
	{
		const VectorMap<CCryName, CStateIndex::StateID>& allStates = pAG->GetAllStates();
		m_cur = allStates.begin();
		m_end = allStates.end();
	}
	void AddRef() 
	{ 
		++m_nRefs; 
	}
	void Release() 
	{ 
		if (0 == --m_nRefs) 
			delete this; 
	}
	bool Next(SState& state)
	{
		if (m_cur != m_end)
		{
			state.name = m_cur->first.c_str();
			++m_cur;
			return true;
		}
		else
		{
			state.name = 0;
			return false;
		}
	}

private:
	typedef VectorMap<CCryName, CStateIndex::StateID>::const_iterator It;
	It m_cur;
	It m_end;
	int m_nRefs;
};

class CAnimationGraphManager::GraphInputs : public IAnimationGraphInputs
{
	struct CSInput : public SInput
	{
		virtual IAnimationGraphInputs::InputType GetType() const { return type; }
		virtual const char* GetName() const { return name.c_str(); }
		virtual int GetValueCount() const { return values.size(); }
		virtual const char* GetValue(int index) const { 
			assert ( index >= 0 && index < values.size() );
			return values[index].c_str();
		}

		string name;
		IAnimationGraphInputs::InputType type;
		std::vector<string> values;
	};

public:
	GraphInputs( XmlNodeRef root ) : m_nRefs(0)
	{
		assert (root != 0);
		XmlNodeRef inputs = root->findChild( "Inputs" );
		if (inputs != 0)
		{
			int numChildren = inputs->getChildCount();
			for (int n=0; n<numChildren; n++)
			{
		
				// get node
				XmlNodeRef inputNode = inputs->getChild(n);
				// get the name of the node
				const char * name = inputNode->getAttr("name");
				if (name !=0 && !name[0])
					continue;

				CSInput input;
				input.name = name;

				// figure out the type and build the support logic
				const char * nodeType = inputNode->getTag();
				if (0 == strcmp("KeyState", nodeType))
				{
					input.type = eAGIT_String;

					int numValues = inputNode->getChildCount();
					for (int i=0; i<numValues; i++)
					{
						XmlNodeRef valueNode = inputNode->getChild(i);
						if (0 != strcmp(valueNode->getTag(), "Key"))
							continue;
						if (valueNode->getNumAttributes() == 0)
							continue;
						if (!valueNode->haveAttr("value"))
							continue;
						input.values.push_back(valueNode->getAttr("value"));
					}
					m_inputs.push_back(input);
				}
				else if (0 == strcmp("IntegerState", nodeType))
				{
					input.type = eAGIT_Integer;
					m_inputs.push_back(input);
				}
				else if (0 == strcmp("FloatState", nodeType))
				{
					input.type = eAGIT_Float;
					m_inputs.push_back(input);
				}
				else
				{
					;
				}
			}
		}
	}

	void AddRef() 
	{ 
		++m_nRefs; 
	}
	void Release() 
	{ 
		if (0 == --m_nRefs) 
			delete this; 
	}
	int GetNumInputs() {
		return m_inputs.size();
	}
	const SInput* GetInput(int index)
	{
		assert ( index >= 0 && index < m_inputs.size() );
		return &m_inputs[index];
	}
private:
	std::vector<CSInput> m_inputs;
	int m_nRefs;
};

class CAnimationGraphManager::QueryResults : public IAnimationGraphQueryResults
{
public:

	struct SResult : public IAnimationGraphQueryResults::IResult
	{
		SResult( const string& sValue, int iRanking ) : value( sValue ), valueNoMixIns( sValue ), ranking( iRanking ) 
		{
			int i = valueNoMixIns.find( '+', 0 );
			if ( i != string::npos )
				valueNoMixIns = valueNoMixIns.substr( 0, i );
		}

		virtual const char* GetValue() const { return value.c_str(); }
		virtual const char* GetValueNoMixIns() const { return valueNoMixIns.c_str(); }
		virtual int GetRanking() const { return ranking; }

		string	value;
		string	valueNoMixIns;
		int			ranking;
	};
	
	QueryResults() : m_iHighestRanking( 0 ) {}

	virtual int GetHighestRankingValue() const { return m_iHighestRanking; }
	virtual int GetNumResults() const { return m_results.size(); }
	virtual const IResult * GetResult( int index ) const 
	{ 
		return ( index >= 0 && index < m_results.size() ) ? &m_results[index] : NULL; 
	}

	virtual const IResult * GetResult( const char * value ) const 
	{ 
		if ( value )
		{
			for ( int i=0; i<m_results.size(); ++i )
				if ( 0==m_results[i].value.compareNoCase( value ) )
					return &m_results[i];
		}
		
		return NULL; 
	}

	virtual bool Contains( const char * value, bool bIgnoreMixIns /*=true*/ ) const 
	{ 
		if ( value )
		{
			if ( bIgnoreMixIns )
			{
				for ( int i=0; i<m_results.size(); ++i )
					if ( 0==m_results[i].valueNoMixIns.compareNoCase( value ) )
						return true;
			}
			else
			{
				for ( int i=0; i<m_results.size(); ++i )
					if ( 0==m_results[i].value.compareNoCase( value ) )
						return true;
			}
		}
		return false; 
	}

	void AddResult( const SResult * pResult ) 
	{ 
		if ( pResult )
		{
			m_results.push_back( *pResult ); 
			m_iHighestRanking = max( m_iHighestRanking, pResult->ranking );
		}
	}
	void Clear()
	{ 
		m_results.clear(); 
		m_iHighestRanking = 0;
	}

	void AddRef() 
	{ 
		++m_nRefs; 
	}
	void Release() 
	{ 
		if (0 == --m_nRefs) 
			delete this; 
	}
	
private:
	std::vector<SResult> m_results;
	int	m_iHighestRanking;
	int m_nRefs;
};

CAnimationGraphManager::CAnimationGraphManager() : m_numOverrides(0)
{
}

CAnimationGraphManager::~CAnimationGraphManager()
{
	FreeGraphs();
	if (!m_graphs.empty())
		GameWarning("[animgraph] dangling graphs after animation graph manager deleted");
}

void CAnimationGraphManager::FreeGraphs()
{
	for (std::map<string, _smart_ptr<CAnimationGraph> >::iterator it = m_graphs.begin(); it != m_graphs.end(); )
	{
		std::map<string, _smart_ptr<CAnimationGraph> >::iterator next = it;
		++next;
		if (it->second->IsLastRef())
			m_graphs.erase(it);
		it = next;
	}
}

bool CAnimationGraphManager::TrialAnimationGraph( const char * name, XmlNodeRef animGraph, bool loadBinary)
{
	if(loadBinary)
		return LoadGraph(string(name), false, true)? true : false;
	return LoadGraph( name, animGraph, NULL );
}

bool CAnimationGraphManager::LoadAnimationGraph( const char * name, const char * filename,  XmlNodeRef animGraph, bool loadBinary /*=false*/ )
{
	bool bRet = false;
	string sPath = "Animations/graphs/";

	// Load binary (.ag) or XML graph from file if necessary
	if( animGraph == NULL && filename )
	{
		if ( loadBinary && name ) 
		{		
			string sFile( filename );
			int pos = sFile.find( '.' );
			if( pos != string::npos )
				sFile.erase( pos, sFile.size()-pos );
			sFile.append( ".ag" );
			string sFullPath = sPath + sFile;

			_smart_ptr<CAnimationGraph> pGraph = CAnimationGraph::CreateInstance( this, filename );
			if( pGraph->SerializeAsFile( sFullPath, true ) )
			{
				// Insert new graph or replace existing one
				string sWorkingName = name;
				sWorkingName.MakeLower();

				std::map<string, _smart_ptr<CAnimationGraph>>::iterator iter = m_graphs.find( sWorkingName );
				if ( iter == m_graphs.end() )
					iter = m_graphs.insert( std::make_pair( sWorkingName, pGraph ) ).first;
				else
				{
					iter->second->SwapAndIncrementVersion( pGraph );
					pGraph = NULL;
				}

				bRet = true;
			}
		}
		else
		{
			// Load XML into internal structure
			animGraph = GetISystem()->LoadXmlFile( sPath + filename );
		}
	}

	// Load graph from internal XML structure if we have one now
	if ( animGraph && name )
	{
		bRet = LoadGraph( name, animGraph, NULL );
	}

	return bRet;
}

bool CAnimationGraphManager::IsAnimationGraphLoaded( const char * name )
{
	string filename = name;
	filename.MakeLower();

	std::map<string, _smart_ptr<CAnimationGraph>>::iterator iter = m_graphs.find(filename);
	bool haveAlready = iter != m_graphs.end();

	return haveAlready;
}

IAnimationGraphQueryResultsPtr CAnimationGraphManager::QueryAnimationGraph( const char * name, const SAnimationGraphQueryInputs * inputs ) 
{
	CAnimationGraphState *pState = NULL;
	string searchName = name;
	searchName.MakeLower();

	// Create graph state
	std::map<string, _smart_ptr<CAnimationGraph>>::iterator iter = m_graphs.find(searchName);
	if ( iter != m_graphs.end() )
	{
		pState = (CAnimationGraphState*) (iter->second->CreateState());
	}
	if ( !pState )
	{
		return NULL;
	}

	// Clear all inputs
	for (int i=0; i<CAnimationGraph::MAX_INPUTS; i++)
	{
		pState->ClearInput(i);
	}

	// Set inputs
	for (int i=0; i<inputs->GetNumInputs(); ++i)
	{
		const SAnimationGraphQueryInputs::SInput *pInput = inputs->GetInput( i );

		if ( !pInput->bAnyValue )
		{
			CStateIndex::InputID id = pState->GetInputId( pInput->sName );

			if ( pInput->eType == SAnimationGraphQueryInputs::eAGQIT_String )
			{
				pState->SetInput( id, pInput->sValue, NULL );
			}
			else if ( pInput->eType == SAnimationGraphQueryInputs::eAGQIT_Float )
			{				
				pState->SetInput( id, pInput->fValue, NULL );
			}
			else if ( pInput->eType == SAnimationGraphQueryInputs::eAGQIT_Integer )
			{				
				pState->SetInput( id, pInput->iValue, NULL );
			}
		}		
	}

	// Set the state to not cull low value rankings so we see the complete set of possible valid states
	bool bOldKeepValue = pState->GetKeepLowValueRankings();
	pState->SetKeepLowValueRankings( true );

	// Perform query
	CStateIndex::StateIDVec validStates;
	pState->BasicQuery( pState->GetInputValues(), validStates);


	// Set results
	QueryResults *pResults = new QueryResults;
	std::vector<uint16> rankings;
	int numInputsUsed = pState->GetRankings( pState->GetInputValues(), rankings );
	for ( int i=0; i<validStates.size(); ++i )
	{
		if (validStates[i] != CAnimationGraphState::INVALID_STATE)
		{
			pResults->AddResult( &QueryResults::SResult( pState->GetGraph()->StateIDToName(validStates[i]).c_str(),	
				!rankings.empty() ? rankings[i] : 0 ) );
		}
	}

	pState->SetKeepLowValueRankings( bOldKeepValue );
	return pResults;
}

void CAnimationGraphManager::AddCategory( const string& name, bool overridable, bool usableWithTemplate/*=false*/ )
{
	SCategoryInfo info;
	if (overridable)
		info.overrideSlot = m_numOverrides++;
	else
		info.overrideSlot = -1;
  
  info.usableWithTemplate = usableWithTemplate;

	m_categoryInfo[name] = info;
}

const CAnimationGraphManager::SCategoryInfo * CAnimationGraphManager::GetCategory( const string& name ) const
{
	std::map<string, SCategoryInfo>::const_iterator iter = m_categoryInfo.find(name);
	if (iter == m_categoryInfo.end())
		return NULL;
	else
		return &iter->second;
}

void CAnimationGraphManager::RegisterStateFactory( const string& name, IAnimationStateNodeFactory *(*factoryFactory)() )
{
	m_stateFactoryFactories[name] = factoryFactory;
}

IAnimationStateNodeFactory * CAnimationGraphManager::CreateStateFactory( const string& name ) const
{
	struct Local
	{
		static IAnimationStateNodeFactory * NullReturn()
		{
			return NULL;
		}
	};
	return stl::find_in_map( m_stateFactoryFactories, name, Local::NullReturn )();
}

XmlNodeRef InternalLoadGraph(const string& filename)
{
	XmlNodeRef data = 0;

	const char* graphPath = "Animations/graphs/";
	data = GetISystem()->LoadXmlFile( (graphPath + filename).c_str() );

	return data;
}


IAnimationGraphPtr CAnimationGraphManager::LoadGraph( const string& filename_, bool reload, bool loadBinary)
{
	string filename = filename_;
	filename.MakeLower();

	std::map<string, _smart_ptr<CAnimationGraph> >::iterator iter = m_graphs.find(filename);
	bool haveAlready = iter != m_graphs.end();

	bool needToLoad = reload || !haveAlready;
	if (needToLoad)
	{
#ifdef AG_MEM_DEBUG
		CCryAction::GetCryAction()->DumpMemInfo("CAnimationGraphManager::LoadGraph '%s' before", filename.c_str());
#endif
/*
		if (reload)
			loadBinary = false;
*/
	
		while (loadBinary) // if with break...
		{
			_smart_ptr<CAnimationGraph> pGraph = NULL;

			string fullPath("Animations/graphs/");
			string file(filename);
			int pos = file.find('.');
			if(pos != string::npos)
				file.erase(pos, file.size()-pos);
			file.append(".ag");
			fullPath.append(file);

			//if(reload)
			//{
			//	assert(false);
			//	break;
			//}
			//else
			{
				pGraph = CAnimationGraph::CreateInstance(this, filename);
				if(!pGraph->SerializeAsFile(fullPath, true))
				{
					//CryWarning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, "Could not serialize binary AnimationGraph for file %s !", filename.c_str());
					pGraph = NULL;
					break;
				}

				if (!haveAlready)
					iter = m_graphs.insert( std::make_pair(filename, pGraph) ).first;
				else
				{
					iter->second->SwapAndIncrementVersion( pGraph );
					pGraph = NULL;
				}
				pGraph = iter->second;
#ifdef AG_MEM_DEBUG
				CCryAction::GetCryAction()->DumpMemInfo("CAnimationGraphManager::LoadGraph '%s' after binary load", filename.c_str());
#endif
				STLALLOCATOR_CLEANUP; 
#ifdef AG_MEM_DEBUG
				CCryAction::GetCryAction()->DumpMemInfo("CAnimationGraphManager::LoadGraph '%s' after cleanup", filename.c_str());
#endif
			}
			return &*pGraph;
		}

		XmlNodeRef data = InternalLoadGraph(filename);
		if (!data)
		{
			return haveAlready? &*iter->second : 0;
		}
		IAnimationGraphPtr pGraph = NULL;
		LoadGraph( filename, data, &pGraph );

#ifdef AG_MEM_DEBUG
		CCryAction::GetCryAction()->DumpMemInfo("CAnimationGraphManager::LoadGraph '%s' after XML load, data present", filename.c_str());
#endif

		// explicitly free memory of XML data here
		data = 0;
		STLALLOCATOR_CLEANUP; 

#ifdef AG_MEM_DEBUG
		CCryAction::GetCryAction()->DumpMemInfo("CAnimationGraphManager::LoadGraph '%s' after cleanup", filename.c_str());
#endif

		return pGraph;
	}
	else
	{
		return &*iter->second;
	}
}

void CAnimationGraphManager::ExportXMLToBinary( const char * filename, XmlNodeRef animGraph )
{
	_smart_ptr< CAnimationGraph > graph = CAnimationGraph::CreateInstance( this, "exporting" );
	if (!graph->Init( animGraph ))
	{
		GameWarning("Unable to initialize animation graph for exporting");
		return;
	}

	string fullPath("Animations/graphs/");
	string file(filename);
	size_t pos = file.find('.');
	if(pos != string::npos)
		file.erase(pos, file.size()-pos);
	pos = file.rfind('/');
	if(pos != string::npos)
		file = file.substr(pos+1);
	pos = file.rfind('\\');
	if(pos != string::npos)
		file = file.substr(pos+1);
	file.append(".ag");
	fullPath.append(file);

	graph->SerializeAsFile( fullPath );
}

bool CAnimationGraphManager::LoadGraph( string filename, XmlNodeRef data, IAnimationGraphPtr* ppGraph )
{
	filename.MakeLower();
	std::map<string, _smart_ptr<CAnimationGraph> >::iterator iter = m_graphs.find(filename);
	bool haveAlready = iter != m_graphs.end();

	_smart_ptr<CAnimationGraph> pGraph = CAnimationGraph::CreateInstance(this, filename);
	if (!pGraph->Init(data))
	{
		GameWarning("Unable to initialize animation graph %s", filename.c_str());
		if (ppGraph)
		{
			*ppGraph = haveAlready?  &*iter->second : NULL;
			return false;
		}
	}
	if (!haveAlready)
		iter = m_graphs.insert( std::make_pair(filename, pGraph) ).first;
	else
	{
		iter->second->SwapAndIncrementVersion( pGraph );
		pGraph = 0;
	}
	if (ppGraph)
		*ppGraph = &*iter->second;
	return true;
}

void CAnimationGraphManager::ReloadAllGraphs( bool fromBin )
{
	CAnimationGraphManager * pAnimGraph = CCryAction::GetCryAction()->GetAnimationGraphManager();

	SGameObjectEvent evt( eGFE_ResetAnimationGraphs, eGOEF_ToAll );
	CCryAction::GetCryAction()->GetIGameObjectSystem()->BroadcastEvent( evt );
	CCryAction::GetCryAction()->ResetMusicGraph();

	for (std::map<string, _smart_ptr<CAnimationGraph> >::iterator iter = pAnimGraph->m_graphs.begin(); iter != pAnimGraph->m_graphs.end(); ++iter)
	{
		LoadGraph( iter->first, true, fromBin );
	}
}

void CAnimationGraphManager::FindDeadInputValues( IAnimationGraphDeadInputReportCallback * pCallback, XmlNodeRef animGraph )
{
	_smart_ptr< CAnimationGraph > graph = CAnimationGraph::CreateInstance(this, "temp_dead_input_values_graph");
	if (!graph->Init( animGraph ))
	{
		GameWarning("Unable to initialize animation graph for dead input report");
		return;
	}
	graph->FindDeadInputValues( pCallback );
}

IAnimationGraphCategoryIteratorPtr CAnimationGraphManager::CreateCategoryIterator()
{
	return new CategoryIt(this);
}

IAnimationGraphStateFactoryIteratorPtr CAnimationGraphManager::CreateStateFactoryIterator()
{
	return new StateFactoryIt(this);
}

IAnimationGraphStateIteratorPtr CAnimationGraphManager::CreateStateIterator( EntityId id)
{
	IEntity * pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (!pEntity)
		return 0;
	CGameObject * pGameObject = (CGameObject*)pEntity->GetProxy(ENTITY_PROXY_USER);
	if (!pGameObject)
		return 0;
	IAnimatedCharacter * pAnimChar = (IAnimatedCharacter*) pGameObject->QueryExtension("AnimatedCharacter");
	if (!pAnimChar)
		return 0;
	IAnimationGraph* pAG = pAnimChar->GetAnimationGraph(eAnimationGraphLayer_FullBody);
	if (!pAG)
		return 0;
	return new StateIt(static_cast<CAnimationGraph*>(pAG), this);
}

IAnimationGraphInputsPtr CAnimationGraphManager::RetrieveInputs( EntityId id)
{
	IEntity * pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (!pEntity)
		return 0;
	CGameObject * pGameObject = (CGameObject*)pEntity->GetProxy(ENTITY_PROXY_USER);
	if (!pGameObject)
		return 0;
	IAnimatedCharacter * pAnimChar = (IAnimatedCharacter*) pGameObject->QueryExtension("AnimatedCharacter");
	if (!pAnimChar)
		return 0;
	IAnimationGraphPtr pAG = pAnimChar->GetAnimationGraph(eAnimationGraphLayer_FullBody);
	if (!pAG)
		return 0;

	// now load the graph again
	XmlNodeRef data = InternalLoadGraph(pAG->GetName());
	if (data == 0)
		return 0;

	return new GraphInputs(data);
}


void CAnimationGraphManager::SetAnimationGraphActivation( EntityId id, bool bActivation )
{
	IEntity * pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (!pEntity)
		return;
	CGameObject * pGameObject = (CGameObject*)pEntity->GetProxy(ENTITY_PROXY_USER);
	if (!pGameObject)
		return;
	IAnimatedCharacter * pAnimChar = (IAnimatedCharacter*) pGameObject->QueryExtension("AnimatedCharacter");
	if (!pAnimChar)
		return;
	pAnimChar->GetAnimationGraphState()->SetAnimationActivation( bActivation );
}

void CAnimationGraphManager::Log( ColorB clr, IEntity* pEnt, const char * fmt, ... )
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_GAME);

	if (!m_cVars.m_log)
		return;

	if (pEnt)
	{
		const char * filterEnt = m_cVars.m_pLogEntity->GetString();
		if (filterEnt[0])
		{
			if (!filterEnt[1] && filterEnt[0] == '0')
				;
			else if (0 != stricmp(pEnt->GetName(), filterEnt))
				return; // filtered out
		}
	}

	SLogRecord rec;
	rec.color = clr;
	rec.entity = pEnt? pEnt->GetName() : "<unknown>";
	rec.when = gEnv->pTimer->GetFrameStartTime();

	char buffer[1024];
	va_list args;
	va_start( args, fmt );
	vsprintf_s( buffer, fmt, args );
	va_end( args );
	rec.message = buffer;

	m_log.push_back(rec);

  if (m_cVars.m_log > 1)
    CryLog("%s %.2f %s", rec.entity.c_str(), rec.when.GetSeconds(), rec.message.c_str());

	if (m_logMarkers.empty())
	{
		while (m_log.size() > MAX_LOG_ENTRIES)
			m_log.pop_front();
	}
}

void CAnimationGraphManager::AddLogMarker()
{
	m_logMarkers.push(m_log.size());
}

void CAnimationGraphManager::DoneLogMarker(bool commit)
{
	if (!commit)
	{
		while (m_log.size() != m_logMarkers.top())
			m_log.pop_back();
	}
	m_logMarkers.pop();
}

bool CAnimationGraphManager::GotDataSinceMarker()
{
	return m_logMarkers.top() != m_log.size();
}

void CAnimationGraphManager::Update()
{
	assert(m_logMarkers.empty());
	while (m_log.size() > MAX_LOG_ENTRIES)
		m_log.pop_front();

	if (m_cVars.m_log)
	{
		int y = 590;
		for (std::list<SLogRecord>::reverse_iterator iter = m_log.rbegin(); iter != m_log.rend(); ++iter)
		{
			float clr[4] = {iter->color[0]/255.0f, iter->color[1]/255.0f, iter->color[2]/255.0f, iter->color[3]/255.0f};
			float white[4] = {1,1,1,1};
			gEnv->pRenderer->Draw2dLabel( 10, y, 1, white, false, "%s", iter->entity.c_str() );
			gEnv->pRenderer->Draw2dLabel( 70, y, 1, white, false, "%.2f", iter->when.GetSeconds() );
			gEnv->pRenderer->Draw2dLabel( 130, y, 1, clr, false, "%s", iter->message.c_str() );
			y -= 10;
		}
	}
}

void CAnimationGraphManager::GetMemoryStatistics(ICrySizer * s)
{
	SIZER_SUBCOMPONENT_NAME(s, "AnimationGraph");

	s->Add(*this);
	s->AddContainer(m_categoryInfo);
	s->AddContainer(m_stateFactoryFactories);
	s->AddContainer(m_graphs);
	s->AddContainer(m_log);
	//s->AddContainer(m_logMarkers);

	for (std::map<string, SCategoryInfo>::iterator iter = m_categoryInfo.begin(); iter != m_categoryInfo.end(); ++iter)
		s->Add(iter->first);
	for (std::map<string, IAnimationStateNodeFactory*(*)()>::iterator iter = m_stateFactoryFactories.begin(); iter != m_stateFactoryFactories.end(); ++iter)
		s->Add(iter->first);
	for (std::map<string, _smart_ptr<CAnimationGraph> >::iterator iter = m_graphs.begin(); iter != m_graphs.end(); ++iter)
	{
		s->Add(iter->first);
		SIZER_SUBCOMPONENT_NAME(s, iter->first.c_str());
		iter->second->GetMemoryStatistics(s);
	}
	for (std::list<SLogRecord>::iterator iter = m_log.begin(); iter != m_log.end(); ++iter)
	{
		s->Add(iter->entity);
		s->Add(iter->message);
	}
}
