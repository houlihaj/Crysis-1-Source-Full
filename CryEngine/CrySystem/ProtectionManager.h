#pragma once

#include "ProjectDefines.h"
#ifdef USING_LICENSE_PROTECTION


#include "IProtectionManager.h"
#include <winsock2.h>


// For interface description, see IProtectionManager
class CProtectionManager : IProtectionManager
{
public:
	CProtectionManager();
	~CProtectionManager();

	TAGES_EXPORT virtual EPMErrorCode	RetrieveClientData(SClientData &data);
	TAGES_EXPORT virtual void		StartLicenseVerificationThread(TProtectionCallback cb);

private:

	// System information

	TAGES_EXPORT bool						GetKeyFromRegistry(string& sKey);
	TAGES_EXPORT bool						GetLocalIPAddress(string& ip);
	TAGES_EXPORT bool						GetLocalMacAddress(string& mac);

	// Helpers for simple cyphering

	TAGES_EXPORT string					Decrypt(const string &str);
	TAGES_EXPORT string					Encrypt(const string &str);

	// extract value from a string of format 'key=value;'.
	TAGES_EXPORT string					GetValue(const string& source, const string& key);

	TAGES_EXPORT int						HexDec(unsigned char val);
	TAGES_EXPORT string					DecHex(int val);

	// Server communication

	TAGES_EXPORT int						ConnectToLicenseServer();
	TAGES_EXPORT void						CloseLicenseServerConnection();
	TAGES_EXPORT EPMErrorCode		RegisterClientWithKey(SClientData& data, const string& ip, const string& mac);

private:
	SOCKET		m_ce2ServerSocket;
};


#endif // USING_LICENSE_PROTECTION
