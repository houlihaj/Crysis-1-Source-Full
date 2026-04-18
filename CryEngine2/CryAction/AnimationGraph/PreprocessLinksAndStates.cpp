#include "StdAfx.h"
#include "PreprocessLinksAndStates.h"
#include "AnimationGraph.h"

namespace
{

	struct SBasicLink
	{
		SBasicLink() : cost(100), forceFollowChance(0), transitionTime(0) {}
		int cost;
		int forceFollowChance;
		float transitionTime;
	};

	struct SLink : public SBasicLink
	{
		XmlNodeRef node;
	};

	struct SState
	{
		bool isNullNode;
		bool isSelectable;
		bool isHurryable;
		bool isInGame;
		string parent;

		XmlNodeRef node;
	};

	struct SInput
	{
		bool forceGuard;
		bool mixinFilter;
		string defaultValue;
		XmlNodeRef node;
	};

	class CPreprocessContext
	{
	public:
		bool Init( XmlNodeRef root );

		typedef std::set<string> SView;
		typedef std::map<string, SView> SViewMap;
		typedef std::pair<string, string> SPair;
		typedef std::map<SPair, SLink> SLinkMap;
		typedef std::map<string, SState> SStateMap;
		typedef std::map<string, SInput> SInputMap;

		typedef SStateMap::iterator state_iterator;
		state_iterator StateBegin()
		{
			return m_stateMap.begin();
		}
		state_iterator StateEnd()
		{
			return m_stateMap.end();
		}
		state_iterator State( const string& name )
		{
			return m_stateMap.find(name);
		}

		typedef SViewMap::iterator view_iterator;
		view_iterator ViewBegin()
		{
			return m_viewMap.begin();
		}
		view_iterator ViewEnd()
		{
			return m_viewMap.end();
		}
		view_iterator View( const string& name )
		{
			return m_viewMap.find(name);
		}

