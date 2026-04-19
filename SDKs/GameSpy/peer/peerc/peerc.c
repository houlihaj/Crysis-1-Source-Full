// GameSpy Peer SDK C Test App
// Dan "Mr. Pants" Schoenblum
// dan@gamespy.com

/*************
** INCLUDES **
*************/
#include "../peer.h"
#include "../../common/gsStringUtil.h"
#include "../../common/gsAvailable.h"

#ifdef UNDER_CE
	void RetailOutputA(CHAR *tszErr, ...);
	#define printf RetailOutputA
#elif defined(_NITRO)
	#include "../../common/nitro/screen.h"
	#define printf Printf
	#define vprintf VPrintf
#endif

#define NICK_SIZE    32

#define LOOP_SECONDS 60

gsi_char RtoS[][16] =
{
	_T("TitleRoom"),
	_T("GroupRoom"),
	_T("StagingRoom")
};

/*************
** SETTINGS **
*************/
gsi_char nick[NICK_SIZE];
int profileID;
gsi_char title[256];
gsi_char qrSecretKey[256];
gsi_char engineName[256];
gsi_char engineSecretKey[256];
int engineMaxUpdates;

PEERBool stop;

/**************
** FUNCTIONS **
**************/
static void DisconnectedCallback
(
	PEER peer,
	const gsi_char * reason,
	void * param
)
{
	_tprintf(_T("Disconnected: %s\n"), reason);
	
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
}

static void RoomMessageCallback
(
	PEER peer,
	RoomType roomType,
	const gsi_char * nick,
	const gsi_char * message,
	MessageType messageType,
	void * param
)
{
	_tprintf(_T("(%s) %s: %s\n"), RtoS[roomType], nick, message);
	//if(strcasecmp(message, _T("quit")) == 0)
	//	stop = PEERTrue;
	//else if((strlen(message) > 5) && (strncasecmp(message, _T("echo"), 4) == 0))
	//	peerMessageRoom(peer, roomType, message + 5, messageType);

	GSI_UNUSED(param);
	GSI_UNUSED(peer);
	GSI_UNUSED(messageType);
}

static void RoomUTMCallback
(
	PEER peer, 
	RoomType roomType, 
	const gsi_char * nick, 
	const gsi_char * command, 
	const gsi_char * parameters,
	PEERBool authenticated, 
	void * param 
)
{
	GSI_UNUSED(peer);
	GSI_UNUSED(roomType);
	GSI_UNUSED(nick);
	GSI_UNUSED(command);
	GSI_UNUSED(parameters);
	GSI_UNUSED(authenticated);
	GSI_UNUSED(param);
}

static void PlayerMessageCallback
(
	PEER peer,
	const gsi_char * nick,
	const gsi_char * message,
	MessageType messageType,
	void * param
)
{
	_tprintf(_T("(PRIVATE) %s: %s\n"), nick, message);
	
	GSI_UNUSED(peer);
	GSI_UNUSED(messageType);
	GSI_UNUSED(param);
}

static void ReadyChangedCallback
(
	PEER peer,
	const gsi_char * nick,
	PEERBool ready,
	void * param
)
{
	if(ready)
		_tprintf(_T("%s is ready\n"), nick);
	else
		_tprintf(_T("%s is not ready\n"), nick);

	GSI_UNUSED(peer);
	GSI_UNUSED(param);
}

static void GameStartedCallback
(
	PEER peer,
	SBServer server,
	const gsi_char * message,
	void * param
)
{
	_tprintf(_T("The game is starting at %s\n"), SBServerGetPublicAddress(server));
	if(message && message[0])
		_tprintf(_T(": %s\n"), message);
	else
		_tprintf(_T("\n"));
		
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
}

static void PlayerJoinedCallback
(
	PEER peer,
	RoomType roomType,
	const gsi_char * nick,
	void * param
)
{
	_tprintf(_T("%s joined the %s\n"), nick, RtoS[roomType]);
	
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
}

static void PlayerLeftCallback
(
	PEER peer,
	RoomType roomType,
	const gsi_char * nick,
	const gsi_char * reason,
	void * param
)
{
	_tprintf(_T("%s left the %s\n"), nick, RtoS[roomType]);
	
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
	GSI_UNUSED(reason);
}

static void PlayerChangedNickCallback
(
	PEER peer,
	RoomType roomType,
	const gsi_char * oldNick,
	const gsi_char * newNick,
	void * param
)
{
	_tprintf(_T("%s in %s changed nicks to %s\n"), oldNick, RtoS[roomType], newNick);
	
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
}

