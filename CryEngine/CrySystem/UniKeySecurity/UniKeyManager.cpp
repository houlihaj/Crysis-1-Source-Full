#include <stdafx.h>
#include "UniKeyManager.h"

#ifdef USING_UNIKEY_SECURITY


#include "ISystem.h"
#include "UniKey/UniKey.h"


#ifdef WIN64
	#pragma comment( lib, "../../SDKs/UniKey/x64/UniKey.lib" )
#endif
#ifdef WIN32
	// currently this lib causes some linking errors. 
	// doesn't matter so far as, we don't provide 32bit builds anymore
	#pragma comment( lib, "../../SDKs/UniKey/x86/UniKey.lib" )
#endif


//////////////////////////////////////////////////////////////////////////
CUniKeyManager::CUniKeyManager() :
	m_bIsConnected(false)
{
	ClearContext();
}


//////////////////////////////////////////////////////////////////////////
CUniKeyManager::~CUniKeyManager()
{
	m_bIsConnected = false;
	ClearContext();
}


//////////////////////////////////////////////////////////////////////////
void CUniKeyManager::ClearContext()
{
	m_p1 = m_p2 = m_p3 = m_p4 = 0;
	memset(&m_sDynamicData, 0, sizeof(SDynamicData));
}


//////////////////////////////////////////////////////////////////////////
bool CUniKeyManager::Connect()
{
	if (m_bIsConnected)
		Disconnect();

	m_p1 = 31128;
	m_p2 = 56235;
	m_p3 = 22924;
	m_p4 = 14016;
	if (!CallKey(UNIKEY_FIND))
		return false;

	m_dwHID = m_lp1;
	if (!CallKey(UNIKEY_LOGON))
		return false;

	m_bIsConnected = true;
	return true;
}


//////////////////////////////////////////////////////////////////////////
void CUniKeyManager::Disconnect()
{
	CallKey(UNIKEY_LOGOFF);
	ClearContext();
	m_bIsConnected = false;
}


//////////////////////////////////////////////////////////////////////////
DWORD CUniKeyManager::GetHID()
{
	return m_dwHID;
}


//////////////////////////////////////////////////////////////////////////
bool CUniKeyManager::CallKey(WORD function)
{
	m_dwLastError = UniKey(function, &m_wHandle, &m_lp1, &m_lp2, &m_p1, &m_p2, &m_p3, &m_p4, m_pBuffer);
	return m_dwLastError==0;
}


//////////////////////////////////////////////////////////////////////////
bool CUniKeyManager::IsValidTimeFrame()
{
	time_t now = time(0);
	time_t nowThreshold = now + 24*60*60; // adding threshold of one day

	// Has time been set back comparing to last usage? We don't like that, sorry.
	if (m_sDynamicData.lastAccessDate>0 && nowThreshold<m_sDynamicData.lastAccessDate)
	{
		m_sDynamicData.lastTimeTamper = now;
		StoreData();
		return false;
	}

	// Store this time as last access date.
	if (m_sDynamicData.lastAccessDate<now)
	{
		m_sDynamicData.lastAccessDate = now;
		StoreData();
	}

	// This date cannot be exceeded by using the dongle.
	if (nowThreshold>m_sDynamicData.expireDate)
		return false;

	return true;
}


//////////////////////////////////////////////////////////////////////////
bool CUniKeyManager::LoadDongleState()
{
	return ReadData()!=NULL;
}


//////////////////////////////////////////////////////////////////////////
char* CUniKeyManager::ReadData()
{
	if (!m_bIsConnected)
		return false;

	bool success = true;
	int dynLen = sizeof(SDynamicData);
	int decDynLen = (dynLen/8+1)*8;

	m_p1 = 0;
	m_p2 = decDynLen;
	success &= CallKey(UNIKEY_READ_MEMORY);
	if (!success)
		return NULL;

	m_lp1 = decDynLen;
	m_lp2 = 1;
	success &= CallKey(UNIKEY_DECRYPT);
	if (!success)
		return NULL;

	memcpy(&m_sDynamicData, m_pBuffer, dynLen);

	if (m_sDynamicData.header!='DATA' || m_sDynamicData.headerVersion<(HIWORD(1)+LOWORD(0)))
		return NULL;

	DWORD len = m_sDynamicData.privateDataLength;
	DWORD decLen = len==0 ? 0 : (len/8+1)*8;

	m_p1 = decDynLen;
	m_p2 = decLen;
	success &= CallKey(UNIKEY_READ_MEMORY);

	m_lp1 = decLen;
	m_lp2 = 1;
	success &= CallKey(UNIKEY_DECRYPT);

	return success ? (char*)m_pBuffer : NULL;
}


