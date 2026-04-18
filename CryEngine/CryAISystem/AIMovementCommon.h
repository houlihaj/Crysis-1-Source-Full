/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Common functionality for AI movement processing

-------------------------------------------------------------------------
History:
- 06:07:2005: Created by MichaelR

*************************************************************************/

#ifndef __AIMOVEMENTCOMMON_H__
#define __AIMOVEMENTCOMMON_H__

#if _MSC_VER > 1000
#pragma once
#endif

struct SPID
{
  SPID();
  void	Reset();
  float	Update( float inputVal, float setPoint, float clampMin, float clampMax );
  float	m_kP;
  float	m_kD;
  float	m_kI;
  float	m_prevErr;
  float	m_intErr;
};

#endif