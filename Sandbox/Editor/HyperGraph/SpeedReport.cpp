#include "stdafx.h"
#include "SpeedReport.h"

static inline void warning(const char * format, ...)
{
	char buffer[4096];
	va_list args;
	va_start(args, format);
	vsprintf(buffer, format, args);
	va_end(args);

	CErrorRecord err;
	err.module = VALIDATOR_MODULE_ANIMATION;
	err.severity = CErrorRecord::ESEVERITY_WARNING;
	err.error = buffer;
	err.pItem = 0;
	GetIEditor()->GetErrorReport()->ReportError( err );
}

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

CString GenerateSpeedReport( IAnimationSet * pAnimSet )
{
	CString os;

	std::map<CString, float> speeds;

  for (uint32 i=0; i<pAnimSet->numAnimations(); i++)
		speeds[pAnimSet->GetNameByAnimID(i)] = pAnimSet->GetSpeed(i);
	for (std::map<CString, float>::const_iterator iter = speeds.begin(); iter != speeds.end(); ++iter)
		os += iter->first + ": " + to_str(iter->second) + " m/sec\n";

	return os;
}

CString MatchMovementSpeedsToAnimations( IAnimationSet * pAnimSet, CAnimationGraph * pGraph )
{
/*
<Param name="animation_slow" type="string"/>
<Param name="animation_fast" type="string"/>
<Param name="turn_left_slow" type="string"/>
<Param name="turn_left_fast" type="string"/>
<Param name="turn_right_slow" type="string"/>
<Param name="turn_right_fast" type="string"/>
<Param name="speed_slow" type="float"/>
<Param name="speed_fast" type="float"/>
<Param name="key_time" type="float"/>
*/

	std::map<CString, float> speeds;
	std::map<CString,CString> couldntProcess;
	std::vector<CString> couldProcess;

	for (uint32 i=0; i<pAnimSet->numAnimations(); i++)
		speeds[pAnimSet->GetNameByAnimID(i)] = pAnimSet->GetSpeed(i);

	for (CAnimationGraph::state_iterator iterState = pGraph->StateBegin(); iterState != pGraph->StateEnd(); ++iterState)
	{
		CAGStatePtr pState = *iterState;

		if (pState->GetTemplateType() == "Movement")
		{
			if (pState->HasTemplateParam("speed_slow") && pState->HasTemplateParam("speed_fast") && pState->HasTemplateParam("animation_slow") && pState->HasTemplateParam("animation_fast"))
			{
				std::map<CString, float>::const_iterator iterSlow = speeds.find(pState->GetTemplateParameter("animation_slow"));
				std::map<CString, float>::const_iterator iterFast = speeds.find(pState->GetTemplateParameter("animation_fast"));
				if (iterSlow != speeds.end() && iterFast != speeds.end())
				{
					if (iterSlow->second != iterFast->second)
					{
						CString s;
						s.Format("%f", iterSlow->second);
						pState->SetTemplateParameter("speed_slow", s);
						s.Format("%f", iterFast->second);
						pState->SetTemplateParameter("speed_fast", s);
						couldProcess.push_back(pState->GetName());
					}
					else
					{
						couldntProcess[pState->GetName()] = "Speeds are identical";
					}
				}
				else
				{
					couldntProcess[pState->GetName()] = "No speeds in animation set (probably missing animations)";
				}
			}
			else
			{
				couldntProcess[pState->GetName()] = "Missing parameters";
			}
		}
	}

	std::sort(couldProcess.begin(), couldProcess.end());

	CString os;
	os += "Changed the following states:\n";
	for (size_t i=0; i<couldProcess.size(); ++i)
	{
		os += CString("  ") + couldProcess[i] + "\n";
	}
	os += "Couldn't process the following states:\n";
	for (std::map<CString,CString>::const_iterator iter = couldntProcess.begin(); iter != couldntProcess.end(); ++iter)
	{
		os += CString("  ") + iter->first + ": " + iter->second + "\n";
	}

	return os;
}

