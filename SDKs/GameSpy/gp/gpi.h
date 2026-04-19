/*
gpi.h
GameSpy Presence SDK 
Dan "Mr. Pants" Schoenblum

Copyright 1999-2001 GameSpy Industries, Inc

18002 Skypark Circle
Irvine, California 92614
949.798.4200 (Tel)
949.798.4299 (Fax)
devsupport@gamespy.com

***********************************************************************
Please see the GameSpy Presence SDK documentation for more information
**********************************************************************/

#ifndef _GPI_H_
#define _GPI_H_

//INCLUDES
//////////
#include "../common/gsCommon.h"
#include "../common/gsAvailable.h"
#include "../hashtable.h"
#include "../darray.h"
#include "../md5.h"
#include "gp.h"

// Extended message support
#define GPI_NEW_AUTH_NOTIFICATION	(1<<0)
#define GPI_NEW_REVOKE_NOTIFICATION (1<<1)
#define GPI_SDKREV (1<<0 | 1<<1)

//TYPES
///////
// Boolean.
///////////
typedef enum _GPIBool
{
	GPIFalse,
	GPITrue
} GPIBool;

#include "gpiUtility.h"
#include "gpiCallback.h"
#include "gpiOperation.h"
#include "gpiConnect.h"
#include "gpiBuffer.h"
#include "gpiInfo.h"
#include "gpiProfile.h"
#include "gpiPeer.h"
#include "gpiSearch.h"
#include "gpiBuddy.h"
#include "gpiTransfer.h"
#include "gpiUnique.h"

// Connection data.
///////////////////
typedef struct
{
  char errorString[GP_ERROR_STRING_LEN];
  GPIBool infoCaching;
  GPIBool infoCachingBuddyOnly;
  GPIBool simulation;
  GPIBool firewall;
  char nick[GP_NICK_LEN];
  char uniquenick[GP_UNIQUENICK_LEN];
  char email[GP_EMAIL_LEN];
  char password[GP_PASSWORD_LEN];
  int sessKey;
  int userid;
  int profileid;
  int partnerID;
  GPICallback callbacks[GPI_NUM_CALLBACKS];
  SOCKET cmSocket;
  int connectState;
  GPIBuffer socketBuffer;
  char * inputBuffer;
  int inputBufferSize;
  GPIBuffer outputBuffer;
  SOCKET peerSocket;
  int peerPort;
  int nextOperationID;
  int numSearches;
  GPEnum lastStatus;
  char lastStatusString[GP_STATUS_STRING_LEN];
  char lastLocationString[GP_LOCATION_STRING_LEN];
  GPErrorCode errorCode;
  GPIBool fatalError;
  FILE * diskCache;
  GPIOperation * operationList;
  GPIProfileList profileList;
  GPIPeer * peerList;
  GPICallbackData * callbackList;
  GPICallbackData * lastCallback;
  GPIBuffer updateproBuffer;
  GPIBuffer updateuiBuffer;
  DArray transfers;
  unsigned int nextTransferID;
  int productID;
  int namespaceID;
  char loginTicket[GP_LOGIN_TICKET_LEN];

#ifdef GSI_UNICODE
  unsigned short errorString_W[GP_ERROR_STRING_LEN];
  unsigned short nick_W[GP_NICK_LEN];
  unsigned short uniquenick_W[GP_UNIQUENICK_LEN];
  unsigned short email_W[GP_EMAIL_LEN];
  unsigned short password_W[GP_PASSWORD_LEN];
  unsigned short lastStatusString_W[GP_STATUS_STRING_LEN];
  unsigned short lastLocationString_W[GP_LOCATION_STRING_LEN];
#endif

} GPIConnection;

//FUNCTIONS
///////////
GPResult
gpiInitialize(
  GPConnection * connection,
  int productID,
  int namespaceID,
  int partnerID
);

void
gpiDestroy(
  GPConnection * connection
);

GPResult
gpiReset(
  GPConnection * connection
);

GPResult
gpiProcessConnectionManager(
  GPConnection * connection
);

GPResult
gpiProcess(
  GPConnection * connection,
  int blockingOperationID
);

GPResult
gpiEnable(
  GPConnection * connection, 
  GPEnum state
);

GPResult
gpiDisable(
  GPConnection * connection, 
  GPEnum state
);

#ifdef _DEBUG
void
gpiReport(
  GPConnection * connection,
  void (* report)(const char * output)
);
#endif

#endif
