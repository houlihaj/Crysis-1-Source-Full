#ifndef INC_NXN_VERSION_H
#define INC_NXN_VERSION_H

/*! \file		NxNVersion.h
 *
 *  \brief		Contains all SDK version related stuff.
 *
 *  \author		Thomas Gawlik
 *
 *  \version	1.00
 *
 *  \date		2003
 *
 */
//---------------------------------------------------------------------------
//	current sdk version defines (DO NOT ALTER THESE CONSTANTS!)
//---------------------------------------------------------------------------
#define VERSION_MAJOR				(1)
#define VERSION_MINOR				(2)
#define VERSION_BUILDNUMBER			(8)

#define NXN_XDK_MAKEVERSION(major, minor, build)    ((long) ((major << 16) | (minor << 8) | build))
#define NXN_XDK_VERSION                             NXN_XDK_MAKEVERSION(VERSION_MAJOR, VERSION_MINOR, VERSION_BUILDNUMBER)

#if _MSC_VER == 1200  // Visual Studio 6.0

#   define NXN_XDK_DLL_NAME        "NxN_alienbrain_XDK_VS60_128.dll"
#   define NXN_XDK_LIB_NAME        "NxN_alienbrain_XDK_VS60_128.lib"
#   define NXN_XDK_DLL_NAME_WIDE	L"NxN_alienbrain_XDK_VS60_128.dll"

#else

#   define NXN_XDK_DLL_NAME        "NxN_alienbrain_XDK_128.dll"
#   define NXN_XDK_LIB_NAME        "NxN_alienbrain_XDK_128.lib"
#   define NXN_XDK_DLL_NAME_WIDE	L"NxN_alienbrain_XDK_128.dll"

#endif // VS60

#endif // INC_NXN_VERSION_H
