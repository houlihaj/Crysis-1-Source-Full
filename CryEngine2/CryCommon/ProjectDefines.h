////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   ProjectDefines.h
//  Version:     v1.00
//  Created:     3/30/2004 by MartinM.
//  Compilers:   Visual Studio.NET
//  Description: to get some defines available in every CryEngine project 
// -------------------------------------------------------------------------
//  History:
//    July 20th 2004 - Mathieu Pinard
//    Updated the structure to handle more easily different configurations
//
////////////////////////////////////////////////////////////////////////////

#ifndef PROJECTDEFINES_H
#define PROJECTDEFINES_H

// Needed so we use our own terminate function in platform_impl.h
#define _STLP_DEBUG_TERMINATE

//VS2005
#if _MSC_VER >= 1400
//#pragma warning( default  : 4996 )  // warning C4996: 'stricmp' was declared deprecated
#endif


// Turns on/off stlport debug mode - needs to be done everywhere (i.e. can't just
// do it per module cos this makes stl behave oddly) and everything needs to be build in debug.
// you also need to add the following to the top of stlport/stl/debug/_debug.c
/*
#include <windows.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
*/
//#define _STLP_DEBUG

#if !defined GAME_IS_TECHDEMO
#	if defined(LINUX)
#		define EXCLUDE_SCALEFORM_SDK
#		define EXCLUDE_CRI_SDK
#		define EXCLUDE_GPU_PARTICLE_PHYSICS
# elif defined(XENON)
//#		define EXCLUDE_SCALEFORM_SDK
#		define EXCLUDE_CRI_SDK
#		define EXCLUDE_GPU_PARTICLE_PHYSICS
# elif defined(PS3)
//#		define EXCLUDE_SCALEFORM_SDK
#		define EXCLUDE_CRI_SDK
#		define EXCLUDE_GPU_PARTICLE_PHYSICS
# elif defined(WIN32) && defined(WIN64)
//#		define EXCLUDE_SCALEFORM_SDK
//#		define EXCLUDE_CRI_SDK
//#		define EXCLUDE_GPU_PARTICLE_PHYSICS
# else
//#		define EXCLUDE_SCALEFORM_SDK
//#		define EXCLUDE_CRI_SDK
//#		define EXCLUDE_GPU_PARTICLE_PHYSICS
#	endif
#endif

// For the Tech Demo, many third party library are disabled and we also disable DATAPROBE
#if defined GAME_IS_TECHDEMO
#	define EXCLUDE_SCALEFORM_SDK
#	define EXCLUDE_BINK_SDK
#	define EXCLUDE_CRI_SDK
#	define EXCLUDE_GPU_PARTICLE_PHYSICS
#endif

#define _DATAPROBE
#define EXCLUDE_CRI_SDK

// uncomment to use the Intel Laptop Gaming SDK
//#define USE_IntelLaptopGaming

#if defined(_XBOX) || defined(XENON) || defined (PS3) || defined(LINUX)
#undef  USE_IJL
#else
// Optionally, the Intel JPEG library can be used on Win32 to add support for 
// writing JPEG files.
#undef  USE_IJL

#include "ProjectDefinesInclude.h"
// In order to enable again the IJL, the following line should be uncommented
//#define USE_IJL
#endif

// Intel Laptop Gaming SDK
// Optionally used by the GameDLL in LaptopUtil.cpp, only for Win32
#if defined (WIN32) && !defined(XENON) && !defined(WIN64)
//#define INCLUDE_ILG_SDK 1
#endif

// Logitech G15 LCD SDK - optionally used by the GameDLLs
//#define USE_G15_LCD

// PunkBuster
//#define __WITH_PB__ 1

#ifdef CRYTEK_SDK_EVALUATION
#define USING_UNIKEY_SECURITY				// Wrapper for UniKey dongle management and security code
#define USING_TAGES_SECURITY				// Wrapper for TGVM security
#define USING_LICENSE_PROTECTION		// Wrapper for License Server protection
#endif

//#define USE_DBOX_MOTION_CODE				// Wrapper for DBox motion code


#ifdef USING_TAGES_SECURITY
	#define TAGES_EXPORT __declspec(dllexport)
#else 
	#define TAGES_EXPORT
#endif // USING_TAGES_SECURITY


// Define TERRAIN_USE_CIE_COLORSPACE if you want to use the higher quality but slower CIE color space for
// terrain textures; changing this requires regeneration of surface textures and configuring the shaders properly
#define TERRAIN_USE_CIE_COLORSPACE

// Option to disable very high spec mode under DX9
//#define DISABLE_DX9_VERYHIGH_SPEC

#endif // PROJECTDEFINES_H
