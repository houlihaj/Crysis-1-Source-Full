#include <stdafx.h>

#include "ProtectionManager.h"
#ifdef USING_LICENSE_PROTECTION


#include "ISystem.h"
#include "IConsole.h"
#include "IEngineSettingsManager.h"
#include <Iphlpapi.h>

#ifdef USING_UNIKEY_SECURITY
#include "UniKeySecurity/IUniKeyManager.h"
#endif // USING_UNIKEY_SECURITY


#pragma comment ( lib, "Iphlpapi.lib" )
#pragma comment( lib, "ws2_32.lib" )


#define LICENSE_SERVER_IP "80.237.158.14"


//////////////////////////////////////////////////////////////////////////
CProtectionManager::CProtectionManager() :
	m_ce2ServerSocket(-1)
{
}


//////////////////////////////////////////////////////////////////////////
CProtectionManager::~CProtectionManager()
{
	CloseLicenseServerConnection();
}


//////////////////////////////////////////////////////////////////////////
bool CProtectionManager::GetKeyFromRegistry(string& sKey)
{
	IEngineSettingsManager* pEsm = gEnv->pSystem->GetEngineSettingsManager();
	if (!pEsm->HasKey("EDT_InstanceKey"))
		return false;

	pEsm->GetValueByRef("EDT_InstanceKey",sKey,true);
	return true;
}


