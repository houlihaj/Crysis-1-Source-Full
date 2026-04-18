/*=============================================================================
AreaLight.cpp : 
Copyright 2005 Crytek Studios. All Rights Reserved.

Revision history:
* Created by Tamas Schlagl
=============================================================================*/
#include "stdafx.h"
#include "AreaLight.h"


f32	 CAreaLight::m_fOnePixel = 0;
CGeomCore*	CAreaLight::pGeomCore = NULL;
bool	CAreaLight::m_bWhite = true;
