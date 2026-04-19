#include "../gp.h"
#include "../../common/gsAvailable.h"

#if defined(_WIN32) && !defined(UNDER_CE)
	#include <conio.h>
#endif

#ifdef UNDER_CE
	void RetailOutputA(CHAR *tszErr, ...);
	#define printf RetailOutputA
#elif defined(_NITRO)
	#include "../../common/nitro/screen.h"
	#define printf Printf
	#define vprintf VPrintf
#endif

#if defined(_WIN32)
// disable the warning about our while(1) statement
#pragma warning(disable:4127)
#endif

#ifdef __MWERKS__	// CodeWarrior will warn if function is not prototyped
int test_main(int argc, char **argv);
#endif 

#define GPTC_PRODUCTID 0
#define GPTC_GAMENAME   _T("gmtest")
#define GPTC_NICK1      _T("gptestc1")
#define GPTC_NICK2      _T("gptestc2")
#define GPTC_EMAIL1     _T("gptestc@gptestc.com")
#define GPTC_EMAIL2     _T("gptestc2@gptestc.com")
#define GPTC_PASSWORD   _T("gptestc")
#define GPTC_PID1       2957553
#define GPTC_PID2       3052160
#define GPTC_FIREWALL_OPTION   GP_FIREWALL

#define CHECK_GP_RESULT(func, errString) if(func != GP_NO_ERROR) { printf("%s\n", errString); return 1; }

GPConnection * pconn;
GPProfile other;

#ifdef GSI_COMMON_DEBUG
	static void DebugCallback(GSIDebugCategory theCat, GSIDebugType theType,
	                          GSIDebugLevel theLevel, const char * theTokenStr,
	                          va_list theParamList)
	{
		GSI_UNUSED(theLevel);

		printf("[%s][%s] ", 
				gGSIDebugCatStrings[theCat], 
				gGSIDebugTypeStrings[theType]);

		vprintf(theTokenStr, theParamList);
	}
#endif