//////////////////////////////////////////////////////////////////////////
int CProtectionManager::ConnectToLicenseServer()
{
	string ip = LICENSE_SERVER_IP;
	int port = 80;

	ICVar* pIPCVar = gEnv->pConsole->GetCVar("net_proxy_ip");
	if (pIPCVar && strlen(pIPCVar->GetString())>0)
		ip = pIPCVar->GetString();
	ICVar* pPortCVar = gEnv->pConsole->GetCVar("net_proxy_port");
	if (pPortCVar && pPortCVar->GetIVal())
		port = pPortCVar->GetIVal();

	SOCKADDR_IN srvAddr;

	/*hostent* myhost = gethostbyname(LICENSE_SERVER_URL); 
	sockaddr_in* addr;
	char **p = myhost -> h_addr_list;
	for (;;) {
		if (*p == 0)
			break;
		srvAddr.sin_addr.s_addr = *reinterpret_cast <unsigned long*> (*p);
		++ p;
	}*/

	srvAddr.sin_addr.s_addr		= inet_addr(ip);
	srvAddr.sin_port					=	htons(port);
	srvAddr.sin_family				=	AF_INET;

	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0)
		return 1;

	if ((m_ce2ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
		return 2;

	if(connect(m_ce2ServerSocket, (struct sockaddr *)&srvAddr, sizeof(srvAddr)) == -1)
		return 3;

	return 0;
}


//////////////////////////////////////////////////////////////////////////
void CProtectionManager::CloseLicenseServerConnection()
{
	if (m_ce2ServerSocket!=-1)
		closesocket(m_ce2ServerSocket);
	m_ce2ServerSocket = -1;
}


//////////////////////////////////////////////////////////////////////////
EPMErrorCode CProtectionManager::RetrieveClientData(SClientData &data)
{
	if (!GetKeyFromRegistry(data.sKey))
		return PMEC_KEY_NOT_FOUND;

	bool bOverruleServerConnectError = false;
	bool bIsValidTimeFrame = true;

#ifdef USING_UNIKEY_SECURITY
	data.sDongleHid.clear();
	data.sDongleExpireDate.clear();

	IUniKeyManager* pUkm = gEnv->pSystem->GetUniKeyManager();
	if (pUkm->Connect())
	{
		char hidBuf[11];
		itoa(pUkm->GetHID(), hidBuf, 10);
		data.sDongleHid = (char*)&hidBuf;

		if (pUkm->LoadDongleState())
		{
			bOverruleServerConnectError = true;
			bIsValidTimeFrame = pUkm->IsValidTimeFrame();
		}

		pUkm->Disconnect();
	}
#endif // USING_UNIKEY_SECURITY

	string ip, mac;
	if (ConnectToLicenseServer()!=0 || !GetLocalIPAddress(ip) || !GetLocalMacAddress(mac))
	{
		if (!bIsValidTimeFrame)
			return PMEC_INVALID_TIMEFRAME;

		if (bOverruleServerConnectError)
			return PMEC_NO_ERROR;

		return PMEC_CONNECT_FAILED;
	}

	// registers client at server and retrieves information from license server
	EPMErrorCode ret = RegisterClientWithKey(data, ip, mac);
	CloseLicenseServerConnection();

	if (ret!=PMEC_NO_ERROR)
		return ret;

#ifdef USING_UNIKEY_SECURITY
	if (!data.sDongleHid.empty() && !data.sDongleExpireDate.empty() && pUkm->Connect())
	{
		pUkm->LoadDongleState();
		pUkm->StoreExpireDate(data.sDongleExpireDate);
		pUkm->Disconnect();
	}
#endif // USING_UNIKEY_SECURITY
	
	return PMEC_NO_ERROR;
}


//////////////////////////////////////////////////////////////////////////
bool CProtectionManager::GetLocalIPAddress(string& ip)
{
	char ac[80];
	if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR)
		return false;

	struct hostent *phe = gethostbyname(ac);
	if (phe == 0)
		return false;

	for (int i = 0; phe->h_addr_list[i] != 0; ++i) 
	{
		struct in_addr addr;
		memcpy(&addr, phe->h_addr_list[i], sizeof(struct in_addr));
		ip = inet_ntoa(addr);
		if (ip!="127.0.0.1")
			return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
bool CProtectionManager::GetLocalMacAddress(string& mac)
{
	PIP_ADAPTER_INFO pAdapterInfo = new IP_ADAPTER_INFO;
	PIP_ADAPTER_INFO pAdapter = NULL;
	DWORD dwBufLen = sizeof(IP_ADAPTER_INFO);

	if (GetAdaptersInfo(pAdapterInfo, &dwBufLen)==ERROR_BUFFER_OVERFLOW)
	{
		delete pAdapterInfo;
		pAdapterInfo = (IP_ADAPTER_INFO*)new char[dwBufLen];
	}

	if (GetAdaptersInfo(pAdapterInfo, &dwBufLen)==NO_ERROR) 
	{
		pAdapter = pAdapterInfo;				
		while (pAdapter)
		{
			mac = "";
			for (int i=0;i<pAdapter->AddressLength;i++)
				mac += DecHex(pAdapter->Address[i]) + (i+1<pAdapter->AddressLength?":":"");
			pAdapter = pAdapter->Next;
		}
		if (mac.length()>0)
			return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
TAGES_EXPORT unsigned long ThreadStart(LPVOID data)
{
	TProtectionCallback pProtectionCallback = ((TProtectionCallback)data);

	while(true)
	{
		pProtectionCallback();
		Sleep(5000);
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////////
void CProtectionManager::StartLicenseVerificationThread(TProtectionCallback cb)
{
	DWORD dwThreadID;
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ThreadStart, cb, 0, &dwThreadID);
}


//////////////////////////////////////////////////////////////////////////
int CProtectionManager::HexDec(unsigned char val)
{
	return val<'a'?val-'0':val-'a'+10;
}


//////////////////////////////////////////////////////////////////////////
string CProtectionManager::DecHex(int val)
{
	int lo = val%16;
	int hi = val/16;
	return string(hi<10?hi+'0':hi-10+'a')+string(lo<10?lo+'0':lo-10+'a');
}


//////////////////////////////////////////////////////////////////////////
string CProtectionManager::Decrypt(const string &str)
{
	string resultString = "";

	for(int i=0;i<strlen(str);i+=4)
	{
		int seed = HexDec(str[i])*16+HexDec(str[i+1]);
		unsigned char dec = ((HexDec(str[i+2])*16+HexDec(str[i+3])) + (256-seed)) % 256;
		resultString += dec;
	}
	return resultString;
}


//////////////////////////////////////////////////////////////////////////
string CProtectionManager::Encrypt(const string &str)
{
	srand(GetCurrentTime());		
	string resultString = "";

	for(int i=0;i<strlen(str);i++)
	{
		int seed = rand()%256;
		resultString += DecHex(seed);
		int dec = (str[i]+seed) % 256;
		resultString += DecHex(dec);
	}
	return resultString;
}



//////////////////////////////////////////////////////////////////////////
string CProtectionManager::GetValue(const string& source, const string& key)
{
	string subSource = source;

	int tagBegin = source.find(key);
	if (tagBegin<0)
		return "";

	subSource = source.substr(tagBegin);
	int tagMiddle = subSource.find("=");
	if (tagMiddle<0)
		return "";	

	subSource = subSource.substr(tagMiddle+1);
	int tagEnd = subSource.find(";");
	if (tagEnd<0)
		return "";

	return subSource.substr(0, tagEnd);
}


static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


string Base64Encode(const char* pBytesToEncode) 
{
	unsigned int in_len = strlen(pBytesToEncode);

	string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while (in_len--) 
	{
		char_array_3[i++] = (unsigned char)*(pBytesToEncode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for(i = 0; (i <4) ; i++)
				ret += base64_chars[char_array_4[i]];
			i = 0;
		}
	}

	if (i)
	{
		for(j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];

		while((i++ < 3))
			ret += '=';

	}

	return ret;
}


//////////////////////////////////////////////////////////////////////////
EPMErrorCode CProtectionManager::RegisterClientWithKey(SClientData &data, const string& ip, const string& mac)
{
	int bytes;
	string received;
	char bBuffer[256];

	ICVar* pIPCVar = gEnv->pConsole->GetCVar("net_proxy_ip");

	if (pIPCVar && strlen(pIPCVar->GetString())>0) // user proxy
	{
		string sAuthUser;
		string sAuthPass;

		ICVar* pUserCVar = gEnv->pConsole->GetCVar("net_proxy_user");
		if (pUserCVar && strlen(pUserCVar->GetString())>0)
			sAuthUser = pUserCVar->GetString();
		ICVar* pPassCVar = gEnv->pConsole->GetCVar("net_proxy_pass");
		if (pPassCVar && strlen(pPassCVar->GetString())>0)
			sAuthPass = pPassCVar->GetString();

		string sUserPass = Base64Encode(sAuthUser+":"+sAuthPass);

		string sProxyAuthorization = "";
		if (sAuthUser!="")
			sProxyAuthorization = "Proxy-authorization: Basic "+sUserPass+"\r\n";

		string sProxyConnect = string("CONNECT ")+LICENSE_SERVER_IP+":80 HTTP/1.0\r\n";
		string sProxyPieces = sProxyConnect + sProxyAuthorization + "\r\n";

		bytes = send(m_ce2ServerSocket, sProxyPieces.c_str(), sProxyPieces.length(), 0);
		if(bytes == -1)
			return PMEC_CANT_WRITE_TO_SOCKET;

		while ((bytes = recv(m_ce2ServerSocket, bBuffer, sizeof(bBuffer) - 1, 0))>0)
		{
			bBuffer[bytes] = 0;
			received += (char*)&bBuffer;
			if (bytes<sizeof(bBuffer)-1)
				break;		
		}

		int iter = 0;
		while (received.c_str()[iter]!=32 && iter<bytes) iter++;
		if (iter==bytes || received.find("200")!=iter+1)
			return PMEC_PROXY_CONNECT_FAILED;
	}

	string sParameters = "key=" + data.sKey + "&ip=" + ip + "&mac=" + mac;
#ifdef USING_UNIKEY_SECURITY
	if (!data.sDongleHid.empty())
		sParameters += "&hid=" + data.sDongleHid;
#endif // USING_UNIKEY_SECURITY

	string sReqest = "GET /request.php?r="+Encrypt(sParameters)+" HTTP/1.0\nHost: "+LICENSE_SERVER_IP+"\n\n";
	bytes = send(m_ce2ServerSocket, sReqest.c_str(), sReqest.length(), 0);
	if(bytes == -1)
		return PMEC_CANT_WRITE_TO_SOCKET;

	while ((bytes = recv(m_ce2ServerSocket, bBuffer, sizeof(bBuffer) - 1, 0))>0)
	{
		bBuffer[bytes] = '\0';
		received += (char*)&bBuffer;
		if (bytes<sizeof(bBuffer)-1)
			break;	
	}

	if (received.length()==0)
		return PMEC_CANT_READ_FROM_SOCKET;

	string sReply = Decrypt(GetValue(received, "result"));

	if (GetValue(sReply, "Pass")=="1")
	{
#ifdef USING_UNIKEY_SECURITY
		data.sDongleExpireDate = GetValue(sReply,"DongleExpire");
#endif // USING_UNIKEY_SECURITY

		// extract remaining days for this company
		string sRemainingDays = GetValue(sReply,"RemainingDays");
		int res = 0;
		for (int i=0;i<sRemainingDays.length();i++)
			res = res*10 + (sRemainingDays[i]-48);
		data.iRemainingDays = res;

		string sCurrentVersion = GetValue(sReply,"CurrentVersion");

		if (data.iRemainingDays==0)
			return PMEC_KEY_EXPIRED;
		if (data.sVersion!=sCurrentVersion)
		{
			data.sVersion = sCurrentVersion;
			return PMEC_OLD_APP_VERSION;
		}
		
		return PMEC_NO_ERROR;
	}

	return PMEC_KEY_INVALID;
}


#endif // USING_LICENSE_PROTECTION
