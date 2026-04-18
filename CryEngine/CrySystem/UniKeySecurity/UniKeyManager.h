#pragma once

#include "ProjectDefines.h"
#ifdef USING_UNIKEY_SECURITY

#include "UniKeySecurity/IUniKeyManager.h"


// UniKey properties
#define UNIKEY_BUFFER_SIZE				4096


struct SDynamicData
{
	DWORD		header;
	DWORD		headerVersion;

	DWORD		privateDataLength;
	time_t	expireDate;
	time_t	lastAccessDate;
	time_t	lastTimeTamper;

	DWORD		reserved[8];
};


// For interface description, see IProtectionManager
class CUniKeyManager : IUniKeyManager
{
public:
	CUniKeyManager();
	~CUniKeyManager();

	TAGES_EXPORT virtual bool			Connect();
	TAGES_EXPORT virtual void			Disconnect();
	TAGES_EXPORT virtual void			ClearContext();

	TAGES_EXPORT virtual DWORD		GetHID();
	TAGES_EXPORT virtual bool			LoadDongleState();
	TAGES_EXPORT virtual bool			IsValidTimeFrame();
	TAGES_EXPORT virtual void			StoreExpireDate(const string& expireDate);

private:
	// Wraps call to UniKey interface.
	TAGES_EXPORT bool							CallKey(WORD function);
	// Reads and decrypts SDynamicData from dongle. Returns the dongles 16 Byte GUID.
	TAGES_EXPORT char* 						ReadData();
	// Encrypts and writes SDynamicData to dongle.
	TAGES_EXPORT bool							StoreData(const char* source=NULL);

	// Generates a string from a timt_t type, depending on standard DateTime notation.
	TAGES_EXPORT string						FormatTime(const string &format, const time_t& rawTime);

private:
	DWORD													m_dwHID;
	SDynamicData									m_sDynamicData;

	DWORD													m_dwLastError;
	WORD													m_wHandle, m_p1, m_p2, m_p3, m_p4;
	BYTE													m_pBuffer[UNIKEY_BUFFER_SIZE];
	DWORD													m_lp1, m_lp2;

	string												m_sDongleState;
	bool													m_bIsConnected;
};


#endif // USING_UNIKEY_SECURITY
