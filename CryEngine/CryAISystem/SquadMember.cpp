/********************************************************************
  CryGame Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   SquadMember.cpp
  Version:     v1.00
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 8:7:2004   12:11 : Created by Kirill Bulatsev

*********************************************************************/

#include "StdAfx.h"
#include "SquadMember.h"
#include "Puppet.h"
#include "GoalOp.h"

//
//------------------------------------------------------------------------------------------------------------------------------
CSquadMember::CSquadMember( CPuppet* user ):
m_vLastMoveDir(0,0,0)
{
	m_pPathfindDirective = new COPPathFind("",user->GetAttentionTarget());
	user->m_nPathDecision = PATHFINDER_STILLFINDING;
}

//
//------------------------------------------------------------------------------------------------------------------------------
CSquadMember::~CSquadMember(void)
{
	if (m_pPathfindDirective)
	{
		delete m_pPathfindDirective;
		m_pPathfindDirective = NULL;
	}
}


//
//------------------------------------------------------------------------------------------------------------------------------
void CSquadMember::Update( CPuppet* user )
{

return;
	if(user->m_State.vMoveDir.len2()<=0.01f)
	{
		user->m_State.vMoveDir = m_vLastMoveDir;
		return;
	}
	m_vLastMoveDir = user->m_State.vMoveDir;

	return;

static int counter = 0;
if(++counter<10)
	return;
counter = 0;

//	user->m_nPathDecision = PATHFINDER_STILLFINDING;
	m_pPathfindDirective->Execute(user);
	
	


}

//
//------------------------------------------------------------------------------------------------------------------------------
