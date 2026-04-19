// Password Reminder Module 
// Interface
///////////////////////////////////////////////////////////////////////////////

#ifndef __PASSWORD_REMINDER_H__
#define __PASSWORD_REMINDER_H__


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Includes

//#include <string>

// not used in this version
//#include "Singleton.h"

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Globals/Types
static const char *EncAspAddress = 
	"http://www.gamespyid.com/password/Enc.aspx?";
static const char *SendAspAddress = 
	"http://www.gamespyid.com/password/Send.aspx?";

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class PasswordReminder
{
	string	m_EmailAddr,
			m_EncAddress,
			m_SendAddress,
			m_Cookie,
			m_Key,
			m_ErrorString,
			m_ResultString,
			m_ProfileId;
	
	int m_EncryptMethod,
		m_EncodeMethod;
		
	bool m_Hardwired;
	bool EncryptEncode();
	bool UrlEscape(string &urlPart);
	void SetError(const char *errorStr)
	{
		m_ErrorString = errorStr;
	}
	
public:
	
	PasswordReminder();
	virtual ~PasswordReminder();
	
	bool Initialize(bool hardwire, const string &key,
					int encryptingMethod, int encodingMethod);
	
	bool RequestMethod();
	
	bool SendReminderUsingEmail(const string &msgTemplate, 
		const string &emailAddr);
	
	bool SendReminderUsingId(const string &msgTemplate, int profileId);
	
	const char * GetResults()
	{
		return m_ResultString.c_str();
	}

	const char * GetLastError()
	{
		return m_ErrorString.c_str();
	}
};


#endif