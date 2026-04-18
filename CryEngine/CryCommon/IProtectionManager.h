#pragma once 

#include "ProjectDefines.h"
#ifdef USING_LICENSE_PROTECTION


// All errors which may occur on license validation (To be handled in owning thread).
enum EPMErrorCode
{
	PMEC_NO_ERROR = 0,

	PMEC_CANT_WRITE_TO_SOCKET,
	PMEC_CANT_READ_FROM_SOCKET,
	PMEC_KEY_EXPIRED,
	PMEC_OLD_APP_VERSION,

	PMEC_KEY_NOT_FOUND,
	PMEC_INVALID_TIMEFRAME,
	PMEC_CONNECT_FAILED,
	PMEC_PROXY_CONNECT_FAILED,
	PMEC_KEY_INVALID
};


// Data which is filled partially on client side and completed from license server side.
struct SClientData
{
	string sKey;
	unsigned int iRemainingDays;
	string sVersion;

#ifdef USING_UNIKEY_SECURITY
	string sDongleHid;
	string sDongleExpireDate;
#endif // USING_UNIKEY_SECURITY
};


typedef void (*TProtectionCallback)();


/************************************************************************/
/* Handles Client-Server license protection				                      */
/************************************************************************/
struct IProtectionManager
{
public:
	// Determines the right of a client to use this software, depending on a license server connection.
	virtual EPMErrorCode	RetrieveClientData(SClientData &data) = 0;
	// Runs a thread, which constantly starts a callback for cleint validation.
	virtual void		StartLicenseVerificationThread(TProtectionCallback cb) = 0;
};


#endif // USING_LICENSE_PROTECTION
