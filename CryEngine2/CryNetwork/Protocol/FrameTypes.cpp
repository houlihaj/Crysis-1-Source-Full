#include <StdAfx.h>
#include <algorithm>
#include "FrameTypes.h"
#include "Config.h"
#include "MTPseudoRandom.h"
#if USE_GAMESPY_SDK
#include "GameSpy/natneg/natneg.h"
#include "GameSpy/qr2/qr2.h"
#endif


uint8 Frame_HeaderToID[256];
uint8 Frame_IDToHeader[256];
uint8 SeqBytes[256];
uint8 UnseqBytes[256];

void InitFrameTypes()
{
	CMTRand_int32 r(0);

	bool isDevmode = gEnv->pSystem->IsDevMode();

	memset(Frame_HeaderToID, 0, sizeof(Frame_HeaderToID));
#if USE_GAMESPY_SDK
	Frame_HeaderToID[0x3b] = eH_GameSpyCD;
	Frame_HeaderToID[QR_MAGIC_1] = eH_GameSpyGeneral;
	Frame_HeaderToID[QR_MAGIC_2] = eH_GameSpyGeneral;
	for (int i=0; i<NATNEG_MAGIC_LEN; i++)
		Frame_HeaderToID[NNMagicData[i]] = eH_GameSpyNAT;
#endif
#ifdef __WITH_PB__
	Frame_HeaderToID[0xff] = eH_PunkBuster;
#endif
	Frame_HeaderToID['P'] = eH_PingServer;
	Frame_HeaderToID['W'] = eH_QueryLan;
	char conn = '<';
	char discon = '>';
	if (isDevmode)
		std::swap(conn,discon);
	Frame_HeaderToID[conn] = eH_ConnectionSetup;
	Frame_HeaderToID[discon] = eH_Disconnect;
	Frame_HeaderToID['='] = eH_DisconnectAck;

	int ofs = 0;
	for (int i=eH_FIRST_GENERAL_ENTRY; i<eH_LAST_GENERAL_ENTRY; i++)
	{
		while (Frame_HeaderToID[ofs]) 
			ofs++; 
		Frame_HeaderToID[ofs++] = i;
	}

	for (int i=0; i<512; i++)
	{
		int ofs1, ofs2;
		while (true)
		{
			ofs1 = ((r.Generate() + PROTOCOL_VERSION*127) % 256) ^ (isDevmode << (r.Generate()%8));
			ofs2 = (r.Generate() + 256 - eH_LAST_GENERAL_ENTRY) % 256;

			if (ofs1 == ofs2)
				continue;
			if (Frame_HeaderToID[ofs1] && Frame_HeaderToID[ofs1] <= eH_FIRST_GENERAL_ENTRY)
				continue;
			if (Frame_HeaderToID[ofs2] && Frame_HeaderToID[ofs2] <= eH_FIRST_GENERAL_ENTRY)
				continue;
			break;
		}
		std::swap(Frame_HeaderToID[ofs1], Frame_HeaderToID[ofs2]);
	}

	for (int i=0; i<256; i++)
	{
		Frame_IDToHeader[Frame_HeaderToID[i]] = i;
	}

	for (int i=0; i<256; i++)
		SeqBytes[i] = i^0xee;
	for (int i=0; i<512; i++)
	{
		uint8 a, b;
		do 
		{
			uint32 rx = r.Generate();
			a = (rx>>10)+PROTOCOL_VERSION;
			b = (rx+PROTOCOL_VERSION)>>20;
		} 
		while(a==b);
		std::swap( SeqBytes[a], SeqBytes[b] );
	}
	for (int i=0; i<256; i++)
		UnseqBytes[SeqBytes[i]] = i;
}
