#ifndef __ANIMATIONGRAPHMANAGER_H__
#define __ANIMATIONGRAPHMANAGER_H__

#pragma once

#include "IAnimationStateNode.h"
#include "IAnimationGraphSystem.h"
#include "AnimationGraphCVars.h"
#include <stack>

struct IAnimationGraph;
class CAnimationGraph;
struct IGameFramework;

class CAnimationGraphManager : public IAnimationGraphSystem
{
public:
	struct SCategoryInfo
	{
		SCategoryInfo() : overrideSlot(-1), usableWithTemplate(false) {}

		// is this category overridden by derived states (otherwise it's appended to)
		int overrideSlot;
    bool usableWithTemplate;
	};

	CAnimationGraphManager();
	~CAnimationGraphManager();
	void AddCategory( const string& name, bool overridable, bool usableWithTemplate=false);
	const SCategoryInfo * GetCategory( const string& name ) const;
	size_t GetCategoryCount() const { return m_categoryInfo.size(); }
	int GetOverrideSlotCount() const { return m_numOverrides; }
	void GetMemoryStatistics(ICrySizer * s);

	void RegisterStateFactory( const string& name, IAnimationStateNodeFactory*(*)() );
	IAnimationStateNodeFactory * CreateStateFactory( const string& name ) const;

	void RegisterFactories( IGameFramework * );

	IAnimationGraphPtr LoadGraph( const string& filename, bool reload = false, bool loadBinary = false);
	void ReloadAllGraphs( bool fromBin );

	void FreeGraphs();

	// IAnimationGraphSystem
	IAnimationGraphCategoryIteratorPtr CreateCategoryIterator();
	IAnimationGraphStateFactoryIteratorPtr CreateStateFactoryIterator();
	bool TrialAnimationGraph( const char * name, XmlNodeRef animGraph, bool loadBinary = false);
	void SetAnimationGraphActivation( EntityId id, bool bActivation );
	IAnimationGraphStateIteratorPtr CreateStateIterator( EntityId id);
	IAnimationGraphInputsPtr RetrieveInputs( EntityId id);
	void FindDeadInputValues( IAnimationGraphDeadInputReportCallback * pCallback, XmlNodeRef animGraph );
	virtual void ExportXMLToBinary( const char * filename, XmlNodeRef animGraph );
	bool LoadAnimationGraph( const char * name, const char * filename, XmlNodeRef animGraph, bool loadBinary /*=false*/ );
	bool IsAnimationGraphLoaded( const char * name );
	IAnimationGraphQueryResultsPtr QueryAnimationGraph( const char * name, const SAnimationGraphQueryInputs * inputs );
	// ~IAnimationGraphSystem

	void AddLogMarker();
	void Log( ColorB clr, IEntity* pEnt, const char * fmt, ... );
	void DoneLogMarker(bool commit);
	bool GotDataSinceMarker();
	void Update();

private:
	class CategoryIt;
	class StateFactoryIt;
	class StateIt;
	class GraphInputs;
	class QueryResults;

	bool LoadGraph( string name, XmlNodeRef xml, IAnimationGraphPtr* ppGraph );

	std::map<string, SCategoryInfo> m_categoryInfo;
	int m_numOverrides;
	std::map<string, IAnimationStateNodeFactory*(*)()> m_stateFactoryFactories;
	std::map<string, _smart_ptr<CAnimationGraph> > m_graphs;

	CAnimationGraphCVars m_cVars;

	struct SLogRecord
	{
		ColorB color;
		string entity;
		string message;
		CTimeValue when;
	};
	std::list<SLogRecord> m_log;
	std::stack<size_t> m_logMarkers;

	static const size_t MAX_LOG_ENTRIES = 50;
};

#endif
