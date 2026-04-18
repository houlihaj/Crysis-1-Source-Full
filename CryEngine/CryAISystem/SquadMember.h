/********************************************************************
  CryGame Source File.
  Copyright (C), Crytek Studios, 2001-2004.
 -------------------------------------------------------------------------
  File name:   SquadMember.h
  Version:     v1.00
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - 8:7:2004   12:11 : Created by Kirill Bulatsev

*********************************************************************/


#ifndef __SquadMember_H__
#define __SquadMember_H__

#if _MSC_VER > 1000
#pragma once
#endif

class CPuppet;
class COPPathFind;

class CSquadMember
{
public:
	CSquadMember(CPuppet* user);
	~CSquadMember(void);

	void Update(CPuppet* user);

protected:
	COPPathFind		*m_pPathfindDirective;

	Vec3			m_vLastMoveDir;
};


#endif __SquadMember_H__
