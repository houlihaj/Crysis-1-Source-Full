/******
sbctest.c
GameSpy Server Browsing SDK
  
Copyright 1999-2002 GameSpy Industries, Inc

18002 Skypark Circle
Irvine, California 92614
949.798.4200 (Tel)
949.798.4299 (Fax)
devsupport@gamespy.com

******

 Please see the GameSpy Server Browsing SDK documentation for more 
 information

******/
#include "../sb_serverbrowsing.h"
#include "../../qr2/qr2.h"
#include "../../common/gsAvailable.h"

#ifdef UNDER_CE
	void RetailOutputA(CHAR *tszErr, ...);
	#define printf RetailOutputA
#elif defined(_NITRO)
	#include "../../common/nitro/screen.h"
	#define printf Printf
	#define vprintf VPrintf
#endif

static gsi_bool UpdateFinished = gsi_false;

static void SBCallback(ServerBrowser sb, SBCallbackReason reason, SBServer server, void *instance)
{
	int updatePercent = 0;
	gsi_char anAddress[20] = { '\0' };
#ifdef GSI_UNICODE
	if (server)
		AsciiToUCS2String(SBServerGetPublicAddress(server),anAddress);
#else
	if (server)
		strcpy(anAddress, SBServerGetPublicAddress(server));
#endif

	switch (reason)
	{
	case sbc_serveradded:
		if (SBFalse == SBServerHasBasicKeys(server))
			_tprintf(_T("Server Added: %s:%d\n"), anAddress, SBServerGetPublicQueryPort(server));
		else
		{
			_tprintf(_T("Firewall Server Added: %s:%d [%d] - %s - %d/%d (%d%%)\n"), anAddress, SBServerGetPublicQueryPort(server), SBServerGetPing(server),
			SBServerGetStringValue(server, _T("hostname"), _T("")), SBServerGetIntValue(server, _T("numplayers"), 0), SBServerGetIntValue(server, _T("maxplayers"), 0), updatePercent);
		}

		break;
	case sbc_serverchallengereceived:
		_tprintf(_T("Server challenge received: %s:%d\n"), anAddress, SBServerGetPublicQueryPort(server));
		break;
		
	case sbc_serverupdated:
		//determine our update percent
		if (ServerBrowserCount(sb) > 0)
			updatePercent = (ServerBrowserCount(sb) - ServerBrowserPendingQueryCount(sb)) * 100 / ServerBrowserCount(sb);
		_tprintf(_T("Server Updated: %s:%d [%d] - %s - %d/%d (%d%%)\n"), anAddress, SBServerGetPublicQueryPort(server), SBServerGetPing(server),
			SBServerGetStringValue(server, _T("hostname"), _T("")), SBServerGetIntValue(server, _T("numplayers"), 0), SBServerGetIntValue(server, _T("maxplayers"), 0), updatePercent);
		break;
	case sbc_serverupdatefailed:
		_tprintf(_T("Update Failed: %s:%d\n"), anAddress, SBServerGetPublicQueryPort(server));
		break;
	case sbc_updatecomplete:
		_tprintf(_T("Server Browser Update Complete\r\n")); 
		UpdateFinished = gsi_true;
		break;
	case sbc_queryerror:
		_tprintf(_T("Query Error: %s\n"), ServerBrowserListQueryError(sb));
		UpdateFinished = gsi_true;
		break;
	default:
		break;
	}

	GSI_UNUSED(instance);
}


#ifdef __MWERKS__ // CodeWarrior will warn if not prototyped
	int test_main(int argc, char **argp);
#endif	
int test_main(int argc, char **argp)
{
	ServerBrowser sb;
	unsigned char basicFields[] = {HOSTNAME_KEY, GAMETYPE_KEY,  MAPNAME_KEY, NUMPLAYERS_KEY, MAXPLAYERS_KEY};
	int numFields = sizeof(basicFields) / sizeof(basicFields[0]);
	GSIACResult result;

	// check that the game's backend is available
	GSIStartAvailableCheck(_T("gmtest"));
	while((result = GSIAvailableCheckThink()) == GSIACWaiting)
		msleep(5);
	if(result != GSIACAvailable)
	{
		printf("The backend is not available\n");
		return 1;
	}

	//create a new server browser object
	sb = ServerBrowserNew (_T("gmtest"), _T("gmtest"), _T("HA6zkS"), 0, 40, QVERSION_QR2, SBFalse, SBCallback, NULL);

	printf("Starting server browser update\n");

	//begin the update (async)
	//ServerBrowserLANUpdate(sb, SBTrue, 11111, 11111);
	//ServerBrowserAuxUpdateIP(sb, _T("192.168.60.75"), 11111, SBFalse, SBTrue, SBTrue);
	ServerBrowserUpdate(sb, SBTrue, SBTrue, basicFields, numFields, NULL);
	
	//think while the update is in progress
	while ((ServerBrowserThink(sb) == sbe_noerror) && (UpdateFinished == gsi_false))
		msleep(10);
	
	ServerBrowserFree(sb); //how to clean up, if we were to actually get here

	GSI_UNUSED(argc);
	GSI_UNUSED(argp);

	return 0;
}