// a functor class to be used to replace all params in a string with real values
class ReplaceParamsFunc
{
	bool m_bResult;
	CString& m_value;
public:
	ReplaceParamsFunc( CString& value ) : m_value( value ), m_bResult( false ) {}
	void operator () ( const std::pair< const CString, CString >& param )
	{
		CString first = param.first;
		first.MakeLower();
		int len = first.GetLength() + 3;
		char* temp = new char[len];
		temp[0] = '[';
		memcpy( temp+1, (LPCSTR)first, len-3 );
		temp[len-2] = ']';
		temp[len-1] = 0;
		int pos = 0;
		while ( (pos = m_value.Find( temp, pos )) != -1 )
		{
			CString lstr( param.second );
			lstr.MakeLower();
			m_value.Delete( pos, len-1 );
			m_value.Insert( pos, lstr );
			m_bResult = true;
		}
		temp[1] = ::toupper( temp[1] );
		pos = 0;
		while ( (pos = m_value.Find( temp, pos )) != -1 )
		{
			m_value.Delete( pos, len-1 );
			m_value.Insert( pos, param.second );
			m_bResult = true;
		}
		for ( int i = 2; i < len-2; ++i )
			temp[i] = ::toupper( temp[i] );
		pos = 0;
		while ( (pos = m_value.Find( temp, pos )) != -1 )
		{
			CString ustr( param.second );
			ustr.MakeUpper();
			m_value.Delete( pos, len-1 );
			m_value.Insert( pos, ustr );
			m_bResult = true;
		}
		delete [] temp;
	}
	operator bool () const { return m_bResult; }
};

CString GenerateBadCALReport( IAnimationSet * pAnimSet, CAnimationGraph * pGraph )
{
	std::set<CString> names;
	std::multimap<CString,CString> couldntProcess;

	uint numAnimations = pAnimSet->numAnimations();
	for (uint32 i=0; i<numAnimations; i++)
		names.insert(pAnimSet->GetNameByAnimID(i));

	for (CAnimationGraph::state_iterator iterState = pGraph->StateBegin(); iterState != pGraph->StateEnd(); ++iterState)
	{
		CAGStatePtr pState = *iterState;

		static CString animation[] =
		{
			"animation",
			"animation2",
			"aim_pose",
			"end_aim_pose",
		};
		CParamsDeclaration& params = *pState->GetParamsDeclaration();
		CAGState::CartesianProductHelper helper;
		std::vector< TParameterizationId > product;

		CParamsDeclaration::iterator itParams, itParamsEnd = params.end();
		for ( itParams = params.begin(); itParams != itParamsEnd; ++itParams )
			helper.sets.push_back(std::make_pair( itParams->first, &itParams->second ));

		helper.Make( product, helper.sets.begin() );

		std::vector< TParameterizationId >::iterator it, itEnd = product.end();
		for ( it = product.begin(); it != itEnd; ++it )
		{
			pState->ActivateParameterization( it->empty() ? NULL : &*it );
			if ( pState->GetExcludeFromGraph() == 1 )
				continue;

			std::map<CString,CString>& tempParams = pState->GetTemplateParameters();

			for (int i = 0; i < sizeof(animation)/sizeof(CString); ++i)
			{
				std::map<CString,CString>::iterator itTemplateParam = tempParams.find( animation[i] );
				if ( itTemplateParam != tempParams.end() )
				{
					CString value = itTemplateParam->second;
					if ( value.IsEmpty() )
						continue;

					// this will replace all parameters with actual values
					std::for_each( it->begin(), it->end(), ReplaceParamsFunc(value) );

					std::set<CString>::const_iterator iter = names.find(value);
					if (iter == names.end())
						couldntProcess.insert( std::make_pair(pState->GetName(),value) );
				}
			}

			// must restore this after use!
			pState->ActivateParameterization( NULL );
		}
	}

	CString os;
	os += "The following states had missing animations in the cal file:\n";
	for (std::multimap<CString,CString>::const_iterator iter = couldntProcess.begin(); iter != couldntProcess.end(); ++iter)
	{
		if (pGraph->FindState(iter->first)->IncludeInGame())
		{
			os += CString("  ") + iter->first + ": " + iter->second + "\n";
			warning("Missing animation on state %s (%s)", iter->first, iter->second);
		}
	}
	os += "\n\n\nThe following states had missing animations in the cal file (but are not included in game):\n";
	for (std::multimap<CString,CString>::const_iterator iter = couldntProcess.begin(); iter != couldntProcess.end(); ++iter)
	{
		if (!pGraph->FindState(iter->first)->IncludeInGame())
			os += CString("  ") + iter->first + ": " + iter->second + "\n";
	}

	return os;
}