PEERBool connectSuccess;
static void ConnectCallback
(
	PEER peer,
	PEERBool success,
	int failureReason,
	void * param
)
{
	connectSuccess = success;
	
	GSI_UNUSED(peer);
	GSI_UNUSED(success);
	GSI_UNUSED(failureReason);
	GSI_UNUSED(param);
}
/*
static void EnumPlayersCallback
(
	PEER peer,
	PEERBool success,
	RoomType roomType,
	int index,
	const gsi_char * nick,
	int flags,
	void * param
)
{
	if(!success)
	{
		_tprintf(_T("Enum %s players failed\n"), RtoS[roomType]);
		return;
	}

	if(index == -1)
	{
		_tprintf(_T("--------------------\n"));
		return;
	}

	_tprintf(_T("%d: %s"), index, nick);
	if(flags & PEER_FLAG_OP)
		_tprintf(_T(" (host)\n"));
	else
		_tprintf(_T("\n"));
		

	GSI_UNUSED(peer);
	GSI_UNUSED(param);
}

int gStartListing;
static void ListingGamesCallback
(
	PEER peer,
	PEERBool success,
	const gsi_char * name,
	SBServer server,
	PEERBool staging,
	int msg,
	int progress,
	void * param
)
{
	if(success)
	{
		gsi_char *msgname;
		switch (msg)
		{
		case PEER_CLEAR:
			msgname = _T("PEER_CLEAR");
			break;
		case PEER_ADD:
			msgname = _T("PEER_ADD");
			if (SBServerHasBasicKeys(server))
				_tprintf(_T("  firewall server name: %s numplayers: %d ping: %d firewall: %s\n"), SBServerGetStringValue(server,_T("hostname"),_T("UNKNOWN")), SBServerGetIntValue(server,_T("numplayers"),0), SBServerGetPing(server), SBServerDirectConnect(server)==SBTrue?_T("NO"):_T("YES"));
			if (SBServerHasFullKeys(server))
				_tprintf(_T("  server gametype: %s \n"), SBServerGetStringValue(server, _T("gametype"), _T("unknown")));
			break;
		case PEER_UPDATE:
			msgname = _T("PEER_UPDATE");
			if(server)
			{
				_tprintf(_T("  update server name: %s numplayers: %d ping: %d firewall: %s\n"), SBServerGetStringValue(server,_T("hostname"),_T("UNKNOWN")), SBServerGetIntValue(server,_T("numplayers"),0), SBServerGetPing(server), SBServerDirectConnect(server)==SBTrue?_T("NO"):_T("YES"));
				_tprintf(_T("  server gametype: %s \n"), SBServerGetStringValue(server, _T("gametype"), _T("unknown")));
			}
			break;
		case PEER_REMOVE:
			msgname = _T("PEER_REMOVE");
			break;
		case PEER_COMPLETE:
			msgname = _T("PEER_COMPLETE");
			gStartListing = 1;
			break;
		default:
			msgname = _T("ERROR!");
		}
		//_tprintf("game: %s\n", msgname);
		//if(server)
		//	_tprintf("  server name: %s numplayers: %d ping: %d\n", SBServerGetStringValue(server,_T("hostname"),_T("UNKNOWN")), SBServerGetIntValue(server,_T("numplayers"),0), SBServerGetPing(server));
	}

	GSI_UNUSED(peer);
	GSI_UNUSED(name);
	GSI_UNUSED(staging);
	GSI_UNUSED(progress);
	GSI_UNUSED(param);
}
*/
static void ListGroupRoomsCallback
(
	PEER peer,
	PEERBool success,
	int groupID,
	SBServer server,
	const gsi_char * name,
	int numWaiting,
	int maxWaiting,
	int numGames,
	int numPlaying,
	void * param
)
{
	if(success && (groupID > 0))
	{
		_tprintf(_T("group: %s\n"), name);
	}

	GSI_UNUSED(peer);
	GSI_UNUSED(server);
	GSI_UNUSED(numWaiting);
	GSI_UNUSED(maxWaiting);
	GSI_UNUSED(numGames);
	GSI_UNUSED(numPlaying);
	GSI_UNUSED(param);
}

PEERBool joinSuccess;
static void JoinCallback
(
	PEER peer,
	PEERBool success,
	PEERJoinResult result,
	RoomType roomType,
	void * param
)
{
	joinSuccess = success;

	GSI_UNUSED(peer);
	GSI_UNUSED(result);
	GSI_UNUSED(roomType);
	GSI_UNUSED(param);
}

