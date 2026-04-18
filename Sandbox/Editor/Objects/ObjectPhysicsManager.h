////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2007.
// -------------------------------------------------------------------------
//  File name:   ObjectPhysicsManager.h
//  Version:     v1.00
//  Created:     28/02/2007 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ObjectPhysicsManager_h__
#define __ObjectPhysicsManager_h__

#if _MSC_VER > 1000
#pragma once
#endif

//////////////////////////////////////////////////////////////////////////
class CObjectPhysicsManager
{
public:
	CObjectPhysicsManager();
	~CObjectPhysicsManager();

	void SimulateSelectedObjectsPositions();
	void Update();

private:
	void Command_SimulateObjects();
	void UpdateSimulatingObjects();

	bool m_bSimulatingObjects;
	float m_fStartObjectSimulationTime;
	int m_wasSimObjects;
	std::vector<_smart_ptr<CBaseObject> > m_simObjects;
	CWaitProgress *m_pProgress;
};


#endif // __ObjectPhysicsManager_h__