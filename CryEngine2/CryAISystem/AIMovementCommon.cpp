
#include "StdAfx.h"
#include "AIMovementCommon.h"

//--------------------------------------------------------------------------------------------
SPID::SPID() :
m_kP( 0 ),
m_kD( 0 ),
m_kI( 0 ),
m_prevErr( 0 ),
m_intErr( 0 )
{
  // empty
}

//--------------------------------------------------------------------------------------------
void SPID::Reset()
{
  m_prevErr = 0;
  m_intErr = 0;
}

//--------------------------------------------------------------------------------------------
float SPID::Update( float inputVal, float setPoint, float clampMin, float clampMax )
{
  float	pError = setPoint - inputVal;
  float	output = m_kP * pError - m_kD * (pError - m_prevErr) + m_kI * m_intErr;
  m_prevErr = pError;

  // Accumulate integral, or clamp.
  if( output > clampMax )
    output = clampMax;
  else if( output < clampMin )
    output = clampMin;
  else
    m_intErr += pError;

  return output;
}