static void NickErrorCallback
(
	PEER peer,
	int type,
	const gsi_char * badNick,
	int numSuggestedNicks,
	const gsi_char ** suggestedNicks,
	void * param
)
{
	static int errCount;

	if(errCount < 20)
	{
		_tsnprintf(nick,NICK_SIZE,_T("peerc%u"),(unsigned int)current_time());
		nick[NICK_SIZE - 1] = '\0';
		peerRetryWithNick(peer, nick);
	}
	else
	{
		//peerDisconnect(peer);
		peerRetryWithNick(peer, NULL);
	}
	errCount++;
	
	GSI_UNUSED(type);
	GSI_UNUSED(badNick);
	GSI_UNUSED(suggestedNicks);
	GSI_UNUSED(numSuggestedNicks);
	GSI_UNUSED(param);
}
/*
static void AutoMatchStatusCallback(PEER thePeer, PEERAutoMatchStatus theStatus, void* theParam)
{
	_tprintf(_T("Automatch status: %d\r\n"), theStatus);
	GSI_UNUSED(thePeer);
	GSI_UNUSED(theStatus);
	GSI_UNUSED(theParam);
}

static int AutoMatchRateCallback(PEER thePeer, SBServer theServer, void* theParam)
{
	GSI_UNUSED(thePeer);
	GSI_UNUSED(theServer);
	GSI_UNUSED(theParam);
	return 0;
}
*/
static void RoomKeyChangedCallback(PEER thePeer, RoomType theType, const gsi_char* theNick, const gsi_char* theKey, const gsi_char* theValue, void* theParam)
{
	GSI_UNUSED(thePeer);
	GSI_UNUSED(theType);
	GSI_UNUSED(theNick);
	GSI_UNUSED(theKey);
	GSI_UNUSED(theValue);
	GSI_UNUSED(theParam);
}

static void QRServerKeyCallback
(
	PEER peer,
	int key,
	qr2_buffer_t buffer,
	void * param
)
{
#if VERBOSE
	gsi_char verbose[256];
	_stprintf(verbose, _T("QR_SERVER_KEY | %d\n"), key);
	OutputDebugString(verbose);
#endif

	switch(key)
	{
	case HOSTNAME_KEY:
		qr2_buffer_add(buffer, _T("My Game"));
		break;
	case NUMPLAYERS_KEY:
		qr2_buffer_add_int(buffer, 1);
		break;
	case GAMEMODE_KEY:
		qr2_buffer_add(buffer, _T("openplaying"));
		break;
	case HOSTPORT_KEY:
		qr2_buffer_add_int(buffer, 15151);
		break;
	case MAPNAME_KEY:
		qr2_buffer_add(buffer, _T("Big Crate Room"));
		break;
	case GAMETYPE_KEY:
		qr2_buffer_add(buffer, _T("Friendly"));
		break;
	case TIMELIMIT_KEY:
		qr2_buffer_add_int(buffer, 100);
		break;
	case FRAGLIMIT_KEY:
		qr2_buffer_add_int(buffer, 0);
		break;
	case TEAMPLAY_KEY:
		qr2_buffer_add_int(buffer, 0);
		break;
	default:
		qr2_buffer_add(buffer, _T(""));
		break;
	}
	
	GSI_UNUSED(peer);
	GSI_UNUSED(param);
}

static void QRKeyListCallback
(
	PEER peer,
	qr2_key_type type,
	qr2_keybuffer_t keyBuffer,
	void * param
)
{
	// register the keys we use
	switch(type)
	{
	case key_server:
		if(!peerIsAutoMatching(peer))
		{
			qr2_keybuffer_add(keyBuffer, HOSTPORT_KEY);
			qr2_keybuffer_add(keyBuffer, MAPNAME_KEY);
			qr2_keybuffer_add(keyBuffer, GAMETYPE_KEY);
			qr2_keybuffer_add(keyBuffer, TIMELIMIT_KEY);
			qr2_keybuffer_add(keyBuffer, FRAGLIMIT_KEY);
			qr2_keybuffer_add(keyBuffer, TEAMPLAY_KEY);
		}
		break;
	case key_player:
		// no custom player keys
		break;
	case key_team:
		// no custom team keys
		break;
	default: break;
	}
	
	GSI_UNUSED(param);
}




#ifdef __MWERKS__ // CodeWarrior will warn if not prototyped
	int test_main(int argc, char **argv);
#endif

