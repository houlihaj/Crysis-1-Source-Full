/********************************************************************
CryGame Source File.
Copyright (C), Crytek Studios, 2001-2008.
-------------------------------------------------------------------------
File name:   ModManager.h
Version:     v1.00
Description: reading out mod information

-------------------------------------------------------------------------
History:
- 10:12:2007   12:20 : Created by Jan Müller

*********************************************************************/

#ifndef __MODMANAGER_H__
#define __MODMANAGER_H__

#pragma once

struct SModInfo;

class CModManager
{
public:

	static bool GetModInfo(SModInfo *info, const char *pModPath /* = 0 */);
	static bool GetModInfo(SModInfo &info, const char *pModPath /* = 0 */);

private:
};

#endif