//////////////////////////////////////////////////////////////////////////
bool CUniKeyManager::StoreData(const char* source)
{
	if (!m_bIsConnected)
		return false;

	bool success = true;
	int dynLen = sizeof(SDynamicData);
	int encDynLen = (dynLen/8+1)*8;

	DWORD len = 0;
	DWORD encLen = 0;
	if (source!=NULL)
	{
		len = (DWORD)strlen(source)+1;
		encLen = (len/8+1)*8;
		m_sDynamicData.privateDataLength = len;
	}

	m_sDynamicData.header = 'DATA';
	m_sDynamicData.headerVersion = HIWORD(1)+LOWORD(0);

	memcpy(m_pBuffer, &m_sDynamicData, dynLen);
	memcpy(&m_pBuffer[encDynLen], source, len);

	m_lp1 = encDynLen + encLen;
	m_lp2 = 1;
	success &= CallKey(UNIKEY_ENCRYPT);

	m_p1 = 0;
	m_p2 = encDynLen + encLen;
	success &= CallKey(UNIKEY_WRITE_MEMORY);

	return success;
}


//////////////////////////////////////////////////////////////////////////
void CUniKeyManager::StoreExpireDate(const string& expireDate)
{
	const char* ed = expireDate.c_str();
	tm timeInfo;
	memset(&timeInfo,0,sizeof(tm));

	timeInfo.tm_year = ((ed[0]-'0')*1000+(ed[1]-'0')*100+(ed[2]-'0')*10+(ed[3]-'0')) - 1900;
	timeInfo.tm_mon = ((ed[5]-'0')*10+(ed[6]-'0')) - 1;
	timeInfo.tm_mday = (ed[8]-'0')*10+(ed[9]-'0');
	time_t tt = mktime(&timeInfo);

	if (tt!=m_sDynamicData.expireDate)
	{
		m_sDynamicData.expireDate = tt;
		StoreData();

		MessageBox( 0, "Your Dongle has been updated to validate the offline usage of Sandbox until " + FormatTime("Y/m/d", tt) + ".", 
			"Dongle Updated", MB_ICONINFORMATION | MB_OK | MB_APPLMODAL | MB_TOPMOST);
	}
}


//////////////////////////////////////////////////////////////////////////
string CUniKeyManager::FormatTime(const string &format, const time_t& rawTime)
{
	tm *timeInfo = localtime( &rawTime );
	string formatCpy = format;
	string result;

	for (int i=0;i<formatCpy.length();i++)
	{
		char ch = formatCpy.c_str()[i];

		if (ch=='Y')
		{
			int year = 1900+timeInfo->tm_year;
			char buf[5] = {year/1000+'0', (year/100)%10+'0', (year/10)%10+'0', year%10+'0', 0};
			result += (char*)&buf;
		}
		else if (ch=='m')
		{
			int mon = 1+timeInfo->tm_mon;
			char buf[3] = {mon/10+'0', mon%10+'0', 0};
			result += (char*)&buf;
		}
		else if (ch=='d')
		{
			char buf[3] = {timeInfo->tm_mday/10+'0', timeInfo->tm_mday%10+'0', 0};
			result += (char*)&buf;
		}
		else if (ch=='H')
		{
			char buf[3] = {timeInfo->tm_hour/10+'0', timeInfo->tm_hour%10+'0', 0};
			result += (char*)&buf;
		}
		else if (ch=='i')
		{
			char buf[3] = {timeInfo->tm_min/10+'0', timeInfo->tm_min%10+'0', 0};
			result += (char*)&buf;
		}
		else if (ch=='s')
		{
			char buf[3] = {timeInfo->tm_sec/10+'0', timeInfo->tm_sec%10+'0', 0};
			result += (char*)&buf;
		}
		else
			result += ch;
	}

	return result;
}


#endif // USING_UNIKEY_SECURITY