		typedef SLinkMap::iterator link_iterator;
		link_iterator LinkBegin() { return m_linkMap.begin(); }
		link_iterator LinkEnd() { return m_linkMap.end(); }
		link_iterator LinkBegin( string from )
		{
			link_iterator iter = m_linkMap.lower_bound(SPair(from,""));
			if (iter == m_linkMap.end() || iter->first.first != from)
				return m_linkMap.end();
			return iter;
		}
		link_iterator InvLinkBegin() { return m_invLinkMap.begin(); }
		link_iterator InvLinkEnd() { return m_invLinkMap.end(); }
		link_iterator InvLinkBegin( string to )
		{
			link_iterator iter = m_invLinkMap.lower_bound(SPair(to,""));
			if (iter == m_invLinkMap.end() || iter->first.first != to)
				return m_invLinkMap.end();
			return iter;
		}
		link_iterator GetLinkIter( const SPair& p )
		{
			return m_linkMap.find(p);
		}
		void RemoveLink( link_iterator iter )
		{
			m_invLinkMap.erase( SPair(iter->first.second, iter->first.first) );
			m_links->removeChild( iter->second.node );
			m_linkMap.erase(iter);
		}
		void RemoveLink( string from, string to )
		{
			link_iterator iter = m_linkMap.find(SPair(from,to));
			if (iter != m_linkMap.end())
				RemoveLink(iter);
		}
		void RemoveLinks( link_iterator itBegin, link_iterator itEnd )
		{
			for ( link_iterator it = itBegin; it != itEnd; ++it )
			{
				m_invLinkMap.erase( SPair(it->first.second, it->first.first) );
				m_links->removeChild( it->second.node );
			}
			m_linkMap.erase( itBegin, itEnd );
		}
		void RemoveLinksTo( const string& to )
		{
			link_iterator itBegin = InvLinkBegin( to ), itEnd;
			for ( itEnd = itBegin; itEnd != InvLinkEnd() && itEnd->first.first == to; ++itEnd )
			{
				m_linkMap.erase( SPair(itEnd->first.second, itEnd->first.first) );
				m_links->removeChild( itEnd->second.node );
			}
			m_invLinkMap.erase( itBegin, itEnd );
		}
		link_iterator AddLink( string from, string to, int cost, int forceFollowChance, float transitionTime )
		{
			if (from == to)
				return m_linkMap.end();

 			SLink link;
			link.cost = cost;
			link.forceFollowChance = forceFollowChance;
			link.transitionTime = transitionTime;
			std::pair<link_iterator,bool> r = m_linkMap.insert( std::make_pair( SPair(from,to), link ) );
			m_invLinkMap.insert( std::make_pair( SPair(to,from), link ) );
			if (!r.first->second.node)
			{
				r.first->second.node = m_links->createNode("Link");
				r.first->second.node->setAttr("from", from);
				r.first->second.node->setAttr("to", to);
				m_links->addChild(r.first->second.node);
			}

			r.first->second.cost = link.cost = min(link.cost, r.first->second.cost);
			r.first->second.node->setAttr("cost", link.cost);

			r.first->second.forceFollowChance = link.forceFollowChance = max(link.forceFollowChance, r.first->second.forceFollowChance);
			r.first->second.node->setAttr("forceFollowChance", link.forceFollowChance);

			r.first->second.transitionTime = link.transitionTime = max(link.transitionTime, r.first->second.transitionTime);
			r.first->second.node->setAttr("transitionTime", link.transitionTime);

			r.first->second.node->setAttr("artificial", true);
			return r.first;
		}
		void RemoveState( state_iterator iter )
		{
			iter->second.isInGame = false;
			iter->second.node->setAttr("includeInGame", false);
		}
		// only really works for keyed values
		string GetSelectionCriteriaForState( string name, string input, float& minValue, float& maxValue )
		{
			minValue = maxValue = -1.f;
			while (true)
			{
				state_iterator iter = State(name);
				if (iter == StateEnd())
					return string();
				XmlNodeRef selectBlock = iter->second.node->findChild("SelectWhen");
				if (selectBlock)
				{
					XmlNodeRef value = selectBlock->findChild(input.c_str());
					if (value && value->haveAttr("value"))
						return value->getAttr("value");
					else if (value && value->haveAttr("min") && value->haveAttr("max"))
					{
						value->getAttr("min", minValue);
						value->getAttr("max", maxValue);
						return string();
					}
					else if (value)
						return string();
				}
				if (!iter->second.node->haveAttr("extend"))
					return string();
				name = iter->second.node->getAttr("extend");
			}
		}
		XmlNodeRef AddState( string name, string parent, bool selectable, bool hurryable )
		{
			SState state;
			state.isNullNode = false;
			state.isInGame = true;
			state.isSelectable = selectable;
			state.isHurryable = hurryable;

			XmlNodeRef parentNode = State(parent)->second.node;

			state.node = m_states->createNode("State");
			for (size_t i=0; i<parentNode->getNumAttributes(); i++)
			{
				const char * key, * value;
				parentNode->getAttributeByIndex( i, &key, &value );
				state.node->setAttr( key, value );
			}
			state.node->setAttr("id", name);
			state.node->setAttr("allowSelect", selectable);
			state.node->setAttr("canMix", name == (parent+"+Nothing"));
			state.node->setAttr("hurryable", hurryable);
			state.node->setAttr("artificial", 1);
			state.parent = parent;
			state.node->setAttr("extend", parent);

			if ( XmlNodeRef guardNode = parentNode->findChild("Guard") )
			{
				state.node->addChild(guardNode->clone());
			}

			std::pair<state_iterator,bool> r = m_stateMap.insert( std::make_pair(name, state) );
			if ( !r.second )
			{
				m_states->removeChild( r.first->second.node );
				m_stateMap.erase( r.first );
				m_stateMap.insert( std::make_pair(name, state) );
			}
			m_states->addChild(state.node);
			return state.node;
		}
		// name=add this state, parent=derive from this, basis=copy contents to create mixed state from this
		void AddState( string name, string parent, string basis )
		{
			bool selectable = State(parent)->second.isSelectable && State(basis)->second.isSelectable;
			bool hurryable = State(parent)->second.isHurryable && State(basis)->second.isHurryable;
			if (XmlNodeRef node = AddState(name, parent, selectable, hurryable))
			{
				XmlNodeRef basisNode = State(basis)->second.node;
				bool animationControlledView = false;
				if (basisNode->getAttr( "animationControlledView", animationControlledView ))
					node->setAttr( "animationControlledView", animationControlledView );
				for (size_t i=0; i<basisNode->getChildCount(); i++)
				{
					node->addChild( basisNode->getChild(i)->clone() );
				}
			}
		}

