#ifndef __ICTPENDPOINTLISTENER_H__
#define __ICTPENDPOINTLISTENER_H__

#pragma once

enum ECTPEndpointEvent
{
	eCEE_NoBlockingMessages = BIT(0),
	eCEE_SendingPacket = BIT(1),
	eCEE_BecomeAlerted = BIT(2),
	eCEE_BackoffTooLong = BIT(3),
};

struct SCTPEndpointEvent
{
	SCTPEndpointEvent( ECTPEndpointEvent evt ) : event(evt) {}
	ECTPEndpointEvent event;
};

struct ICTPEndpointListener
{
	virtual void OnEndpointEvent( const SCTPEndpointEvent& event ) = 0;
};

#endif
