#ifndef __ANIMATIONGRAPHUNITTESTER_H__
#define __ANIMATIONGRAPHUNITTESTER_H__

#pragma once

#include "AnimationGraph.h"

class CAnimationGraphUnitTester
{
public:
	
	bool Load( CAnimationGraphPtr pGraph, const CString &sGraphFileName, const CString &sGraphWorkingName, CString *sError=NULL );
	bool RunAllTests( std::vector<CString> *sErrors=NULL, int *pNumTestsRun=NULL, int *pNumTestsPassed=NULL );
	bool RunTest( const CString &sTestName, std::vector<CString> *sErrors=NULL );
	bool RunTest( XmlNodeRef testXmlNode, std::vector<CString> *sErrors=NULL );

private:
	
	CAnimationGraphPtr	m_pGraph;
	CString							m_sGraphFileName;
	CString							m_sGraphWorkingName;
	XmlNodeRef					m_unitTests;
};

#endif // __ANIMATIONGRAPHUNITTESTER_H__