		SInput GetInput( const string& name )
		{
			SInputMap::iterator iter = m_inputMap.find(name);
			if (iter != m_inputMap.end())
				return iter->second;
			SInput blah;
			blah.defaultValue = "";
			blah.forceGuard = false;
			return blah;
		}
		typedef SInputMap::iterator input_iterator;
		input_iterator InputBegin()
		{
			return m_inputMap.begin();
		}
		input_iterator InputEnd()
		{
			return m_inputMap.end();
		}

	private:
		XmlNodeRef m_root;
		XmlNodeRef m_links;
		XmlNodeRef m_states;
		XmlNodeRef m_inputs;

		SLinkMap m_linkMap;
		SLinkMap m_invLinkMap;
		SStateMap m_stateMap;
		SViewMap m_viewMap;
		SInputMap m_inputMap;
	};

	bool CPreprocessContext::Init( XmlNodeRef root )
	{
		bool ok = true;
		m_root = root;
		m_links = root->findChild("Transitions");
		m_states = root->findChild("States");
		m_inputs = root->findChild("Inputs");
		if (!m_links || !m_states || !m_inputs)
			return false;
		for (size_t i=0; i<m_links->getChildCount(); i++)
		{
			SLink link;
			SPair idx;
			bool linkOk = true;
			link.node = m_links->getChild(i);
			idx.first = link.node->getAttr("from");
			idx.second = link.node->getAttr("to");
			if (idx.first.empty() || idx.second.empty())
				linkOk = false;
			link.cost = 100;
			link.forceFollowChance = 0;
			link.transitionTime = 0.0f;
			link.node->getAttr("cost", link.cost);
			link.node->getAttr("forceFollowChance", link.forceFollowChance);
			link.node->getAttr("transitionTime", link.transitionTime);
			if (linkOk)
			{
				m_linkMap.insert(std::make_pair(idx,link));
				m_invLinkMap.insert(std::make_pair(SPair(idx.second,idx.first),link));
			}
			else
				ok = false;
		}
		for (size_t i=0; i<m_states->getChildCount(); i++)
		{
			SState state;
			bool stateOk = true;
			state.node = m_states->getChild(i);
			string name = state.node->getAttr("id");
			if (name.empty())
				stateOk = false;
			state.isNullNode = true;
			state.isInGame = true;
			stateOk &= state.node->getAttr("allowSelect", state.isSelectable);
			state.node->getAttr("hurryable", state.isHurryable);
			state.node->getAttr("includeInGame", state.isInGame);
			state.parent = state.node->getAttr("extend");
			for (size_t j=0; j<state.node->getChildCount(); j++)
			{
				string tag = state.node->getChild(j)->getTag();
				if (tag == "SelectWhen" || tag == "Param" || tag == "Guard" || tag == "MixIn")
					continue;
				state.isNullNode = false;
			}
			if (stateOk)
				m_stateMap.insert( std::make_pair(name,state) );
			else
				ok = false;
		}
		if (XmlNodeRef views = m_root->findChild("Views"))
		{
			for (size_t i=0; i<views->getChildCount(); i++)
			{
				XmlNodeRef view = views->getChild(i);
				for (size_t j=0; j<view->getChildCount(); j++)
				{
					XmlNodeRef state = view->getChild(j);
					m_viewMap[view->getAttr("name")].insert(state->getAttr("id"));
				}
			}
		}
		for (size_t i=0; i<m_inputs->getChildCount(); i++)
		{
			SInput input;
			input.node = m_inputs->getChild(i);
			input.defaultValue = input.node->getAttr("defaultValue");
			input.forceGuard = false;
			input.node->getAttr("forceGuard", input.forceGuard);
			input.mixinFilter = false;
			input.node->getAttr("mixinFilter", input.mixinFilter);
			m_inputMap.insert( std::make_pair(input.node->getAttr("name"), input) );
		}
		return ok;
	}

