#include "stdafx.h"
#include "ISystem.h"
#include "AnimationGraphUnitTester.h"
#include "IAnimationGraphSystem.h"


#define XML_TAG_UNITTESTS		"UnitTests"
#define XML_TAG_TEST				"Test"
#define XML_TAG_INPUTS			"Inputs"
#define XML_TAG_OUTPUTS			"Outputs"
#define XML_TAG_INPUT				"Input"
#define XML_TAG_EXCLUDED		"ExcludedState"
#define XML_TAG_INCLUDED		"IncludedState"
#define XML_TAG_USEPRESELECTION	"UsePreselection"

#define XML_ATTR_UNITTESTS_FILENAME		"filename"
#define XML_ATTR_TEST_ID							"id"
#define XML_ATTR_TEST_DESCRIPTION			"description"
#define XML_ATTR_INPUT_ID							"id"
#define XML_ATTR_INPUT_VALUE					"value"
#define XML_ATTR_OUTPUTS_ALLOWED			"onlyListedStatesAllowed"
#define XML_ATTR_INCLUDED_VALUE				"value"
#define XML_ATTR_INCLUDED_TOPRANK			"mustBeInTopRanking"
#define XML_ATTR_EXCLUDED_VALUE				"value"
#define XML_ATTR_PRESELECTION_NAME			"name"

#define XML_TAG_PRESELECTIONLISTS	"PreselectionLists"
#define XML_TAG_PRESELECTION		"Preselection"
#define XML_TAG_SELECTION			"Selection"
#define XML_ATTR_NAME				"Name"
#define XML_ATTR_TYPE				"Type"
#define XML_ATTR_INPUT				"Input"
#define XML_ATTR_VALUE				"Value"



bool CAnimationGraphUnitTester::Load( CAnimationGraphPtr pGraph, const CString &sGraphFileName, const CString &sGraphWorkingName, CString *sError )
{
	bool bRet = false;

	// Build unit test graph name
	CString sFileName( "Animations/graphs/" + sGraphFileName );
	int pos = sFileName.Find( ".xml" );
	if( pos != string::npos )
	{
		sFileName.Insert( pos, "_unittests" );

		XmlNodeRef pNode = GetISystem()->LoadXmlFile( sFileName );
		if ( pNode != NULL && pGraph != NULL)
		{
			m_unitTests = pNode;
			m_pGraph = pGraph;
			m_sGraphFileName = sGraphFileName;
			m_sGraphWorkingName = sGraphWorkingName;
			bRet = true;
		}
	}

	if ( !bRet && sError != NULL )
	{
		sError->Format( "Unable to load unit test file\n%s\n(error in file or file doesn't exist)", sFileName );
	}
	
	return bRet;
}

bool CAnimationGraphUnitTester::RunAllTests( std::vector<CString> *sErrors, int *pNumTestsRun, int *pNumTestsPassed )
{
	bool bAbleToLoadTests = false;
	int iNumRun = 0;
	int iNumPassed = 0;
	int iNumChildren = 0;
	XmlNodeRef tests;

	if ( m_unitTests )
	{
		tests = m_unitTests->isTag( XML_TAG_UNITTESTS ) ? m_unitTests : m_unitTests->findChild( XML_TAG_UNITTESTS );
		if ( tests )
		{
			iNumChildren = tests->getChildCount();		
		}
	}

	if ( iNumChildren > 0 )
	{
		bAbleToLoadTests = true;

		// Run through all tests
		for ( int i=0; i<iNumChildren; ++i )
		{
			XmlNodeRef currentChild = tests->getChild(i);
			if ( currentChild->isTag( XML_TAG_TEST ) )
			{
				if ( RunTest( currentChild, sErrors ) )
				{
					++iNumPassed;
				}

				++iNumRun;
			}				
		}  
	}

	if ( !bAbleToLoadTests && sErrors != NULL )
	{
		sErrors->push_back( "No unit tests loaded or no tests in unit test file" );
	}

	*pNumTestsRun = pNumTestsRun ? iNumRun : NULL;
	*pNumTestsPassed = pNumTestsPassed ? iNumPassed : NULL;

	return ( bAbleToLoadTests && iNumPassed == iNumRun );
}


