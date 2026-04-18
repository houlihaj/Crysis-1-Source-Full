#ifndef cryphysics_h
#define cryphysics_h
#pragma once

#ifdef PHYSICS_EXPORTS
	#define CRYPHYSICS_API DLL_EXPORT
#else
	#define CRYPHYSICS_API DLL_IMPORT
#endif

#define vector_class Vec3_tpl


#include <CrySizer.h>



//#include "utils.h"
#include "Cry_Math.h"
#include "primitives.h"
#include "physinterface.h"

extern "C" CRYPHYSICS_API IPhysicalWorld *CreatePhysicalWorld(struct ISystem *pLog);

#endif