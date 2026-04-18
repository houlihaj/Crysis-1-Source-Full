#include "stdafx.h"
#include "IslandDetection.h"

inline const char* to_str( float val )
{
	static char temp[32];
	sprintf( temp,"%g",val );
	return temp;
}

inline const char* to_str( int val )
{
	static char temp[32];
	sprintf( temp,"%d",val );
	return temp;
}

struct SortAGStateByName
{
	bool operator()( CAGStatePtr a, CAGStatePtr b ) const
	{
		return a->GetName() < b->GetName();
	}
};

bool IncludeState( CAGStatePtr pState )
{
	return pState->IncludeInGame();
}

void GenerateIslands( TAGIslands& islands, CAnimationGraphPtr pGraph )
{
	islands.clear();

	typedef std::map<CAGStatePtr, TAGIslands::iterator> TStateToIsland;
	TStateToIsland stateToIsland;

	// pass 1: add all states to individual islands
	for (CAnimationGraph::state_iterator iter = pGraph->StateBegin(); iter != pGraph->StateEnd(); ++iter)
	{
		if (!IncludeState(*iter))
			continue;

		TAGIsland island;
		island.push_back(*iter);

		islands.push_front(island);
		stateToIsland[*iter] = islands.begin();
	}

	// pass 2:
	//  for each link,
	//    if this link joins two different islands,
	//      merge the islands
	typedef CAnimationGraph::TLinkVec TLinkVec;
	TLinkVec linkVec;
	pGraph->GetLinks(linkVec);

	for (TLinkVec::const_iterator linkIter = linkVec.begin(); linkIter != linkVec.end(); ++linkIter)
	{
		if (!IncludeState(linkIter->first) || !IncludeState(linkIter->second))
			continue;

		TStateToIsland::iterator iterMapIslandFirst = stateToIsland.find( linkIter->first );
		TStateToIsland::iterator iterMapIslandSecond = stateToIsland.find( linkIter->second );
		if (iterMapIslandFirst->second != iterMapIslandSecond->second)
		{
			TAGIslands::iterator iterIslandFirst = iterMapIslandFirst->second;
			TAGIslands::iterator iterIslandSecond = iterMapIslandSecond->second;
			TAGIsland& islandFirst = *iterIslandFirst;
			TAGIsland& islandSecond = *iterIslandSecond;

			for (TAGIsland::iterator iterSecond = islandSecond.begin(); iterSecond != islandSecond.end(); ++iterSecond)
			{
				islandFirst.push_back(*iterSecond);
				stateToIsland[*iterSecond] = iterIslandFirst;
			}

			islands.erase( iterIslandSecond );
		}
		assert( stateToIsland.find( linkIter->first )->second == stateToIsland.find( linkIter->second )->second );
	}

	// pass 3: sorting
	for (TAGIslands::iterator iter = islands.begin(); iter != islands.end(); ++iter)
		std::sort( iter->begin(), iter->end(), SortAGStateByName() );
}

CString GenerateIslandReport( const TAGIslands& islands )
{
	CString os;

	TAGIsland unlinked;

	int i = 0;
	for (TAGIslands::const_iterator iter = islands.begin(); iter != islands.end(); ++iter)
	{
		if (iter->size() == 1)
			unlinked.push_back((*iter)[0]);
		else
		{
			os += CString("island ") + to_str(i) + "\r\n";
			os += "--------------------------------------------\r\n";
			for (TAGIsland::const_iterator iter2 = iter->begin(); iter2 != iter->end(); ++iter2)
				os += CString((*iter2)->GetName()) + "\r\n";
			os += "\r\n\r\n";
			i++;
		}
	}

	os += "unlinked items\r\n";
	os += "--------------------------------------------\r\n";
	std::sort(unlinked.begin(), unlinked.end());
	for (TAGIsland::const_iterator iter2 = unlinked.begin(); iter2 != unlinked.end(); ++iter2)
		os += CString((*iter2)->GetName()) + "\r\n";
	os += "\r\n\r\n";

	return os;
}

CString GenerateIslandReport( CAnimationGraphPtr pGraph )
{
	TAGIslands islands;
	GenerateIslands( islands, pGraph );
	return GenerateIslandReport( islands );
}
