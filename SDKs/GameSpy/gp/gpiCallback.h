/*
gpiCallback.h
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

#ifndef _GPICALLBACK_H_
#define _GPICALLBACK_H_

//INCLUDES
//////////
#include "gpi.h"

//DEFINES
/////////
// Unsolicited Callbacks.
/////////////////////////
#define GPI_ERROR                      GP_ERROR
#define GPI_RECV_BUDDY_REQUEST         GP_RECV_BUDDY_REQUEST
#define GPI_RECV_BUDDY_STATUS          GP_RECV_BUDDY_STATUS
#define GPI_RECV_BUDDY_MESSAGE         GP_RECV_BUDDY_MESSAGE
#define GPI_RECV_BUDDY_UTM             GP_RECV_BUDDY_UTM
#define GPI_RECV_GAME_INVITE           GP_RECV_GAME_INVITE
#define GPI_TRANSFER_CALLBACK          GP_TRANSFER_CALLBACK
#define GPI_RECV_BUDDY_AUTH            GP_RECV_BUDDY_AUTH
#define GPI_RECV_BUDDY_REVOKE          GP_RECV_BUDDY_REVOKE
#define GPI_NUM_CALLBACKS              9

// Add type - not 0 only for a few.
///////////////////////////////////
#define GPI_ADD_NORMAL                 0
#define GPI_ADD_ERROR                  1
#define GPI_ADD_MESSAGE                2
#define GPI_ADD_NICKS                  3
#define GPI_ADD_PMATCH                 4
#define GPI_ADD_STATUS                 5
#define GPI_ADD_BUDDDYREQUEST          6
#define GPI_ADD_TRANSFER_CALLBACK      7
#define GPI_ADD_REVERSE_BUDDIES        8
#define GPI_ADD_SUGGESTED_UNIQUE       9
#define GPI_ADD_BUDDYAUTH              10
#define GPI_ADD_BUDDYUTM               11
#define GPI_ADD_BUDDYREVOKE            12

//TYPES
///////
// A Callback.
//////////////
typedef struct
{
  GPCallback callback;
  void * param;
} GPICallback;

// Data for a pending callback.
///////////////////////////////
typedef struct GPICallbackData
{
	GPICallback callback;
	void * arg;
	int type;
	int operationID;
	struct GPICallbackData * pnext;
} GPICallbackData;

//FUNCTIONS
///////////
void
gpiCallErrorCallback(
  GPConnection * connection,
  GPResult result,
  GPEnum fatal
);

typedef struct GPIOperation_s *GPIOperation_st;

GPResult
gpiAddCallback(
  GPConnection * connection,
  GPICallback callback,
  void * arg,
  const struct GPIOperation_s * operation,
  int type
);

GPResult
gpiProcessCallbacks(
  GPConnection * connection,
  int blockingOperationID
);

#endif