	static const char MIXIN_PREFIX = '+';

	struct StateToRemove
	{
		int numLinks;
		CPreprocessContext::state_iterator state;
		StateToRemove() {}
		StateToRemove( int _numLinks, CPreprocessContext::state_iterator _state ) : numLinks(_numLinks), state(_state) {}
		bool operator < ( const StateToRemove& rhs ) const
		{
			return numLinks > rhs.numLinks;
		}
	};

	bool RemoveExtraneousNullNodes( CPreprocessContext& ctx, bool allowMixins )
	{
		PROFILE_LOADING_FUNCTION;

		std::vector< StateToRemove > statesToRemove;
		CPreprocessContext::link_iterator linkItersFromBegin, linkItersFromEnd;
		CPreprocessContext::link_iterator linkItersToBegin, linkItersToEnd;

		for (CPreprocessContext::state_iterator stateIter = ctx.StateBegin(); stateIter != ctx.StateEnd(); ++stateIter)
		{
			if (!stateIter->second.isNullNode || stateIter->second.isSelectable || !stateIter->second.isInGame)
				continue;

			if (stateIter->first.c_str()[0] == MIXIN_PREFIX)
				if (!allowMixins)
					continue;

			int numLinksFrom = 0;
			int numLinksTo = 0;
			bool anyForceFollows = false;

			linkItersFromBegin = linkItersFromEnd = ctx.LinkBegin(stateIter->first);
			if ( linkItersFromEnd == ctx.LinkEnd() )
				continue;
			++numLinksFrom;
			while (!anyForceFollows && ++linkItersFromEnd != ctx.LinkEnd() && linkItersFromEnd->first.first == stateIter->first)
			{
				if (linkItersFromEnd->second.forceFollowChance)
					anyForceFollows = true;
				++numLinksFrom;
			}

			if (!anyForceFollows)
			{
				// find all links leading to this state
				linkItersToBegin = linkItersToEnd = ctx.InvLinkBegin(stateIter->first);
				if ( linkItersToEnd == ctx.InvLinkEnd() )
					continue;
				++numLinksTo;
				while (++linkItersToEnd != ctx.InvLinkEnd() && linkItersToEnd->first.first == stateIter->first)
					++numLinksTo;

				// remember to remove this node
				statesToRemove.push_back( StateToRemove(numLinksFrom*numLinksTo, stateIter) );
			}
		}

		while ( !statesToRemove.empty() )
		{
			CPreprocessContext::state_iterator stateIter = statesToRemove.back().state;
			statesToRemove.pop_back();

			bool anyForceFollows = false;

			linkItersFromBegin = linkItersFromEnd = ctx.LinkBegin(stateIter->first);
			if ( linkItersFromEnd == ctx.LinkEnd() )
				continue;
			while (!anyForceFollows && ++linkItersFromEnd != ctx.LinkEnd() && linkItersFromEnd->first.first == stateIter->first)
				if (linkItersFromEnd->second.forceFollowChance)
					anyForceFollows = true;

			if (!anyForceFollows)
			{
				// find all links leading to this state
				linkItersToBegin = linkItersToEnd = ctx.InvLinkBegin(stateIter->first);
				if ( linkItersToEnd == ctx.InvLinkEnd() )
					continue;
				while (++linkItersToEnd != ctx.InvLinkEnd() && linkItersToEnd->first.first == stateIter->first)
					;

				CryLog("Bypassing redundant null node %s", stateIter->first.c_str());
				int state_cost = 100;
				stateIter->second.node->getAttr("cost", state_cost);

				for (CPreprocessContext::link_iterator linkIterToIter = linkItersToBegin; linkIterToIter != linkItersToEnd; ++linkIterToIter)
				{
					for (CPreprocessContext::link_iterator linkIterFromIter = linkItersFromBegin; linkIterFromIter != linkItersFromEnd; ++linkIterFromIter)
					{
						int cost_to = linkIterToIter->second.cost;
						int cost_from = linkIterFromIter->second.cost;
						int cost = cost_to + state_cost + cost_from;
						// this will mess up force follow chances (they'd need to be averaged)
						// but I don't expect any to show up here
						float transitionTime = max(linkIterToIter->second.transitionTime, linkIterFromIter->second.transitionTime);
						ctx.AddLink(linkIterToIter->first.second, linkIterFromIter->first.second, cost, linkIterToIter->second.forceFollowChance, transitionTime);
					}
				}
				ctx.RemoveLinks( linkItersFromBegin, linkItersFromEnd );
				ctx.RemoveLinksTo( stateIter->first );
			}

			std::sort( statesToRemove.begin(), statesToRemove.end() );
		}

		return true;
	}
	
