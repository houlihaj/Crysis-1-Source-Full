#pragma once 

#include "ProjectDefines.h"
#ifdef USING_UNIKEY_SECURITY


/************************************************************************/
/* IUniKeyManager interfaces functions to the SDK licensing dongle      */
/************************************************************************/
struct IUniKeyManager
{
public:
	// Opens a read-write connection to the first connected dongle.
	virtual bool		Connect() = 0;
	// Closes connection and clears related context data.
	virtual void		Disconnect() = 0;
	// Clears data related to dongle communication.
	virtual void		ClearContext() = 0;

	// Returns hardware ID after successful dongle connection.
	virtual DWORD		GetHID() = 0;
	// Reads and decrypts specific data struct from dongle.
	virtual bool		LoadDongleState() = 0;
	// Checks current connection time against expire date, in dependence of previously read dongle data struct.
	virtual bool		IsValidTimeFrame() = 0;
	// Writes a new expire date to the dongle.
	virtual void		StoreExpireDate(const string& expireDate) = 0;
};


#endif // USING_UNIKEY_SECURITY
