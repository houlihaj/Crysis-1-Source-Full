// LogFile.h: interface for the CLogFile class.
//
//////////////////////////////////////////////////////////////////////
 
#if !defined(AFX_LOGFILE_H__9809D818_0A64_4187_A2EB_C3878F61C229__INCLUDED_)
#define AFX_LOGFILE_H__9809D818_0A64_4187_A2EB_C3878F61C229__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ILog.h"
#include <IConsole.h>

//struct IConsole;
//struct ICVar;

//////////////////////////////////////////////////////////////////////////
// Global log functions.
//////////////////////////////////////////////////////////////////////////
//! Displays error message.
extern void Error( const char *format,... );
//! Log to console and file.
extern void Log( const char *format,... );
//! Display Warning dialog.
extern void Warning( const char *format,... );

/*!
 *	CLogFile implements ILog interface.
 */
class CLogFile : public ILogCallback
{
public:
	CLogFile();
	~CLogFile();

	static const char* GetLogFileName();
	static void AttachListBox(HWND hWndListBox) { m_hWndListBox = hWndListBox; };
	static void AttachEditBox(HWND hWndEditBox) { m_hWndEditBox = hWndEditBox; };

	//! Write to log spanpshot of current process memory usage.
	static CString GetMemUsage();

	static void WriteString(const char * pszString);
	static void WriteLine(const char * pszLine);
	static void FormatLine(PSTR pszMessage, ...);

	//////////////////////////////////////////////////////////////////////////
	// ILogCallback
	//////////////////////////////////////////////////////////////////////////
	virtual void OnWriteToConsole( const char *sText,bool bNewLine );
	virtual void OnWriteToFile( const char *sText,bool bNewLine );
	//////////////////////////////////////////////////////////////////////////

	static void AboutSystem();

private:
	static unsigned int GetCPUSpeed();
	static void GetCPUModel(char szType[]);

	static void OpenFile();

	// Attached control(s)
	static HWND m_hWndListBox;
	static HWND m_hWndEditBox;
	static bool m_bShowMemUsage;
};

#endif // !defined(AFX_LOGFILE_H__9809D818_0A64_4187_A2EB_C3878F61C229__INCLUDED_)