static void Error(GPConnection * pconnection, GPErrorArg * arg, void * param)
{
	gsi_char * errorCodeString;
	gsi_char * resultString;

#define RESULT(x) case x: resultString = _T(#x); break;
	switch(arg->result)
	{
	RESULT(GP_NO_ERROR)
	RESULT(GP_MEMORY_ERROR)
	RESULT(GP_PARAMETER_ERROR)
	RESULT(GP_NETWORK_ERROR)
	RESULT(GP_SERVER_ERROR)
	default:
		resultString = _T("Unknown result!\n");
	}

#define ERRORCODE(x) case x: errorCodeString = _T(#x); break;
	switch(arg->errorCode)
	{
	ERRORCODE(GP_GENERAL)
	ERRORCODE(GP_PARSE)
	ERRORCODE(GP_NOT_LOGGED_IN)
	ERRORCODE(GP_BAD_SESSKEY)
	ERRORCODE(GP_DATABASE)
	ERRORCODE(GP_NETWORK)
	ERRORCODE(GP_FORCED_DISCONNECT)
	ERRORCODE(GP_CONNECTION_CLOSED)
	ERRORCODE(GP_LOGIN)
	ERRORCODE(GP_LOGIN_TIMEOUT)
	ERRORCODE(GP_LOGIN_BAD_NICK)
	ERRORCODE(GP_LOGIN_BAD_EMAIL)
	ERRORCODE(GP_LOGIN_BAD_PASSWORD)
	ERRORCODE(GP_LOGIN_BAD_PROFILE)
	ERRORCODE(GP_LOGIN_PROFILE_DELETED)
	ERRORCODE(GP_LOGIN_CONNECTION_FAILED)
	ERRORCODE(GP_LOGIN_SERVER_AUTH_FAILED)
	ERRORCODE(GP_NEWUSER)
	ERRORCODE(GP_NEWUSER_BAD_NICK)
	ERRORCODE(GP_NEWUSER_BAD_PASSWORD)
	ERRORCODE(GP_UPDATEUI)
	ERRORCODE(GP_UPDATEUI_BAD_EMAIL)
	ERRORCODE(GP_NEWPROFILE)
	ERRORCODE(GP_NEWPROFILE_BAD_NICK)
	ERRORCODE(GP_NEWPROFILE_BAD_OLD_NICK)
	ERRORCODE(GP_UPDATEPRO)
	ERRORCODE(GP_UPDATEPRO_BAD_NICK)
	ERRORCODE(GP_ADDBUDDY)
	ERRORCODE(GP_ADDBUDDY_BAD_FROM)
	ERRORCODE(GP_ADDBUDDY_BAD_NEW)
	ERRORCODE(GP_ADDBUDDY_ALREADY_BUDDY)
	ERRORCODE(GP_AUTHADD)
	ERRORCODE(GP_AUTHADD_BAD_FROM)
	ERRORCODE(GP_AUTHADD_BAD_SIG)
	ERRORCODE(GP_STATUS)
	ERRORCODE(GP_BM)
	ERRORCODE(GP_BM_NOT_BUDDY)
	ERRORCODE(GP_GETPROFILE)
	ERRORCODE(GP_GETPROFILE_BAD_PROFILE)
	ERRORCODE(GP_DELBUDDY)
	ERRORCODE(GP_DELBUDDY_NOT_BUDDY)
	ERRORCODE(GP_DELPROFILE)
	ERRORCODE(GP_DELPROFILE_LAST_PROFILE)
	ERRORCODE(GP_SEARCH)
	ERRORCODE(GP_SEARCH_CONNECTION_FAILED)
	default:
		errorCodeString = _T("Unknown error code!\n");
	}

	if(arg->fatal)
	{
		printf( "-----------\n");
		printf( "FATAL ERROR\n");
		printf( "-----------\n");
	}
	else
	{
		printf( "-----\n");
		printf( "ERROR\n");
		printf( "-----\n");
	}
	_tprintf( _T("RESULT: %s (%d)\n"), resultString, arg->result);
	_tprintf( _T("ERROR CODE: %s (0x%X)\n"), errorCodeString, arg->errorCode);
	_tprintf( _T("ERROR STRING: %s\n"), arg->errorString);
	
	GSI_UNUSED(pconnection);
	GSI_UNUSED(param);
}

gsi_char whois[GP_NICK_LEN];
static void Whois(GPConnection * pconnection, GPGetInfoResponseArg * arg, void * param)
{
	if(arg->result == GP_NO_ERROR)
	{
		_tcscpy(whois, arg->nick);
		_tcscat(whois, _T("@"));
		_tcscat(whois, arg->email);
	}
	else
		printf( "WHOIS FAILED\n");
	
	GSI_UNUSED(pconnection);
	GSI_UNUSED(param);
}	

static void ConnectResponse(GPConnection * pconnection, GPConnectResponseArg * arg, void * param)
{
	if(arg->result == GP_NO_ERROR)
		printf("CONNECTED\n");
	else
		printf( "CONNECT FAILED\n");
		
	GSI_UNUSED(pconnection);
	GSI_UNUSED(param);
}

static void ProfileSearchResponse(GPConnection * pconnection, GPProfileSearchResponseArg * arg, void * param)
{
	GPResult result;
	int i;
	if(arg->result == GP_NO_ERROR)
	{
		if(arg->numMatches > 0)
		{
			for(i = 0 ; i < arg->numMatches ; i++)
			{
				result = gpGetInfo(pconn, arg->matches[i].profile, GP_DONT_CHECK_CACHE, GP_BLOCKING, (GPCallback)Whois, NULL);
				if(result != GP_NO_ERROR)
					printf("gpGetInfo failed\n");
				else
					_tprintf(_T("FOUND: %s\n"), whois);
			}
		}
		else
			printf( "NO MATCHES\n");
	}
	else
		printf( "SEARCH FAILED\n");
		
	GSI_UNUSED(pconnection);
	GSI_UNUSED(param);
}

