//Start [ATI / jisidoro]
////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
// -------------------------------------------------------------------------
//  File name:   particleemittergpu.h
//  Version:     v1.00
//  Created:     [ATI / jisidoro]
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//    derived particle emitter class for GPU based particle physics
//   
//
////////////////////////////////////////////////////////////////////////////

#ifndef EXCLUDE_GPU_PARTICLE_PHYSICS

#ifndef __particleemittergpu_h__
#define __particleemittergpu_h__
#pragma once

#include "ParticleEmitter.h"

//
// Implement GPU versions of subemitter and container here.
// Handles a single particle effect.
// 

//////////////////////////////////////////////////////////////////////////
class CParticleSubEmitterGPU : public CParticleSubEmitter
{
};

class CParticleContainerGPU : public CParticleContainer
{
};


#endif // __particleemittergpu_h__


#endif