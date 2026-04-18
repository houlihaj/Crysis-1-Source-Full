/********************************************************************
  CryGame Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   AIFaceManager.cpp
  Version:     v1.00
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 05:05:2007   12:05 : Created by Kirill Bulatsev

*********************************************************************/
#include "StdAfx.h"
#include "AIFaceManager.h"
#include <ICryAnimation.h>
#include <ISerialize.h>
#include <IActorSystem.h>
#include <IAnimationGraph.h>


CAIFaceManager::TExprStateMap CAIFaceManager::s_Expressions;
//
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
CAIFaceManager::CAIFaceManager(IEntity* pEntity):
m_pEntity(pEntity),
m_CurrentState(EE_IDLE),
m_CurrentExprIdx(0),
m_ExprStartTime(0.f),
m_ChangeExpressionTime(0)
{

}


//
//------------------------------------------------------------------------------
CAIFaceManager::~CAIFaceManager(void)
{

}

//
//------------------------------------------------------------------------------
float	CAIFaceManager::SelectExpressionTime() const
{
	return 30.f + Random(20.f);
}

//
//------------------------------------------------------------------------------
void	CAIFaceManager::Reset()
{
	m_CurrentState = EE_IDLE;
	m_CurrentExprIdx = 0;
	m_ExprStartTime =  0.f;
}

//
//------------------------------------------------------------------------------
void CAIFaceManager::PrecacheSequences()
{
	IActorSystem* pASystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	IActor* pActor = (pASystem && m_pEntity ? pASystem->GetActor(m_pEntity->GetId()) : 0);

	for (e_ExpressionEvent eventType = e_ExpressionEvent(0); eventType <= EE_Count; eventType = e_ExpressionEvent(eventType + 1))
	{
		const TExprState& stateSequences = s_Expressions[eventType];
		for (int stateSequenceIndex = 0, stateSequenceCount = stateSequences.size(); stateSequenceIndex < stateSequenceCount; ++stateSequenceIndex)
		{
			const string& expressionName = stateSequences[stateSequenceIndex];

			if (pActor)
				pActor->PrecacheFacialExpression(expressionName.c_str());
		}
	}
}

//
//------------------------------------------------------------------------------
bool	CAIFaceManager::LoadStatic()
{

	s_Expressions.clear();
	string sPath = PathUtil::Make("Libs/Readability/Faces", "Faces.xml" );
	return Load(sPath);
/*
	m_SoundPacks.clear();
	//	Load("korean_01.xml");

	string path( "Libs/Readability/Sound" );
	ICryPak * pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	string fileName;

	string searchPath(path + "/*.xml");
	intptr_t handle = pCryPak->FindFirst( searchPath.c_str(), &fd );
	if (handle != -1)
	{
		do
		{
			fileName = path;
			fileName += "/" ;
			fileName += fd.name;
			Load(fileName);
		} while ( pCryPak->FindNext( handle, &fd ) >= 0 );
		pCryPak->FindClose( handle );
	}
	m_reloadID++;
*/
}

//
//----------------------------------------------------------------------------------------------------------
bool CAIFaceManager::Load(const char* szPackName)
{
	string sPath = PathUtil::Make("Libs/Readability/Faces", "Faces.xml" );

	XmlNodeRef root = GetISystem()->LoadXmlFile(sPath);
	if (!root)
		return false;

	XmlNodeRef nodeWorksheet = root->findChild("Worksheet");
	if (!nodeWorksheet)
		return false;

	XmlNodeRef nodeTable = nodeWorksheet->findChild("Table");
	if (!nodeTable)
		return false;

	e_ExpressionEvent	curExpression(EE_NONE);
	const char* szSignal = 0;
	for (int rowCntr = 0, childN = 0; childN < nodeTable->getChildCount(); ++childN)
	{
		XmlNodeRef nodeRow = nodeTable->getChild(childN);
		if (!nodeRow->isTag("Row"))
			continue;
		++rowCntr;
		if (rowCntr == 1) // Skip the first row, it only have description/header.
			continue;
		const char* szFaceName = 0;

		for (int childrenCntr = 0,cellIndex = 1; childrenCntr < nodeRow->getChildCount(); ++childrenCntr,++cellIndex)
		{
			XmlNodeRef nodeCell = nodeRow->getChild(childrenCntr);
			if (!nodeCell->isTag("Cell"))
				continue;

			if (nodeCell->haveAttr("ss:Index"))
			{
				const char *pStrIdx=nodeCell->getAttr("ss:Index");
				sscanf(pStrIdx, "%d", &cellIndex);
			}
			XmlNodeRef nodeCellData = nodeCell->findChild("Data");
			if (!nodeCellData)
				continue;

			switch (cellIndex)
			{
			case 1:		// Readability Signal name
				szSignal = nodeCellData->getContent();
				curExpression = StringToEnum(szSignal);
				continue;
			case 2:		// The expression name
				szFaceName = nodeCellData->getContent();
				break;
			}
		}
		if( szFaceName != NULL )
			s_Expressions[curExpression].push_back(szFaceName);
	}
	return true;
}


//
//------------------------------------------------------------------------------
CAIFaceManager::e_ExpressionEvent CAIFaceManager::StringToEnum(const char* szFaceName)
{
	if(!strcmp( szFaceName, "IDLE"))
		return EE_IDLE;
	else if(!strcmp( szFaceName, "ALERT"))
		return EE_ALERT;
	else if(!strcmp( szFaceName, "COMBAT"))
		return EE_COMBAT;

	return EE_NONE;
}

//
//------------------------------------------------------------------------------
void	CAIFaceManager::Update()
{
	if(m_CurrentState == EE_NONE)
	{
		MakeFace(NULL);
		return;
	}
	float timePassed((gEnv->pAISystem->GetFrameStartTime()-m_ExprStartTime).GetSeconds());
	if(timePassed>m_ChangeExpressionTime)
		SetExpression(m_CurrentState, true);
}

//
//------------------------------------------------------------------------------
void	CAIFaceManager::SetExpression(e_ExpressionEvent expression, bool forceChange)
{
	if(m_CurrentState == expression && !forceChange)
		return;
	TExprStateMap::iterator iState = s_Expressions.find(expression);

	if(iState == s_Expressions.end())
		return;
	TExprState	&theState(iState->second); 
	if (!theState.empty())
	{
		m_CurrentExprIdx = Random(static_cast<uint32>(theState.size()));
		MakeFace(theState[m_CurrentExprIdx]);
	}
	m_ChangeExpressionTime = SelectExpressionTime();
	m_ExprStartTime = gEnv->pAISystem->GetFrameStartTime();
	m_CurrentState = expression;
}



//
//------------------------------------------------------------------------------
void CAIFaceManager::MakeFace(const char* pFaceName)
{
	IActorSystem* pASystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	if ( pASystem )
	{
		IActor* pActor = pASystem->GetActor( m_pEntity->GetId() );
		if ( pActor )
			pActor->RequestFacialExpression(pFaceName);
	}
}




//----------------------------------------------------------------------------------------------------------
//
//
//----------------------------------------------------------------------------------------------------------