int msgCount = 0;
static void RecvBuddyMessage(GPConnection * pconnection, GPRecvBuddyMessageArg * arg, void * param)
{
	GPResult result;

	msgCount++;
	result = gpGetInfo(pconn, arg->profile, GP_DONT_CHECK_CACHE, GP_BLOCKING, (GPCallback)Whois, NULL);
	if(result != GP_NO_ERROR)
		printf("gpGetInfo failed\n");
	else
		_tprintf(_T("MESSAGE (%d): %s: %s\n"), msgCount,whois, arg->message);

	if(msgCount < 5)
	{
		result = gpSendBuddyMessage(pconn, other, _T("HELLO!"));
		if(result != GP_NO_ERROR)
			printf("gpSendBuddyMessage failed\n");
	}

	_tprintf(_T("RECEIVED BUDDY MESSAGE %d/5\n"), msgCount);
		
	GSI_UNUSED(pconnection);
	GSI_UNUSED(param);
}


static void GetInfoResponse(GPConnection * pconnection, GPGetInfoResponseArg * arg, void * param)
{
	_tprintf(_T("INFO: FN: %s LN: %s (%s@%s)\n"), arg->firstname, arg->lastname, arg->nick, arg->email);
	
	GSI_UNUSED(pconnection);
	GSI_UNUSED(param);
}