bool CAnimationGraphUnitTester::RunTest( const CString &sTestName, std::vector<CString> *sErrors )
{
	bool bAbleToLoadTest = false;
	bool bRet = false;
	int iNumChildren = 0;
	XmlNodeRef tests;

	if ( m_unitTests )
	{
		tests = m_unitTests->isTag( XML_TAG_UNITTESTS ) ? m_unitTests : m_unitTests->findChild( XML_TAG_UNITTESTS );
		if ( tests )
		{
			iNumChildren = tests->getChildCount();			
		}
	}

	if ( iNumChildren > 0 )
	{
		// Find the test with the given name
		for ( int i=0; i<iNumChildren; ++i )
		{
			XmlNodeRef currentChild = tests->getChild(i);
			if ( currentChild->isTag( XML_TAG_TEST ) )
			{
				const char *sName = currentChild->getAttr( XML_ATTR_TEST_ID );
				if ( sName[0] && sTestName.Compare( sName )==0 )
				{
					bAbleToLoadTest = true;
					bRet = RunTest( currentChild, sErrors );
				}	
			}				
		}  
	}

	if ( !bAbleToLoadTest && sErrors != NULL )
	{
		sErrors->push_back( "[" + sTestName + "] unable to find test" );
	}

	return bRet;
}