	bool RemoveTopLevelMixins( CPreprocessContext& ctx )
	{
		std::vector<CPreprocessContext::state_iterator> remove;

		for (CPreprocessContext::state_iterator stateIter = ctx.StateBegin(); stateIter != ctx.StateEnd(); ++stateIter)
		{
			if (stateIter->first.c_str()[0] == MIXIN_PREFIX)
				remove.push_back(stateIter);
		}
		while (!remove.empty())
		{
			ctx.RemoveState(remove.back());
			remove.pop_back();
		}

		return true;
	}

	// this function is probably a very good candidate for splitting up at this point
	// merges mixin nodes to create a graph with no mixin nodes
	bool MergeExtraNodes( CPreprocessContext& ctx )
	{
		PROFILE_LOADING_FUNCTION;

		string NothingState = MIXIN_PREFIX + string("Nothing");

		std::vector< CPreprocessContext::state_iterator > statesToMix;
		for (CPreprocessContext::state_iterator stateIter = ctx.StateBegin(); stateIter != ctx.StateEnd(); ++stateIter)
		{
			CPreprocessContext::state_iterator checkIter = stateIter;
			while (checkIter != ctx.StateEnd())
			{
				if (checkIter->second.node->findChild("MixIn"))
				{
					statesToMix.push_back(stateIter);
					break;
				}
				checkIter = ctx.State(checkIter->second.parent);
			}
		}

		bool ok = true;

		std::multimap<string, string> toLink;
		std::map< string, std::set<string> > derivedStates;
		std::set<string> mixedStates;

		std::vector< CPreprocessContext::state_iterator >::iterator stateToMixIt, stateToMixItEnd = statesToMix.end();
		for ( stateToMixIt = statesToMix.begin(); stateToMixIt != stateToMixItEnd; ++stateToMixIt )
		{
			string stateName = (*stateToMixIt)->first;
			std::set<string> mixedAlready;
			if (stateName.empty())
			{
				GameWarning("Invalid AG state name for a mixin");
				continue;
			}
			if (stateName.find(MIXIN_PREFIX) != string::npos)
			{
				GameWarning("Upper body AG state '%s' derived from a full body state", stateName.c_str());
				continue;
			}
			mixedStates.insert(stateName);
			CPreprocessContext::state_iterator mixingState = *stateToMixIt;
			while (mixingState != ctx.StateEnd())
			{
				for (size_t i=0; i<mixingState->second.node->getChildCount(); i++)
				{
					XmlNodeRef node = mixingState->second.node->getChild(i);
					if (node->getTag() == string("MixIn"))
					{
						string mixinName = node->getAttr("id");

						// is it a mix-in state or a mix-in view?
						CPreprocessContext::state_iterator addState = ctx.State(mixinName);
						if (addState != ctx.StateEnd())
						{
							ctx.AddState( stateName + mixinName, stateName, mixinName );
							toLink.insert( std::make_pair(stateName, mixinName) );
							derivedStates[mixinName].insert(stateName);
						}
						else
						{
							CPreprocessContext::view_iterator viewIter = ctx.View(mixinName);
							if (viewIter == ctx.ViewEnd())
								ok = false;
							else
							{
								for (CPreprocessContext::SView::iterator iter = viewIter->second.begin(); iter != viewIter->second.end(); ++iter)
								{
									CPreprocessContext::state_iterator stateIter = ctx.State(*iter);
									if (stateIter != ctx.StateEnd() && stateIter->second.isInGame)
									{
										// should we mix this state based on inputs
										bool shouldMix = mixedAlready.find(*iter) == mixedAlready.end();

										string valueState, valueMixin;
										float minValueState, maxValueState, minValueMixin, maxValueMixin;
										for (CPreprocessContext::input_iterator iterInput = ctx.InputBegin(); shouldMix && iterInput != ctx.InputEnd(); ++iterInput)
										{
											if ( iterInput->second.mixinFilter || iterInput->second.forceGuard )
											{
												valueState = ctx.GetSelectionCriteriaForState(stateName, iterInput->first, minValueState, maxValueState);
												valueMixin = ctx.GetSelectionCriteriaForState(*iter, iterInput->first, minValueMixin, maxValueMixin);
											}
											while (iterInput->second.mixinFilter)
											{
												if (valueState.empty() && valueMixin.empty())
												{
													if ( minValueState == -1.f && maxValueState == -1.f && minValueMixin == -1.f && maxValueMixin == -1.f )
														break;
													if ( minValueMixin == -1.f && maxValueMixin == -1.f )
														break;
													if ( minValueMixin < minValueState || maxValueMixin > maxValueState )
														shouldMix = false;
													break;
												}
												if (valueMixin.empty())
													break;
												if (valueState != valueMixin)
													shouldMix = false;
												break;
											}
											while (iterInput->second.forceGuard)
											{
												if (valueState.empty() || valueState == iterInput->second.defaultValue)
												{
													if ( minValueState == -1.f && maxValueState == -1.f || minValueMixin == -1.f && maxValueMixin == -1.f )
														break;
													if ( minValueMixin == -1.f && maxValueMixin == -1.f )
														break;
													if ( minValueMixin < minValueState || maxValueMixin > maxValueState )
														shouldMix = false;
													break;
												}
												if (valueMixin.empty())
													break;
												if (valueState != valueMixin)
													shouldMix = false;
												break;
											}
										}

										// finally, schedule the mixing
										if (shouldMix)
										{
											ctx.AddState( stateName + *iter, stateName, *iter );
											toLink.insert( std::make_pair(stateName, *iter) );
											derivedStates[*iter].insert(stateName);
											mixedAlready.insert(*iter);
										}
									}
								}
							}
						}
					}
				}
				mixingState = ctx.State(mixingState->second.parent);
			}
			(*stateToMixIt)->second.node->setAttr("allowSelect", false);
		}

		std::map< std::pair<string, string>, SBasicLink > addLinks;
		std::set< std::pair<string, string> > removeLinks;

		for (CPreprocessContext::link_iterator linkIter = ctx.LinkBegin(); linkIter != ctx.LinkEnd(); ++linkIter)
		{
			string from = linkIter->first.first;
			string to = linkIter->first.second;

			// skip cyclic links
			if (from == to)
			{
				removeLinks.insert(std::make_pair(from, to));
				continue;
			}

			// add the "inner" links (links that were in views between child states, that need to be propagated into the real graph)
			if (from[0] == MIXIN_PREFIX && to[0] == MIXIN_PREFIX)
			{
				std::set<string>& fromStates = derivedStates[from];
				std::set<string>& toStates = derivedStates[to];

				for (std::set<string>::iterator iter = fromStates.begin(); iter != fromStates.end(); ++iter)
				{
					if (toStates.find(*iter) != toStates.end())
						addLinks.insert(std::make_pair( std::make_pair(*iter + from, *iter + to), linkIter->second ));
				}
				continue;
			}
			// and skip things that were not involved in mixing
			else if (mixedStates.find(from) == mixedStates.end() && mixedStates.find(to) == mixedStates.end())
				continue;

			// add "outer" links (links between derived states that were originally between parent states)
			std::multimap<string,string>::iterator fromIter = toLink.find(from);
			if (fromIter != toLink.end())
			{
				removeLinks.insert(std::make_pair(from, to));

				while (fromIter != toLink.end() && fromIter->first == from)
				{
					std::set<string>& possibleToStates = derivedStates[fromIter->second];
					if (possibleToStates.find(to) != possibleToStates.end())
						addLinks.insert(std::make_pair( std::make_pair(from + fromIter->second, to + fromIter->second), linkIter->second ));
					else if (fromIter->second == NothingState && mixedStates.find(to) == mixedStates.end())
						addLinks.insert(std::make_pair( std::make_pair(from + fromIter->second, to), linkIter->second ));

					++fromIter;
				}
			}
			if (mixedStates.find(from) == mixedStates.end())
			{
				removeLinks.insert(std::make_pair(from, to));
				std::set<string>& possibleToStates = derivedStates[NothingState];
				if (possibleToStates.find(to) != possibleToStates.end())
					addLinks.insert(std::make_pair( std::make_pair(from, to + NothingState), linkIter->second ));
			}
		}

		for (std::set< std::pair<string, string> >::iterator iter = removeLinks.begin(); iter != removeLinks.end(); ++iter)
		{
			ctx.RemoveLink(iter->first, iter->second);
		}

		for (std::map< std::pair<string, string>, SBasicLink >::iterator iter = addLinks.begin(); iter != addLinks.end(); ++iter)
		{
			ctx.AddLink(iter->first.first, iter->first.second, iter->second.cost, iter->second.forceFollowChance, iter->second.transitionTime);
		}

		return ok;
	}