CString GenerateNullNodesWithNoForceLeaveReport( CAnimationGraph * pGraph )
{
	std::set<CString> names;

	for (CAnimationGraph::state_iterator iterState = pGraph->StateBegin(); iterState != pGraph->StateEnd(); ++iterState)
	{
		CAGStatePtr pState = *iterState;

		if (!pState->HasAnimation())
		{
			std::vector<CAGLinkPtr> links;
			pGraph->StateLinks( links, pState, false, true );

			bool ok = false;
			for (std::vector<CAGLinkPtr>::iterator iter = links.begin(); iter != links.end(); ++iter)
			{
				CAGLinkPtr pLink = *iter;
				if (pLink->GetForceFollowChance() > 0)
					ok = true;
			}
			if (!ok)
				names.insert(pState->GetName());
		}
	}

	CString os;
	os += "The following states are null states but have no force-follow links from them:\n";
	for (std::set<CString>::const_iterator iter = names.begin(); iter != names.end(); ++iter)
	{
		if (pGraph->FindState(*iter)->IncludeInGame() && pGraph->FindState(*iter)->AllowSelect())
		{
			os += CString("  ") + *iter + "\n";
			warning("Null state %s selectable with no force follows", *iter);
		}
	}
	os += "\n\n\nThe following states are null states but have no force-follow links from them (but are not included in game):\n";
	for (std::set<CString>::const_iterator iter = names.begin(); iter != names.end(); ++iter)
	{
		if (!pGraph->FindState(*iter)->IncludeInGame() && pGraph->FindState(*iter)->AllowSelect())
		{
			os += CString("  ") + *iter + "\n";
		}
	}

	return os;
}

CString GenerateOrphanNodesReport( CAnimationGraph * pGraph )
{
	std::set<CString> names;

	for (CAnimationGraph::state_iterator iterState = pGraph->StateBegin(); iterState != pGraph->StateEnd(); ++iterState)
	{
		CAGStatePtr pState = *iterState;

		if (!pState->GetParent())
			names.insert(pState->GetName());
	}

	CString os;
	os += "The following states are orphans (no parent state):\n";
	for (std::set<CString>::const_iterator iter = names.begin(); iter != names.end(); ++iter)
	{
		if (pGraph->FindState(*iter)->IncludeInGame())
		{
			os += "  " + *iter + "\n";
			if (*iter != "Alive" && *iter != "Dead")
				warning("%s has no parent state", *iter);
		}
	}
	os += "\n\n\nThe following states are null orphans (no parent state) (but are not included in game):\n";
	for (std::set<CString>::const_iterator iter = names.begin(); iter != names.end(); ++iter)
	{
		if (!pGraph->FindState(*iter)->IncludeInGame() && pGraph->FindState(*iter)->AllowSelect())
		{
			os += CString("  ") + *iter + "\n";
		}
	}

	return os;
}

CString GenerateDeadInputsReport( CAnimationGraph * pGraph )
{
	struct Callback : public IAnimationGraphDeadInputReportCallback
	{
		void OnDeadInputValues( std::pair<const char*, const char*>* pInputValues )
		{
			os += "------------------------------------------------------------";
			while (pInputValues->first)
				os += CString(pInputValues->first) + ": " + pInputValues->second + "\n";
		}
		CString os;
	};
	Callback callback;

	GetISystem()->GetIAnimationGraphSystem()->FindDeadInputValues( &callback, pGraph->ToXml() );

	return callback.os;
}
