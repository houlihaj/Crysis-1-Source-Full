////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2007.
// -------------------------------------------------------------------------
//  File name:   ObjectPhysicsManager.cpp
//  Version:     v1.00
//  Created:     28/02/2007 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ObjectPhysicsManager.h"
#include "ObjectManager.h"
#include "BaseObject.h"
#include "GameEngine.h"

#define MAX_OBJECTS_PHYS_SIMULATION_TIME (5)

//////////////////////////////////////////////////////////////////////////
CObjectPhysicsManager::CObjectPhysicsManager()
{
	GetIEditor()->GetCommandManager()->RegisterCommand( "Physics.SimulateObjects",functor(*this,&CObjectPhysicsManager::Command_SimulateObjects) );
	
	m_fStartObjectSimulationTime = 0;
	m_pProgress = 0;
	m_bSimulatingObjects = false;
	m_wasSimObjects = 0;
}

//////////////////////////////////////////////////////////////////////////
CObjectPhysicsManager::~CObjectPhysicsManager()
{
	SAFE_DELETE(m_pProgress);
}

//////////////////////////////////////////////////////////////////////////
void CObjectPhysicsManager::Command_SimulateObjects()
{
	SimulateSelectedObjectsPositions();
}

//////////////////////////////////////////////////////////////////////////
void CObjectPhysicsManager::Update()
{
	if (m_bSimulatingObjects)
		UpdateSimulatingObjects();

}

//////////////////////////////////////////////////////////////////////////
void CObjectPhysicsManager::SimulateSelectedObjectsPositions()
{
	CSelectionGroup *pSel = GetIEditor()->GetObjectManager()->GetSelection();
	if (pSel->IsEmpty())
		return;

	if (GetIEditor()->GetGameEngine()->GetSimulationMode())
		return;

	m_pProgress = new CWaitProgress("Simulating Objects");

	GetIEditor()->GetGameEngine()->SetSimulationMode(true,true);

	m_simObjects.clear();
	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CBaseObject *pObject = pSel->GetObject(i);
		IPhysicalEntity *pPhysEntity = pObject->GetCollisionEntity();
		if (!pPhysEntity)
			continue;
		
		pe_action_awake aa;
		aa.bAwake = 1;
		pPhysEntity->Action(&aa);

		m_simObjects.push_back(pSel->GetObject(i));
	}
	m_wasSimObjects = m_simObjects.size();

	m_fStartObjectSimulationTime = GetISystem()->GetITimer()->GetAsyncCurTime();
	m_bSimulatingObjects = true;
}

//////////////////////////////////////////////////////////////////////////
void CObjectPhysicsManager::UpdateSimulatingObjects()
{
	{
		CUndo undo("Simulate");
		for (int i = 0; i < m_simObjects.size(); )
		{
			CBaseObject *pObject = m_simObjects[i];
			IPhysicalEntity *pPhysEntity = pObject->GetCollisionEntity();
			if (pPhysEntity)
			{
				pe_status_awake awake_status;
				if (!pPhysEntity->GetStatus(&awake_status))
				{
					// When object went sleeping, record his position.
					pObject->OnEvent( EVENT_PHYSICS_GETSTATE );

					m_simObjects.erase(m_simObjects.begin()+i);
					continue;
				}
			}
			i++;
		}
	}

	float curTime = GetISystem()->GetITimer()->GetAsyncCurTime();
	float runningTime = (curTime - m_fStartObjectSimulationTime);

	m_pProgress->Step( 100*runningTime/MAX_OBJECTS_PHYS_SIMULATION_TIME );

	if (m_simObjects.empty() || (runningTime > MAX_OBJECTS_PHYS_SIMULATION_TIME))
	{
		m_fStartObjectSimulationTime = 0;
		m_bSimulatingObjects = false;
		SAFE_DELETE(m_pProgress);
		GetIEditor()->GetGameEngine()->SetSimulationMode(false,true);
	}
}