	bool AddForcedGuards( CPreprocessContext& ctx )
	{
		for (CPreprocessContext::state_iterator iterState = ctx.StateBegin(); iterState != ctx.StateEnd(); ++iterState)
		{
			std::set< string > inputsDone;
			XmlNodeRef guard = iterState->second.node->findChild("Guard");
			CPreprocessContext::state_iterator iterCurrentState = iterState;
			for ( ; iterCurrentState != ctx.StateEnd(); iterCurrentState
				= iterCurrentState->second.parent.empty() ? ctx.StateEnd() : ctx.State(iterCurrentState->second.parent) )
			{
				XmlNodeRef selectCritNode = iterCurrentState->second.node->findChild("SelectWhen");
				if ( !selectCritNode )
					continue;

				for (size_t i=0; i<selectCritNode->getChildCount(); i++)
				{
					XmlNodeRef critNode = selectCritNode->getChild(i);
					const char* inputName = critNode->getTag();
					if ( inputsDone.find(inputName) != inputsDone.end() )
						continue;
					inputsDone.insert(inputName);
					SInput input = ctx.GetInput(inputName);
					if (input.forceGuard)
					{
						if (critNode->haveAttr("value") && critNode->getAttr("value") != input.defaultValue)
						{
							if (!guard)
							{
								guard = iterState->second.node->createNode("Guard");
								iterState->second.node->addChild(guard);
							}
							XmlNodeRef node = critNode->clone();
							node->setAttr("min", critNode->getAttr("value"));
							node->setAttr("max", critNode->getAttr("value"));
							node->delAttr("value");
							guard->addChild(node);
						}
						else if (critNode->haveAttr("min") && critNode->haveAttr("max"))
						{
							if (!guard)
							{
								guard = iterState->second.node->createNode("Guard");
								iterState->second.node->addChild(guard);
							}
							XmlNodeRef node = critNode->clone();
							node->setAttr("min", critNode->getAttr("min"));
							node->setAttr("max", critNode->getAttr("max"));
							guard->addChild(node);
						}
					}
				}
			}
		}
		return true;
	}

}

bool PreprocessLinksAndStates( XmlNodeRef root )
{
 	PROFILE_LOADING_FUNCTION;

	CPreprocessContext ctx;
	if (!ctx.Init(root))
		return false;

	bool ok = true;

//	if (!RemoveExtraneousNullNodes(ctx, false))
//		ok = false;
	if (!MergeExtraNodes(ctx))
		ok = false;
	if (!RemoveTopLevelMixins(ctx))
		ok = false;
//	if (!RemoveExtraneousNullNodes(ctx, true))
//		ok = false;
	if (!AddForcedGuards(ctx))
		ok = false;

	return ok;
}
