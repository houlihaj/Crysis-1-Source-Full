/////////////////////////////////////////////////////////////////////////////
//
// Crytek Source File
// Copyright (C), Crytek Studios, 2001-2007.
//
// Description: Implementation of the CryThread API for PS3.
//
// History:
// Jun 22, 2007: Created by Sascha Demetrio
//
/////////////////////////////////////////////////////////////////////////////

#if !defined __CRYTHREAD_PS3_H__
#define __CRYTHREAD_PS3_H__ 1

// We'll use the POSIX based implementation for now, so this file is just a
// placeholder.  The major problem with the POSIX implementation is that the
// lock type of a mutex is checked at runtime.

#if !defined __SPU__
#include <CryThread_pthreads.h>
#else
#include <CryThread_dummy.h>
#endif

#endif

// vim:ts=2:sw=2

