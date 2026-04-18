//////////////////////////////////////////////////////////////////////////////
// CryEngine specific Scaleform configuration header 
//////////////////////////////////////////////////////////////////////////////

#ifndef _CONFIG_SCALEFORM_H_
#define _CONFIG_SCALEFORM_H_

#pragma once


#ifndef EXCLUDE_SCALEFORM_SDK

#if defined(WIN32)
#	if defined(WIN64)
#		ifdef _DEBUG
#			pragma message (">>> include lib: GFx_win64_debug.lib")
#			pragma comment ( lib, "GFx_win64_debug.lib" )
#			pragma message (">>> include lib: libjpeg_win64_debug.lib")
#			pragma comment ( lib, "libjpeg_win64_debug.lib" )
#		else
#			pragma message (">>> include lib: GFx_win64_release.lib")
#			pragma comment ( lib, "GFx_win64_release.lib" )
#			pragma message (">>> include lib: libjpeg_win64_release.lib")
#			pragma comment ( lib, "libjpeg_win64_release.lib" )
#		endif
#	else
#		ifdef _DEBUG
#			pragma message (">>> include lib: GFx_win32_debug.lib")
#			pragma comment ( lib, "GFx_win32_debug.lib" )
#			pragma message (">>> include lib: libjpeg_win32_debug.lib")
#			pragma comment ( lib, "libjpeg_win32_debug.lib" )
#		else
#			pragma message (">>> include lib: GFx_win32_release.lib")
#			pragma comment ( lib, "GFx_win32_release.lib" )
#			pragma message (">>> include lib: libjpeg_win32_release.lib")
#			pragma comment ( lib, "libjpeg_win32_release.lib" )
#		endif
#	endif
#elif defined(XENON)
#	ifdef _DEBUG
#		pragma message (">>> include lib: GFx_xbox360_debug.lib")
#		pragma comment ( lib, "GFx_xbox360_debug.lib" )
#		pragma message (">>> include lib: libjpeg_xbox360_debug.lib")
#		pragma comment ( lib, "libjpeg_xbox360_debug.lib" )
#	else
#		pragma message (">>> include lib: GFx_xbox360_release.lib")
#		pragma comment ( lib, "GFx_xbox360_release.lib" )
#		pragma message (">>> include lib: libjpeg_xbox360_release.lib")
#		pragma comment ( lib, "libjpeg_xbox360_release.lib" )
#	endif
#endif

#endif // #ifndef EXCLUDE_SCALEFORM_SDK


#endif // #ifndef _CONFIG_SCALEFORM_H_