int test_main(int argc, char **argv)
{
	gsi_char * nick;
	gsi_char * email;
	gsi_char * password;
	GPConnection connection;
	int which;
	GSIACResult acResult = GSIACWaiting;

#ifdef GSI_COMMON_DEBUG
	// Define GSI_COMMON_DEBUG if you want to view the SDK debug output
	// Set the SDK debug log file, or set your own handler using gsSetDebugCallback
	//gsSetDebugFile(stdout); // output to console
	gsSetDebugCallback(DebugCallback);

	// Set some debug levels
	gsSetDebugLevel(GSIDebugCat_All, GSIDebugType_All, GSIDebugLevel_Debug);
	//gsSetDebugLevel(GSIDebugCat_QR2, GSIDebugType_Network, GSIDebugLevel_Verbose);   // Show Detailed data on network traffic
	//gsSetDebugLevel(GSIDebugCat_App, GSIDebugType_All, GSIDebugLevel_Hardcore);  // Show All app comment
#endif

	GSI_UNUSED(argv);

	// check that the game's backend is available
	GSIStartAvailableCheck(GPTC_GAMENAME);
	while((acResult = GSIAvailableCheckThink()) == GSIACWaiting)
		msleep(5);
	if(acResult != GSIACAvailable)
	{
		printf("The backend is not available\n");
		return 1;
	}

	pconn = &connection;

	if(argc == 1)
	{
		which = 1;
		nick = GPTC_NICK1;
		email = GPTC_EMAIL1;
	}
	else
	{
		which = 0;
		nick = GPTC_NICK2;
		email = GPTC_EMAIL2;
	}
	_tprintf(_T("Using email= %s, nick= %s\r\n"), email, nick);

	password = GPTC_PASSWORD;

	//INITIALIZE
	////////////
	CHECK_GP_RESULT(gpInitialize(pconn, GPTC_PRODUCTID, 0, GP_PARTNERID_GAMESPY), "gpInitialize failed");
	gpEnable(pconn, GP_INFO_CACHING_BUDDY_ONLY);
	printf("INITIALIZED\n");
	CHECK_GP_RESULT(gpSetCallback(pconn, GP_ERROR, (GPCallback)Error, NULL), "gpSetCallback failed");
	CHECK_GP_RESULT(gpSetCallback(pconn, GP_RECV_BUDDY_MESSAGE, (GPCallback)RecvBuddyMessage, NULL), "gpSetCallback failed");

	//CONNECT
	/////////
	printf("CONNECTING...\n");
	CHECK_GP_RESULT(gpConnect(pconn, nick, email, password, GPTC_FIREWALL_OPTION, GP_BLOCKING, (GPCallback)ConnectResponse, NULL), "gpConnect failed");

	//SEARCH (blocking)
	///////////////////
	printf("SEARCHING\n");
	CHECK_GP_RESULT(gpProfileSearch(pconn, _T(""), _T(""), _T("dan@gamespy.com"), _T(""), _T(""), 0, GP_BLOCKING, (GPCallback)ProfileSearchResponse, NULL), "gpProfileSearch failed");
	CHECK_GP_RESULT(gpProfileSearch(pconn, _T("crt"), _T(""), _T(""), _T(""), _T(""), 0, GP_BLOCKING, (GPCallback)ProfileSearchResponse, NULL), "gpProfileSearch failed");
	CHECK_GP_RESULT(gpProfileSearch(pconn, GPTC_NICK1, _T(""), GPTC_EMAIL1, _T(""), _T(""), 0, GP_BLOCKING, (GPCallback)ProfileSearchResponse, NULL), "gpProfileSearch failed");

	if(which)
	{
		CHECK_GP_RESULT(gpProfileFromID(pconn, &other, GPTC_PID2), "gpProfileFromID failed");
	}
	else
	{
		CHECK_GP_RESULT(gpProfileFromID(pconn, &other, GPTC_PID1), "gpProfileFromID failed");
	}
	
	//PROCESS
	/////////
	CHECK_GP_RESULT(gpProcess(pconn), "gpProcess failed");
	
	//GET INFO
	//////////
	printf("GETTING INFO...\n");
	CHECK_GP_RESULT(gpGetInfo(pconn, GPTC_PID1, GP_DONT_CHECK_CACHE, GP_BLOCKING, (GPCallback)GetInfoResponse, NULL), "gpGetInfo failed");
	
	//MESSAGE
	/////////
	CHECK_GP_RESULT(gpSendBuddyMessage(pconn, other, _T("HELLO!")), "gpSendBuddyMessage failed");

	//PROCESS
	/////////
	printf("WAITING FOR BUDDY MESSAGE REPLIES...\n");
	while(msgCount < 5) // wait for previous operations to complete
	{
		CHECK_GP_RESULT(gpProcess(pconn), "gpProcess failed");
		msleep(10);
	}

	//SEARCH (non-blocking)
	///////////////////////
	printf("SEARCHING\n");
	CHECK_GP_RESULT(gpProfileSearch(pconn, _T(""), _T(""), _T("dan@gamespy.com"), _T(""), _T(""), 0, GP_BLOCKING, (GPCallback)ProfileSearchResponse, NULL), "gpProfileSearch failed");
	CHECK_GP_RESULT(gpProfileSearch(pconn, _T("crt"), _T(""), _T(""), _T(""), _T(""), 0, GP_BLOCKING, (GPCallback)ProfileSearchResponse, NULL), "gpProfileSearch failed");
	CHECK_GP_RESULT(gpProfileSearch(pconn, GPTC_NICK1, _T(""), GPTC_EMAIL1, _T(""), _T(""), 0, GP_BLOCKING, (GPCallback)ProfileSearchResponse, NULL), "gpProfileSearch failed");

	printf("LOOPING\n");
	while (1)
	{
		CHECK_GP_RESULT(gpProcess(pconn), "gpProcess failed");
		msleep(10);
		#if defined(_WIN32) && !defined(UNDER_CE)
			if (_kbhit())
				break;
		#else
		 //on other systems, you gotta kill it hard
		#endif
	}

#if defined(_WIN32) && !defined(UNDER_CE)
	//DISCONNECT
	////////////
	gpDisconnect(pconn);
	printf("DISCONNECTED\n");

	//DESTROY
	/////////
	gpDestroy(pconn);
	printf("DESTROYED\n");
	return 0;
#endif
}