int test_main(int argc, char **argv)
{
	PEER peer;
	PEERCallbacks callbacks;
	PEERBool pingRooms[NumRooms];
	PEERBool crossPingRooms[NumRooms];
	GSIACResult result;
	gsi_time startTime;

	// check that the game's backend is available
	GSIStartAvailableCheck(_T("gmtest"));
	while((result = GSIAvailableCheckThink()) == GSIACWaiting)
		msleep(5);
	if(result != GSIACAvailable)
	{
		_tprintf(_T("The backend is not available\n"));
		return 1;
	}

	// Defaults.
	////////////
	_tsnprintf(nick,NICK_SIZE,_T("peerc%u"),(unsigned int)current_time());
	nick[NICK_SIZE - 1] = '\0';
	profileID = 0;
	_tcscpy(title, _T("gmtest"));
	_tcscpy(qrSecretKey, _T("HA6zkS"));
	_tcscpy(engineName, _T("gmtest"));
	_tcscpy(engineSecretKey, _T("HA6zkS"));
	
	engineMaxUpdates = 10;

	// Setup the callbacks.
	///////////////////////
	memset(&callbacks, 0, sizeof(PEERCallbacks));
	callbacks.disconnected = DisconnectedCallback;
	//callbacks.qrNatNegotiateCallback
	callbacks.gameStarted = GameStartedCallback;
	callbacks.playerChangedNick = PlayerChangedNickCallback;
	callbacks.playerJoined = PlayerJoinedCallback;
	callbacks.playerLeft = PlayerLeftCallback;
	callbacks.readyChanged = ReadyChangedCallback;
	callbacks.roomMessage = RoomMessageCallback;
	callbacks.playerMessage = PlayerMessageCallback;
	callbacks.roomUTM = RoomUTMCallback;
	callbacks.roomKeyChanged = RoomKeyChangedCallback;
	callbacks.qrKeyList = QRKeyListCallback;
	callbacks.qrServerKey = QRServerKeyCallback;
	callbacks.param = NULL;

	// Init.
	////////
	peer = peerInitialize(&callbacks);
	if(!peer)
	{
		_tprintf(_T("Failed to init\n"));
		return 1;
	}

	// Ping/cross-ping in every room.
	/////////////////////////////////
	pingRooms[TitleRoom] = PEERTrue;
	pingRooms[GroupRoom] = PEERTrue;
	pingRooms[StagingRoom] = PEERTrue;
	crossPingRooms[TitleRoom] = PEERTrue;
	crossPingRooms[GroupRoom] = PEERTrue;
	crossPingRooms[StagingRoom] = PEERTrue;

	// Set the title.
	/////////////////
	if(!peerSetTitle(peer, title, qrSecretKey, engineName, engineSecretKey, 0, engineMaxUpdates, PEERFalse, pingRooms, crossPingRooms))
	{
		peerShutdown(peer);
		_tprintf(_T("Failed to set the title\n"));
		return 1;
	}

	// Connect.
	///////////
	_tprintf(_T("Connecting as %s...\n"), nick);
	peerConnect(peer, nick, profileID, NickErrorCallback, ConnectCallback, NULL, PEERTrue);
	if(!connectSuccess)
	{
		peerShutdown(peer);
		_tprintf(_T("Failed to connect\n"));
		return 1;
	}
	printf("Connected\n");

	// Join the title room.
	///////////////////////
	printf("Joining title room...\n");
	peerJoinTitleRoom(peer, NULL, JoinCallback, NULL, PEERTrue);
	if(!joinSuccess)
	{
		peerDisconnect(peer);
		peerShutdown(peer);
		_tprintf(_T("Failed to join the title room\n"));
		return 1;
	}
	printf("Joined\n");

	printf("Listing group rooms\n");
	peerListGroupRooms(peer, _T(""), ListGroupRoomsCallback, NULL, PEERFalse);
	//peerMessageRoom(peer, TitleRoom, _T("peerMessageRoom"), NormalMessage);
	//peerStartAutoMatch(peer, 3, _T(""), AutoMatchStatusCallback, AutoMatchRateCallback, NULL, PEERFalse);

	// Loop for a while.
	////////////////////
	printf("Looping for %d seconds\n", LOOP_SECONDS);
	startTime = current_time();
	while((current_time() - startTime) < (LOOP_SECONDS * 1000))
	{
		peerThink(peer);
		msleep(1);
	} 

	peerStopListingGames(peer);

	// Leave the title room.
	////////////////////////
	peerLeaveRoom(peer, TitleRoom, NULL);

	// Stop auto match if it's in progress
	//////////////
	peerStopAutoMatch(peer);

	// Disconnect.
	//////////////
	peerDisconnect(peer);

	// Shutdown.
	////////////
	peerShutdown(peer);
	_tprintf(_T("All done!\n"));



	GSI_UNUSED(argc);
	GSI_UNUSED(argv);
	return 0;
}
