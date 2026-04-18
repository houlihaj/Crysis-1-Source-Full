// AIPIDController.cpp: implementation of the CAIPIDController class.
//
//////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "AIPIDController.h"
void CAIPIDController::Serialize(TSerialize ser, class CObjectTracker& objectTracker)
{
	ser.BeginGroup( "AIPIDController" );
	ser.Value("CP",CP);
	ser.Value("CI",CI);
	ser.Value("CD",CD);
	ser.Value("runningIntegral",runningIntegral);
	ser.Value("lastError",lastError);
	ser.Value("proportionalPower",proportionalPower);
	ser.Value("integralTimescale",integralTimescale);
	ser.EndGroup();
}