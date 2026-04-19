// Password Reminder Module:
// Implementation
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Includes
#include "stdafx.h"
#include "GameSpy/ghttp/ghttp.h"
#include "PasswordReminder.h"
//#include "../nonport.h"

//using namespace std;
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Globals/Types
const int HTTP_BUFFER_SIZE = (16384);
const char *URL_UNSAFE_CHARS = "#<>\"%{}|\\^~[]`";
enum EncodingMethod
{
	ENCODE_NONE,
	ENCODE_B64SAFE
};

enum EncrytMethod
{
	ENCRYPT_NONE,
	ENCRYPT_XXTEA
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static GHTTPBool EncCompleteCallback(GHTTPRequest request, 
	GHTTPResult result, char * buffer, GHTTPByteCount bufferLen, void * param)
{
	char *objectBuffer = static_cast<char *>(param);
	if (!objectBuffer)
		return GHTTPTrue;

	switch(result)
	{
		case GHTTPSuccess:
			strncpy(objectBuffer, buffer, HTTP_BUFFER_SIZE);
			break;
		case GHTTPOutOfMemory: 
			strcpy(objectBuffer, "Out of Memory");
			break;
		case GHTTPBufferOverflow:
			strcpy(objectBuffer, "Buffer overflow");
			break;
		case GHTTPParseURLFailed:
			strcpy(objectBuffer, "Failed to Parse URL");
			break;
		case GHTTPHostLookupFailed:
			strcpy(objectBuffer, "Host lookup Failed");
			break;
		case GHTTPSocketFailed:
			strcpy(objectBuffer, "Socket Init Failed");
			break;
		case GHTTPConnectFailed:
			strcpy(objectBuffer, "Connection Failed");
			break;
		case GHTTPBadResponse: 
			strcpy(objectBuffer, "Bad Responce");
			break;
		case GHTTPRequestRejected:
			strcpy(objectBuffer, "Host Rejected Request");
			break;
		case GHTTPUnauthorized:
			strcpy(objectBuffer, "Not Authorized for Request");
			break;
		case GHTTPForbidden:
			strcpy(objectBuffer, "Forbidden!");
			break;
		case GHTTPFileNotFound:
			strcpy(objectBuffer, "File not Found");
			break;
		case GHTTPServerError:
			strcpy(objectBuffer, "Server Error");
			break;
		case GHTTPFileReadFailed:
			strcpy(objectBuffer, "File read Failed");
			break;
		case GHTTPFileIncomplete:
			strcpy(objectBuffer, "File Incomplete");
			break;
		case GHTTPFileToBig:
			strcpy(objectBuffer, "File is Too Big!");
			break;
		default: 
			strcpy(objectBuffer, "Unknown Error!");
			break;
	}
	return GHTTPTrue;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static GHTTPBool SendCompleteCallback(GHTTPRequest request, 
	GHTTPResult result, char * buffer, GHTTPByteCount bufferLen, void * param)
{
	char *objectBuffer = static_cast<char *>(param);
	if (!objectBuffer)
		return GHTTPTrue;

	switch(result)
	{
		case GHTTPSuccess:
			strncpy(objectBuffer, buffer, HTTP_BUFFER_SIZE);
			break;
		case GHTTPOutOfMemory: 
			strcpy(objectBuffer, "Out of Memory");
			break;
		case GHTTPBufferOverflow:
			strcpy(objectBuffer, "Buffer overflow");
			break;
		case GHTTPParseURLFailed:
			strcpy(objectBuffer, "Failed to Parse URL");
			break;
		case GHTTPHostLookupFailed:
			strcpy(objectBuffer, "Host lookup Failed");
			break;
		case GHTTPSocketFailed:
			strcpy(objectBuffer, "Socket Init Failed");
			break;
		case GHTTPConnectFailed:
			strcpy(objectBuffer, "Connection Failed");
			break;
		case GHTTPBadResponse: 
			strcpy(objectBuffer, "Bad Responce");
			break;
		case GHTTPRequestRejected:
			strcpy(objectBuffer, "Host Rejected Request");
			break;
		case GHTTPUnauthorized:
			strcpy(objectBuffer, "Not Authorized for Request");
			break;
		case GHTTPForbidden:
			strcpy(objectBuffer, "Forbidden!");
			break;
		case GHTTPFileNotFound:
			strcpy(objectBuffer, "File not Found");
			break;
		case GHTTPServerError:
			strcpy(objectBuffer, "Server Error");
			break;
		case GHTTPFileReadFailed:
			strcpy(objectBuffer, "File read Failed");
			break;
		case GHTTPFileIncomplete:
			strcpy(objectBuffer, "File Incomplete");
			break;
		case GHTTPFileToBig:
			strcpy(objectBuffer, "File is Too Big!");
			break;
		default: 
			strcpy(objectBuffer, "Unknown Error!");
			break;
	}
	return GHTTPTrue; 
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
PasswordReminder::PasswordReminder()
{
	m_EmailAddr =
	m_EncAddress =
	m_SendAddress =
	m_Key =
	m_ErrorString =
	m_ResultString = 
	m_Cookie = "";
	
	m_EncodeMethod = 
	m_EncryptMethod = -1;
	
	m_Hardwired = false;
	ghttpStartup();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
PasswordReminder::~PasswordReminder()
{
	ghttpCleanup();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Initialize: used to initialize the password reminder 
// params:
// hardwire - if using hardwired values
// key - if using hardwired values, key is copied, otherwise ignored
// encryptingMethod - the method used to encrypt, can be 0,1,2, 7 for any
//		only 0, 1, 7 are supported for now, others can be easily be added
//		used to tell what algorithm to encrypt email with
// encodeingMethod - the method used to encode encrypted or normal emails
bool PasswordReminder::Initialize(bool hardwire, const string &key,
	int encryptingMethod, int encodingMethod)
{
	// set the hardwire flag for future functions
	m_Hardwired = hardwire;
	
	// use a hardwired cookie and key 
	if (hardwire)
	{
		if (key.empty())
		{
			SetError("Error: emtpy key");
			return false;
		}
		m_Cookie = ".";
		m_Key = key;
	}

	// check that we have good values for encrypting 
	// and encoding methods 
	// save them if they are good
	if (encodingMethod < 0 || encryptingMethod < 0)
	{
		SetError("Error: Bad encoding method or encrypting method\n"
				 "Please check your method selections");
		return false;
	}
	if (encodingMethod > 7 || encryptingMethod > 7)
	{
		SetError("Error: Bad encoding method or encrypting method\n"
				 "Please check your method selections");
		return false;
	}
	/*
	if (encodingMethod == 0 && encryptingMethod == 1)
	{
		SetError("Error: Bad encoding method and encrypting method\n"
				 "Encryption cannot be done by itself");
		return false;
	}
	*/
	m_EncodeMethod = encodingMethod;
	m_EncryptMethod = encryptingMethod;

	return true;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// RequestMethod should only be called when accessing the Enc.aspx page
// params:
// none
//
bool PasswordReminder::RequestMethod()
{
	char temp[33];
	char httpBuffer[HTTP_BUFFER_SIZE];
	
	// Check that we are not using hardwired values 
	// even though this function shouldn't even be called 
	// for hardwired values
	if (m_Hardwired)
	{
		SetError("Error: Using Request method when hardwired");
		return false;
	}
 
	// put together the query string sent to 
	// the asp page "Enc.aspx"
	m_EncAddress = EncAspAddress;
	m_EncAddress += "cry=";
	m_EncAddress += itoa(m_EncryptMethod, temp, 10);
	memset(&temp[0], 0, 33);
	m_EncAddress += "&cod=";
	m_EncAddress += itoa(m_EncodeMethod, temp, 10);

	// fill in an http buffer that is passed to a get request
	// query the page for encode and encrypt methods
	memset(&httpBuffer[0], 0, HTTP_BUFFER_SIZE);
	GHTTPRequest encRequest = ghttpGet(m_EncAddress.c_str(), GHTTPTrue, 
							EncCompleteCallback, (void *)httpBuffer);
	
	// Check for empty buffer!
	string  bufferCopy(httpBuffer?httpBuffer:"");
	if (bufferCopy.empty())
	{
		SetError("Error: empty buffer received from query to Enc.aspx");
		return false;
	}

	// Fill in parameters for send query based on buffer	
	char *tokenizer = strtok(const_cast<char*>(bufferCopy.data()), " \r\n");
	if (strcmp(tokenizer, "OK") != 0)
	{
		SetError("Error: bad buffer received from query to Enc.aspx\n"
				 "Message did not contain 'OK'");
		return false;
	}
	
	m_Cookie = strtok(NULL, " \r\n");
	m_EncryptMethod = atoi(strtok(NULL, " \r\n"));
	m_EncodeMethod = atoi(strtok(NULL, " \r\n"));
	
	// Are we encrypting? copy the key if so
	if (m_EncryptMethod != 0)
		m_Key = strtok(NULL, " \r\n");
	
	m_ResultString = tokenizer;
	//clean the http buffer copy
	//delete[]  bufferCopy;
	//bufferCopy = NULL;
	tokenizer = NULL;
	
	return true;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool PasswordReminder::EncryptEncode()
{
	string encrypEncEmail = m_EmailAddr;
	if (!m_Key.empty())
	{
		char key[XXTEA_KEY_SIZE];
		int lengthOutput = 0;
		memset(key, '\0', XXTEA_KEY_SIZE);
		switch(m_EncryptMethod)
		{
			case ENCRYPT_NONE:
				break;
			case ENCRYPT_XXTEA:
			{				
				strncpy(key, m_Key.c_str(), XXTEA_KEY_SIZE);
				char *temp = gsXxteaEncrypt(m_EmailAddr.c_str(), 
					m_EmailAddr.length(), key, &lengthOutput);
				encrypEncEmail = temp;
				delete[] temp;
				temp = NULL;
				break;
			}
			default: 
			{
				SetError("Error: encryption algorithm not supported");
				return false;
			}
		}
	}

	const int eLength = 128;
	char output[eLength];
	memset(&output[0], 0, eLength);
	char *encOut = &output[0];
	switch(m_EncodeMethod)
	{
		case ENCODE_NONE:
			break;
		case ENCODE_B64SAFE:
		{
			B64Encode(encrypEncEmail.c_str(), encOut, encrypEncEmail.length(), 
				2);
			encrypEncEmail = output;
			break;
		}
		default:
		{	
			SetError("Error: encoding algorithm not supported");
			return false;
		}
	}
	m_EmailAddr = encrypEncEmail;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool PasswordReminder::UrlEscape(string &urlPart)
{
	string urlSafeString = "";
	
	if (urlPart.empty())
		return false;
	// Bad characters which should not be printed!
	// 0x00 - 0x1F, 0x7F, 0x80 - 0xFF 
	unsigned char checkCh;
	char dummy[33];
	for (int i = 0; i < urlPart.length(); i++)
	{
		checkCh = urlPart[i];
		if ((checkCh < 0x1F) || checkCh >= 0x7F)
		{
			urlSafeString += "%";
			sprintf(dummy, "%2X",(int)checkCh);
			urlSafeString += dummy[0];
			urlSafeString += dummy[1];
		}
		else if (strchr(URL_UNSAFE_CHARS, checkCh) != NULL)
		{			
			urlSafeString += "%";
			sprintf(dummy, "%2X",(int)checkCh);
			urlSafeString += dummy[0];
			urlSafeString += dummy[1];
		}
		else urlSafeString += checkCh;
	}
	urlPart = urlSafeString;
	return true;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// SendReminderRequest: used to send the request to Send.aspx for password
// reminding
// params:
// msgTemplate - the template to use for sending reminder emails
// emailAddr   - the email address of the account getting reminder
// 
bool PasswordReminder::SendReminderUsingEmail(const string &msgTemplate, 
											  const string &emailAddr)
{
	// Check for empty template and email address!
	// can't move forward with those
	if (msgTemplate.empty())
	{
		SetError("Error: empty template used");
		return false;
	}
	if (emailAddr.empty())
	{
		SetError("Error: empty email address used\n");
			return false;
	}
	
	m_EmailAddr = emailAddr;
			
	// encrypt and encode the email address if possible
	if (!EncryptEncode())
		return false;
	
	// setup the address to make the query
	string urlPart = "";
	m_SendAddress = SendAspAddress;
	urlPart = "template"; 
	UrlEscape(urlPart); 
	m_SendAddress += urlPart; 
	m_SendAddress += "=";
	urlPart = msgTemplate; 
	UrlEscape(urlPart);	
	m_SendAddress += urlPart;
	m_SendAddress += "&"; 
	urlPart = "email"; 
	UrlEscape(urlPart); 
	m_SendAddress += urlPart; 
	m_SendAddress += "=";
	urlPart = m_EmailAddr; 
	UrlEscape(urlPart); 
	m_SendAddress += urlPart;
	
	if (!m_Cookie.empty() || (m_EncryptMethod != 0 && m_EncodeMethod != 0))
	{
		
		m_SendAddress += "&";
		urlPart = "cv"; 
		UrlEscape(urlPart);
		m_SendAddress += urlPart;  
		m_SendAddress += "=";
		urlPart = m_Cookie;
		UrlEscape(urlPart);
		m_SendAddress += urlPart;
		
	}
	
	// Query the send page Send.aspx and fill a buffer with the results
	char httpBuffer[HTTP_BUFFER_SIZE];
	memset(&httpBuffer[0], 0, HTTP_BUFFER_SIZE);
	GHTTPRequest sendRequest = ghttpGet(m_SendAddress.c_str(), GHTTPTrue, 
		SendCompleteCallback, (void *)httpBuffer);

	// Check the buffer if empty!
	string  bufferCopy(httpBuffer?httpBuffer:"");
	if (bufferCopy.empty())
	{
		SetError("Error: empty buffer received from query to Send.aspx");
		return false;
	}
	
	// Read results to determine if the query was successful or not
	char *tokenizer = strtok(const_cast<char*>(bufferCopy.data()), " \r\n");
	if (strcmp(tokenizer, "sent") != 0)
	{
		string temp;
		temp += "Error: bad buffer received from query to Send.aspx\n";
		temp += "Message did not contain 'sent':\n";
		temp += tokenizer;
		temp += strtok(NULL, "\r\n");
		SetError(temp.c_str());
		return false;
	}

	// save the results if we had a successful send
	m_ResultString = tokenizer;
	return true;
}

bool PasswordReminder::SendReminderUsingId(const string &msgTemplate, 
										   int profileId)
{
	char temp[33];
	// Check for empty template and email address!
	// can't move forward with those
	if (msgTemplate.empty())
	{
		SetError("Error: empty template used");
		return false;
	}

	if (profileId == 0 || profileId< 100000)
	{ 
		SetError("Error: invalid profile id used.");
		return false;
	}

	m_ProfileId = itoa(profileId, temp, 10);

	
	// Non encrypted and non encoded profile ID
	
	m_SendAddress += m_ProfileId;

	// setup the address to make the query
	string urlPart = "";
	m_SendAddress = SendAspAddress;
	urlPart = "template"; 
	UrlEscape(urlPart); 
	m_SendAddress += urlPart; 
	m_SendAddress += "=";
	urlPart += msgTemplate; 
	UrlEscape(urlPart);	
	m_SendAddress += urlPart;
	m_SendAddress += "&"; 
	urlPart = "pid";
	UrlEscape(urlPart);
	m_SendAddress += urlPart;
	m_SendAddress += "=";
	urlPart = m_ProfileId;
	UrlEscape(urlPart);
	m_SendAddress += urlPart;

	// Query the send page Send.aspx and fill a buffer with the results
	char httpBuffer[HTTP_BUFFER_SIZE];
	memset(&httpBuffer[0], 0, HTTP_BUFFER_SIZE);
	GHTTPRequest sendRequest = ghttpGet(m_SendAddress.c_str(), GHTTPTrue, 
		SendCompleteCallback, (void *)httpBuffer);

	// Check the buffer if empty!
	string  bufferCopy(httpBuffer?httpBuffer:"");
	if (bufferCopy.empty())
	{
		SetError("Error: empty buffer received from query to Send.aspx");
		return false;
	}
	
	// Read results to determine if the query was successful or not
	char *tokenizer = strtok(const_cast<char*>(bufferCopy.data()), " \r\n");
	if (strcmp(tokenizer, "sent") != 0)
	{
		string temp;
		temp += "Error: bad buffer received from query to Send.aspx\n";
		temp += "Message did not contain 'sent':\n";
		temp += tokenizer;
		temp += strtok(NULL, "\r\n");
		SetError(temp.c_str());
		return false;
	}
	
	// save the results if we had a successful send
	m_ResultString = tokenizer;
	return true;
}