bool CAnimationGraphUnitTester::RunTest( XmlNodeRef testXmlNode, std::vector<CString> *sErrors )
{
	CString sMessage;

	XmlNodeRef preselectionXmlNode = testXmlNode->findChild( XML_TAG_USEPRESELECTION );
	XmlNodeRef inputs = testXmlNode->findChild( XML_TAG_INPUTS );
	XmlNodeRef outputs = testXmlNode->findChild( XML_TAG_OUTPUTS );

	if ( !inputs || !outputs )
	{
		if ( sErrors != NULL )
		{
			sMessage.Format( "[%s] Test is missing inputs and/or outputs to test against", testXmlNode->getAttr( XML_ATTR_TEST_ID) );
			sErrors->push_back( sMessage );
		}

		return false;
	}

	// Set inputs for test query
	SAnimationGraphQueryInputs queryInputs;
	int preselectionInputs = 0;

	// first set the values from the preselection
	// because the Unit Test Inputs override the preselection ones later
	if (preselectionXmlNode)
	{
		// If the file can't be opened or the list cannot be found
		// some kind of error or warning would be nice - 
		// although this is not fatal. 
		// It will just most likely result in a failing unit test.

		for (int i = 0; i < 1; ++ i)
		{
			// retrieve the name of the preselection list
			CString preselectionName;
			preselectionName.SetString( preselectionXmlNode->getAttr( XML_ATTR_PRESELECTION_NAME ) );

			// open the preselection file for this graph
			// get the file name
			CString sFileName( "Animations/graphs/" + m_sGraphFileName );
			int pos = sFileName.Find( ".xml" );
			if( pos != string::npos )
				sFileName.Insert( pos, "_preselections" );
			else
			{
				Warning("Cannot create PreselectionLists Filename from %s", m_sGraphFileName);
				break;
			}

			// check if file exists and get the top node
			XmlNodeRef pNode = GetISystem()->LoadXmlFile( sFileName );
			if ( pNode == NULL)
			{
				Warning("Cannot load PreselectionList %s", sFileName);
				break;
			}

			// check whether the wanted list exists inside the file
			// go through all childs and read the names
			if ( pNode != NULL)
			{
				XmlNodeRef preselections = pNode;	
				preselections = preselections->isTag( XML_TAG_PRESELECTIONLISTS ) ? preselections : preselections->findChild( XML_TAG_PRESELECTIONLISTS );

				// load in the list names
				int listCount = preselections->getChildCount();
				XmlNodeRef currentChild = NULL;
				bool found = false;
				for (int i = 0; i < listCount; ++i)
				{
					currentChild = preselections->getChild(i);
					if ( currentChild->isTag( XML_TAG_PRESELECTION ) )
					{
						// this child is a preselection list so get it's name
						const char *sName = currentChild->getAttr(XML_ATTR_NAME);
						if (preselectionName.Compare( sName ) == 0)
						{
							found = true;
							break;
						}
					}
				}

				if (!found) // there is no list with that name in the file
				{
					Warning("Cannot find the preselection '%s' in file '%s'", preselectionName, sFileName);
					break;
				}

				// retrieve all the inputs from this preselection list
				listCount = preselections->getChildCount();
				XmlNodeRef currentSelection = NULL;
				for (int i = 0; i < listCount; ++i)
				{
					currentSelection = currentChild->getChild(i);

					// Check if this child even is a selection
					if (!currentSelection->isTag( XML_TAG_SELECTION ))
						continue;

					// get input name
					CAGInputPtr pInput = m_pGraph->FindInput( currentSelection->getAttr( XML_ATTR_INPUT ) );
					if ( pInput )
					{
						SAnimationGraphQueryInputs::SInput sInput = SAnimationGraphQueryInputs::SInput( pInput->GetName() );

						EAnimationGraphInputType eType = pInput->GetType();
						if ( eType == eAGIT_Integer )
						{
							sInput.eType = SAnimationGraphQueryInputs::eAGQIT_Integer;
							currentSelection->getAttr( XML_ATTR_VALUE, sInput.iValue );
						}
						else if ( eType == eAGIT_Float )
						{
							sInput.eType = SAnimationGraphQueryInputs::eAGQIT_Float;
							currentSelection->getAttr( XML_ATTR_VALUE, sInput.fValue );
						}
						else if ( eType == eAGIT_String )
						{
							sInput.eType = SAnimationGraphQueryInputs::eAGQIT_String;
							strcpy_s( sInput.sValue, currentSelection->getAttr( XML_ATTR_VALUE ) );
						}

						queryInputs.AddInput( sInput );
						++preselectionInputs;
					}  // end if ( pInput != NULL )   // if the name is actually a valid input
					else
					{
						Warning("Illegal Input name '%s' in preselection list '%s'", currentSelection->getAttr( XML_ATTR_INPUT ), preselectionName);
					}
				}  // end of loop over all inputs in this preselection list
			} // if pNode != NULL  // XML file was loaded
		}  // end of auxiliary loop
	}  // Use Preselection List

	// Set the Inputs from the Unit Test File now
	int iNumChildren = inputs->getChildCount();
	for ( int i=0; i<iNumChildren; ++i )
	{
		XmlNodeRef currentChild = inputs->getChild(i);
		if ( currentChild->isTag( XML_TAG_INPUT ) )
		{
			CAGInputPtr pInput = m_pGraph->FindInput( currentChild->getAttr( XML_ATTR_INPUT_ID ) );
			if ( pInput )
			{
				SAnimationGraphQueryInputs::SInput sInput = SAnimationGraphQueryInputs::SInput( pInput->GetName() );

				EAnimationGraphInputType eType = pInput->GetType();
				if ( eType == eAGIT_Integer )
				{
					sInput.eType = SAnimationGraphQueryInputs::eAGQIT_Integer;
					currentChild->getAttr( XML_ATTR_INPUT_VALUE, sInput.iValue );
				}
				else if ( eType == eAGIT_Float )
				{
					sInput.eType = SAnimationGraphQueryInputs::eAGQIT_Float;
					currentChild->getAttr( XML_ATTR_INPUT_VALUE, sInput.fValue );
				}
				else if ( eType == eAGIT_String )
				{
					sInput.eType = SAnimationGraphQueryInputs::eAGQIT_String;
					strcpy_s( sInput.sValue, currentChild->getAttr( XML_ATTR_INPUT_VALUE ) );
				}

				queryInputs.AddInput( sInput );
			}
			else if ( sErrors != NULL )
			{
				sMessage.Format( "[%s] Invalid input: %s", testXmlNode->getAttr( XML_ATTR_TEST_ID ), currentChild->getAttr( XML_ATTR_INPUT_ID ) );
				sErrors->push_back( sMessage );
			}
		}	
	}

	if ( (iNumChildren + preselectionInputs) != queryInputs.GetNumInputs() )
	{
		if ( sErrors != NULL )
		{
			sMessage.Format( "[%s] Test contains invalid data in <Inputs> section", testXmlNode->getAttr( XML_ATTR_TEST_ID ) );
			sErrors->push_back( sMessage );
		}

		return false;
	}


	// Get output requirements

	bool bOnlyListedStatesAllowed = false;
	std::vector<XmlNodeRef> vIncluded;
	std::vector<XmlNodeRef> vExcluded;

	if ( 0 ==_stricmp( "true", outputs->getAttr( XML_ATTR_OUTPUTS_ALLOWED ) ) )
	{
		bOnlyListedStatesAllowed = true;
	}

	iNumChildren = outputs->getChildCount();
	for ( int i=0; i<iNumChildren; ++i )
	{
		XmlNodeRef currentChild = outputs->getChild(i);
		if ( currentChild->isTag( XML_TAG_INCLUDED ) )
		{
			vIncluded.push_back( currentChild );
		}
		else if ( currentChild->isTag( XML_TAG_EXCLUDED ) )
		{
			vExcluded.push_back( currentChild );
		}
	}

	if ( iNumChildren != ( vIncluded.size() + vExcluded.size() ) )
	{
		if ( sErrors != NULL )
		{
			sMessage.Format( "[%s] Test contains invalid data in <Outputs> section", testXmlNode->getAttr( XML_ATTR_TEST_ID ) );
			sErrors->push_back( sMessage );
		}

		return false;
	}

	// Do query

	IAnimationGraphQueryResultsPtr pResults = GetISystem()->GetIAnimationGraphSystem()->QueryAnimationGraph( m_sGraphWorkingName, &queryInputs );

	// Test query results against output requirements
	
	bool bSuccess = true;
	int iNumIncludedMatches = 0;

	// Test states that must be included in results
	for ( int i=0; i<vIncluded.size(); ++i )
	{
		const char *sName = vIncluded[i]->getAttr( XML_ATTR_INCLUDED_VALUE );
		if ( !pResults->Contains( sName ) )
		{
			bSuccess = false;
			sMessage.Format( "[%s] State %s was not included, must be present", testXmlNode->getAttr( XML_ATTR_TEST_ID ), sName );
			sErrors->push_back( sMessage );
		}
		else
		{
			// Check if included value must also be in the top rank of results
			if ( 0 ==_stricmp( "true", outputs->getAttr( XML_ATTR_INCLUDED_TOPRANK ) ) )
			{
				int iTopRank = pResults->GetHighestRankingValue();
				int iThisRank = pResults->GetResult( sName )->GetRanking();
				if ( iTopRank != iThisRank )
				{
					bSuccess = false;
					sMessage.Format( "[%s] State %s had ranking [%d], must have top ranking [%d]", testXmlNode->getAttr( XML_ATTR_TEST_ID ), sName, iThisRank, iTopRank );
					sErrors->push_back( sMessage );
				}
			}
			
			// We increment this value whether or not we failed the top rank test because we use
			// this value just to store how many of the results have been matched to an included value
			++iNumIncludedMatches;
		}
	}

	// Test states that must be excluded from results
	for ( int i=0; i<vExcluded.size(); ++i )
	{
		const char *sName = vExcluded[i]->getAttr( XML_ATTR_EXCLUDED_VALUE );
		if ( pResults->Contains( sName ) )
		{
			bSuccess = false;
			sMessage.Format( "[%s] State %s was included, must not be present", testXmlNode->getAttr( XML_ATTR_TEST_ID ), sName );
			sErrors->push_back( sMessage );
		}
	}

	int iNumExtra = pResults->GetNumResults() - iNumIncludedMatches;
	if ( bOnlyListedStatesAllowed && ( iNumExtra > 0 ) )
	{
		bSuccess = false;
		sMessage.Format( "[%s] Required only specified states, got %d extra", testXmlNode->getAttr( XML_ATTR_TEST_ID ), iNumExtra );
		sErrors->push_back( sMessage );
	}

	return bSuccess;
}

