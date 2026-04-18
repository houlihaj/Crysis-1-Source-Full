/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2004.
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 28:7:2004   11:41 : Created by Marco Koegler
- 30:7:2004   11:02 : Taken-over by Márcio Martins
- xx:x:2006		xx:xx : Taken-over by Jan Müller
- 30:08:2006  21:12 : CraigT Made dedicated server .exe

*************************************************************************/
#include "StdAfx.h"
#include <CryModuleDefs.h>
#include <CryLibrary.h>
#include <IGameStartup.h>
#include <IConsole.h>

#define GAMEDLL_FILENAME "CryGame.dll"

static unsigned key[4] = {1339822019,3471820962,4179589276,4119647811};
static unsigned text[4] = {4114048726,1217549643,1454516917,859556405};
static unsigned hash[4] = {324609294,3710280652,1292597317,513556273};

// src and trg can be the same pointer (in place encryption)
// len must be in bytes and must be multiple of 8 byts (64bits).
// key is 128bit:  int key[4] = {n1,n2,n3,n4};
// void encipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k )
#define TEA_ENCODE( src,trg,len,key ) {\
	register unsigned int *v = (src), *w = (trg), *k = (key), nlen = (len) >> 3; \
	register unsigned int delta=0x9E3779B9,a=k[0],b=k[1],c=k[2],d=k[3]; \
	while (nlen--) {\
	register unsigned int y=v[0],z=v[1],n=32,sum=0; \
	while(n-->0) { sum += delta; y += (z << 4)+a ^ z+sum ^ (z >> 5)+b; z += (y << 4)+c ^ y+sum ^ (y >> 5)+d; } \
	w[0]=y; w[1]=z; v+=2,w+=2; }}

// src and trg can be the same pointer (in place decryption)
// len must be in bytes and must be multiple of 8 byts (64bits).
// key is 128bit: int key[4] = {n1,n2,n3,n4};
// void decipher(unsigned int *const v,unsigned int *const w,const unsigned int *const k)
#define TEA_DECODE( src,trg,len,key ) {\
	register unsigned int *v = (src), *w = (trg), *k = (key), nlen = (len) >> 3; \
	register unsigned int delta=0x9E3779B9,a=k[0],b=k[1],c=k[2],d=k[3]; \
	while (nlen--) { \
	register unsigned int y=v[0],z=v[1],sum=0xC6EF3720,n=32; \
	while(n-->0) { z -= (y << 4)+c ^ y+sum ^ (y >> 5)+d; y -= (z << 4)+a ^ z+sum ^ (z >> 5)+b; sum -= delta; } \
	w[0]=y; w[1]=z; v+=2,w+=2; }}

ILINE unsigned Hash( unsigned a )
{
	a = (a+0x7ed55d16) + (a<<12);
	a = (a^0xc761c23c) ^ (a>>19);
	a = (a+0x165667b1) + (a<<5);
	a = (a+0xd3a2646c) ^ (a<<9);
	a = (a+0xfd7046c5) + (a<<3);
	a = (a^0xb55a4f09) ^ (a>>16);
	return a;
}

// encode size ignore last 3 bits of size in bytes. (encode by 8bytes min)
#define TEA_GETSIZE( len ) ((len) & (~7))

ILINE int RunGame(const char *commandLine)
{
	//restart parameters
	static const char logFileName[] = "Server.log";

	unsigned buf[4];
	TEA_DECODE((unsigned*)text,buf,16,(unsigned*)key);

	// load the game dll
	HMODULE gameDll = CryLoadLibrary(GAMEDLL_FILENAME);

	if (!gameDll)
	{
		MessageBox(0, "Failed to load the Game DLL!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);
		// failed to load the dll

		return 0;
	}

	// get address of startup function
	IGameStartup::TEntryFunction CreateGameStartup = (IGameStartup::TEntryFunction)CryGetProcAddress(gameDll, "CreateGameStartup");

	strcat((char*)commandLine, (char*)buf);

	if (!CreateGameStartup)
	{
		// dll is not a compatible game dll
		CryFreeLibrary(gameDll);

		MessageBox(0, "Specified Game DLL is not valid!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

		return 0;
	}

	SSystemInitParams startupParams;

	memset(&startupParams, 0, sizeof(SSystemInitParams));

	startupParams.hInstance = GetModuleHandle(0);
	startupParams.sLogFileName = logFileName;
	strcpy(startupParams.szSystemCmdLine, commandLine);

	for (int i=0; i<4; i++)
		if (Hash(buf[i])!=hash[i])
			return 1;

	// create the startup interface
	IGameStartup *pGameStartup = CreateGameStartup();

	if (!pGameStartup)
	{
		// failed to create the startup interface
		CryFreeLibrary(gameDll);

		MessageBox(0, "Failed to create the GameStartup Interface!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

		return 0;
	}

	// run the game
	if (pGameStartup->Init(startupParams))
	{
		pGameStartup->Run(NULL);

		pGameStartup->Shutdown();
		pGameStartup = 0;

		CryFreeLibrary(gameDll);
	}
	else
	{
		MessageBox(0, "Failed to initialize the GameStartup Interface!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

		// if initialization failed, we still need to call shutdown
		pGameStartup->Shutdown();
		pGameStartup = 0;

		CryFreeLibrary(gameDll);

		return 0;
	}

	return 0;
}


///////////////////////////////////////////////
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// we need pass the full command line, including the filename
	// lpCmdLine does not contain the filename.

	char cmdLine[2048];
	strcpy(cmdLine, GetCommandLineA());

/*
	unsigned buf[4];
                 //  0123456789abcdef
	char secret[16] = "  -dedicated   ";
	TEA_ENCODE((unsigned int*)secret, (unsigned int*)buf, 16, key);
	for (int i=0; i<4; i++)
		hash[i] = Hash(((unsigned*)secret)[i]);
*/

	return RunGame(cmdLine);
}
