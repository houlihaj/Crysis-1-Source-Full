#ifndef _AISIGNAL_H_
#define _AISIGNAL_H_

#if _MSC_VER > 1000
#pragma once
#endif

#include "IAISystem.h"
#include <ISerialize.h>

struct AISignalExtraData : public IAISignalExtraData
{
	AISignalExtraData();
	AISignalExtraData( const AISignalExtraData& other ) : sObjectName(0) { *this = other; }
	virtual ~AISignalExtraData();

	AISignalExtraData& operator = ( const AISignalExtraData& other );
	virtual void Serialize( TSerialize ser );

	virtual const char* GetObjectName() const { return sObjectName ? sObjectName : ""; }
	virtual void SetObjectName(const char* objectName);

private:
	char* sObjectName;
};

#endif